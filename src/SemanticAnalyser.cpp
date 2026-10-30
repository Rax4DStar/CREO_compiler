#include "SemanticAnalyser.hpp"

#include <stdexcept>
#include <string>
#include <utility>

void SemanticAnalyser::analyze(ProgramNode& root) {
    _symbols = SymbolTable{};
    _currentReturnType = Type::Void;
    _lastType = Type::Unknown;

    registerFunctions(root);
    validateMainFunction();
    root.accept(*this);
}

void SemanticAnalyser::registerFunctions(ProgramNode& root) {
    for (auto& statement : root.nodes) {
        auto* function =
            dynamic_cast<FunctionDeclarationNode*>(statement.get());

        if (function == nullptr) {
            continue;
        }

        FunctionSymbol symbol;
        symbol.name = function->name;
        symbol.returnType = function->returnType;

        for (const ParameterNode& parameter : function->parameters) {
            if (parameter.type == Type::Void) {
                semanticError(
                    *function,
                    "parameter '" + parameter.name +
                    "' cannot have type void"
                );
            }

            symbol.parameterTypes.push_back(parameter.type);
        }

        if (!_symbols.declareFunction(std::move(symbol))) {
            semanticError(
                *function,
                "function '" + function->name +
                "' is already declared"
            );
        }
    }
}

void SemanticAnalyser::validateMainFunction() const {
    const auto mainFunction = _symbols.lookupFunction("main");

    if (!mainFunction) {
        throw std::runtime_error(
            "Semantic error: program must define 'fn main(): int'"
        );
    }

    if (!mainFunction->parameterTypes.empty()) {
        throw std::runtime_error(
            "Semantic error: main function cannot take parameters"
        );
    }

    if (mainFunction->returnType != Type::Int) {
        throw std::runtime_error(
            "Semantic error: main function must return int"
        );
    }
}

bool SemanticAnalyser::blockAlwaysReturns(const BlockNode& block) const {
    for (const auto& statement : block.statements) {
        if (dynamic_cast<const ReturnStatementNode*>(statement.get()) != nullptr) {
            return true;
        }
        const auto* ifStatement =
            dynamic_cast<const IfStatementNode*>(statement.get());

        if (
            ifStatement != nullptr &&
            ifStatement->elseBranch != nullptr &&
            blockAlwaysReturns(*ifStatement->thenBranch) &&
            blockAlwaysReturns(*ifStatement->elseBranch)
        ) {
            return true;
        }
    }
    return false;
}

Type SemanticAnalyser::evaluate(ExpressionNode& expression) {
    expression.accept(*this);
    return _lastType;
}

void SemanticAnalyser::semanticError(
    const ASTNode& node,
    const std::string& message
) const {
    throw std::runtime_error(
        "Semantic error at " +
        std::to_string(node.line) + ":" +
        std::to_string(node.column) + ": " +
        message
    );
}

void SemanticAnalyser::visit(ProgramNode& node) {
    for (auto& statement : node.nodes) {
        auto* function =
            dynamic_cast<FunctionDeclarationNode*>(statement.get());

        if (function == nullptr) {
            semanticError(
                *statement,
                "only function declarations are allowed at global scope"
            );
        }
        function->accept(*this);
    }
}

void SemanticAnalyser::visit(FunctionDeclarationNode& node) {
    _symbols.enterScope();
    const Type previousReturnType = _currentReturnType;
    _currentReturnType = node.returnType;

    for (const ParameterNode& parameter : node.parameters) {
        if (!_symbols.declareVariable(parameter.name, parameter.type)) {
            semanticError(
                node,
                "duplicate parameter '" + parameter.name + "'"
            );
        }
    }

    for (auto& statement : node.body->statements) {
        statement->accept(*this);
    }

    if (
        node.returnType != Type::Void &&
        !blockAlwaysReturns(*node.body)
    ) {
        semanticError(
            node,
            "not all control paths return " +
            std::string(toString(node.returnType))
        );
    }
    _currentReturnType = previousReturnType;
    _symbols.exitScope();
}

void SemanticAnalyser::visit(BlockNode& node) {
    _symbols.enterScope();

    for (auto& statement : node.statements) {
        statement->accept(*this);
    }
    _symbols.exitScope();
}
void SemanticAnalyser::visit(LetStatementNode& node) {
    if (node.type == Type::Void) {
        semanticError(
            node,
            "variable '" + node.identifier +
            "' cannot have type void"
        );
    }

    const Type initialiserType = evaluate(*node.initialiser);

    if (initialiserType != node.type) {
        semanticError(
            node,
            "cannot initialise '" + node.identifier +
            "' of type " + toString(node.type) +
            " with " + toString(initialiserType)
        );
    }

    if (!_symbols.declareVariable(node.identifier, node.type)) {
        semanticError(
            node,
            "variable '" + node.identifier +
            "' is already declared in this scope"
        );
    }
}

void SemanticAnalyser::visit(AssignmentNode& node) {
    const auto variable = _symbols.lookupVariable(node.identifier);

    if (!variable) {
        semanticError(
            node,
            "undeclared variable '" + node.identifier + "'"
        );
    }

    const Type expressionType = evaluate(*node.expression);

    if (expressionType != variable->type) {
        semanticError(
            node,
            "cannot assign " + std::string(toString(expressionType)) +
            " to variable '" + node.identifier +
            "' of type " + toString(variable->type)
        );
    }
}

void SemanticAnalyser::visit(ReturnStatementNode& node) {
    if (_currentReturnType == Type::Void) {
        if (node.expression != nullptr) {
            semanticError(
                node,
                "void function cannot return a value"
            );
        }
        return;
    }

    if (node.expression == nullptr) {
        semanticError(
            node,
            "expected a return value of type " +
            std::string(toString(_currentReturnType))
        );
    }

    const Type returnedType = evaluate(*node.expression);

    if (returnedType != _currentReturnType) {
        semanticError(
            node,
            "expected return type " +
            std::string(toString(_currentReturnType)) +
            ", got " + toString(returnedType)
        );
    }
}

void SemanticAnalyser::visit(IfStatementNode& node) {
    if (evaluate(*node.condition) != Type::Bool) {
        semanticError(node, "'if' condition must have type bool");
    }

    node.thenBranch->accept(*this);

    if (node.elseBranch != nullptr) {
        node.elseBranch->accept(*this);
    }
}

void SemanticAnalyser::visit(WhileStatementNode& node) {
    if (evaluate(*node.condition) != Type::Bool) {
        semanticError(node, "'while' condition must have type bool");
    }

    node.body->accept(*this);
}

void SemanticAnalyser::visit(ExpressionStatementNode& node) {
    (void)evaluate(*node.expression);
}
void SemanticAnalyser::visit(FunctionCallNode& node) {
    const auto function = _symbols.lookupFunction(node.name);

    if (!function) {
        semanticError(
            node,
            "undefined function '" + node.name + "'"
        );
    }

    if (node.arguments.size() != function->parameterTypes.size()) {
        semanticError(
            node,
            "function '" + node.name + "' expects " +
            std::to_string(function->parameterTypes.size()) +
            " argument(s), got " +
            std::to_string(node.arguments.size())
        );
    }

    for (std::size_t index = 0; index < node.arguments.size(); ++index) {
        const Type actualType = evaluate(*node.arguments[index]);
        const Type expectedType = function->parameterTypes[index];

        if (actualType != expectedType) {
            semanticError(
                *node.arguments[index],
                "argument " + std::to_string(index + 1) +
                " of function '" + node.name +
                "' expects " + toString(expectedType) +
                ", got " + toString(actualType)
            );
        }
    }

    _lastType = function->returnType;
}

void SemanticAnalyser::visit(IdentifierNode& node) {
    const auto variable = _symbols.lookupVariable(node.name);

    if (!variable) {
        semanticError(
            node,
            "undeclared variable '" + node.name + "'"
        );
    }

    _lastType = variable->type;
}

void SemanticAnalyser::visit(IntLiteralNode&) {
    _lastType = Type::Int;
}

void SemanticAnalyser::visit(BoolLiteralNode&) {
    _lastType = Type::Bool;
}

void SemanticAnalyser::visit(UnaryExpressionNode& node) {
    const Type operandType = evaluate(*node.right);

    switch (node.oper) {
        case UnaryOp::Negate:
            if (operandType != Type::Int) {
                semanticError(node, "unary '-' requires an int operand");
            }
            _lastType = Type::Int;
            return;

        case UnaryOp::Not:
            if (operandType != Type::Bool) {
                semanticError(node, "unary '!' requires a bool operand");
            }
            _lastType = Type::Bool;
            return;
    }

    semanticError(node, "unknown unary operator");
}

void SemanticAnalyser::visit(BinaryExpressionNode& node) {
    const Type leftType = evaluate(*node.left);
    const Type rightType = evaluate(*node.right);

    switch (node.oper) {
        case BinaryOp::Add:
        case BinaryOp::Subtract:
        case BinaryOp::Multiply:
        case BinaryOp::Divide:
            if (leftType != Type::Int || rightType != Type::Int) {
                semanticError(
                    node,
                    "arithmetic operators require int operands"
                );
            }
            _lastType = Type::Int;
            return;

        case BinaryOp::Less:
        case BinaryOp::LessEqual:
        case BinaryOp::Greater:
        case BinaryOp::GreaterEqual:
            if (leftType != Type::Int || rightType != Type::Int) {
                semanticError(
                    node,
                    "ordering operators require int operands"
                );
            }
            _lastType = Type::Bool;
            return;

        case BinaryOp::Equal:
        case BinaryOp::NotEqual:
            if (leftType == Type::Void || rightType == Type::Void) {
                semanticError(
                    node,
                    "equality operators cannot compare void expressions"
                );
            }

            if (leftType != rightType) {
                semanticError(
                    node,
                    "equality operands must have the same type"
                );
            }

            _lastType = Type::Bool;
            return;
    }

    semanticError(node, "unknown binary operator");
}