#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "AST.hpp"
#include "IR.hpp"
#include "Operator.hpp"
#include "Type.hpp"

class IRGenerator {
public:
    [[nodiscard]] IRProgram generate(ProgramNode& program);

private:
    struct IRValue {
        std::string name;
        Type type{Type::Unknown};
    };

    struct VariableInfo {
        std::string storageName;
        Type type{Type::Unknown};
    };

    struct FunctionInfo {
        std::vector<Type> parameterTypes;
        Type returnType{Type::Void};
    };

    enum class StorageKind {
        Parameter,
        Local
    };

    IRProgram _program;
    IRFunction* _currentFunction{nullptr};
    IRBlock* _currentBlock{nullptr};

    std::unordered_map<std::string, FunctionInfo> _functions;
    std::vector<std::unordered_map<std::string, VariableInfo>> _scopes;
    std::unordered_map<std::string, int> _storageNameCounts;
    std::unordered_set<std::string> _blockNames;

    int _temporaryCounter{0};
    int _labelCounter{0};

    void reset();
    void registerBuiltins();
    void collectFunctionSignatures(ProgramNode& program);
    void enterScope();
    void exitScope();

    [[nodiscard]] std::string declareVariable(
        const std::string& sourceName,
        Type type,
        StorageKind kind
    );

    [[nodiscard]] VariableInfo lookupVariable(
        const std::string& sourceName
    ) const;

    [[nodiscard]] std::string makeStorageName(
        const std::string& sourceName
    );

    [[nodiscard]] std::string newTemporary();
    [[nodiscard]] std::string newLabel(const std::string& prefix);

    void startBlock(const std::string& name);
    void emit(IRInstruction instruction);

    [[nodiscard]] bool currentBlockTerminated() const;

    void emitFunction(FunctionDeclarationNode& node);
    void emitBlock(BlockNode& node, bool createScope = true);
    void emitStatement(StatementNode& node);

    void emitLetStatement(LetStatementNode& node);
    void emitAssignment(AssignmentNode& node);
    void emitIfStatement(IfStatementNode& node);
    void emitWhileStatement(WhileStatementNode& node);
    void emitReturnStatement(ReturnStatementNode& node);
    void emitExpressionStatement(ExpressionStatementNode& node);

    [[nodiscard]] IRValue emitExpression(ExpressionNode& node);
    [[nodiscard]] IRValue emitIntLiteral(IntLiteralNode& node);
    [[nodiscard]] IRValue emitBoolLiteral(BoolLiteralNode& node);
    [[nodiscard]] IRValue emitIdentifier(IdentifierNode& node);
    [[nodiscard]] IRValue emitBinaryExpression(BinaryExpressionNode& node);
    [[nodiscard]] IRValue emitUnaryExpression(UnaryExpressionNode& node);
    [[nodiscard]] IRValue emitFunctionCall(FunctionCallNode& node);

    
    [[nodiscard]] IROp binaryOpToIROp(BinaryOp op) const;
    [[nodiscard]] IROp unaryOpToIROp(UnaryOp op) const;
    [[nodiscard]] Type binaryResultType(BinaryOp op) const;
};