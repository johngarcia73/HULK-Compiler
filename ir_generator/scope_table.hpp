#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <stdexcept>

class ScopeTable {
private:
    std::vector<std::unordered_map<std::string, std::string>> lexicalScopes;

public:
    ScopeTable();
    ~ScopeTable();

    void enterScope();
    void exitScope();

    void defineVariable(std::string hulkVariableName, std::string qbeRegisterName);
    std::string resolveVariable(std::string hulkVariableName);
};