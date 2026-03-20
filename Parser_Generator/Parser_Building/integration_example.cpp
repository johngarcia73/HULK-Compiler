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
        std::cout << "\n=== Hulk Small Parser + AST pipeline test ===\n\n";

        // ------------------------------------------------------------
        // 1. Load grammar
        // ------------------------------------------------------------

        Grammar grammar = build_grammar_from_file("grammar.y");

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

        // Tokens necesarios
        uint32_t FUNCTION     = sym.at("FUNCTION");
        uint32_t IDENTIFIER   = sym.at("IDENTIFIER");
        uint32_t L_PAREN      = sym.at("L_PAREN");
        uint32_t R_PAREN      = sym.at("R_PAREN");
        uint32_t L_CURL_BRACK = sym.at("L_CURL_BRACK");
        uint32_t R_CURL_BRACK = sym.at("R_CURL_BRACK");
        uint32_t SEMICOLON    = sym.at("SEMICOLON");
        uint32_t COMMA        = sym.at("COMMA");
        uint32_t LET          = sym.at("LET");
        uint32_t IN           = sym.at("IN");
        uint32_t IF           = sym.at("IF");      // no usado en este test pero necesario
        uint32_t ELSE         = sym.at("ELSE");
        uint32_t PLUS         = sym.at("PLUS");
        uint32_t MINUS        = sym.at("MINUS");
        uint32_t STAR         = sym.at("STAR");
        uint32_t SLASH        = sym.at("SLASH");
        uint32_t NUMBER       = sym.at("NUMBER");
        uint32_t EQUAL        = sym.at("EQUAL");
        uint32_t EOF_SYM      = sym.at("$");

        // ------------------------------------------------------------
        // 3. Build parser
        // ------------------------------------------------------------

        LALRParser parser = LALRParser::from_grammar(grammar, EOF_SYM);

        std::cout << "Parser generated\n";
        std::cout << "States: " << parser.state_count() << "\n\n";

        // ------------------------------------------------------------
        // 4. Attach AST builder
        // ------------------------------------------------------------

        parser.set_builder(std::make_unique<ASTBuilder>());

        // ------------------------------------------------------------
        // 5. Token stream para el programa:
        //    function add(x, y) { x + y; }
        //    let a = 5 in add(a, 3);
        // ------------------------------------------------------------

        std::vector<Token> tokens =
        {
            // function add(x, y) {
            Token(FUNCTION,     "function", 1, 1),
            Token(IDENTIFIER,   "add",      1, 10),
            Token(L_PAREN,      "(",        1, 13),
            Token(IDENTIFIER,   "x",        1, 14),
            Token(COMMA,        ",",        1, 15),
            Token(IDENTIFIER,   "y",        1, 17),
            Token(R_PAREN,      ")",        1, 18),
            Token(L_CURL_BRACK, "{",        1, 20),

            // x + y;
            Token(IDENTIFIER,   "x",        1, 22),
            Token(PLUS,         "+",        1, 24),
            Token(IDENTIFIER,   "y",        1, 26),
            Token(SEMICOLON,    ";",        1, 27),

            // }
            Token(R_CURL_BRACK, "}",        1, 28),

            // let a = 5 in add(a, 3);
            Token(LET,          "let",      2, 1),
            Token(IDENTIFIER,   "a",        2, 5),
            Token(EQUAL,        "=",        2, 7),
            Token(NUMBER,       "5",        2, 9),
            Token(IN,           "in",       2, 11),
            Token(IDENTIFIER,   "add",      2, 14),
            Token(L_PAREN,      "(",        2, 17),
            Token(IDENTIFIER,   "a",        2, 18),
            Token(COMMA,        ",",        2, 19),
            Token(NUMBER,       "3",        2, 21),
            Token(R_PAREN,      ")",        2, 22),
            Token(SEMICOLON,    ";",        2, 23),

            Token(EOF_SYM,      "$",        2, 24)
        };

        std::cout << "Parsing: function add(x, y) { x + y; } let a = 5 in add(a, 3);\n\n";

        ParseResult result = parser.parse(tokens);

        // ------------------------------------------------------------
        // 6. Resultado
        // ------------------------------------------------------------

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
            std::cout << "\nAST:\n";
            result.ast->print(std::cout);
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