#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Lexer.hpp"
#include "Parser.hpp"
#include "SemanticAnalyser.hpp"

namespace {

void analyse(std::string source) {
    Lexer lexer(std::move(source));
    std::vector<Token> tokens = lexer.tokenise();

    Parser parser(std::move(tokens));
    std::unique_ptr<ProgramNode> program = parser.parseProgram();

    SemanticAnalyser analyser;
    analyser.analyze(*program);
}

void expectValid(const std::string& source) {
    analyse(source);
}

void expectInvalid(
    const std::string& source,
    const std::string& expectedMessage
) {
    try {
        analyse(source);
    } catch (const std::runtime_error& error) {
        const std::string actual = error.what();

        if (actual.find(expectedMessage) == std::string::npos) {
            throw std::runtime_error(
                "Expected an error containing '" +
                expectedMessage + "', got '" + actual + "'"
            );
        }

        return;
    }

    throw std::runtime_error(
        "Expected semantic analysis to fail"
    );
}

void acceptsValidProgram() {
    expectValid(R"(
        fn main(): int {
            let value: int = double(21);

            if (value == 42) {
                let visible: int = value;
                out(visible);
            }

            return value;
        }

        fn double(value: int): int {
            return value * 2;
        }
    )");
}

void allowsNestedShadowing() {
    expectValid(R"(
        fn main(): int {
            let value: int = 1;

            if (true) {
                let value: int = 2;
                out(value);
            }

            return value;
        }
    )");
}

void rejectsUndeclaredVariable() {
    expectInvalid(R"(
        fn main(): int {
            return missing;
        }
    )", "undeclared variable 'missing'");
}

void rejectsInitialiserTypeMismatch() {
    expectInvalid(R"(
        fn main(): int {
            let value: int = true;
            return 0;
        }
    )", "cannot initialise 'value'");
}

void rejectsOutOfScopeVariable() {
    expectInvalid(R"(
        fn main(): int {
            if (true) {
                let local: int = 1;
            }

            return local;
        }
    )", "undeclared variable 'local'");
}

void rejectsWrongArgumentType() {
    expectInvalid(R"(
        fn identity(value: int): int {
            return value;
        }

        fn main(): int {
            return identity(false);
        }
    )", "expects int, got bool");
}

void rejectsMissingReturnPath() {
    expectInvalid(R"(
        fn choose(flag: bool): int {
            if (flag) {
                return 1;
            }
        }

        fn main(): int {
            return choose(true);
        }
    )", "not all control paths return int");
}

void rejectsInvalidMainSignature() {
    expectInvalid(R"(
        fn main(value: int): int {
            return value;
        }
    )", "main function cannot take parameters");
}

void rejectsVoidVariable() {
    expectInvalid(R"(
        fn helper(): void {
            return;
        }

        fn main(): int {
            let value: void = helper();
            return 0;
        }
    )", "cannot have type void");
}

void acceptsForwardFunctionCalls() {
    expectValid(R"(
        fn main(): int {
            return increment(4);
        }

        fn increment(value: int): int {
            return value + 1;
        }
    )");
}

void acceptsReturnsFromBothIfBranches() {
    expectValid(R"(
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
}

void rejectsDuplicateFunction() {
    expectInvalid(R"(
        fn main(): int { return 0; }
        fn main(): int { return 1; }
    )", "function 'main' is already declared");
}

void rejectsDuplicateVariableInSameScope() {
    expectInvalid(R"(
        fn main(): int {
            let value: int = 1;
            let value: int = 2;
            return value;
        }
    )", "already declared in this scope");
}

void rejectsDuplicateParameter() {
    expectInvalid(R"(
        fn identity(value: int, value: int): int {
            return value;
        }
        fn main(): int { return 0; }
    )", "duplicate parameter 'value'");
}

void rejectsVoidParameter() {
    expectInvalid(R"(
        fn invalid(value: void) { return; }
        fn main(): int { return 0; }
    )", "parameter 'value' cannot have type void");
}

void rejectsProgramWithoutMain() {
    expectInvalid(R"(
        fn helper(): int { return 1; }
    )", "program must define 'fn main(): int'");
}

void rejectsMainWithWrongReturnType() {
    expectInvalid(R"(
        fn main(): bool { return true; }
    )", "main function must return int");
}

void rejectsValueReturnedFromVoidFunction() {
    expectInvalid(R"(
        fn helper(): void { return 1; }
        fn main(): int { return 0; }
    )", "void function cannot return a value");
}

void rejectsBareReturnFromNonVoidFunction() {
    expectInvalid(R"(
        fn helper(): int { return; }
        fn main(): int { return 0; }
    )", "expected a return value of type int");
}

void rejectsMismatchedFunctionReturnType() {
    expectInvalid(R"(
        fn helper(): int { return true; }
        fn main(): int { return 0; }
    )", "expected return type int, got bool");
}

void rejectsNonBooleanIfCondition() {
    expectInvalid(R"(
        fn main(): int {
            if (1) { return 1; }
            return 0;
        }
    )", "'if' condition must have type bool");
}

void rejectsNonBooleanWhileCondition() {
    expectInvalid(R"(
        fn main(): int {
            while (1) { }
            return 0;
        }
    )", "'while' condition must have type bool");
}

void rejectsArithmeticOnBooleanOperands() {
    expectInvalid(R"(
        fn main(): int { return 1 + true; }
    )", "arithmetic operators require int operands");
}

void rejectsEqualityBetweenDifferentTypes() {
    expectInvalid(R"(
        fn main(): int {
            if (1 == true) { return 1; }
            return 0;
        }
    )", "equality operands must have the same type");
}

void rejectsAssignmentTypeMismatch() {
    expectInvalid(R"(
        fn main(): int {
            let value: int = 1;
            value = false;
            return value;
        }
    )", "cannot assign bool to variable 'value'");
}

void rejectsUndefinedFunction() {
    expectInvalid(R"(
        fn main(): int { return missing(); }
    )", "undefined function 'missing'");
}

void rejectsWrongFunctionArgumentCount() {
    expectInvalid(R"(
        fn identity(value: int): int { return value; }
        fn main(): int { return identity(); }
    )", "expects 1 argument(s), got 0");
}

void rejectsGlobalVariableDeclaration() {
    expectInvalid(R"(
        let value: int = 1;
        fn main(): int { return 0; }
    )", "only function declarations are allowed at global scope");
}

void rejectsUnaryNotOnInteger() {
    expectInvalid(R"(
        fn main(): int { return !1; }
    )", "unary '!' requires a bool operand");
}

} // namespace

int main() {
    struct TestCase {
        const char* name;
        std::function<void()> run;
    };

    const std::vector<TestCase> tests{
        {"accepts valid program", acceptsValidProgram},
        {"allows nested shadowing", allowsNestedShadowing},
        {"rejects undeclared variable", rejectsUndeclaredVariable},
        {"rejects initialiser type mismatch", rejectsInitialiserTypeMismatch},
        {"rejects out-of-scope variable", rejectsOutOfScopeVariable},
        {"rejects wrong argument type", rejectsWrongArgumentType},
        {"rejects missing return path", rejectsMissingReturnPath},
        {"rejects invalid main signature", rejectsInvalidMainSignature},
        {"rejects void variable", rejectsVoidVariable},
        {"accepts forward function calls", acceptsForwardFunctionCalls},
        {"accepts returns from both if branches", acceptsReturnsFromBothIfBranches},
        {"rejects duplicate function", rejectsDuplicateFunction},
        {"rejects duplicate variable in same scope", rejectsDuplicateVariableInSameScope},
        {"rejects duplicate parameter", rejectsDuplicateParameter},
        {"rejects void parameter", rejectsVoidParameter},
        {"rejects program without main", rejectsProgramWithoutMain},
        {"rejects main with wrong return type", rejectsMainWithWrongReturnType},
        {"rejects value returned from void function", rejectsValueReturnedFromVoidFunction},
        {"rejects bare return from non-void function", rejectsBareReturnFromNonVoidFunction},
        {"rejects mismatched function return type", rejectsMismatchedFunctionReturnType},
        {"rejects non-boolean if condition", rejectsNonBooleanIfCondition},
        {"rejects non-boolean while condition", rejectsNonBooleanWhileCondition},
        {"rejects arithmetic on booleans", rejectsArithmeticOnBooleanOperands},
        {"rejects equality across types", rejectsEqualityBetweenDifferentTypes},
        {"rejects assignment type mismatch", rejectsAssignmentTypeMismatch},
        {"rejects undefined function", rejectsUndefinedFunction},
        {"rejects wrong function argument count", rejectsWrongFunctionArgumentCount},
        {"rejects global variable", rejectsGlobalVariableDeclaration},
        {"rejects unary not on integer", rejectsUnaryNotOnInteger},
    };

    std::size_t passed = 0;

    for (const TestCase& test : tests) {
        try {
            test.run();
            std::cout << "[PASS] " << test.name << '\n';
            ++passed;
        } catch (const std::exception& error) {
            std::cerr
                << "[FAIL] " << test.name
                << ": " << error.what() << '\n';
        }
    }

    std::cout
        << passed << '/' << tests.size()
        << " tests passed\n";

    return passed == tests.size() ? 0 : 1;
}
