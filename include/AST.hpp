#pragma once 

#include <memory>
#include <string>
#include <vector>

#include <Operator.hpp>
#include "Type.hpp"

struct IntLiteralNode;
struct BoolLiteralNode;
struct IdentifierNode;
struct BinaryExpressionNode;
struct UnaryExpressionNode;
struct FunctionCallNode;

struct BlockNode;
struct LetStatementNode;
struct ReturnStatementNode;
struct FunctionDeclarationNode;
struct ExpressionStatementNode;
struct AssignmentNode;
struct WhileStatementNode;
struct IfStatementNode;
struct ProgramNode;

class ASTVisitor{
    public:
        virtual ~ASTVisitor() = default;

        virtual void visit(IntLiteralNode&) = 0;
        virtual void visit(BoolLiteralNode&) = 0;
        virtual void visit(IdentifierNode&) = 0;
        virtual void visit(BinaryExpressionNode&) = 0;
        virtual void visit(UnaryExpressionNode&) = 0;
        virtual void visit(FunctionCallNode&) = 0;

        virtual void visit(BlockNode&) = 0;
        virtual void visit(LetStatementNode&) = 0;
        virtual void visit(ReturnStatementNode&) = 0;
        virtual void visit(FunctionDeclarationNode&) = 0;
        virtual void visit(ExpressionStatementNode&) = 0;
        virtual void visit(AssignmentNode&) = 0;
        virtual void visit(WhileStatementNode&) = 0;
        virtual void visit(IfStatementNode&) = 0;
        virtual void visit(ProgramNode&) = 0;
};

struct ASTNode{
    int line{};
    int column{};

    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor)=0;

};

struct ExpressionNode : ASTNode {};
struct StatementNode : ASTNode {};

template <typename Derived, typename Base>
struct Visitable : Base {
    void accept(ASTVisitor& visitor) override {
        visitor.visit(static_cast<Derived&>(*this));
    }
};

struct IntLiteralNode : Visitable<IntLiteralNode, ExpressionNode> {
    int value{};
};

struct BoolLiteralNode : Visitable<BoolLiteralNode, ExpressionNode> {
    bool value{};
};

struct IdentifierNode : Visitable<IdentifierNode, ExpressionNode> {
    std::string name;
};

struct BinaryExpressionNode : Visitable<BinaryExpressionNode, ExpressionNode> {
    std::unique_ptr<ExpressionNode> left;
    BinaryOp oper;
    std::unique_ptr<ExpressionNode> right;
};

struct UnaryExpressionNode : Visitable<UnaryExpressionNode, ExpressionNode> {
    UnaryOp oper;
    std::unique_ptr<ExpressionNode> right;
};

struct FunctionCallNode : Visitable<FunctionCallNode, ExpressionNode> {
    std::string name;
    std::vector<std::unique_ptr<ExpressionNode>> arguments;
};

struct BlockNode : Visitable<BlockNode, StatementNode> {
    std::vector<std::unique_ptr<StatementNode>> statements;
};

struct LetStatementNode : Visitable<LetStatementNode, StatementNode> {
    std::string identifier;
    Type type{Type::Unknown};
    std::unique_ptr<ExpressionNode> initialiser;
};

struct ReturnStatementNode : Visitable<ReturnStatementNode, StatementNode> {
    std::unique_ptr<ExpressionNode> expression;
};

struct ParameterNode {
    std::string name;
    Type type{Type::Unknown};
};

struct FunctionDeclarationNode : Visitable<FunctionDeclarationNode, StatementNode> {
    std::string name;
    std::vector<ParameterNode> parameters;
    Type returnType{Type::Void};
    std::unique_ptr<BlockNode> body;
};

struct ExpressionStatementNode : Visitable<ExpressionStatementNode, StatementNode> {
    std::unique_ptr<ExpressionNode> expression;
};

struct AssignmentNode : Visitable<AssignmentNode, StatementNode> {
    std::string identifier;
    std::unique_ptr<ExpressionNode> expression;
};

struct WhileStatementNode : Visitable<WhileStatementNode, StatementNode> {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> body;
};

struct IfStatementNode : Visitable<IfStatementNode, StatementNode> {
    std::unique_ptr<ExpressionNode> condition;
    std::unique_ptr<BlockNode> thenBranch;
    std::unique_ptr<BlockNode> elseBranch;
};

struct ProgramNode : Visitable<ProgramNode, ASTNode> {
    std::vector<std::unique_ptr<StatementNode>> nodes;
};
