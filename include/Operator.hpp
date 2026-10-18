#pragma once

#include <ostream>

enum class BinaryOp{
    Add,
    Subtract,
    Multiply,
    Divide,

    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual
};

enum class UnaryOp{
    Negate,
    Not
};

[[nodiscard]] inline const char* toString(BinaryOp op) noexcept {
    switch (op) {
        case BinaryOp::Add: return "+";
        case BinaryOp::Subtract: return "-";
        case BinaryOp::Multiply: return "*";
        case BinaryOp::Divide: return "/";
        case BinaryOp::Less: return "<";
        case BinaryOp::LessEqual: return "<=";
        case BinaryOp::Greater: return ">";
        case BinaryOp::GreaterEqual: return ">=";
        case BinaryOp::Equal: return "==";
        case BinaryOp::NotEqual: return "!=";
    }
    return "?";
}

[[nodiscard]] inline const char* toString(UnaryOp op) noexcept {
    switch (op) {
        case UnaryOp::Negate: return "-";
        case UnaryOp::Not: return "!";
    }
    return "?";
}

inline std::ostream& operator<<(std::ostream& os, BinaryOp op) {
    return os << toString(op);
}

inline std::ostream& operator<<(std::ostream& os, UnaryOp op) {
    return os << toString(op);
}
