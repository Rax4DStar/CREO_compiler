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

