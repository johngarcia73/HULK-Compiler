#pragma once
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <stdexcept>
#include <utility>      // for std::pair (if you prefer a pair over a struct)

// Forward-declare your Type class (adjust as needed)
class Type;

class ScopeTable {
private:
    // Each scope maps a variable name to its QBE register name and its type.
    struct VariableInfo {
        std::string qbeRegisterName;
        Type* type = nullptr;   // null means "not yet typed"
    };

    // The vector of scopes; the innermost scope is at the back.
    std::vector<std::unordered_map<std::string, VariableInfo>> lexicalScopes;

public:
    ScopeTable() {
        // Start with a global scope.
        lexicalScopes.emplace_back();
    }

    ~ScopeTable() = default;

    void enterScope() {
        lexicalScopes.emplace_back();
    }

    void exitScope() {
        if (!lexicalScopes.empty()) {
            lexicalScopes.pop_back();
        }
    }

    // ------------------------------------------------------------------
    // Define a variable in the current scope, optionally with a type.
    // If you always know the type at definition time, you can pass it here.
    // Otherwise you can call saveVariable later.
    void defineVariable(const std::string& hulkVariableName,
                        const std::string& qbeRegisterName,
                        Type* type = nullptr) {
        if (!lexicalScopes.empty()) {
            lexicalScopes.back()[hulkVariableName] = {qbeRegisterName, type};
        }
    }

    // ------------------------------------------------------------------
    // Store (or update) the type of a variable that already exists.
    // By default, it looks in the innermost scope only (the most common use case).
    // If you need to resolve through outer scopes, you can add an overload.
    void saveVariable(const std::string& hulkVariableName, Type* type) {
       if (lexicalScopes.empty()) return;   // should never happen (global scope exists)

    auto& currentScope = lexicalScopes.back();
    auto it = currentScope.find(hulkVariableName);
    if (it != currentScope.end()) {
        // Update existing variable's type
        it->second.type = type;
    } else {
        // Add new variable to the current scope with the given type.
        // Since no QBE register name is provided, we store an empty string.
        // You may want to generate a placeholder or throw if a register is mandatory.
        currentScope[hulkVariableName] = {"", type};
    }
    }

    // ------------------------------------------------------------------
    // Resolve a variable name to its QBE register (unchanged behaviour).
    std::string resolveVariable(const std::string& hulkVariableName) const {
        for (auto it = lexicalScopes.rbegin(); it != lexicalScopes.rend(); ++it) {
            auto found = it->find(hulkVariableName);
            if (found != it->end()) {
                return found->second.qbeRegisterName;
            }
        }
        throw std::runtime_error(
            "Semantic Error: Variable not found in lexical scope: " + hulkVariableName
        );
    }

    // ------------------------------------------------------------------
    // (Optional) Resolve a variable and return its type.
    Type* resolveType(const std::string& hulkVariableName) const {
        for (auto it = lexicalScopes.rbegin(); it != lexicalScopes.rend(); ++it) {
            auto found = it->find(hulkVariableName);
            if (found != it->end()) {
                return found->second.type;
            }
        }
        throw std::runtime_error(
            "Semantic Error: Variable not found in lexical scope: " + hulkVariableName
        );
    }

    // ------------------------------------------------------------------
    // Return all visible variables (from innermost to outermost) along with their types.
    // The result is a vector of pairs (name, type).  If a variable has not been typed yet,
    // its type will be nullptr (or you can filter/exclude those if you prefer).
    std::vector<std::pair<std::string, Type*>> getAllVariables() const {
        std::vector<std::pair<std::string, Type*>> result;
        std::unordered_set<std::string> seen;

        for (auto it = lexicalScopes.rbegin(); it != lexicalScopes.rend(); ++it) {
            for (const auto& [name, info] : *it) {
                if (seen.insert(name).second) {   // only the first (innermost) occurrence
                    result.emplace_back(name, info.type);
                }
            }
        }
        return result;
    }
};