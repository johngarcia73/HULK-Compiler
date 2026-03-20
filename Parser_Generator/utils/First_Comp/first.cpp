#include "first.hpp"

FirstResult compute_nullable_first(const Grammar& G) {
    size_t N = G.symbol_count();

    FirstResult result;
    result.nullable.assign(N, false);
    result.FIRST.reserve(N);

    for (size_t i = 0; i < N; ++i)
        result.FIRST.emplace_back(N);

    bool changed = true;

    while (changed) {
        changed = false;

        for (const auto& p : G.get_productions()) {

            // Nullable
            bool all_nullable = true;
            for (auto sym : p.rhs) {
                if (!result.nullable[sym]) {
                    all_nullable = false;
                    break;
                }
            }

            if (all_nullable && !result.nullable[p.lhs]) {
                result.nullable[p.lhs] = true;
                changed = true;
            }

            // FIRST
            for (auto sym : p.rhs) {

                if (G.symtab[sym].kind == SymbolKind::Terminal) {
                    if (!result.FIRST[p.lhs].contains(sym)) {
                        result.FIRST[p.lhs].insert(sym);
                        changed = true;
                    }
                    break;
                }

                if (result.FIRST[p.lhs].unite(result.FIRST[sym]))
                    changed = true;

                if (!result.nullable[sym])
                    break;
            }
        }
    }

    return result;
}