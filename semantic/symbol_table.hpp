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
    Type* type;
    SemanticSymbolKind kind;
    int scopeLevel;
    // For functions: is already in type (FunctionType)
    // For future extenions: visibility, const, virtual, offset, etc.
    bool isConst = false;
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
        // Verify it doesnt exist in curret scope
        auto& cur = scopes.back();
        if (cur.find(name) != cur.end()) return false;
        cur[name] = info;
        return true;
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

    // For debgging
    void dump() const {
        std::cout << "Symbol table (scopes: " << scopes.size() << ")\n";
        for (size_t i = 0; i < scopes.size(); ++i) {
            std::cout << "  Scope " << i << ":\n";
            for (const auto& [name, info] : scopes[i]) {
                std::cout << "    " << name << " : " << info.type->toString()
                        << " (kind=" << static_cast<int>(info.kind) << ")\n";
            }
        }
    }
};