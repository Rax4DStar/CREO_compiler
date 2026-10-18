#pragma once 

#include <ostream>

enum class Type{
    Int,
    Bool,
    Void,
    Unknown
};

[[nodiscard]] inline const char* toString(Type type) noexcept{
    switch(type){
        case Type::Int: return "int";
        case Type::Bool: return "bool";
        case Type::Void: return "void";
        case Type::Unknown: return "unknown";
    }
    return "unknown";
}

inline std::ostream& operator<<(std::ostream& os, Type type){
    return os<<toString(type);
}