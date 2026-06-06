#include "test_harness.hpp"

#include "../../lexer/lexer.hpp"
#include "../../lexer/tokens.hpp"
#include "../../common/token.hpp"

namespace {

void print_tokens(const std::vector<Token>& tokens) {
    for (const auto& token : tokens) {
        std::cout << "  token(" << token.symbol_id << ") = \"" << token.value
                  << "\" @ " << token.line << ":" << token.column << "\n";
    }
}

}  // namespace

void test_lexer(TestRunner& r) {
    std::cout << "[test_lexer] Tokenizando \"let x = 42;\"\n";
    Lexer lex(default_token_specs());
    auto tokens = lex.tokenize("let x = 42;");

    if (r.verbose) {
        print_tokens(tokens);
    }

    EXPECT_EQ(r, tokens.size(), (size_t)5);
    if (tokens.size() >= 5) {
        EXPECT_EQ(r, tokens[0].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_LET)));
        EXPECT_EQ(r, tokens[1].value, std::string("x"));
        EXPECT_EQ(r, tokens[2].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_EQUAL)));
        EXPECT_EQ(r, tokens[3].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_NUMBER)));
        EXPECT_EQ(r, tokens[4].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_SEMICOLON)));
    }

    auto member_tokens = lex.tokenize("self.x");
    EXPECT_EQ(r, member_tokens.size(), (size_t)3);
    if (member_tokens.size() >= 3) {
        EXPECT_EQ(r, member_tokens[0].value, std::string("self"));
        EXPECT_EQ(r, member_tokens[1].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_DOT)));
        EXPECT_EQ(r, member_tokens[2].value, std::string("x"));
    }

    auto protocol_tokens = lex.tokenize("protocol Named extends Hashable");
    EXPECT_EQ(r, protocol_tokens.size(), (size_t)4);
    if (protocol_tokens.size() >= 4) {
        EXPECT_EQ(r, protocol_tokens[0].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_PROTOCOL)));
        EXPECT_EQ(r, protocol_tokens[1].value, std::string("Named"));
        EXPECT_EQ(r, protocol_tokens[2].symbol_id, static_cast<uint32_t>(token_to_id(TokenType::TOKEN_EXTENDS)));
        EXPECT_EQ(r, protocol_tokens[3].value, std::string("Hashable"));
    }
}
