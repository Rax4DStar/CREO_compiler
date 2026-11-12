#include <algorithm>
#include <exception>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "IR.hpp"
#include "IRGenerator.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyser.hpp"
#include "Token.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

std::unique_ptr<ProgramNode> parseSource(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenise();

    Parser parser(std::move(tokens));
    return parser.parseProgram();
}

IRProgram generateWithoutSemanticAnalysis(const std::string& source) {
    std::unique_ptr<ProgramNode> ast = parseSource(source);
    IRGenerator generator;
    return generator.generate(*ast);
}

IRProgram lower(const std::string& source) {
    std::unique_ptr<ProgramNode> ast = parseSource(source);

    SemanticAnalyser analyser;
    analyser.analyze(*ast);

    IRGenerator generator;
    return generator.generate(*ast);
}

const IRFunction& findFunction(
    const IRProgram& program,
    const std::string& name
) {
    const auto found = std::find_if(
        program.functions.begin(),
        program.functions.end(),
        [&](const IRFunction& function) {
            return function.name == name;
        }
    );

    require(
        found != program.functions.end(),
        "missing IR function '" + name + "'"
    );

    return *found;
}

std::size_t countOperation(
    const IRFunction& function,
    IROp operation
) {
    std::size_t count = 0;

    for (const IRBlock& block : function.blocks) {
        count += static_cast<std::size_t>(
            std::count_if(
                block.instructions.begin(),
                block.instructions.end(),
                [&](const IRInstruction& instruction) {
                    return instruction.op == operation;
                }
            )
        );
    }

    return count;
}

void expectGenerationError(
    const std::string& source,
    const std::string& expectedMessage
) {
    try {
        (void)generateWithoutSemanticAnalysis(source);
    } catch (const std::runtime_error& error) {
        const std::string actual = error.what();
        require(
            actual.find(expectedMessage) != std::string::npos,
            "expected IR error containing '" + expectedMessage +
                "', got '" + actual + "'"
        );
        return;
    }

    throw std::runtime_error(
        "expected IR generation to fail with '" + expectedMessage + "'"
    );
}

void requireValidBranchTargets(const IRFunction& function) {
    for (const IRBlock& block : function.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            if (instruction.op == IROp::Jump) {
                const auto target = std::find_if(
                    function.blocks.begin(),
                    function.blocks.end(),
                    [&](const IRBlock& candidate) {
                        return candidate.name == instruction.target;
                    }
                );
                require(
                    target != function.blocks.end(),
                    "jump references missing block '" + instruction.target + "'"
                );
            } else if (instruction.op == IROp::Branch) {
                const auto hasBlock = [&](const std::string& name) {
                    return std::find_if(
                        function.blocks.begin(),
                        function.blocks.end(),
                        [&](const IRBlock& candidate) {
                            return candidate.name == name;
                        }
                    ) != function.blocks.end();
                };
                require(
                    hasBlock(instruction.trueTarget),
                    "branch references missing true block '" +
                        instruction.trueTarget + "'"
                );
                require(
                    hasBlock(instruction.falseTarget),
                    "branch references missing false block '" +
                        instruction.falseTarget + "'"
                );
            }
        }
    }
}

void testArithmeticPrecedence() {
    const IRProgram program = lower(R"(
        fn main(): int {
            let value: int = 5 + 2 * 10;
            return value;
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    const IRBlock& entry = main.blocks.at(0);

    auto multiply = std::find_if(
        entry.instructions.begin(),
        entry.instructions.end(),
        [](const IRInstruction& instruction) {
            return instruction.op == IROp::Multiply;
        }
    );

    auto add = std::find_if(
        entry.instructions.begin(),
        entry.instructions.end(),
        [](const IRInstruction& instruction) {
            return instruction.op == IROp::Add;
        }
    );

    require(
        multiply != entry.instructions.end(),
        "expected multiply instruction"
    );

    require(
        add != entry.instructions.end(),
        "expected add instruction"
    );

    require(
        multiply < add,
        "multiplication must be lowered before addition"
    );
}

void testWhileControlFlow() {
    const IRProgram program = lower(R"(
        fn main(): int {
            let i: int = 0;

            while (i < 3) {
                i = i + 1;
            }

            return i;
        }
    )");

    const IRFunction& main = findFunction(program, "main");

    require(
        countOperation(main, IROp::Branch) == 1,
        "while loop should emit one conditional branch"
    );

    require(
        countOperation(main, IROp::Jump) >= 2,
        "while loop should emit entry and back-edge jumps"
    );

    require(
        main.blocks.size() == 4,
        "while loop should create entry, condition, body and exit blocks"
    );
}

void testValueAndVoidCalls() {
    const IRProgram program = lower(R"(
        fn add(a: int, b: int): int {
            return a + b;
        }

        fn main(): int {
            let result: int = add(20, 22);
            out(result);
            return result;
        }
    )");

    const IRFunction& main = findFunction(program, "main");

    const IRInstruction* addCall = nullptr;
    const IRInstruction* outCall = nullptr;

    for (const IRBlock& block : main.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            if (instruction.op != IROp::Call) {
                continue;
            }

            if (instruction.callee == "add") {
                addCall = &instruction;
            }

            if (instruction.callee == "out") {
                outCall = &instruction;
            }
        }
    }

    require(addCall != nullptr, "missing call to add");
    require(outCall != nullptr, "missing call to out");

    require(
        !addCall->dst.empty() && addCall->type == Type::Int,
        "non-void call should produce an int temporary"
    );

    require(
        outCall->dst.empty() && outCall->type == Type::Void,
        "void call should not produce a destination"
    );
}

void testVariableShadowingUsesDistinctStorage() {
    const IRProgram program = lower(R"(
        fn main(): int {
            let x: int = 1;

            if (true) {
                let x: int = 2;
                out(x);
            }

            return x;
        }
    )");

    const IRFunction& main = findFunction(program, "main");

    require(
        main.locals.size() == 2,
        "expected two local storage slots"
    );

    require(
        main.locals[0].name == "x",
        "first declaration should keep the readable name x"
    );

    require(
        main.locals[1].name == "x$1",
        "shadowed declaration should use x$1"
    );

    bool loadedOuter = false;
    bool loadedInner = false;

    for (const IRBlock& block : main.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            if (instruction.op != IROp::Load) {
                continue;
            }

            loadedOuter |= instruction.args.at(0) == "x";
            loadedInner |= instruction.args.at(0) == "x$1";
        }
    }

    require(
        loadedOuter && loadedInner,
        "IR should load both the outer and inner storage slots"
    );
}

void testEveryBlockEndsWithATerminator() {
    const IRProgram program = lower(R"(
        fn choose(flag: bool): int {
            if (flag) {
                return 1;
            } else {
                return 2;
            }
        }

        fn main(): int {
            return choose(true);
        }
    )");

    for (const IRFunction& function : program.functions) {
        for (const IRBlock& block : function.blocks) {
            require(
                !block.instructions.empty(),
                "basic block must not be empty"
            );

            require(
                isTerminator(block.instructions.back().op),
                "basic block '" + block.name +
                "' must end with a terminator"
            );
        }
    }
}

void testGeneratorCanBeReused() {
    const std::string source = R"(
        fn main(): int {
            return 1;
        }
    )";

    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenise();

    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> ast =
        parser.parseProgram();

    SemanticAnalyser analyser;
    analyser.analyze(*ast);

    IRGenerator generator;

    const IRProgram first = generator.generate(*ast);
    const IRProgram second = generator.generate(*ast);

    require(
        first.functions.size() == 1 &&
        second.functions.size() == 1,
        "generator state should reset between runs"
    );

    require(
        first.functions[0].blocks[0].instructions[0].dst == "%t0" &&
        second.functions[0].blocks[0].instructions[0].dst == "%t0",
        "temporary numbering should restart for each generation"
    );
}

void testParametersBecomeStorageAndLoads() {
    const IRProgram program = lower(R"(
        fn identity(value: int): int {
            return value;
        }

        fn main(): int {
            return identity(7);
        }
    )");

    const IRFunction& identity = findFunction(program, "identity");
    require(identity.parameters.size() == 1, "expected one IR parameter");
    require(identity.parameters[0].name == "value", "parameter name mismatch");
    require(identity.parameters[0].type == Type::Int, "parameter type mismatch");
    require(identity.locals.empty(), "parameter should not be a local");

    const IRBlock& entry = identity.blocks.at(0);
    require(
        entry.instructions.size() == 2 &&
            entry.instructions[0].op == IROp::Load &&
            entry.instructions[0].args == std::vector<std::string>{"value"} &&
            entry.instructions[1].op == IROp::Return &&
            entry.instructions[1].args ==
                std::vector<std::string>{entry.instructions[0].dst},
        "parameter return should load the parameter and return the loaded value"
    );
}

void testBooleanAndAllBinaryOperators() {
    const IRProgram program = lower(R"(
        fn main(): int {
            let value: int = -4 + 6 - 2 * 3 / 1;
            let less: bool = value < 1;
            let lessEqual: bool = value <= 1;
            let greater: bool = value > 1;
            let greaterEqual: bool = value >= 1;
            let equal: bool = value == 1;
            let notEqual: bool = value != 1;
            let negated: bool = !false;
            return value;
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    const std::vector<std::pair<IROp, std::string>> operations{
        {IROp::Negate, "unary minus"},
        {IROp::Add, "addition"},
        {IROp::Subtract, "subtraction"},
        {IROp::Multiply, "multiplication"},
        {IROp::Divide, "division"},
        {IROp::Less, "less-than comparison"},
        {IROp::LessEqual, "less-equal comparison"},
        {IROp::Greater, "greater-than comparison"},
        {IROp::GreaterEqual, "greater-equal comparison"},
        {IROp::Equal, "equality comparison"},
        {IROp::NotEqual, "inequality comparison"},
        {IROp::Not, "logical not"}
    };

    for (const auto& [operation, name] : operations) {
        require(
            countOperation(main, operation) == 1,
            "expected exactly one instruction for " + name
        );
    }

    const auto boolConstant = std::find_if(
        main.blocks[0].instructions.begin(),
        main.blocks[0].instructions.end(),
        [](const IRInstruction& instruction) {
            return instruction.op == IROp::ConstBool;
        }
    );
    require(boolConstant != main.blocks[0].instructions.end(), "missing bool constant");
    require(!boolConstant->boolValue, "false literal should lower to false");
    require(boolConstant->type == Type::Bool, "bool constant should have bool type");
}

void testIfWithoutElseHasFalsePathJoin() {
    const IRProgram program = lower(R"(
        fn main(): int {
            if (true) {
                out(1);
            }
            return 0;
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    const IRInstruction* branch = nullptr;
    for (const IRBlock& block : main.blocks) {
        for (const IRInstruction& instruction : block.instructions) {
            if (instruction.op == IROp::Branch) {
                branch = &instruction;
            }
        }
    }

    require(branch != nullptr, "if statement should emit a branch");
    require(
        branch->falseTarget == "if_end1",
        "if without else should use its join block as the false target"
    );
    require(
        std::any_of(
            main.blocks.begin(), main.blocks.end(),
            [&](const IRBlock& block) {
                return block.name == branch->falseTarget;
            }
        ),
        "if false target should be materialized as a block"
    );
}

void testNestedControlFlowTargetsExist() {
    const IRProgram program = lower(R"(
        fn main(): int {
            let i: int = 0;
            while (i < 2) {
                if (i == 1) {
                    out(i);
                } else {
                    i = i + 1;
                }
            }
            return i;
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    requireValidBranchTargets(main);
    require(
        countOperation(main, IROp::Branch) == 2,
        "nested while and if should emit two branches"
    );
}

void testVoidFunctionGetsImplicitReturn() {
    const IRProgram program = lower(R"(
        fn helper() {
            out(1);
        }
        fn main(): int {
            return 0;
        }
    )");

    const IRFunction& helper = findFunction(program, "helper");
    const IRBlock& entry = helper.blocks.at(0);
    require(
        entry.instructions.back().op == IROp::Return,
        "void function should end with an implicit return"
    );
    require(
        entry.instructions.back().type == Type::Void &&
            entry.instructions.back().args.empty(),
        "implicit void return should carry no value"
    );
}

void testExplicitReturnStopsUnreachableInstructions() {
    const IRProgram program = lower(R"(
        fn main(): int {
            return 0;
            out(1);
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    require(
        countOperation(main, IROp::Call) == 0,
        "instructions after return should not be emitted"
    );
    require(
        main.blocks.at(0).instructions.back().op == IROp::Return,
        "entry block should end at the explicit return"
    );
}

void testSiblingScopesUseDistinctStorageNames() {
    const IRProgram program = lower(R"(
        fn main(): int {
            if (true) { let item: int = 1; out(item); }
            if (false) { let item: int = 2; out(item); }
            return 0;
        }
    )");

    const IRFunction& main = findFunction(program, "main");
    require(main.locals.size() == 2, "expected a local for each sibling scope");
    require(main.locals[0].name == "item", "first local should retain its name");
    require(main.locals[1].name == "item$1", "second local name should be unique");
}

void testGeneratorRejectsMissingNonVoidReturn() {
    expectGenerationError(R"(
        fn main(): int {
            let value: int = 1;
        }
    )", "reaches the end without returning");
}

void testGeneratorRejectsUnknownFunction() {
    expectGenerationError(R"(
        fn main(): int {
            return missing();
        }
    )", "unknown function 'missing'");
}

void testGeneratorRejectsUnknownVariable() {
    expectGenerationError(R"(
        fn main(): int {
            return missing;
        }
    )", "unknown variable 'missing'");
}

void testGeneratorRejectsDuplicateFunctions() {
    expectGenerationError(R"(
        fn main(): int { return 0; }
        fn main(): int { return 1; }
    )", "duplicate function 'main'");
}

void testGeneratorRejectsGlobalStatements() {
    expectGenerationError(R"(
        let value: int = 1;
        fn main(): int { return 0; }
    )", "only functions are allowed at global scope");
}

bool runTest(
    const std::string& name,
    const std::function<void()>& test
) {
    try {
        test();
        std::cout << "[PASS] " << name << '\n';
        return true;
    }
    catch (const std::exception& error) {
        std::cerr
            << "[FAIL] " << name
            << ": " << error.what()
            << '\n';

        return false;
    }
}

} 
int main() {
    using Test = std::pair<std::string, std::function<void()>>;
    const std::vector<Test> tests{
        {"arithmetic precedence", testArithmeticPrecedence},
        {"while control flow", testWhileControlFlow},
        {"value and void calls", testValueAndVoidCalls},
        {"variable shadowing", testVariableShadowingUsesDistinctStorage},
        {"block terminators", testEveryBlockEndsWithATerminator},
        {"generator reuse", testGeneratorCanBeReused},
        {"parameters and loads", testParametersBecomeStorageAndLoads},
        {"boolean and binary operators", testBooleanAndAllBinaryOperators},
        {"if without else", testIfWithoutElseHasFalsePathJoin},
        {"nested branch targets", testNestedControlFlowTargetsExist},
        {"implicit void return", testVoidFunctionGetsImplicitReturn},
        {"unreachable instructions after return", testExplicitReturnStopsUnreachableInstructions},
        {"sibling scope storage", testSiblingScopesUseDistinctStorageNames},
        {"missing non-void return", testGeneratorRejectsMissingNonVoidReturn},
        {"unknown function", testGeneratorRejectsUnknownFunction},
        {"unknown variable", testGeneratorRejectsUnknownVariable},
        {"duplicate functions", testGeneratorRejectsDuplicateFunctions},
        {"global statements", testGeneratorRejectsGlobalStatements}
    };

    std::size_t passed = 0;
    for (const auto& [name, test] : tests) {
        if (runTest(name, test)) {
            ++passed;
        }
    }

    std::cout
        << passed << '/' << tests.size()
        << " IR generator tests passed\n";
    return passed == tests.size() ? 0 : 1;
}
