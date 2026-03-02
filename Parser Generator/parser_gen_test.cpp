// test.cpp
// Pruebas para el generador LALR(1) funcional (usa tus funciones libres).
//
// Requisitos: tener compilables e incluidos estos módulos:
//  - grammar.hpp       (SymbolTable, Grammar, Production)
//  - symbol.hpp        (Symbol, SymbolKind)
//  - first.hpp         (FirstResult, compute_nullable_first)
//  - lalr_builder.hpp  (LALRBuilder, ParseTables, Conflict, ActionKind)
//  - parser_interface.hpp  (Token, ParseResult, LALRParser)
// Ajusta rutas de include si tus archivos están en otro directorio.

#include <iostream>
#include <cassert>
#include <vector>
#include <optional>

#include "grammar.hpp"
#include "symbol.hpp"
#include "first.hpp"
#include "lalr_builder.hpp"
#include "parser_interface.hpp"

// ============================================================================
// Helper functions to build various test grammars
// ============================================================================

// Grammar 1: Classic expression grammar with precedence (UNAMBIGUOUS)
// E -> E + T | T
// T -> T * F | F
// F -> ( E ) | id
static void build_expression_grammar(Grammar &G, SymbolId &dollar_out) {
    SymbolId id_sym   = G.symtab.add(SymbolKind::Terminal, "id");
    SymbolId plus     = G.symtab.add(SymbolKind::Terminal, "+");
    SymbolId star     = G.symtab.add(SymbolKind::Terminal, "*");
    SymbolId lpar     = G.symtab.add(SymbolKind::Terminal, "(");
    SymbolId rpar     = G.symtab.add(SymbolKind::Terminal, ")");
    SymbolId dollar   = G.symtab.add(SymbolKind::End, "$");

    SymbolId E_sym    = G.symtab.add(SymbolKind::NonTerminal, "E");
    SymbolId T_sym    = G.symtab.add(SymbolKind::NonTerminal, "T");
    SymbolId F_sym    = G.symtab.add(SymbolKind::NonTerminal, "F");
    SymbolId S_prime  = G.symtab.add(SymbolKind::NonTerminal, "S'");

    G.start_symbol = S_prime;

    G.add_production(E_sym, {E_sym, plus, T_sym});
    G.add_production(E_sym, {T_sym});
    G.add_production(T_sym, {T_sym, star, F_sym});
    G.add_production(T_sym, {F_sym});
    G.add_production(F_sym, {lpar, E_sym, rpar});
    G.add_production(F_sym, {id_sym});
    G.add_production(S_prime, {E_sym});

    G.build_indices();
    dollar_out = dollar;
}

// Grammar 2: Simple statement grammar (UNAMBIGUOUS)
// S' -> S
// S -> if E then S else S | while E do S | id := E
// E -> id
static void build_statement_grammar(Grammar &G, SymbolId &dollar_out) {
    SymbolId id       = G.symtab.add(SymbolKind::Terminal, "id");
    SymbolId assign   = G.symtab.add(SymbolKind::Terminal, ":=");
    SymbolId if_kw    = G.symtab.add(SymbolKind::Terminal, "if");
    SymbolId then_kw  = G.symtab.add(SymbolKind::Terminal, "then");
    SymbolId else_kw  = G.symtab.add(SymbolKind::Terminal, "else");
    SymbolId while_kw = G.symtab.add(SymbolKind::Terminal, "while");
    SymbolId do_kw    = G.symtab.add(SymbolKind::Terminal, "do");
    SymbolId dollar   = G.symtab.add(SymbolKind::End, "$");

    SymbolId S_sym    = G.symtab.add(SymbolKind::NonTerminal, "S");
    SymbolId E_sym    = G.symtab.add(SymbolKind::NonTerminal, "E");
    SymbolId S_prime  = G.symtab.add(SymbolKind::NonTerminal, "S'");

    G.start_symbol = S_prime;

    // S -> if E then S else S
    G.add_production(S_sym, {if_kw, E_sym, then_kw, S_sym, else_kw, S_sym});
    // S -> while E do S
    G.add_production(S_sym, {while_kw, E_sym, do_kw, S_sym});
    // S -> id := E
    G.add_production(S_sym, {id, assign, E_sym});
    // E -> id
    G.add_production(E_sym, {id});
    // S' -> S
    G.add_production(S_prime, {S_sym});

    G.build_indices();
    dollar_out = dollar;
}

// Grammar 3: Simple list grammar (UNAMBIGUOUS)
// S' -> L
// L -> L , num | num
static void build_list_grammar(Grammar &G, SymbolId &dollar_out) {
    SymbolId num      = G.symtab.add(SymbolKind::Terminal, "num");
    SymbolId comma    = G.symtab.add(SymbolKind::Terminal, ",");
    SymbolId dollar   = G.symtab.add(SymbolKind::End, "$");

    SymbolId L_sym    = G.symtab.add(SymbolKind::NonTerminal, "L");
    SymbolId S_prime  = G.symtab.add(SymbolKind::NonTerminal, "S'");

    G.start_symbol = S_prime;

    // L -> L , num
    G.add_production(L_sym, {L_sym, comma, num});
    // L -> num
    G.add_production(L_sym, {num});
    // S' -> L
    G.add_production(S_prime, {L_sym});

    G.build_indices();
    dollar_out = dollar;
}

// Grammar 4: Ambiguous expression grammar (WITH SHIFT/REDUCE CONFLICTS)
// E -> E + E | E * E | id
static void build_ambiguous_expr_grammar(Grammar &G, SymbolId &dollar_out) {
    SymbolId id       = G.symtab.add(SymbolKind::Terminal, "id");
    SymbolId plus     = G.symtab.add(SymbolKind::Terminal, "+");
    SymbolId star     = G.symtab.add(SymbolKind::Terminal, "*");
    SymbolId dollar   = G.symtab.add(SymbolKind::End, "$");

    SymbolId E_sym    = G.symtab.add(SymbolKind::NonTerminal, "E");
    SymbolId S_prime  = G.symtab.add(SymbolKind::NonTerminal, "S'");

    G.start_symbol = S_prime;

    G.add_production(E_sym, {E_sym, plus, E_sym});
    G.add_production(E_sym, {E_sym, star, E_sym});
    G.add_production(E_sym, {id});
    G.add_production(S_prime, {E_sym});

    G.build_indices();
    dollar_out = dollar;
}

// ============================================================================
// Test functions
// ============================================================================

struct GrammarTest {
    const char* name;
    void (*builder)(Grammar&, SymbolId&);
    bool expect_conflicts;
    int expected_conflict_count;  // -1 means "any number > 0"
};

static void test_grammar_properties(const GrammarTest &test) {
    std::cout << "\n  [Subtest] " << test.name << "\n";

    Grammar G;
    uint32_t dollar;
    test.builder(G, dollar);

    // Create parser from grammar using LALRBuilder
    std::vector<Conflict> conflicts;
    ParseTables tables = LALRBuilder::build(G, dollar, conflicts);

    std::cout << "    - Parse tables built (" << tables.ACTION.size() << " states)\n";
    std::cout << "    - Conflicts found: " << conflicts.size() << "\n";

    // Validate table integrity
    assert(!tables.ACTION.empty());
    assert(tables.ACTION.size() == tables.GOTO.size());

    // Check conflict expectations
    if (test.expect_conflicts) {
        assert(!conflicts.empty());
        if (test.expected_conflict_count > 0) {
            assert(conflicts.size() == test.expected_conflict_count);
        }
        std::cout << "    - Conflict detection: PASSED (expected ambiguity)\n";
    } else {
        assert(conflicts.empty());
        std::cout << "    - Conflict detection: PASSED (no conflicts as expected)\n";
    }

    // Validate ACCEPT action exists
    bool found_accept = false;
    for (uint32_t s = 0; s < tables.ACTION.size(); ++s) {
        auto it = tables.ACTION[s].find(dollar);
        if (it != tables.ACTION[s].end() && it->second.kind == ActionKind::Accept) {
            found_accept = true;
            break;
        }
    }
    assert(found_accept);
    std::cout << "    - ACCEPT action found\n";

    // Validate that reduce actions exist for all completed items
    int reduce_count = 0;
    for (uint32_t s = 0; s < tables.ACTION.size(); ++s) {
        for (auto& kv : tables.ACTION[s]) {
            if (kv.second.kind == ActionKind::Reduce) {
                reduce_count++;
            }
        }
    }
    assert(reduce_count > 0);
    std::cout << "    - Reduce actions found (" << reduce_count << " total)\n";

    // Validate GOTO entries exist
    int goto_count = 0;
    for (uint32_t s = 0; s < tables.GOTO.size(); ++s) {
        goto_count += tables.GOTO[s].size();
    }
    assert(goto_count > 0);
    std::cout << "    - GOTO actions found (" << goto_count << " total)\n";

    std::cout << "    ✓ PASSED\n";
}

static void test_multiple_grammars() {
    std::cout << "[TEST] Multiple grammar validation\n";

    GrammarTest tests[] = {
        {"Expression Grammar (unambiguous)", build_expression_grammar, false, 0},
        {"Statement Grammar (unambiguous)", build_statement_grammar, false, 0},
        {"List Grammar (unambiguous)", build_list_grammar, false, 0},
        {"Ambiguous Expression Grammar", build_ambiguous_expr_grammar, true, 4},
    };

    for (const auto &test : tests) {
        test_grammar_properties(test);
    }

    std::cout << "  All grammar tests passed!\n";
}

static void test_first_and_nullable() {
    std::cout << "[TEST] FIRST/nullable computation\n";
    Grammar G;
    SymbolId dollar;
    build_expression_grammar(G, dollar);

    FirstResult fr = compute_nullable_first(G);

    SymbolId id_sym = G.symtab.get("id");
    SymbolId lpar = G.symtab.get("(");
    SymbolId F_sym = G.symtab.get("F");
    SymbolId T_sym = G.symtab.get("T");
    SymbolId E_sym = G.symtab.get("E");

    // Check nullable
    assert(fr.nullable[F_sym] == false);
    assert(fr.nullable[T_sym] == false);
    assert(fr.nullable[E_sym] == false);

    // Check FIRST sets
    assert(fr.FIRST[F_sym].contains(id_sym));
    assert(fr.FIRST[F_sym].contains(lpar));
    assert(fr.FIRST[E_sym].contains(id_sym));
    assert(fr.FIRST[E_sym].contains(lpar));

    std::cout << "  ✓ PASSED\n";
}

int main() {
    std::cout << "\n" << std::string(70, '=') << "\n";
    std::cout << "=== LALR(1) Parser Generator - Comprehensive Test Suite ===\n";
    std::cout << std::string(70, '=') << "\n";

    try {
        test_first_and_nullable();
        test_multiple_grammars();

        std::cout << "\n" << std::string(70, '=') << "\n";
        std::cout << "✓ ALL TESTS PASSED - LALR(1) Generator is functional\n";
        std::cout << std::string(70, '=') << "\n\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "\n✗ TEST FAILED: " << e.what() << "\n\n";
        return 1;
    }
}