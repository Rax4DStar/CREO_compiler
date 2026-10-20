#pragma once

#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <algorithm>

#include "AST.hpp"
#include "Token.hpp"
#include "Type.hpp"
#include "Operator.hpp"

class Parser{
    public:
        explicit Parser(std::vector<Token> tokens): _tokens(std::move(tokens)){
            if(_tokens.empty() || _tokens.back().type != TokenType::EOF_Token){
                throw std::invalid_argument("Parser requires a token stream ending with EOF");

            }

        }
        [[nodiscard]] std::unique_ptr<ProgramNode> parseProgram();


    private:
        std::vector<Token> _tokens;
        size_t _pos{0};

        [[nodiscard]] const Token& peek() const noexcept {
        return _tokens[_pos];
    }

    [[nodiscard]] const Token& peekNext() const noexcept {
        return (_pos + 1 < _tokens.size())
            ? _tokens[_pos + 1]
            : _tokens.back();
    }

    const Token& advance() noexcept {
        if (!isAtEnd()) {
            return _tokens[_pos++];
        }

        return peek();
    }

    [[nodiscard]] bool check(TokenType type) const noexcept {
        return peek().type == type;
    }

    [[nodiscard]] bool isAtEnd() const noexcept {
        return check(TokenType::EOF_Token);
    }

    bool match(TokenType type) noexcept {
        if (!check(type)) {
            return false;
        }

        advance();
        return true;
    }

    const Token& expect(TokenType type, const std::string& message) {
        if (check(type)) {
            return advance();
        }

        error(peek(), message);
    }

    [[noreturn]] void error(const Token &token, const std::string &message) const {
        const std::string found =
            token.type == TokenType::EOF_Token
                ? "end of file"
                : "'" + std::string(token.lexeme) + "'";

        throw std::runtime_error(
            "Parse error at " +
            std::to_string(token.line) + ":" +
            std::to_string(token.column) + ": " +
            message + "; found " + found);
    }

    template <typename T>
    [[nodiscard]] std::unique_ptr<T> createNode(const Token &token) {
        auto node = std::make_unique<T>();
        node->line = token.line;
        node->column = token.column;
        return node;
    }

    Type parseType();
    BinaryOp toBinaryOp(const Token &token) const;
    UnaryOp toUnaryOp(const Token &token) const;

    std::unique_ptr<ExpressionNode> parseBinaryExpression(
        std::unique_ptr<ExpressionNode> (Parser::*nextLevel)(),
        std::initializer_list<TokenType> operators);

    std::unique_ptr<StatementNode> parseStatement();
    std::unique_ptr<FunctionDeclarationNode> parseFunctionDeclaration();
    std::unique_ptr<LetStatementNode> parseLetStatement();
    std::unique_ptr<IfStatementNode> parseIfStatement();
    std::unique_ptr<WhileStatementNode> parseWhileStatement();
    std::unique_ptr<ReturnStatementNode> parseReturnStatement();
    std::unique_ptr<BlockNode> parseBlock();
    std::unique_ptr<StatementNode> parseAssignment();
    std::unique_ptr<StatementNode> parseExpressionStatement();

    std::unique_ptr<ExpressionNode> parseExpression();
    std::unique_ptr<ExpressionNode> parseEquality();
    std::unique_ptr<ExpressionNode> parseComparison();
    std::unique_ptr<ExpressionNode> parseAdditive();
    std::unique_ptr<ExpressionNode> parseMultiplicative();
    std::unique_ptr<ExpressionNode> parseUnary();
    std::unique_ptr<ExpressionNode> parsePrimary();
};
