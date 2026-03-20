#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

using SymbolId = uint32_t;

enum class SymbolKind {
    Terminal,
    NonTerminal,
    End
};

struct Symbol {
    SymbolKind kind;
    std::string name;
    SymbolId id;
};

class SymbolTable {
public:
    SymbolId add(SymbolKind kind, const std::string& name) {
        SymbolId id = symbols.size();
        symbols.push_back(Symbol{kind, name, id});
        by_name[name] = id;
        return id;
    }

    const Symbol& operator[](SymbolId id) const {
        return symbols[id];
    }

    SymbolId get(const std::string& name) const {
        return by_name.at(name);
    }

    size_t size() const { return symbols.size(); }

private:
    std::vector<Symbol> symbols;
    std::unordered_map<std::string, SymbolId> by_name;
};