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

void parsesDefaultVoidReturnType() {
    auto program = parse(R"(
        fn greet() {
            return;
        }
    )");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );
    require(function.returnType == Type::Void, "Expected default void return type");

    auto& statement = requireNode<ReturnStatementNode>(
        *function.body->statements.at(0),
        "Expected a return statement"
    );
    require(statement.expression == nullptr, "Expected a valueless return");
}

void parsesBooleanUnaryAndEqualityExpressions() {
    auto program = parse(R"(
        fn main(): bool {
            return !false == true;
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
    auto& equality = requireNode<BinaryExpressionNode>(
        *returnStatement.expression,
        "Expected equality at the expression root"
    );
    require(equality.oper == BinaryOp::Equal, "Expected equality operator");

    auto& negation = requireNode<UnaryExpressionNode>(
        *equality.left,
        "Expected unary negation on the left"
    );
    require(negation.oper == UnaryOp::Not, "Expected logical not operator");
    require(
        requireNode<BoolLiteralNode>(*negation.right, "Expected false literal").value == false,
        "Expected false as the negation operand"
    );
}

void respectsLeftAssociativity() {
    auto program = parse(R"(
        fn main(): int {
            return 10 - 3 - 2;
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
    auto& outer = requireNode<BinaryExpressionNode>(
        *returnStatement.expression,
        "Expected subtraction at the expression root"
    );
    require(outer.oper == BinaryOp::Subtract, "Expected root subtraction");

    auto& inner = requireNode<BinaryExpressionNode>(
        *outer.left,
        "Expected left-associated subtraction"
    );
    require(inner.oper == BinaryOp::Subtract, "Expected subtraction on the left");
}

void parsesComparisonAndArithmeticPrecedence() {
    auto program = parse(R"(
        fn main(): bool {
            return 1 + 2 * 3 < 8;
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
    auto& comparison = requireNode<BinaryExpressionNode>(
        *returnStatement.expression,
        "Expected comparison at the expression root"
    );
    require(comparison.oper == BinaryOp::Less, "Expected '<' at root");

    auto& addition = requireNode<BinaryExpressionNode>(
        *comparison.left,
        "Expected addition on the left side of comparison"
    );
    require(addition.oper == BinaryOp::Add, "Expected addition below comparison");
    requireNode<BinaryExpressionNode>(
        *addition.right,
        "Expected multiplication to bind more tightly than addition"
    );
}

void parsesWhileLoopAndAssignment() {
    auto program = parse(R"(
        fn main(): int {
            let i: int = 0;
            while (i < 2) {
                i = i + 1;
            }
            return i;
        }
    )");

    auto& function = requireNode<FunctionDeclarationNode>(
        *program->nodes.at(0),
        "Expected a function declaration"
    );
    auto& loop = requireNode<WhileStatementNode>(
        *function.body->statements.at(1),
        "Expected a while statement"
    );
    require(loop.body->statements.size() == 1, "Expected one loop-body statement");
    requireNode<AssignmentNode>(
        *loop.body->statements.at(0),
        "Expected an assignment in the loop body"
    );
}

void parsesZeroArgumentFunctionCall() {
    auto program = parse(R"(
        fn main(): int {
            return answer();
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
    auto& call = requireNode<FunctionCallNode>(
        *returnStatement.expression,
        "Expected a function call"
    );
    require(call.name == "answer", "Expected call to answer");
    require(call.arguments.empty(), "Expected no call arguments");
}

void rejectsIntegerLiteralOverflow() {
    try {
        (void)parse(R"(
            fn main(): int {
                return 999999999999999999999999999999999999;
            }
        )");
    } catch (const std::runtime_error& error) {
        require(
            std::string(error.what()).find("Integer literal is out of range")
                != std::string::npos,
            "Expected an integer-overflow diagnostic"
        );
        return;
    }

    throw std::runtime_error("Expected integer literal overflow to fail parsing");
}

} 

int main() {
    using Test = std::pair<std::string, std::function<void()>>;

    const std::vector<Test> tests{
        {"parses function declaration", parsesFunctionDeclaration},
        {"respects operator precedence", respectsOperatorPrecedence},
        {"parentheses override precedence", parenthesesOverridePrecedence},
        {"parses nested function calls", parsesNestedFunctionCalls},
        {"reports missing semicolon", reportsMissingSemicolon},
        {"parses default void return type", parsesDefaultVoidReturnType},
        {"parses boolean unary and equality expressions", parsesBooleanUnaryAndEqualityExpressions},
        {"respects left associativity", respectsLeftAssociativity},
        {"respects comparison and arithmetic precedence", parsesComparisonAndArithmeticPrecedence},
        {"parses while loops and assignments", parsesWhileLoopAndAssignment},
        {"parses zero-argument calls", parsesZeroArgumentFunctionCall},
        {"rejects integer literal overflow", rejectsIntegerLiteralOverflow}
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

