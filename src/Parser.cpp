#include "Parser.hpp"

#include <charconv>
#include <system_error>

Type Parser::parseType(){
    const Token& token = peek();

    switch(token.type){
        case TokenType::IntType:
            advance();
            return Type::Int;
        case TokenType::BoolType:
            advance();
            return Type::Bool;
        case TokenType::VoidType:
            advance();
            return Type::Void;
        default:
            error(token, "Expected type");

    }
}

BinaryOp Parser::toBinaryOp(const Token& token) const{
    switch(token.type){
        case TokenType::Plus: return BinaryOp::Add;
        case TokenType::Minus: return BinaryOp::Subtract;
        case TokenType::Star: return BinaryOp::Multiply;
        case TokenType::Slash: return BinaryOp::Divide;
        case TokenType::Less: return BinaryOp::Less;
        case TokenType::LessEqual: return BinaryOp::LessEqual;
        case TokenType::Greater: return BinaryOp::Greater;
        case TokenType::GreaterEqual: return BinaryOp::GreaterEqual;
        case TokenType::DoubleEquals: return BinaryOp::Equal;
        case TokenType::NotEqual: return BinaryOp::NotEqual;
        default:
            error(token, "Invalid binary operator");
    }
}

UnaryOp Parser::toUnaryOp(const Token& token) const{
    switch(token.type){
        case TokenType::Minus: return UnaryOp::Negate;
        case TokenType::Not: return UnaryOp::Not;

        default:
            error(token, "Invalid unary operator");


    }
}

std::unique_ptr<ProgramNode> Parser::parseProgram(){
    auto program = createNode<ProgramNode>(peek());

    while(!isAtEnd()){
        program->nodes.push_back(parseStatement());
    }
    return program;
}

std::unique_ptr<StatementNode> Parser::parseStatement(){
    switch (peek().type) {
        case TokenType::Fn:
            return parseFunctionDeclaration();
        case TokenType::Let:
            return parseLetStatement();
        case TokenType::If:
            return parseIfStatement();
        case TokenType::While:
            return parseWhileStatement();
        case TokenType::Return:
            return parseReturnStatement();
        case TokenType::Identifier:
            if (peekNext().type == TokenType::Equals) {
                return parseAssignment();
            }
            return parseExpressionStatement();
        default:
          return parseExpressionStatement();
    }
}

std::unique_ptr<FunctionDeclarationNode>
Parser::parseFunctionDeclaration() {
    const Token& fnToken = expect(TokenType::Fn, "Expected 'fn'");
    auto node = createNode<FunctionDeclarationNode>(fnToken);

    node->name = std::string(
        expect(TokenType::Identifier, "Expected function name").lexeme
    );

    expect(TokenType::OpenParen, "Expected '('");

    if (!check(TokenType::CloseParen)) {
        do {
            ParameterNode parameter;
            parameter.name = std::string(
                expect(
                    TokenType::Identifier,
                    "Expected parameter name"
                ).lexeme
            );

            expect(TokenType::Colon, "Expected ':' after parameter name");
            parameter.type = parseType();
            node->parameters.push_back(std::move(parameter));
        } while (match(TokenType::Comma));
    }

    expect(TokenType::CloseParen, "Expected ')' after parameters");

    node->returnType =
        match(TokenType::Colon)
            ? parseType()
            : Type::Void;

    node->body = parseBlock();
    return node;
}

std::unique_ptr<BlockNode> Parser::parseBlock() {
    const Token& brace =
        expect(TokenType::OpenBrace, "Expected '{'");

    auto node = createNode<BlockNode>(brace);

    while (!check(TokenType::CloseBrace) && !isAtEnd()) {
        node->statements.push_back(parseStatement());
    }

    expect(TokenType::CloseBrace, "Expected '}' after block");
    return node;
}

std::unique_ptr<LetStatementNode> Parser::parseLetStatement() {
    const Token& letToken =
        expect(TokenType::Let, "Expected 'let'");

    auto node = createNode<LetStatementNode>(letToken);

    node->identifier = std::string(
        expect(TokenType::Identifier, "Expected variable name").lexeme
    );

    expect(TokenType::Colon, "Expected ':' after variable name");
    node->type = parseType();

    expect(TokenType::Equals, "Expected '=' after variable type");
    node->initialiser = parseExpression();

    expect(
        TokenType::SemiColon,
        "Expected ';' after variable declaration"
    );

    return node;
}

std::unique_ptr<StatementNode> Parser::parseAssignment() {
    const Token& identifier =
        expect(TokenType::Identifier, "Expected identifier");

    auto node = createNode<AssignmentNode>(identifier);
    node->identifier = std::string(identifier.lexeme);

    expect(TokenType::Equals, "Expected '=' after identifier");
    node->expression = parseExpression();
    expect(TokenType::SemiColon, "Expected ';' after assignment");

    return node;
}

std::unique_ptr<ReturnStatementNode>
Parser::parseReturnStatement() {
    const Token& returnToken =
        expect(TokenType::Return, "Expected 'return'");

    auto node = createNode<ReturnStatementNode>(returnToken);

    if (!check(TokenType::SemiColon)) {
        node->expression = parseExpression();
    }

    expect(TokenType::SemiColon, "Expected ';' after return value");
    return node;
}

std::unique_ptr<StatementNode>
Parser::parseExpressionStatement() {
    auto node = createNode<ExpressionStatementNode>(peek());
    node->expression = parseExpression();

    expect(TokenType::SemiColon, "Expected ';' after expression");
    return node;
}

std::unique_ptr<IfStatementNode> Parser::parseIfStatement() {
    const Token& ifToken = expect(TokenType::If, "Expected 'if'");
    auto node = createNode<IfStatementNode>(ifToken);

    expect(TokenType::OpenParen, "Expected '(' after 'if'");
    node->condition = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after condition");

    node->thenBranch = parseBlock();

    if (match(TokenType::Else)) {
        if (check(TokenType::If)) {
            auto elseBlock = createNode<BlockNode>(peek());
            elseBlock->statements.push_back(parseIfStatement());
            node->elseBranch = std::move(elseBlock);
        } else {
            node->elseBranch = parseBlock();
        }
    }

    return node;
}

std::unique_ptr<WhileStatementNode>
Parser::parseWhileStatement() {
    const Token& whileToken =
        expect(TokenType::While, "Expected 'while'");

    auto node = createNode<WhileStatementNode>(whileToken);

    expect(TokenType::OpenParen, "Expected '(' after 'while'");
    node->condition = parseExpression();
    expect(TokenType::CloseParen, "Expected ')' after condition");

    node->body = parseBlock();
    return node;
}

std::unique_ptr<ExpressionNode>
Parser::parseBinaryExpression(
    std::unique_ptr<ExpressionNode> (Parser::*nextLevel)(),
    std::initializer_list<TokenType> operators
) {
    auto left = (this->*nextLevel)();

    while (
        std::find(
            operators.begin(),
            operators.end(),
            peek().type
        ) != operators.end()
    ) {
        const Token& operatorToken = advance();
        auto right = (this->*nextLevel)();

        auto node =
            createNode<BinaryExpressionNode>(operatorToken);

        node->oper = toBinaryOp(operatorToken);
        node->left = std::move(left);
        node->right = std::move(right);
        left = std::move(node);
    }

    return left;
}

std::unique_ptr<ExpressionNode> Parser::parseExpression() {
    return parseEquality();
}

std::unique_ptr<ExpressionNode> Parser::parseEquality() {
    return parseBinaryExpression(
        &Parser::parseComparison,
        {
            TokenType::DoubleEquals,
            TokenType::NotEqual
        }
    );
}

std::unique_ptr<ExpressionNode> Parser::parseComparison() {
    return parseBinaryExpression(
        &Parser::parseAdditive,
        {
            TokenType::Less,
            TokenType::LessEqual,
            TokenType::Greater,
            TokenType::GreaterEqual
        }
    );
}

std::unique_ptr<ExpressionNode> Parser::parseAdditive() {
    return parseBinaryExpression(
        &Parser::parseMultiplicative,
        {
            TokenType::Plus,
            TokenType::Minus
        }
    );
}

std::unique_ptr<ExpressionNode>
Parser::parseMultiplicative() {
    return parseBinaryExpression(
        &Parser::parseUnary,
        {
            TokenType::Star,
            TokenType::Slash
        }
    );
}

std::unique_ptr<ExpressionNode> Parser::parseUnary() {
    if (check(TokenType::Minus) || check(TokenType::Not)) {
        const Token& operatorToken = advance();

        auto node =
            createNode<UnaryExpressionNode>(operatorToken);

        node->oper = toUnaryOp(operatorToken);
        node->right = parseUnary();
        return node;
    }

    return parsePrimary();
}

std::unique_ptr<ExpressionNode> Parser::parsePrimary() {
    const Token& token = advance();

    switch (token.type) {
        case TokenType::Identifier: {
            if (match(TokenType::OpenParen)) {
                auto call = createNode<FunctionCallNode>(token);
                call->name = std::string(token.lexeme);

                if (!check(TokenType::CloseParen)) {
                    do {
                        call->arguments.push_back(
                            parseExpression()
                        );
                    } while (match(TokenType::Comma));
                }

                expect(
                    TokenType::CloseParen,
                    "Expected ')' after function arguments"
                );
                return call;
            }

            auto identifier =
                createNode<IdentifierNode>(token);

            identifier->name = std::string(token.lexeme);
            return identifier;
        }

        case TokenType::IntLiteral: {
            int value{};
            const char* begin = token.lexeme.data();
            const char* end = begin + token.lexeme.size();

            const auto [ptr, errorCode] =
                std::from_chars(begin, end, value);

            if (
                errorCode == std::errc::result_out_of_range
            ) {
                error(token, "Integer literal is out of range");
            }

            if (
                errorCode != std::errc{} ||
                ptr != end
            ) {
                error(token, "Invalid integer literal");
            }

            auto number = createNode<IntLiteralNode>(token);
            number->value = value;
            return number;
        }

        case TokenType::BoolLiteral: {
            auto boolean =
                createNode<BoolLiteralNode>(token);

            boolean->value = token.lexeme == "true";
            return boolean;
        }

        case TokenType::OpenParen: {
            auto expression = parseExpression();
            expect(
                TokenType::CloseParen,
                "Expected ')' after expression"
            );
            return expression;
        }

        default:
            error(token, "Expected expression");
    }
}