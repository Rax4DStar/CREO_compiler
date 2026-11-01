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