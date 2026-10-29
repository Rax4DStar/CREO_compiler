#pragma once 

#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Type.hpp"

struct VariableSymbol{
    std::string name;
    Type type{Type::Unknown};
};

struct FunctionSymbol{
    std::string name;
    std::vector<Type> parameterTypes;
    Type returnType{Type::Void};
};

class SymbolTable{
    public:
        SymbolTable(){
            _variableScopes.emplace_back();
            _functions.emplace(
                "out",
                FunctionSymbol{"out", {Type:: Int}, Type::Void}
            );

        }

        void enterScope(){
            _variableScopes.emplace_back();
        }

        void exitScope(){
            if(_variableScopes.size()<=1){
                return;
            }
            _variableScopes.pop_back();
        }

        bool declareVariable(std::string name, Type type){
            auto& currentScope = _variableScopes.back();
            const std::string key = name;

            const auto result = currentScope.emplace(
                key,
                VariableSymbol{std::move(name), type}
            );
            return result.second;
        }

        [[nodiscard]] std::optional<VariableSymbol> lookupVariable(
            const std::string& name
        ) const{
            for(
                auto scope = _variableScopes.rbegin();
                scope != _variableScopes.rend();
                ++scope
            ){
                if(const auto found = scope->find(name); found!= scope->end()){
                    return found->second;
                }
            }
            return std::nullopt;
        }

        bool declareFunction(FunctionSymbol function){
            const std::string name = function.name;
            return _functions.emplace(name, std::move(function)).second;
        }
        
        [[nodiscard]] std::optional<FunctionSymbol> lookupFunction(
            const std::string& name
        ) const {
            if (const auto found = _functions.find(name); found != _functions.end()) {
                return found->second;
            }
            return std::nullopt;
        }
        private:
            std::vector<std::unordered_map<std::string, VariableSymbol>> _variableScopes;
            std::unordered_map<std::string, FunctionSymbol> _functions;
};
