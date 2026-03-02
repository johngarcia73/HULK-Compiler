// ============================================================================
// code_generator.hpp
//
// Genera código C++ a partir de la Grammar con action_code
// Reemplaza notación Bison ($1, $2, $$) con notación de runtime (rhs[0], result)
// ============================================================================

#pragma once

#include "grammar.hpp"
#include "genparser.hpp"
#include <string>
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream>

// ============================================================================
// Procesa action_code Bison-style y lo convierte a runtime C++
// ============================================================================

inline std::string convert_action_code(const std::string& action_code, 
                                       const std::vector<uint32_t>& rhs_symbols,
                                       const Grammar& grammar) {
    std::string result = action_code;
    
    // Reemplazar $1, $2, $3... con rhs[0], rhs[1], rhs[2]...
    // Debe hacerse EN ORDEN INVERSO para evitar conflictos ($1 antes de $10, etc)
    for (int i = (int)rhs_symbols.size(); i >= 1; --i) {
        std::string dollar_n = "$" + std::to_string(i);
        std::string rhs_n = "rhs[" + std::to_string(i - 1) + "]";
        
        // Usar \b para word boundary
        std::string pattern = R"(\)" + dollar_n + R"(\b)";
        result = std::regex_replace(result, std::regex(pattern), rhs_n);
    }
    
    // Reemplazar $$ con result (al final para evitar conflictos)
    result = std::regex_replace(result, std::regex(R"(\$\$)"), "result");
    
    return result;
}

// ============================================================================
// Genera ast_builder.cpp a partir de la Grammar
// ============================================================================

inline void generate_ast_builder(const std::string& grammar_file,
                                 const std::string& output_file) {
    // Parse grammar
    Grammar grammar = build_grammar_from_file(grammar_file);
    const auto& productions = grammar.get_productions();
    
    // Generar código
    std::stringstream code;
    
    // Header
    code << "#include \"ast_node.hpp\"\n";
    code << "#include \"ast_builder.hpp\"\n";
    code << "#include <cassert>\n\n";
    
    code << "void ExprASTBuilder::onShift(uint32_t symbol, const Token& token, ASTNode*& result) {\n";
    code << "    // Terminal 'number' con ID " << 0 << "\n";
    code << "    if (symbol == 0) {\n";
    code << "        result = new NumberNode(std::stoi(token.value));\n";
    code << "    } else {\n";
    code << "        result = nullptr; // otros terminales no generan nodo\n";
    code << "    }\n";
    code << "}\n\n";
    
    code << "void ExprASTBuilder::onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) {\n";
    
    // Generar comentarios de producción
    code << "    // Producciones:\n";
    for (size_t i = 0; i < productions.size(); ++i) {
        const auto& prod = productions[i];
        code << "    // " << i << ": " << grammar.symtab[prod.lhs].name << " →";
        for (const auto& sym_id : prod.rhs) {
            code << " " << grammar.symtab[sym_id].name;
        }
        code << "\n";
    }
    code << "\n";
    
    code << "    switch (prod_id) {\n";
    
    // Generar casos
    for (size_t i = 0; i < productions.size(); ++i) {
        const auto& prod = productions[i];
        code << "        case " << i << ": // " << grammar.symtab[prod.lhs].name << " →";
        for (const auto& sym_id : prod.rhs) {
            code << " " << grammar.symtab[sym_id].name;
        }
        code << "\n";
        
        if (!prod.action_code.empty()) {
            // Convertir action_code Bison-style a runtime style
            std::string converted = convert_action_code(prod.action_code, prod.rhs, grammar);
            code << "            " << converted << "\n";
        } else {
            // Si no hay action_code, passthrough por defecto
            code << "            result = rhs[0];\n";
        }
        code << "            break;\n";
    }
    
    code << "        default:\n";
    code << "            result = nullptr;\n";
    code << "            assert(false && \"Producción desconocida\");\n";
    code << "    }\n";
    code << "}\n";
    
    // Escribir archivo
    std::ofstream out(output_file);
    if (!out) {
        throw std::runtime_error("Cannot open output file: " + output_file);
    }
    out << code.str();
    out.close();
    
    std::cout << "✓ Generated " << output_file << "\n";
    std::cout << "  Productions: " << productions.size() << "\n";
}
