#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include "type.hpp"

using namespace std;

enum class SemanticSymbolKind {
    Variable,
    Parameter,
    Function,
    // Future: Class, Field, Method, ...
};

struct SymbolInfo {
    std::string name;
    std::vector<FunctionType*> overloads;  // just functions
    Type* type;                            // variables/params and not overladed funcs
    SemanticSymbolKind kind;
    int scopeLevel;
    bool isConst = false;

    // Constructor for vars and params (not functions)
    SymbolInfo(const std::string& n, Type* t, SemanticSymbolKind k, int level)
        : name(n), type(t), kind(k), scopeLevel(level) {}

    // Constructor for functions (not overloaded)
    SymbolInfo(const std::string& n, FunctionType* ft, SemanticSymbolKind k, int level)
        : name(n), overloads{ft}, type(ft), kind(k), scopeLevel(level) {}

    // Constructor for overloaded functions
    SymbolInfo(const std::string& n, const std::vector<FunctionType*>& ov, SemanticSymbolKind k, int level)
        : name(n), overloads(ov), type(ov.empty() ? nullptr : ov[0]), kind(k), scopeLevel(level) {}
};

class SemanticSymbolTable {
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    int currentLevel = 0;
public:
    void enterScope() {
        scopes.emplace_back();
        ++currentLevel;
    }
    void exitScope() {
        scopes.pop_back();
        --currentLevel;
    }
    int getCurrentLevel() const { return currentLevel; }

    bool insert(const std::string& name, const SymbolInfo& info) {
        if (scopes.empty()) scopes.emplace_back();
        auto& cur = scopes.back();
        auto result = cur.emplace(name, info);
        return result.second; 
    }

    bool insertFunction(const std::string& name, FunctionType* funcType) {
        if (scopes.empty()) scopes.emplace_back();
        auto& cur = scopes.back();
        auto it = cur.find(name);
        if (it != cur.end()) {
            SymbolInfo& existing = it->second;
            if (existing.kind != SemanticSymbolKind::Function) return false;
         
            for (auto* ft : existing.overloads) {
                if (ft->equals(funcType)) return false;
            }
            existing.overloads.push_back(funcType);
            return true;
        } else {
            cur.emplace(name, SymbolInfo(name, funcType, SemanticSymbolKind::Function, currentLevel));
            return true;
        }
    }

    SymbolInfo* lookup(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    SymbolInfo* lookupCurrent(const std::string& name) {
        auto& cur = scopes.back();
        auto it = cur.find(name);
        return it != cur.end() ? &it->second : nullptr;
    }


    void update(const std::string& name, const SymbolInfo& info) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                found->second = info;
                return;
            }
        }
    }
    
    // For debgging
    void dump() const {
        std::cout << "Symbol table (scopes: " << scopes.size() << ")\n";
        for (size_t i = 0; i < scopes.size(); ++i) {
            std::cout << "  Scope " << i << ":\n";
            for (const auto& [name, info] : scopes[i]) {
                std::cout << "    " << name << " : ";
                if (info.kind == SemanticSymbolKind::Function && info.overloads.size() > 1) {
                    for (auto* ft : info.overloads) {
                        std::cout << ft->toString() << " ";
                    }
                } else if (info.type) {
                    std::cout << info.type->toString();
                } else {
                    std::cout << "unknown";
                }
                std::cout << " (kind=" << static_cast<int>(info.kind) << ")\n";
            }
        }
    }

    FunctionType* lookupFunction(const std::string& name, const std::vector<Type*>& argTypes) {
        SymbolInfo* info = lookup(name); 
        if (!info || info->kind != SemanticSymbolKind::Function) return nullptr;
        for (auto* ft : info->overloads) {
            const auto& params = ft->getParamTypes();
            if (params.size() != argTypes.size()) continue;
            bool match = true;
            for (size_t i = 0; i < params.size(); ++i) {
                if (!params[i]->equals(argTypes[i])) {
                    match = false;
                    break;
                }
            }
            if (match) return ft;
        }
        return nullptr;
    }
};