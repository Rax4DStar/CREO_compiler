#pragma once

#include <string>

#include "AST.hpp"
#include "SymbolTable.hpp"
#include "Type.hpp"

class SemanticAnalyser final : public ASTVisitor {
public:
    void analyze(ProgramNode& root);

    void visit(IntLiteralNode& node) override;
    void visit(BoolLiteralNode& node) override;
    void visit(IdentifierNode& node) override;
    void visit(BinaryExpressionNode& node) override;
    void visit(UnaryExpressionNode& node) override;
    void visit(FunctionCallNode& node) override;

    void visit(BlockNode& node) override;
    void visit(LetStatementNode& node) override;
    void visit(ReturnStatementNode& node) override;
    void visit(FunctionDeclarationNode& node) override;
    void visit(ExpressionStatementNode& node) override;
    void visit(AssignmentNode& node) override;
    void visit(WhileStatementNode& node) override;
    void visit(IfStatementNode& node) override;
    void visit(ProgramNode& node) override;

private:
    SymbolTable _symbols;
    Type _currentReturnType{Type::Void};
    Type _lastType{Type::Unknown};

    void registerFunctions(ProgramNode& root);
    void validateMainFunction() const;

    [[nodiscard]] bool blockAlwaysReturns(const BlockNode& block) const;
    [[nodiscard]] Type evaluate(ExpressionNode& expression);

    [[noreturn]] void semanticError(
        const ASTNode& node,
        const std::string& message
    ) const;
};