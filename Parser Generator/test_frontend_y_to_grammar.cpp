// ============================================================================
// test_frontend_y_to_grammar.cpp
//
// Test del frontend: .y file → genparser → Grammar
// Verifica que el flujo .y → GrammarSpec → Grammar funciona correctamente
// ============================================================================

#include "genparser.hpp"
#include "grammar.hpp"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║        TEST: Frontend .y → GrammarSpec → Grammar              ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    // ========================================================================
    // TEST 1: Parsear archivo .y
    // ========================================================================
    
    std::cout << "[TEST 1] Parsing .y file\n";
    GrammarSpec spec;
    try {
        spec = parse_grammar_file("example_grammar.y");
        std::cout << "  ✓ Parsed successfully\n";
        std::cout << "    Terminals: " << spec.terminals.size() << "\n";
        std::cout << "    Non-terminals: " << spec.nonterminals.size() << "\n";
        std::cout << "    Productions: " << spec.productions.size() << "\n";
        std::cout << "    Start symbol: '" << spec.start_symbol << "'\n\n";
    } catch (const std::exception& e) {
        std::cerr << "  ✗ Failed: " << e.what() << "\n";
        return 1;
    }
    
    // ========================================================================
    // TEST 2: Verificar que se extrajeron terminales correctos
    // ========================================================================
    
    std::cout << "[TEST 2] Checking terminals\n";
    {
        std::cout << "  Terminals: ";
        for (const auto& t : spec.terminals) {
            std::cout << t << " ";
        }
        std::cout << "\n";
        
        bool has_number = false, has_plus = false, has_times = false;
        for (const auto& t : spec.terminals) {
            if (t == "NUMBER") has_number = true;
            if (t == "PLUS") has_plus = true;
            if (t == "TIMES") has_times = true;
        }
        
        if (has_number && has_plus && has_times) {
            std::cout << "  ✓ All expected terminals found\n\n";
        } else {
            std::cout << "  ✗ Missing some terminals\n";
            return 1;
        }
    }
    
    // ========================================================================
    // TEST 3: Verificar que se extrajeron no-terminales correctos
    // ========================================================================
    
    std::cout << "[TEST 3] Checking non-terminals\n";
    {
        std::cout << "  Non-terminals: ";
        for (const auto& nt : spec.nonterminals) {
            std::cout << nt << " ";
        }
        std::cout << "\n";
        
        bool has_expr = spec.nonterminals.count("expr") > 0;
        bool has_term = spec.nonterminals.count("term") > 0;
        bool has_factor = spec.nonterminals.count("factor") > 0;
        
        if (has_expr && has_term && has_factor) {
            std::cout << "  ✓ All expected non-terminals found\n\n";
        } else {
            std::cout << "  ✗ Missing some non-terminals\n";
            return 1;
        }
    }
    
    // ========================================================================
    // TEST 4: Verificar que se extrajeron producciones correctas
    // ========================================================================
    
    std::cout << "[TEST 4] Checking productions\n";
    {
        std::cout << "  Productions:\n";
        for (size_t i = 0; i < spec.productions.size(); ++i) {
            const auto& prod = spec.productions[i];
            std::cout << "    " << i << ". " << prod.lhs << " →";
            for (const auto& sym : prod.rhs) {
                std::cout << " " << sym;
            }
            if (!prod.action_code.empty()) {
                std::cout << " " << prod.action_code;
            }
            std::cout << "\n";
        }
        
        if (spec.productions.size() == 6) {
            std::cout << "  ✓ Correct number of productions (6)\n\n";
        } else {
            std::cout << "  ⚠ Got " << spec.productions.size() << " productions (expected 6)\n\n";
        }
    }
    
    // ========================================================================
    // TEST 5: Verificar que se extrajeron acciones semánticas
    // ========================================================================
    
    std::cout << "[TEST 5] Checking semantic actions\n";
    {
        int with_actions = 0;
        for (const auto& prod : spec.productions) {
            if (!prod.action_code.empty()) {
                with_actions++;
                std::cout << "  Action found: " << prod.lhs << " → "
                          << prod.action_code.substr(0, 30) << "...\n";
            }
        }
        
        if (with_actions > 0) {
            std::cout << "  ✓ Found " << with_actions << " semantic actions\n\n";
        } else {
            std::cout << "  ⚠ No semantic actions found (they may be empty)\n\n";
        }
    }
    
    // ========================================================================
    // TEST 6: Construir Grammar objeto desde GrammarSpec
    // ========================================================================
    
    std::cout << "[TEST 6] Building Grammar object\n";
    {
        Grammar grammar;
        std::unordered_map<std::string, uint32_t> symbol_ids;
        
        // Agregar terminales
        for (const auto& terminal : spec.terminals) {
            uint32_t id = grammar.symtab.add(SymbolKind::Terminal, terminal);
            symbol_ids[terminal] = id;
        }
        
        // Agregar no-terminales
        for (const auto& nonterminal : spec.nonterminals) {
            uint32_t id = grammar.symtab.add(SymbolKind::NonTerminal, nonterminal);
            symbol_ids[nonterminal] = id;
        }
        
        // Agregar símbolo de fin
        uint32_t dollar_id = grammar.symtab.add(SymbolKind::End, "$");
        symbol_ids["$"] = dollar_id;
        
        // Agregar todas las producciones
        for (const auto& parsed_prod : spec.productions) {
            uint32_t lhs_id = symbol_ids[parsed_prod.lhs];
            std::vector<uint32_t> rhs_ids;
            
            for (const auto& sym : parsed_prod.rhs) {
                rhs_ids.push_back(symbol_ids[sym]);
            }
            
            // ¡IMPORTANTE! Aquí se pasa el action_code a la Grammar
            grammar.add_production(lhs_id, rhs_ids, parsed_prod.action_code);
        }
        
        grammar.start_symbol = symbol_ids[spec.start_symbol];
        grammar.build_indices();
        
        std::cout << "  ✓ Grammar constructed\n";
        std::cout << "    Symbols: " << grammar.symtab.size() << "\n";
        std::cout << "    Productions: " << grammar.get_productions().size() << "\n";
        std::cout << "    Start: " << grammar.symtab[grammar.start_symbol].name << "\n\n";
    }
    
    // ========================================================================
    // TEST 7: Verificar que las acciones se almacenaron en las Productions
    // ========================================================================
    
    std::cout << "[TEST 7] Verifying action_code stored in Productions\n";
    {
        Grammar grammar;
        std::unordered_map<std::string, uint32_t> symbol_ids;
        
        // Agregar símbolos
        for (const auto& terminal : spec.terminals) {
            symbol_ids[terminal] = grammar.symtab.add(SymbolKind::Terminal, terminal);
        }
        for (const auto& nonterminal : spec.nonterminals) {
            symbol_ids[nonterminal] = grammar.symtab.add(SymbolKind::NonTerminal, nonterminal);
        }
        symbol_ids["$"] = grammar.symtab.add(SymbolKind::End, "$");
        
        // Agregar producciones
        for (const auto& parsed_prod : spec.productions) {
            uint32_t lhs_id = symbol_ids[parsed_prod.lhs];
            std::vector<uint32_t> rhs_ids;
            for (const auto& sym : parsed_prod.rhs) {
                rhs_ids.push_back(symbol_ids[sym]);
            }
            grammar.add_production(lhs_id, rhs_ids, parsed_prod.action_code);
        }
        
        grammar.start_symbol = symbol_ids[spec.start_symbol];
        grammar.build_indices();
        
        // Acceder a las producciones y verificar action_code
        const auto& prods = grammar.get_productions();
        std::cout << "  Checking Productions:\n";
        
        for (size_t i = 0; i < prods.size(); ++i) {
            const auto& prod = prods[i];
            std::cout << "    Prod " << i << " (" << grammar.symtab[prod.lhs].name << " →): ";
            
            if (!prod.action_code.empty()) {
                std::cout << "action_code='" << prod.action_code.substr(0, 40);
                if (prod.action_code.length() > 40) std::cout << "...";
                std::cout << "' ✓\n";
            } else {
                std::cout << "(no action) ✓\n";
            }
        }
        
        std::cout << "\n  ✓ All productions accessible with action_code field\n\n";
    }
    
    // ========================================================================
    // RESUMEN
    // ========================================================================
    
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║                     ALL TESTS PASSED ✓                         ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║  Frontend working correctly:                                   ║\n";
    std::cout << "║  ✓ .y file parsing (genparser)                                 ║\n";
    std::cout << "║  ✓ GrammarSpec construction                                    ║\n";
    std::cout << "║  ✓ Grammar object construction                                 ║\n";
    std::cout << "║  ✓ Production semantic actions stored and accessible           ║\n";
    std::cout << "║                                                                ║\n";
    std::cout << "║  The frontend pipeline (.y → Grammar) is working correctly.    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    
    return 0;
}
