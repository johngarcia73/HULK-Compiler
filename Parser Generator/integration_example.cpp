// ============================================================================
// integration_example.cpp
//
// Example showing integration between Lexer and Parser with AST construction
// ============================================================================

#include "grammar.hpp"
#include "lalr_builder.hpp"
#include "parser_interface.hpp"
#include "ast_node.hpp"   
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cassert>

int main() {
    std::cout << "\n=== LALR(1) Parser - AST Integration Example ===\n\n";

    // Create grammar
    Grammar grammar;

    // Add terminals (from lexer)
    uint32_t NUMBER = grammar.symtab.add(SymbolKind::Terminal, "number");
    uint32_t PLUS   = grammar.symtab.add(SymbolKind::Terminal, "+");
    uint32_t TIMES  = grammar.symtab.add(SymbolKind::Terminal, "*");
    uint32_t DOLLAR = grammar.symtab.add(SymbolKind::End, "$");

    // Add non-terminals
    uint32_t E = grammar.symtab.add(SymbolKind::NonTerminal, "E");
    uint32_t T = grammar.symtab.add(SymbolKind::NonTerminal, "T");
    uint32_t F = grammar.symtab.add(SymbolKind::NonTerminal, "F");
    uint32_t S_prime = grammar.symtab.add(SymbolKind::NonTerminal, "S'");

    // Add productions
    // E -> E + T | T
    grammar.add_production(E, {E, PLUS, T});  // prod 0
    grammar.add_production(E, {T});           // prod 1
    // T -> T * F | F
    grammar.add_production(T, {T, TIMES, F}); // prod 2
    grammar.add_production(T, {F});           // prod 3
    // F -> number
    grammar.add_production(F, {NUMBER});      // prod 4
    // S' -> E
    grammar.add_production(S_prime, {E});     // prod 5

    grammar.start_symbol = S_prime;
    grammar.build_indices();

    // Generate parser
    LALRParser parser = LALRParser::from_grammar(grammar, DOLLAR);
    std::cout << "✓ Parser generated: " << parser.state_count() << " states\n";
    std::cout << "  Symbols in grammar: " << grammar.symtab.size() << "\n";
    std::cout << "  Productions: " << grammar.get_productions().size() << "\n";
    std::cout << "  NUMBER symbol ID: " << NUMBER << "\n\n";

    

    auto builder = std::make_unique<ExprASTBuilder>();
    parser.set_builder(std::move(builder));

    // Example: Parse "2 + 3 * 4"
    std::vector<Token> tokens = {
        Token(NUMBER, "2", 1, 1),
        Token(PLUS,   "+", 1, 3),
        Token(NUMBER, "3", 1, 5),
        Token(TIMES,  "*", 1, 7),
        Token(NUMBER, "4", 1, 9),
    };

    std::cout << "Parsing: 2 + 3 * 4\n";
    ParseResult result = parser.parse(tokens);

    if (result.is_success()) {
        std::cout << "✓ Parse succeeded!\n";
        std::cout << "  Reductions: ";
        for (size_t i = 0; i < result.reduction_sequence.size(); ++i) {
            if (i > 0) std::cout << " ";
            std::cout << "P" << result.reduction_sequence[i];
        }
        std::cout << "\n";

        if (result.ast) {
            std::cout << "  AST construido: ";
            //printAST(result.ast.get());
            std::cout << "\n";

            ExprNode* root = dynamic_cast<ExprNode*>(result.ast.get());
            
        } else {
            std::cout << "  No se construyó AST (builder no configurado o error)\n";
        }
    } else {
        std::cout << "✗ Parse failed: " << result.error_message << "\n";
    }

    std::cout << "\n";
    return 0;
}