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

IRProgram lower(const std::string& source) {
    Lexer lexer(source);
    std::vector<Token> tokens = lexer.tokenise();

    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> ast =
        parser.parseProgram();

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

void runTest(
    const std::string& name,
    const std::function<void()>& test
) {
    try {
        test();
        std::cout << "[PASS] " << name << '\n';
    }
    catch (const std::exception& error) {
        std::cerr
            << "[FAIL] " << name
            << ": " << error.what()
            << '\n';

        throw;
    }
}

} 
int main() {
    try {
        runTest(
            "arithmetic precedence",
            testArithmeticPrecedence
        );

        runTest(
            "while control flow",
            testWhileControlFlow
        );

        runTest(
            "value and void calls",
            testValueAndVoidCalls
        );

        runTest(
            "variable shadowing",
            testVariableShadowingUsesDistinctStorage
        );

        runTest(
            "block terminators",
            testEveryBlockEndsWithATerminator
        );

        runTest(
            "generator reuse",
            testGeneratorCanBeReused
        );
    }
    catch (...) {
        return 1;
    }

    std::cout << "6/6 IR generator tests passed\n";
    return 0;
}