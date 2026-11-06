#include "IRGenerator.hpp"

#include <stdexcept>
#include <utility>

void IRGenerator::reset() {
    _program = IRProgram{};
    _currentFunction = nullptr;
    _currentBlock = nullptr;
    _functions.clear();
    _scopes.clear();
    _storageNameCounts.clear();
    _blockNames.clear();
    _temporaryCounter = 0;
    _labelCounter = 0;
}

IRProgram IRGenerator::generate(ProgramNode& program) {
    reset();
    registerBuiltins();
    collectFunctionSignatures(program);
    for (auto& statement : program.nodes) {
        auto* function =
            dynamic_cast<FunctionDeclarationNode*>(statement.get());
        if (!function) {
            throw std::runtime_error(
                "IR error: only functions are allowed at global scope"
            );
        }
        emitFunction(*function);
    }
    return std::move(_program);
}
void IRGenerator::registerBuiltins() {
    _functions.emplace(
        "out",
        FunctionInfo{
            .parameterTypes = {Type::Int},
            .returnType = Type::Void
        }
    );
}

void IRGenerator::collectFunctionSignatures(ProgramNode& program) {
    for (auto& statement : program.nodes) {
        auto* function =
            dynamic_cast<FunctionDeclarationNode*>(statement.get());
        if (!function) {
            throw std::runtime_error(
                "IR error: only functions are allowed at global scope"
            );
        }
        FunctionInfo signature;
        signature.returnType = function->returnType;

        for (const ParameterNode& parameter : function->parameters) {
            signature.parameterTypes.push_back(parameter.type);
        }
        const auto [_, inserted] =
            _functions.emplace(function->name, std::move(signature));
        if (!inserted) {
            throw std::runtime_error(
                "IR error: duplicate function '" + function->name + "'"
            );
        }
    }
}

void IRGenerator::enterScope() {
    _scopes.emplace_back();
}

void IRGenerator::exitScope() {
    if (_scopes.empty()) {
        throw std::runtime_error(
            "IR internal error: no active scope to exit"
        );
    }
    _scopes.pop_back();
}

std::string IRGenerator::makeStorageName(
    const std::string& sourceName
) {
    int& count = _storageNameCounts[sourceName];

    if (count++ == 0) {
        return sourceName;
    }
    return sourceName + "$" + std::to_string(count - 1);
}

std::string IRGenerator::declareVariable(
    const std::string& sourceName,
    Type type,
    StorageKind kind
) {
    if (_scopes.empty() || !_currentFunction) {
        throw std::runtime_error(
            "IR internal error: variable declared without an active function"
        );
    }

    auto& currentScope = _scopes.back();
    if (currentScope.contains(sourceName)) {
        throw std::runtime_error(
            "IR error: duplicate variable '" + sourceName + "'"
        );
    }

    std::string storageName = makeStorageName(sourceName);
    currentScope.emplace(
        sourceName,
        VariableInfo{
            .storageName = storageName,
            .type = type
        }
    );

    if (kind == StorageKind::Parameter) {
        _currentFunction->parameters.push_back(
            IRParameter{storageName, type}
        );
    } else {
        _currentFunction->locals.push_back(
            IRLocal{storageName, type}
        );
    }
    return storageName;
}

IRGenerator::VariableInfo IRGenerator::lookupVariable(
    const std::string& sourceName
) const {
    for (auto scope = _scopes.rbegin();
         scope != _scopes.rend();
         ++scope) {
        const auto found = scope->find(sourceName);

        if (found != scope->end()) {
            return found->second;
        }
    }
    throw std::runtime_error(
        "IR error: unknown variable '" + sourceName + "'"
    );
}
std::string IRGenerator::newTemporary() {
    return "%t" + std::to_string(_temporaryCounter++);
}
std::string IRGenerator::newLabel(const std::string& prefix) {
    return prefix + std::to_string(_labelCounter++);
}

void IRGenerator::startBlock(const std::string& name) {
    if (!_currentFunction) {
        throw std::runtime_error(
            "IR internal error: no active function"
        );
    }
    if (!_blockNames.insert(name).second) {
        throw std::runtime_error(
            "IR internal error: duplicate block '" + name + "'"
        );
    }
    _currentFunction->blocks.push_back(IRBlock{
        .name = name
    });
    _currentBlock = &_currentFunction->blocks.back();
}
void IRGenerator::emit(IRInstruction instruction) {
    if (!_currentBlock) {
        throw std::runtime_error(
            "IR internal error: no active block"
        );
    }
    if (currentBlockTerminated()) {
        throw std::runtime_error(
            "IR internal error: instruction emitted after terminator"
        );
    }
    _currentBlock->instructions.push_back(std::move(instruction));
}

bool IRGenerator::currentBlockTerminated() const {
    if (!_currentBlock || _currentBlock->instructions.empty()) {
        return false;
    }
    return isTerminator(_currentBlock->instructions.back().op);
}
void IRGenerator::emitFunction(FunctionDeclarationNode& node) {
    _program.functions.push_back(IRFunction{});
    _currentFunction = &_program.functions.back();
    _currentFunction->name = node.name;
    _currentFunction->returnType = node.returnType;
    _currentBlock = nullptr;
    _scopes.clear();
    _storageNameCounts.clear();
    _blockNames.clear();
    _temporaryCounter = 0;
    _labelCounter = 0;
    enterScope();

    for (const ParameterNode& parameter : node.parameters) {
        (void)declareVariable(
            parameter.name,
            parameter.type,
            StorageKind::Parameter
        );
    }
    startBlock("entry");
    emitBlock(*node.body, false);

    if (!currentBlockTerminated()) {
        if (node.returnType != Type::Void) {
            throw std::runtime_error(
                "IR error: non-void function '" +
                node.name +
                "' reaches the end without returning"
            );
        }
        emit(IRInstruction{
            .op = IROp::Return
        });
    }
    exitScope();
    _currentBlock = nullptr;
    _currentFunction = nullptr;
}
void IRGenerator::emitBlock(
    BlockNode& node,
    bool createScope
) {
    if (createScope) {
        enterScope();
    }
    for (auto& statement : node.statements) {
        if (currentBlockTerminated()) {
            break;
        }
        emitStatement(*statement);
    }
    if (createScope) {
        exitScope();
    }
}
void IRGenerator::emitStatement(StatementNode& node) {
    if (auto* let = dynamic_cast<LetStatementNode*>(&node)) {
        emitLetStatement(*let);
        return;
    }

    if (auto* assignment = dynamic_cast<AssignmentNode*>(&node)) {
        emitAssignment(*assignment);
        return;
    }

    if (auto* ifStatement = dynamic_cast<IfStatementNode*>(&node)) {
        emitIfStatement(*ifStatement);
        return;
    }

    if (auto* whileStatement = dynamic_cast<WhileStatementNode*>(&node)) {
        emitWhileStatement(*whileStatement);
        return;
    }

    if (auto* returnStatement =
            dynamic_cast<ReturnStatementNode*>(&node)) {
        emitReturnStatement(*returnStatement);
        return;
    }

    if (auto* expressionStatement =
            dynamic_cast<ExpressionStatementNode*>(&node)) {
        emitExpressionStatement(*expressionStatement);
        return;
    }

    if (dynamic_cast<FunctionDeclarationNode*>(&node)) {
        throw std::runtime_error(
            "IR error: nested functions are not supported"
        );
    }

    throw std::runtime_error(
        "IR internal error: unsupported statement node"
    );
}
void IRGenerator::emitLetStatement(LetStatementNode& node) {
    IRValue initialValue = emitExpression(*node.initialiser);
    const std::string storageName = declareVariable(
        node.identifier,
        node.type,
        StorageKind::Local
    );
    emit(IRInstruction{
        .op = IROp::Store,
        .args = {initialValue.name, storageName}
    });
}
void IRGenerator::emitAssignment(AssignmentNode& node) {
    const VariableInfo variable =
        lookupVariable(node.identifier);

    IRValue value = emitExpression(*node.expression);

    emit(IRInstruction{
        .op = IROp::Store,
        .args = {value.name, variable.storageName}
    });
}
void IRGenerator::emitIfStatement(IfStatementNode& node) {
    IRValue condition = emitExpression(*node.condition);
    const std::string thenLabel = newLabel("if_then");
    const std::string endLabel = newLabel("if_end");
    const bool hasElse = static_cast<bool>(node.elseBranch);
    const std::string elseLabel =
        hasElse ? newLabel("if_else") : endLabel;

    emit(IRInstruction{
        .op = IROp::Branch,
        .args = {condition.name},
        .trueTarget = thenLabel,
        .falseTarget = elseLabel
    });
    startBlock(thenLabel);
    emitBlock(*node.thenBranch);

    const bool thenFallsThrough =
        !currentBlockTerminated();
    if (thenFallsThrough) {
        emit(IRInstruction{
            .op = IROp::Jump,
            .target = endLabel
        });
    }
    bool elseFallsThrough = !hasElse;
    if (hasElse) {
        startBlock(elseLabel);
        emitBlock(*node.elseBranch);

        elseFallsThrough = !currentBlockTerminated();

        if (elseFallsThrough) {
            emit(IRInstruction{
                .op = IROp::Jump,
                .target = endLabel
            });
        }
    }
    if (thenFallsThrough || elseFallsThrough) {
        startBlock(endLabel);
    }
}
void IRGenerator::emitWhileStatement(WhileStatementNode& node) {
    const std::string conditionLabel =
        newLabel("while_cond");
    const std::string bodyLabel =
        newLabel("while_body");
    const std::string endLabel =
        newLabel("while_end");
    emit(IRInstruction{
        .op = IROp::Jump,
        .target = conditionLabel
    });
    startBlock(conditionLabel);
    IRValue condition = emitExpression(*node.condition);

    emit(IRInstruction{
        .op = IROp::Branch,
        .args = {condition.name},
        .trueTarget = bodyLabel,
        .falseTarget = endLabel
    });
    startBlock(bodyLabel);
    emitBlock(*node.body);

    if (!currentBlockTerminated()) {
        emit(IRInstruction{
            .op = IROp::Jump,
            .target = conditionLabel
        });
    }
    startBlock(endLabel);
}
void IRGenerator::emitReturnStatement(
    ReturnStatementNode& node
) {
    if (!node.expression) {
        emit(IRInstruction{
            .op = IROp::Return
        });
        return;
    }
    IRValue value = emitExpression(*node.expression);
    emit(IRInstruction{
        .op = IROp::Return,
        .args = {value.name}
    });
}
void IRGenerator::emitExpressionStatement(
    ExpressionStatementNode& node
) {
    (void)emitExpression(*node.expression);
}
IRGenerator::IRValue IRGenerator::emitExpression(
    ExpressionNode& node
) {
    if (auto* integer = dynamic_cast<IntLiteralNode*>(&node)) {
        return emitIntLiteral(*integer);
    }
    if (auto* boolean = dynamic_cast<BoolLiteralNode*>(&node)) {
        return emitBoolLiteral(*boolean);
    }
    if (auto* identifier = dynamic_cast<IdentifierNode*>(&node)) {
        return emitIdentifier(*identifier);
    }
    if (auto* binary =
            dynamic_cast<BinaryExpressionNode*>(&node)) {
        return emitBinaryExpression(*binary);
    }
    if (auto* unary =
            dynamic_cast<UnaryExpressionNode*>(&node)) {
        return emitUnaryExpression(*unary);
    }
    if (auto* call = dynamic_cast<FunctionCallNode*>(&node)) {
        return emitFunctionCall(*call);
    }
    throw std::runtime_error(
        "IR internal error: unsupported expression node"
    );
}
IRGenerator::IRValue IRGenerator::emitIntLiteral(
    IntLiteralNode& node
) {
    const std::string destination = newTemporary();
    emit(IRInstruction{
        .op = IROp::ConstInt,
        .type = Type::Int,
        .dst = destination,
        .intValue = node.value
    });
    return IRValue{
        .name = destination,
        .type = Type::Int
    };
}

IRGenerator::IRValue IRGenerator::emitBoolLiteral(
    BoolLiteralNode& node
) {
    const std::string destination = newTemporary();
    emit(IRInstruction{
        .op = IROp::ConstBool,
        .type = Type::Bool,
        .dst = destination,
        .boolValue = node.value
    });
    return IRValue{
        .name = destination,
        .type = Type::Bool
    };
}
IRGenerator::IRValue IRGenerator::emitIdentifier(
    IdentifierNode& node
) {
    const VariableInfo variable =
        lookupVariable(node.name);

    const std::string destination = newTemporary();
    emit(IRInstruction{
        .op = IROp::Load,
        .type = variable.type,
        .dst = destination,
        .args = {variable.storageName}
    });
    return IRValue{
        .name = destination,
        .type = variable.type
    };
}
IRGenerator::IRValue IRGenerator::emitBinaryExpression(
    BinaryExpressionNode& node
) {
    IRValue left = emitExpression(*node.left);
    IRValue right = emitExpression(*node.right);

    const std::string destination = newTemporary();
    const Type resultType = binaryResultType(node.oper);

    emit(IRInstruction{
        .op = binaryOpToIROp(node.oper),
        .type = resultType,
        .dst = destination,
        .args = {left.name, right.name}
    });
    return IRValue{
        .name = destination,
        .type = resultType
    };
}
IRGenerator::IRValue IRGenerator::emitUnaryExpression(
    UnaryExpressionNode& node
) {
    IRValue operand = emitExpression(*node.right);
    const std::string destination = newTemporary();

    emit(IRInstruction{
        .op = unaryOpToIROp(node.oper),
        .type = operand.type,
        .dst = destination,
        .args = {operand.name}
    });
    return IRValue{
        .name = destination,
        .type = operand.type
    };
}
IRGenerator::IRValue IRGenerator::emitFunctionCall(
    FunctionCallNode& node
) {
    const auto function = _functions.find(node.name);
    if (function == _functions.end()) {
        throw std::runtime_error(
            "IR error: unknown function '" + node.name + "'"
        );
    }

    const FunctionInfo& signature = function->second;
    std::vector<std::string> arguments;
    arguments.reserve(node.arguments.size());

    for (auto& argument : node.arguments) {
        IRValue value = emitExpression(*argument);
        arguments.push_back(value.name);
    }
    std::string destination;

    if (signature.returnType != Type::Void) {
        destination = newTemporary();
    }
    emit(IRInstruction{
        .op = IROp::Call,
        .type = signature.returnType,
        .dst = destination,
        .args = std::move(arguments),
        .callee = node.name
    });
    return IRValue{
        .name = destination,
        .type = signature.returnType
    };
}
IROp IRGenerator::binaryOpToIROp(BinaryOp op) const {
    switch (op) {
        case BinaryOp::Add: return IROp::Add;
        case BinaryOp::Subtract: return IROp::Subtract;
        case BinaryOp::Multiply: return IROp::Multiply;
        case BinaryOp::Divide: return IROp::Divide;

        case BinaryOp::Less: return IROp::Less;
        case BinaryOp::LessEqual: return IROp::LessEqual;
        case BinaryOp::Greater: return IROp::Greater;
        case BinaryOp::GreaterEqual: return IROp::GreaterEqual;

        case BinaryOp::Equal: return IROp::Equal;
        case BinaryOp::NotEqual: return IROp::NotEqual;
    }
    throw std::runtime_error(
        "IR internal error: unsupported binary operator"
    );
}

IROp IRGenerator::unaryOpToIROp(UnaryOp op) const {
    switch (op) {
        case UnaryOp::Negate: return IROp::Negate;
        case UnaryOp::Not:    return IROp::Not;
    }

    throw std::runtime_error(
        "IR internal error: unsupported unary operator"
    );
}

Type IRGenerator::binaryResultType(BinaryOp op) const {
    switch (op) {
        case BinaryOp::Add:
        case BinaryOp::Subtract:
        case BinaryOp::Multiply:
        case BinaryOp::Divide:
            return Type::Int;
        case BinaryOp::Less:
        case BinaryOp::LessEqual:
        case BinaryOp::Greater:
        case BinaryOp::GreaterEqual:
        case BinaryOp::Equal:
        case BinaryOp::NotEqual:
            return Type::Bool;
    }
    throw std::runtime_error(
        "IR internal error: unsupported binary operator"
    );
}