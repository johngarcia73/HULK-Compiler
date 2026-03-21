#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>
#include <vector>
#include <stdexcept>

enum class TokenType : int {
    TOKEN_FUNCTION = 0,
    TOKEN_LET,
    TOKEN_IN,
    TOKEN_IF,
    TOKEN_ELSE,
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_LPAREN,
    TOKEN_RPAREN,
    TOKEN_L_CURL_BRACK,
    TOKEN_R_CURL_BRACK,
    TOKEN_SEMICOLON,
    TOKEN_COMMA,
    TOKEN_EQUAL,
    TOKEN_IDENTIFIER,
    TOKEN_WHITESPACE,
    TOKEN_EOF,
    TOKEN_UNKNOWN
};

enum class RegexTokenType {
    Literal,        // alphabet symbol
    Union,          // |
    Concat,         // .
    Star,           // *
    Plus,           // +
    Optional,       // ?
    LParen,         // (
    RParen          // )
};

struct RegexToken {
    RegexTokenType type;
    char value;
};

// Convert TokenType to string (for mapping to grammar symbols)
inline std::string token_type_to_string(TokenType t) {
    switch (t) {
        case TokenType::TOKEN_NUMBER:          return "NUMBER";
        case TokenType::TOKEN_PLUS:         return "PLUS";
        case TokenType::TOKEN_MINUS:        return "MINUS";
        case TokenType::TOKEN_STAR:         return "STAR";
        case TokenType::TOKEN_SLASH:        return "SLASH";
        case TokenType::TOKEN_LPAREN:       return "L_PAREN";
        case TokenType::TOKEN_RPAREN:       return "R_PAREN";
        case TokenType::TOKEN_FUNCTION:     return "FUNCTION";
        case TokenType::TOKEN_IDENTIFIER:   return "IDENTIFIER";
        case TokenType::TOKEN_L_CURL_BRACK: return "L_CURL_BRACK";
        case TokenType::TOKEN_R_CURL_BRACK: return "R_CURL_BRACK";
        case TokenType::TOKEN_SEMICOLON:    return "SEMICOLON";
        case TokenType::TOKEN_COMMA:        return "COMMA";
        case TokenType::TOKEN_LET:          return "LET";
        case TokenType::TOKEN_IN:           return "IN";
        case TokenType::TOKEN_IF:           return "IF";
        case TokenType::TOKEN_ELSE:         return "ELSE";
        case TokenType::TOKEN_EQUAL:        return "EQUAL";
        case TokenType::TOKEN_EOF:          return "$";
        default:                            return "UNKNOWN";
    }
}

// Token specification for lexer generator
struct TokenSpec {
    TokenType type;
    std::string regex_infix;
    bool skip;
    TokenSpec(TokenType t, const std::string& r, bool s = false)
        : type(t), regex_infix(r), skip(s) {}
};

// Default token specifications (used by lexer generator)
inline std::vector<TokenSpec> default_token_specs() {
    return {
        // Keywords
        { TokenType::TOKEN_FUNCTION, "function", false },
        { TokenType::TOKEN_LET,      "let",      false },
        { TokenType::TOKEN_IN,       "in",       false },
        { TokenType::TOKEN_IF,       "if",       false },
        { TokenType::TOKEN_ELSE,     "else",     false },

        // Numbers
        { TokenType::TOKEN_NUMBER,      "(0|1|2|3|4|5|6|7|8|9)+", false },
        // Operators
        { TokenType::TOKEN_PLUS,     "\\+", false },
        { TokenType::TOKEN_MINUS,    "-",   false },
        { TokenType::TOKEN_STAR,     "\\*", false },
        { TokenType::TOKEN_SLASH,    "/",   false },
        { TokenType::TOKEN_LPAREN,   "\\(", false },
        { TokenType::TOKEN_RPAREN,   "\\)", false },
        { TokenType::TOKEN_L_CURL_BRACK, "\\{", false },
        { TokenType::TOKEN_R_CURL_BRACK, "\\}", false },
        { TokenType::TOKEN_SEMICOLON, ";", false },
        { TokenType::TOKEN_COMMA,    ",", false },
        { TokenType::TOKEN_EQUAL,    "=",  false },
        // Identifiers
        { TokenType::TOKEN_IDENTIFIER,
          "(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z|A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z|_)(a|b|c|d|e|f|g|h|i|j|k|l|m|n|o|p|q|r|s|t|u|v|w|x|y|z|A|B|C|D|E|F|G|H|I|J|K|L|M|N|O|P|Q|R|S|T|U|V|W|X|Y|Z|0|1|2|3|4|5|6|7|8|9|_)*",
          false },
        // Whitespaces (skipped)
        { TokenType::TOKEN_WHITESPACE, "(\\ )+", true }
    };
}

// Helper to get integer from TokenType
inline int token_to_id(TokenType t) { return static_cast<int>(t); }

#endif