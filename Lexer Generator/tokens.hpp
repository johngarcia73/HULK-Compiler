// tokens.hpp
#ifndef TOKENS_HPP
#define TOKENS_HPP
#include <string>
#include <vector>
#include <stdexcept>

enum class TokenType : int {
    TOKEN_INT = 0,     // literales enteros
    TOKEN_PLUS,        // '+'
    TOKEN_MINUS,       // '-'
    TOKEN_STAR,        // '*'
    TOKEN_SLASH,       // '/'
    TOKEN_LPAREN,      // '('
    TOKEN_RPAREN,      // ')'
    //TOKEN_WHITESPACE,  // espacios (usualmente 'skip' = true)
    TOKEN_UNKNOWN      // fallback (si quieres)
};


enum class RegexTokenType {
    Literal,        // símbolo del alfabeto
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


inline int token_to_id(TokenType t) { return static_cast<int>(t); }

struct TokenSpec {
    TokenType type;
    std::string regex_infix;
    bool skip; // true => recognized token but it is ignored (e.g. whitespaces)
    TokenSpec(TokenType t, const std::string& r, bool s = false)
        : type(t), regex_infix(r), skip(s) {}
};



inline std::vector<TokenSpec> default_token_specs() {
    return {
        { TokenType::TOKEN_INT,       "(0|1|2|3|4|5|6|7|8|9)+",    false }, // entero 
        { TokenType::TOKEN_PLUS,      "\\+",       false }, // '+'
        { TokenType::TOKEN_MINUS,     "-",         false }, // '-'
        { TokenType::TOKEN_STAR,      "\\*",       false }, // '*'
        { TokenType::TOKEN_SLASH,     "/",         false }, // '/'
        { TokenType::TOKEN_LPAREN,    "\\(",       false }, // '('
        { TokenType::TOKEN_RPAREN,    "\\)",       false }, // ')'
        //{ TokenType::TOKEN_WHITESPACE,"(\\t|\\r|\\n)+", true } // whitespace (skip)
    };
}


#endif
