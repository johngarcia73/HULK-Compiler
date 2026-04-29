#include "test_harness.hpp"

#include "../../parser/utils/Grammar/grammar.hpp"
#include "../../parser/utils/Symbol/symbol.hpp"
#include "../../parser/Parser_Generator/parser_interface.hpp"
#include "../../lexer/lexer.hpp"
#include "../../lexer/tokens.hpp"
#include "../../common/token.hpp"

void test_parser(TestRunner& r) {
    std::cout << "[test_parser] Parseando una produccion minima de asignacion\n";

    Grammar grammar;

    // Add placeholder terminals so their SymbolId == token id (T0..TN)
    int max_tok = static_cast<int>(TokenType::TOKEN_UNKNOWN);
    for (int i = 0; i <= max_tok; ++i) {
        grammar.symtab.add(SymbolKind::Terminal, std::string("T") + std::to_string(i));
    }

    // Non-terminal
    SymbolId S = grammar.symtab.add(SymbolKind::NonTerminal, "S");

    // Production: S -> LET IDENTIFIER EQUAL NUMBER SEMICOLON
    std::vector<SymbolId> rhs = {
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_LET)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_IDENTIFIER)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_EQUAL)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_NUMBER)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_SEMICOLON))
    };

    grammar.add_production(S, rhs);
    grammar.start_symbol = S;
    grammar.build_indices();

    uint32_t dollar = static_cast<uint32_t>(token_to_id(TokenType::TOKEN_EOF));
    LALRParser parser = LALRParser::from_grammar(grammar, dollar);

    // Tokenize sample input
    Lexer lex(default_token_specs());
    auto tokens = lex.tokenize("let x = 42;");
    tokens.emplace_back(dollar, "$", 0, 0);

    ParseResult res = parser.parse(tokens);
    EXPECT_TRUE(r, res.is_success());

    if (r.verbose && !res.is_success()) {
        std::cout << "Parser error: " << res.error_message << "\n";
    }
}
