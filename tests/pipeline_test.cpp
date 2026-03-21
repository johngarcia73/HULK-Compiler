#include "../common/token.hpp"
#include "../parser/utils/Grammar/grammar.hpp"
#include "../parser/Parser_Generator/lalr_builder.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "../parser/AST_Builder/ast_builder.hpp"
#include "../parser/Parser_Generator/genparser.hpp"
#include "../lexer/lexer.hpp"  
#include "../semantic/analyzer.hpp"   

#include <string>
#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <fstream>
#include <sstream>

std::string grammar_dir = "../parser/Parser_Generator/grammar.y";
std::string program_dir = "program.hulk";


int main()
{
    try
    {
        std::cout << "\n=== FULL PIPELINE TEST (Lexer + Parser + AST) ===\n\n";

        // ------------------------------------------------------------
        // 1. Load grammar from file (grammar_small.y)
        // ------------------------------------------------------------
        Grammar grammar = build_grammar_from_file(grammar_dir);

        std::cout << "Grammar loaded\n";
        std::cout << "Symbols: " << grammar.symtab.size() << "\n";
        std::cout << "Productions: " << grammar.get_productions().size() << "\n\n";

        // ------------------------------------------------------------
        // 2. Build mapping from token name to symbol id
        // ------------------------------------------------------------
        std::unordered_map<std::string, uint32_t> name_to_id;
        for (size_t i = 0; i < grammar.symtab.size(); ++i) {
            name_to_id[grammar.symtab[i].name] = i;
        }

        // ------------------------------------------------------------
        // 3. Build parser from grammar
        // ------------------------------------------------------------
        uint32_t eof_id = name_to_id["$"];
        LALRParser parser = LALRParser::from_grammar(grammar, eof_id);
        std::cout << "Parser generated\n";
        std::cout << "States: " << parser.state_count() << "\n\n";

        // ------------------------------------------------------------
        // 4. Attach AST builder
        // ------------------------------------------------------------
        parser.set_builder(std::make_unique<ASTBuilder>());

        // ------------------------------------------------------------
        // 5. Lexer setup (using default token specifications)
        // ------------------------------------------------------------
        auto token_specs = default_token_specs();
        Lexer lexer(token_specs);

        // Input program (must match grammar.y)
        std::string input;
        std::ifstream file(program_dir);
        if (!file.is_open()) {
            std::cerr << "Error: Couldnt open test program\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        input = buffer.str();
        file.close();

        std::cout << "Input program:\n" << input << "\n";

        // ------------------------------------------------------------
        // 6. Tokenize input with lexer
        // ------------------------------------------------------------
        std::vector<Token> raw_tokens = lexer.tokenize(input);

        // ------------------------------------------------------------
        // 7. Map raw tokens to grammar symbol IDs
        // ------------------------------------------------------------
        std::vector<Token> tokens;
        for (auto& t : raw_tokens) {
            // t.symbol_id is the TokenType enum value; convert to name
            std::string token_name = token_type_to_string(static_cast<TokenType>(t.symbol_id));
            auto it = name_to_id.find(token_name);
            if (it == name_to_id.end()) {
                std::cerr << "Error: token name '" << token_name
                          << "' not found in grammar symbols\n";
                return 1;
            }
            uint32_t id = it->second;
            tokens.emplace_back(id, t.value, t.line, t.column);
        }

        // Add EOF token manually (since lexer may not generate it)
        tokens.emplace_back(name_to_id["$"], "$", 0, 0);

        // ------------------------------------------------------------
        // 8. Parse tokens
        // ------------------------------------------------------------
        std::cout << "Parsing...\n";

        std::cout << "Tokens from lexer:\n";
        for (auto& t : tokens) {
            std::cout << "  " << t.value << " -> sym=" << t.symbol_id << "\n";
        }
        ParseResult result = parser.parse(tokens);
        
        
        if (result.ast) {
            ProgramNode* program = dynamic_cast<ProgramNode*>(result.ast.get());
            if (!program) {
                std::cerr << "Error: Root node is not a ProgramNode\n";
                return 1;
            }
            std::cerr << "ProgramNode address: " << program << "\n";

            program->test();
            SemanticAnalyzer analyzer;
            analyzer.analyze(program);
            
            if (analyzer.hasErrors()) {
                
                std::cout << "✗ Semantic analysis failed.\n";
                return 1;
            } else {
                std::cout << "✓ Semantic analysis successful.\n";
                
                analyzer.getSymbolTable().dump();
            }

            
        } else {
            std::cout << "✗ No AST to analyze.\n";
            return 1;
        }
        
        // ------------------------------------------------------------
        // 9. Show result
        // ------------------------------------------------------------
        if (!result.is_success()) {
            std::cout << "✗ Parse failed\n";
            std::cout << result.error_message << "\n";
            return 1;
        }

        std::cout << "✓ Parse succeeded\n";

        std::cout << "Reductions:";
        for (auto r : result.reduction_sequence)
            std::cout << " P" << r;
        std::cout << "\n";

        // ------------------------------------------------------------
        // 10. Print AST
        // ------------------------------------------------------------
        if (result.ast) {
            std::cout << "✓ AST succesfully built\n";
            std::cout << "\nAST:\n";
            result.ast->print(std::cout);
        } else {
            std::cout << "✗ AST was not built\n";
        }

        std::cout << "\n";

        
        return 0;
    }
    catch (const std::exception &e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}//