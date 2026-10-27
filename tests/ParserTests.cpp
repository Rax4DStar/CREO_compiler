#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "AST.hpp"
#include "Lexer.hpp"
#include "Parser.hpp"

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

template <typename Expected, typename Actual>
Expected& requireNode(Actual& node, const std::string& message) {
    auto* result = dynamic_cast<Expected*>(&node);

    if (result == nullptr) {
        throw std::runtime_error(message);
    }

    return *result;
}

std::unique_ptr<ProgramNode> parse(std::string source) {
    Lexer lexer(std::move(source));
    std::vector<Token> tokens = lexer.tokenise();

    Parser parser(std::move(tokens));
    return parser.parseProgram();
}

void parsesFunctionDeclaration() {
    auto program = parse(R"(
        fn add(a: int, b: int): int {
            return a + b;
        }
    )");

    require(program->nodes.size() == 1, "Expected one top-level node");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );

    require(function.name == "add", "Expected function name 'add'");
    require(function.parameters.size() == 2, "Expected two parameters");
    require(function.returnType == Type::Int, "Expected int return type");
    require(function.body->statements.size() == 1, "Expected one statement");
}

void respectsOperatorPrecedence() {
    auto program = parse(R"(
        fn main(): int {
            return 1 + 2 * 3;
        }
    )");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );

    auto& returnStatement = requireNode<ReturnStatementNode>(
        *function.body->statements.at(0),
        "Expected a return statement"
    );

    auto& addition = requireNode<BinaryExpressionNode>(
        *returnStatement.expression,
        "Expected addition at the expression root"
    );

    require(addition.oper == BinaryOp::Add, "Expected root operator '+'");

    auto& multiplication = requireNode<BinaryExpressionNode>(
        *addition.right,
        "Expected multiplication on the right"
    );

    require(
        multiplication.oper == BinaryOp::Multiply,
        "Expected '*' to bind more tightly than '+'"
    );
}

void parenthesesOverridePrecedence() {
    auto program = parse(R"(
        fn main(): int {
            return (1 + 2) * 3;
        }
    )");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );

    auto& returnStatement = requireNode<ReturnStatementNode>(
        *function.body->statements.at(0),
        "Expected a return statement"
    );

    auto& multiplication = requireNode<BinaryExpressionNode>(
        *returnStatement.expression,
        "Expected multiplication at the expression root"
    );

    require(
        multiplication.oper == BinaryOp::Multiply,
        "Expected root operator '*'"
    );

    auto& addition = requireNode<BinaryExpressionNode>(
        *multiplication.left,
        "Expected parenthesised addition on the left"
    );

    require(addition.oper == BinaryOp::Add, "Expected left operator '+'");
}

void parsesNestedFunctionCalls() {
    auto program = parse(R"(
        fn main(): int {
            out(add(1, 2));
            return 0;
        }
    )");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );

    auto& statement = requireNode<ExpressionStatementNode>(
        *function.body->statements.at(0),
        "Expected an expression statement"
    );

    auto& outerCall = requireNode<FunctionCallNode>(
        *statement.expression,
        "Expected outer function call"
    );

    require(outerCall.name == "out", "Expected call to 'out'");
    require(outerCall.arguments.size() == 1, "Expected one argument");

    auto& innerCall = requireNode<FunctionCallNode>(
        *outerCall.arguments.at(0),
        "Expected nested function call"
    );

    require(innerCall.name == "add", "Expected nested call to 'add'");
    require(innerCall.arguments.size() == 2, "Expected two arguments");
}

void reportsMissingSemicolon() {
    try {
        parse(R"(
            fn main(): int {
                let x: int = 1
                return x;
            }
        )");

        throw std::runtime_error("Expected parsing to fail");
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();

        require(
            message.find("Expected ';'") != std::string::npos,
            "Expected a missing-semicolon error"
        );

        require(
            message.find("Parse error at") != std::string::npos,
            "Expected source location in parser error"
        );
    }
}

} 

int main() {
    using Test = std::pair<std::string, std::function<void()>>;

    const std::vector<Test> tests{
        {"parses function declaration", parsesFunctionDeclaration},
        {"respects operator precedence", respectsOperatorPrecedence},
        {"parentheses override precedence", parenthesesOverridePrecedence},
        {"parses nested function calls", parsesNestedFunctionCalls},
        {"reports missing semicolon", reportsMissingSemicolon}
    };

    int failed = 0;

    for (const auto& [name, test] : tests) {
        try {
            test();
            std::cout << "[PASS] " << name << '\n';
        } catch (const std::exception& error) {
            ++failed;
            std::cerr
                << "[FAIL] " << name
                << ": " << error.what() << '\n';
        }
    }

    if (failed != 0) {
        std::cerr << failed << " test(s) failed\n";
        return 1;
    }

    std::cout << "All parser tests passed\n";
    return 0;
}

