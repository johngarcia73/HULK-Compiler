// ============================================================================
// test_build_grammar.cpp
//
// Demuestra el uso de build_grammar_from_spec() y build_grammar_from_file()
// para construir objetos Grammar listos para usar en el parser generator
// ============================================================================

#include "genparser.hpp"
#include "grammar.hpp"
#include <iostream>
#include <iomanip>

void print_grammar(const Grammar& grammar) {
    std::cout << "\n┌─ Grammar Information ─────────────────────────────────┐\n";
    
    // Información general
    std::cout << "│ Start Symbol: " << std::left << std::setw(38)
              << grammar.symtab[grammar.start_symbol].name << "│\n";
    std::cout << "│ Total Symbols: " << std::left << std::setw(37)
              << grammar.symbol_count() << "│\n";
    std::cout << "│ Total Productions: " << std::left << std::setw(33)
              << grammar.get_productions().size() << "│\n";
    
    std::cout << "├────────────────────────────────────────────────────────┤\n";
    std::cout << "│ Symbols:\n";
    
    // Terminales
    std::cout << "│   Terminals:\n";
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        const auto& sym = grammar.symtab[i];
        if (sym.kind == SymbolKind::Terminal) {
            std::cout << "│     • " << sym.name << " (ID: " << i << ")\n";
        }
    }
    
    // No-terminales
    std::cout << "│   Non-Terminals:\n";
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        const auto& sym = grammar.symtab[i];
        if (sym.kind == SymbolKind::NonTerminal) {
            std::cout << "│     • " << sym.name << " (ID: " << i << ")\n";
        }
    }
    
    // EOF
    std::cout << "│   Special:\n";
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        const auto& sym = grammar.symtab[i];
        if (sym.kind == SymbolKind::End) {
            std::cout << "│     • " << sym.name << " (ID: " << i << ")\n";
        }
    }
    
    std::cout << "├────────────────────────────────────────────────────────┤\n";
    std::cout << "│ Productions:\n";
    
    const auto& prods = grammar.get_productions();
    for (size_t i = 0; i < prods.size(); ++i) {
        const auto& prod = prods[i];
        std::cout << "│   [" << std::right << std::setw(2) << i << "] "
                  << std::left << std::setw(15) << grammar.symtab[prod.lhs].name
                  << " →";
        
        if (prod.rhs.empty()) {
            std::cout << " ε";
        } else {
            for (const auto& sym_id : prod.rhs) {
                std::cout << " " << grammar.symtab[sym_id].name;
            }
        }
        
        if (!prod.action_code.empty()) {
            std::string action_preview = prod.action_code.substr(0, 30);
            if (prod.action_code.length() > 30) action_preview += "...";
            std::cout << "  « " << action_preview << " »";
        }
        std::cout << "\n";
    }
    
    std::cout << "└────────────────────────────────────────────────────────┘\n\n";
}

int main() {
    std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
    std::cout << "║     TEST: Construir Grammar desde .y file              ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════╝\n";
    
    try {
        // ====================================================================
        // MÉTODO 1: Usando build_grammar_from_file()
        // ====================================================================
        
        std::cout << "\n[MÉTODO 1] Construir Grammar directamente desde archivo:\n";
        std::cout << "  Grammar g = build_grammar_from_file(\"example_grammar.y\");\n";
        
        Grammar grammar1 = build_grammar_from_file("example_grammar.y");
        std::cout << "  ✓ Grammar construida exitosamente\n";
        print_grammar(grammar1);
        
        // ====================================================================
        // MÉTODO 2: Usando parse_grammar_file() + build_grammar_from_spec()
        // ====================================================================
        
        std::cout << "\n[MÉTODO 2] Construir Grammar en dos pasos (más control):\n";
        std::cout << "  GrammarSpec spec = parse_grammar_file(\"example_grammar.y\");\n";
        std::cout << "  Grammar g = build_grammar_from_spec(spec);\n";
        
        GrammarSpec spec = parse_grammar_file("example_grammar.y");
        Grammar grammar2 = build_grammar_from_spec(spec);
        std::cout << "  ✓ Grammar construida exitosamente\n";
        print_grammar(grammar2);
        
        // ====================================================================
        // VERIFICACIÓN: Acceso a Productions con action_code
        // ====================================================================
        
        std::cout << "\n[VERIFICACIÓN] Acceso a datos de producciones:\n";
        std::cout << "├─ Ejemplo de acceso a production.action_code:\n";
        
        const auto& prods = grammar1.get_productions();
        for (size_t i = 0; i < std::min(size_t(3), prods.size()); ++i) {
            const auto& prod = prods[i];
            std::cout << "│  prods[" << i << "].lhs = " << grammar1.symtab[prod.lhs].name << "\n";
            std::cout << "│  prods[" << i << "].rhs = [";
            for (size_t j = 0; j < prod.rhs.size(); ++j) {
                if (j > 0) std::cout << ", ";
                std::cout << grammar1.symtab[prod.rhs[j]].name;
            }
            std::cout << "]\n";
            std::cout << "│  prods[" << i << "].action_code = \""
                      << prod.action_code << "\"\n";
        }
        std::cout << "└─ ✓ Todos los datos accesibles\n";
        
        // ====================================================================
        // USO TÍPICO: Listo para usar en LALR builder
        // ====================================================================
        
        std::cout << "\n[LISTO PARA] Usar en LALR builder:\n";
        std::cout << "  • grammar.symtab → Tabla de símbolos\n";
        std::cout << "  • grammar.get_productions() → Reglas de producción\n";
        std::cout << "  • grammar.start_symbol → Símbolo de inicio\n";
        std::cout << "  • grammar.productions_of(nt) → Producciones por no-terminal\n";
        std::cout << "  • production.action_code → Código semántico\n";
        
        std::cout << "\n╔══════════════════════════════════════════════════════════╗\n";
        std::cout << "║              ✓ TODO FUNCIONA CORRECTAMENTE              ║\n";
        std::cout << "╚══════════════════════════════════════════════════════════╝\n\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << "\n\n";
        return 1;
    }
}
