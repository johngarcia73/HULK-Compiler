#include "symbol.hpp"
#include "grammar.hpp"
#include "first.hpp"

int main() {
    Grammar G;

    auto E = G.symtab.add(SymbolKind::NonTerminal, "E");
    auto T = G.symtab.add(SymbolKind::NonTerminal, "T");
    auto plus = G.symtab.add(SymbolKind::Terminal, "+");
    auto id = G.symtab.add(SymbolKind::Terminal, "id");

    G.start_symbol = E;

    G.add_production(E, {E, plus, T});
    G.add_production(E, {T});
    G.add_production(T, {id});

    G.build_indices();

    auto first = compute_nullable_first(G);

    return 0;
}