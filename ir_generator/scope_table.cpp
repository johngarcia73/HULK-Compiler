#include "scope_table.hpp"

ScopeTable::ScopeTable() {
    this->lexicalScopes.push_back(std::unordered_map<std::string, std::string>());
}

ScopeTable::~ScopeTable() {
    // std::vector automatically handles memory deallocation of its elements.
}

void ScopeTable::enterScope() {
    this->lexicalScopes.push_back(std::unordered_map<std::string, std::string>());
}

void ScopeTable::exitScope() {
    if (!this->lexicalScopes.empty()) {
        this->lexicalScopes.pop_back();
    }
}

void ScopeTable::defineVariable(std::string hulkVariableName, std::string qbeRegisterName) {
    if (!this->lexicalScopes.empty()) {
        this->lexicalScopes.back()[hulkVariableName] = qbeRegisterName;
    }
}

std::string ScopeTable::resolveVariable(std::string hulkVariableName) {
    for (auto it = this->lexicalScopes.rbegin(); it != this->lexicalScopes.rend(); ++it) {
        if (it->count(hulkVariableName) > 0) {
            return (*it)[hulkVariableName];
        }
    }
    
    throw std::runtime_error("Semantic Error: Variable not found in lexical scope: " + hulkVariableName);
}