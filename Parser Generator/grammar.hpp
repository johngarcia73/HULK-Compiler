#pragma once
#include "symbol.hpp"
#include <vector>

using ProdId = uint32_t;

struct Production {
    ProdId id;
    SymbolId lhs;
    std::vector<SymbolId> rhs;
};

class Grammar {
public:
    SymbolTable symtab;

    SymbolId start_symbol;

    ProdId add_production(SymbolId lhs, const std::vector<SymbolId>& rhs) {
        ProdId id = productions.size();
        productions.push_back({id, lhs, rhs});
        return id;
    }

    void build_indices();

    const std::vector<Production>& get_productions() const {
        return productions;
    }

    const std::vector<ProdId>& productions_of(SymbolId nt) const {
        return prods_by_lhs[nt];
    }

    size_t symbol_count() const { return symtab.size(); }

private:
    std::vector<Production> productions;
    std::vector<std::vector<ProdId>> prods_by_lhs;
};