#include "grammar.hpp"

void Grammar::build_indices() {
    prods_by_lhs.assign(symtab.size(), {});
    for (const auto& p : productions) {
        prods_by_lhs[p.lhs].push_back(p.id);
    }
}