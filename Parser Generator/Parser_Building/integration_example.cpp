// ============================================================================
// test_pipeline_ast.cpp
//
// Test del pipeline completo:
//
// .y → Grammar → LALR → Parser → ASTBuilder → AST
// ============================================================================

#include "../../common/token.hpp"
#include "../utils/Grammar/grammar.hpp"
#include "lalr_builder.hpp"
#include "parser_interface.hpp"
#include "../AST_Builder/ast_node.hpp"
#include "../AST_Builder/ast_builder.hpp"
#include "genparser.hpp"

#include <iostream>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

int main()
{
    try
    {
        std::cout << "\n=== Parser + AST pipeline test ===\n\n";

        // ------------------------------------------------------------
        // 1. Load grammar
        // ------------------------------------------------------------

        Grammar grammar = build_grammar_from_file("grammar.y");

       
        std::cout << "Symbols:\n";
        for (size_t i = 0; i < grammar.symtab.size(); ++i) {
            std::cout << "  " << i << ": " << grammar.symtab[i].name 
                    << " (" << (grammar.symtab[i].kind == SymbolKind::Terminal ? "T" : "NT") << ")\n";
        }

        std::cout << "Grammar loaded\n";
        std::cout << "Symbols: " << grammar.symtab.size() << "\n";
        std::cout << "Productions: " << grammar.get_productions().size() << "\n\n";


        // ------------------------------------------------------------
        // 2. Build symbol lookup
        // ------------------------------------------------------------

        std::unordered_map<std::string, uint32_t> sym;

        for (size_t i = 0; i < grammar.symtab.size(); ++i)
        {
            sym[grammar.symtab[i].name] = i;
        }


        uint32_t NUMBER     = sym.at("NUMBER");
        uint32_t PLUS       = sym.at("PLUS");
        uint32_t MINUS       = sym.at("MINUS");
        uint32_t STAR       = sym.at("STAR");
        uint32_t SEMICOLON  = sym.at("SEMICOLON");
        uint32_t EOF_SYM    = sym.at("$");


        // ------------------------------------------------------------
        // 3. Build parser
        // ------------------------------------------------------------

        LALRParser parser =
            LALRParser::from_grammar(grammar, EOF_SYM);

        std::cout << "Parser generated\n";
        std::cout << "States: " << parser.state_count() << "\n\n";


        // ------------------------------------------------------------
        // 4. Attach AST builder
        // ------------------------------------------------------------

        parser.set_builder(std::make_unique<ASTBuilder>());


        // ------------------------------------------------------------
        // 5. Token stream
        // 2 + 3 * 4;
        // ------------------------------------------------------------

        std::vector<Token> tokens =
        {
            Token(NUMBER,"2",1,1),
            Token(PLUS,"+",1,2),
            Token(NUMBER,"3",1,3),
            Token(STAR,"*",1,4),
            Token(NUMBER,"4",1,5),
            Token(MINUS,"-",1,6),
            Token(NUMBER,"1",1,7),
            Token(SEMICOLON,";",1,7),
            Token(EOF_SYM,"$",1,8)
        };


        std::cout << "Parsing: 2 + 3 * 4 - 1;\n\n";

        ParseResult result = parser.parse(tokens);


        // ------------------------------------------------------------
        // 6. Result
        // ------------------------------------------------------------
        if (result.ast) {
            std::cout << "\nAST:\n";
            result.ast->print(std::cout);
        }
        
        if (!result.is_success())
        {
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
        // 7. AST validation
        // ------------------------------------------------------------

        if (result.ast)
        {
            std::cout << "✓ AST construido correctamente\n";
        }
        else
        {
            std::cout << "✗ No se construyó AST\n";
        }

        std::cout << "\n";

        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}
//x