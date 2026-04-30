#pragma once

#include "../utils/Grammar/grammar.hpp"
#include "../Parser_Building/genparser.hpp"
#include <string>
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream>

// Process action_code Bison-style y turns into C++ codde
inline std::string convert_action_code(const std::string& action_code, 
                                       const std::vector<uint32_t>& rhs_symbols,
                                       const Grammar& grammar) {
    std::string result = action_code;
    
    // Replace $1, $2, $3... by rhs[0], rhs[1], rhs[2]..., in  reversed order to avoid conflicts (e.g. $10 contains $1)
    for (int i = (int)rhs_symbols.size(); i >= 1; --i) {
        std::string dollar_n = "$" + std::to_string(i);
        std::string rhs_n = "rhs[" + std::to_string(i - 1) + "]";
        
        // Usar \b para word boundary
        std::string pattern = R"(\)" + dollar_n + R"(\b)";
        result = std::regex_replace(result, std::regex(pattern), rhs_n);
    }
    
    // Replace $$ by  result
    result = std::regex_replace(result, std::regex(R"(\$\$)"), "result");
    
    return result;
}

// Generates ASTBuilder subclass code based on grammar
inline void generate_ast_builder(const std::string& grammar_file,
                                 const std::string& output_file) {
    // Parse grammar
    Grammar grammar = build_grammar_from_file(grammar_file);
    const auto& productions = grammar.get_productions();
    
    std::stringstream code;
    
    // C++ Header
    code << "#include \"ast_node.hpp\"\n";
    code << "#include \"ast_builder.hpp\"\n";
    code << "#include <cassert>\n\n";
    
    code << "void ExprASTBuilder::onShift(uint32_t symbol, const Token& token, ASTNode*& result) {\n";
    code << "    // Terminal 'number' with ID " << 0 << "\n";
    code << "    if (symbol == 0) {\n";
    code << "        result = new NumberNode(token.value, classify_number_kind(token.value));\n";
    code << "    } else {\n";
    code << "        result = nullptr; // other terminals does not generate nodes\n";
    code << "    }\n";
    code << "}\n\n";
    
    code << "void ExprASTBuilder::onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) {\n";
    
    // Production Generation Comments
    code << "    // Productions:\n";
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
    
    // Generate cases
    for (size_t i = 0; i < productions.size(); ++i) {
        const auto& prod = productions[i];
        code << "        case " << i << ": // " << grammar.symtab[prod.lhs].name << " →";
        for (const auto& sym_id : prod.rhs) {
            code << " " << grammar.symtab[sym_id].name;
        }
        code << "\n";
        
        if (!prod.action_code.empty()) {
            // Convert action_code Bison-style to runtime style
            std::string converted = convert_action_code(prod.action_code, prod.rhs, grammar);
            code << "            " << converted << "\n";
        } else {
            // If no action_code, default passthrough
            code << "            result = rhs[0];\n";
        }
        code << "            break;\n";
    }
    
    code << "        default:\n";
    code << "            result = nullptr;\n";
    code << "            assert(false && \"Unknown production\");\n";
    code << "    }\n";
    code << "}\n";
    
    // Write file
    std::ofstream out(output_file);
    if (!out) {
        throw std::runtime_error("Cannot open output file: " + output_file);
    }
    out << code.str();
    out.close();
    
    std::cout << "✓ Generated " << output_file << "\n";
    std::cout << "  Productions: " << productions.size() << "\n";
}
