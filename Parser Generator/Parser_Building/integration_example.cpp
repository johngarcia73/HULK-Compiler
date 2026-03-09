// ============================================================================
// integration_example.cpp
//
// Example showing integration between Lexer and Parser with AST construction
// ============================================================================

#include "../utils/Grammar/grammar.hpp"
#include "lalr_builder.hpp"
#include "parser_interface.hpp"
#include "../AST_Builder/ast_builder.hpp"
#include "genparser.hpp"
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <cassert>

int main() {
    std::cout << "\n=== LALR(1) Parser - AST Integration Example ===\n\n";

    // Create grammar from .y file
    Grammar grammar = build_grammar_from_file("integration_grammar.y");

    // Get symbol IDs from the grammar
    uint32_t NUMBER = 0;  // First terminal
    uint32_t PLUS   = 1;  // Second terminal
    uint32_t TIMES  = 2;  // Third terminal
    uint32_t DOLLAR = 3;  // End symbol

    // Find DOLLAR symbol ID dynamically
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        if (grammar.symtab[i].kind == SymbolKind::End && grammar.symtab[i].name == "$") {
            DOLLAR = i;
            break;
        }
    }

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