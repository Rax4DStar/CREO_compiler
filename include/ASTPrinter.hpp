#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <string_view>

#include "AST.hpp"

class ASTPrinter final : public ASTVisitor {
public:
    explicit ASTPrinter(std::ostream& output)
        : _output(output) {}

    void print(ProgramNode& node) {
        node.accept(*this);
    }

    void visit(IntLiteralNode& node) override {
        line("IntLiteral " + std::to_string(node.value));
    }

    void visit(BoolLiteralNode& node) override {
        line(
            std::string("BoolLiteral ") +
            (node.value ? "true" : "false")
        );
    }

    void visit(IdentifierNode& node) override {
        line("Identifier " + node.name);
    }

    void visit(BinaryExpressionNode& node) override {
        line(
            "BinaryExpression " +
            std::string(toString(node.oper))
        );

        withIndent([&] {
            node.left->accept(*this);
            node.right->accept(*this);
        });
    }

    void visit(UnaryExpressionNode& node) override {
        line(
            "UnaryExpression " +
            std::string(toString(node.oper))
        );

        withIndent([&] {
            node.right->accept(*this);
        });
    }

    void visit(FunctionCallNode& node) override {
        line("FunctionCall " + node.name);

        withIndent([&] {
            for (auto& argument : node.arguments) {
                argument->accept(*this);
            }
        });
    }

    void visit(BlockNode& node) override {
        line("Block");

        withIndent([&] {
            for (auto& statement : node.statements) {
                statement->accept(*this);
            }
        });
    }

    void visit(LetStatementNode& node) override {
        line(
            "Let " + node.identifier +
            ": " + std::string(toString(node.type))
        );

        withIndent([&] {
            node.initialiser->accept(*this);
        });
    }

    void visit(ReturnStatementNode& node) override {
        line("Return");

        if (node.expression) {
            withIndent([&] {
                node.expression->accept(*this);
            });
        }
    }

    void visit(FunctionDeclarationNode& node) override {
        line(
            "Function " + node.name +
            ": " + std::string(toString(node.returnType))
        );

        withIndent([&] {
            if (!node.parameters.empty()) {
                line("Parameters");

                withIndent([&] {
                    for (const auto& parameter : node.parameters) {
                        line(
                            parameter.name +
                            ": " +
                            std::string(toString(parameter.type))
                        );
                    }
                });
            }

            node.body->accept(*this);
        });
    }

    void visit(ExpressionStatementNode& node) override {
        line("ExpressionStatement");

        withIndent([&] {
            node.expression->accept(*this);
        });
    }

    void visit(AssignmentNode& node) override {
        line("Assignment " + node.identifier);

        withIndent([&] {
            node.expression->accept(*this);
        });
    }

    void visit(WhileStatementNode& node) override {
        line("While");

        withIndent([&] {
            line("Condition");

            withIndent([&] {
                node.condition->accept(*this);
            });

            node.body->accept(*this);
        });
    }

    void visit(IfStatementNode& node) override {
        line("If");

        withIndent([&] {
            line("Condition");

            withIndent([&] {
                node.condition->accept(*this);
            });

            line("Then");
            withIndent([&] {
                node.thenBranch->accept(*this);
            });

            if (node.elseBranch) {
                line("Else");

                withIndent([&] {
                    node.elseBranch->accept(*this);
                });
            }
        });
    }

    void visit(ProgramNode& node) override {
        line("Program");

        withIndent([&] {
            for (auto& statement : node.nodes) {
                statement->accept(*this);
            }
        });
    }

private:
    std::ostream& _output;
    std::size_t _indent{0};

    void line(std::string_view text) {
        _output << std::string(_indent * 2, ' ')
                << text << '\n';
    }

    template <typename Function>
    void withIndent(Function&& function) {
        ++_indent;
        function();
        --_indent;
    }
};

