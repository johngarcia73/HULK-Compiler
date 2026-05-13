#ifndef TOKENS_HPP
#define TOKENS_HPP

#include <string>
#include <vector>
#include <stdexcept>

enum class TokenType : int {
    TOKEN_FUNCTION = 0,
    TOKEN_TYPE,
    TOKEN_PROTOCOL,
    TOKEN_INHERITS,
    TOKEN_EXTENDS,
    TOKEN_NEW,
    TOKEN_NUMBER_TYPE, 
    TOKEN_BOOL_TYPE,  
    TOKEN_STRING_TYPE,
    TOKEN_WHILE,
    TOKEN_FOR,
    TOKEN_LET,
    TOKEN_IN,
    TOKEN_IF,
    TOKEN_ELIF,
    TOKEN_ELSE,
    TOKEN_RETURN,
    TOKEN_TRUE,
    TOKEN_FALSE,
    TOKEN_IS,
    TOKEN_AS,
    
    TOKEN_NUMBER,
    TOKEN_STRING,
    TOKEN_ARROW,
    TOKEN_MODULE,
    TOKEN_NOT,
    TOKEN_AND,
    TOKEN_OR,
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
    TOKEN_EQUALITY,
    TOKEN_NOT_EQUAL,
    TOKEN_EQUAL,
    TOKEN_LESS_THAN,
    TOKEN_GREATER_THAN,
    TOKEN_LESS_EQUALS,
    TOKEN_GREATER_EQUALS,
    TOKEN_CONCAT,
    TOKEN_IDENTIFIER,
    TOKEN_WHITESPACE,
    TOKEN_NEWLINE,
    TOKEN_COLON,
    TOKEN_DOT,
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
        case TokenType::TOKEN_NUMBER_TYPE:  return "NUMBER_TYPE";
        case TokenType::TOKEN_BOOL_TYPE:    return "BOOL_TYPE";
        case TokenType::TOKEN_STRING_TYPE:  return "STRING_TYPE";
        case TokenType::TOKEN_TYPE:         return "TYPE";
        case TokenType::TOKEN_PROTOCOL:     return "PROTOCOL";
        case TokenType::TOKEN_INHERITS:     return "INHERITS";
        case TokenType::TOKEN_EXTENDS:      return "EXTENDS";
        case TokenType::TOKEN_NEW:          return "NEW";
        case TokenType::TOKEN_NUMBER:       return "NUMBER";
        case TokenType::TOKEN_STRING:       return "STRING";
        case TokenType::TOKEN_MODULE:       return "MODULE";
        case TokenType::TOKEN_PLUS:         return "PLUS";
        case TokenType::TOKEN_MINUS:        return "MINUS";
        case TokenType::TOKEN_STAR:         return "STAR";
        case TokenType::TOKEN_SLASH:        return "SLASH";
        case TokenType::TOKEN_CONCAT:        return "CONCAT";
        case TokenType::TOKEN_LPAREN:       return "L_PAREN";
        case TokenType::TOKEN_RPAREN:       return "R_PAREN";
        case TokenType::TOKEN_FUNCTION:     return "FUNCTION";
        
        
        case TokenType::TOKEN_IDENTIFIER:   return "IDENTIFIER";
        case TokenType::TOKEN_L_CURL_BRACK: return "L_CURL_BRACK";
        case TokenType::TOKEN_R_CURL_BRACK: return "R_CURL_BRACK";
        case TokenType::TOKEN_SEMICOLON:    return "SEMICOLON";
        case TokenType::TOKEN_COMMA:        return "COMMA";
        case TokenType::TOKEN_WHILE:        return "WHILE";
        case TokenType::TOKEN_FOR:          return "FOR";
        case TokenType::TOKEN_LET:          return "LET";
        case TokenType::TOKEN_IN:           return "IN";
        case TokenType::TOKEN_IF:           return "IF";
        case TokenType::TOKEN_ELIF:         return "ELIF";
        case TokenType::TOKEN_ELSE:         return "ELSE";
        case TokenType::TOKEN_RETURN:       return "RETURN";
        case TokenType::TOKEN_TRUE:         return "TRUE";
        case TokenType::TOKEN_FALSE:        return "FALSE";
        case TokenType::TOKEN_IS:           return "IS";
        case TokenType::TOKEN_AS:           return "AS";
        case TokenType::TOKEN_EQUALITY:     return "EQUALITY";
        case TokenType::TOKEN_NOT_EQUAL:    return "NOT_EQUAL";
        case TokenType::TOKEN_EQUAL:        return "EQUAL";
        case TokenType::TOKEN_ARROW:        return "ARROW";
        case TokenType::TOKEN_NOT:          return "NOT";
        case TokenType::TOKEN_AND:          return "AND";
        case TokenType::TOKEN_OR:           return "OR";
        case TokenType::TOKEN_LESS_THAN:    return "LESS_THAN";
        case TokenType::TOKEN_GREATER_THAN: return "GREATER_THAN";
        case TokenType::TOKEN_LESS_EQUALS: return "LESS_EQUALS";
        case TokenType::TOKEN_GREATER_EQUALS: return "GREATER_EQUALS";
        case TokenType::TOKEN_COLON:        return "COLON";
        case TokenType::TOKEN_DOT:          return "DOT";
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
inline std::vector<TokenSpec> default_token_specs() {
    return {
        // Keywords
        { TokenType::TOKEN_FUNCTION, "function", false },
        { TokenType::TOKEN_TYPE, "type", false },
        { TokenType::TOKEN_PROTOCOL, "protocol", false },
        { TokenType::TOKEN_INHERITS, "inherits", false },
        { TokenType::TOKEN_EXTENDS, "extends", false },
        { TokenType::TOKEN_NEW, "new", false },
        { TokenType::TOKEN_NUMBER_TYPE, "Number", false },
        { TokenType::TOKEN_BOOL_TYPE,   "Bool",   false },
        { TokenType::TOKEN_STRING_TYPE, "String", false },
        { TokenType::TOKEN_WHILE,      "while", false },
        { TokenType::TOKEN_FOR,      "for", false },
        { TokenType::TOKEN_LET,      "let",      false },
        { TokenType::TOKEN_IN,       "in",       false },
        { TokenType::TOKEN_IF,       "if",       false },
        { TokenType::TOKEN_ELIF,     "elif",     false },
        { TokenType::TOKEN_ELSE,     "else",     false },
        { TokenType::TOKEN_RETURN,   "return",   false },
        { TokenType::TOKEN_TRUE,     "true",     false },
        { TokenType::TOKEN_FALSE,    "false",    false },
        { TokenType::TOKEN_IS,       "is",       false },
        { TokenType::TOKEN_AS,       "as",       false },

        { TokenType::TOKEN_STRING,   R"("([^"\\]|\\.)*")", false },

        { TokenType::TOKEN_NUMBER,   "[0-9]+|[0-9]+\\.[0-9]+", false },

        // Operators and symbols
        { TokenType::TOKEN_NOT_EQUAL, "!=", false },
        { TokenType::TOKEN_LESS_EQUALS, "<=", false },
        { TokenType::TOKEN_GREATER_EQUALS, ">=", false },
        { TokenType::TOKEN_EQUALITY, "==", false },
        { TokenType::TOKEN_MODULE,     "%", false },
        { TokenType::TOKEN_NOT,      "!", false },
        { TokenType::TOKEN_AND,      "&", false },
        { TokenType::TOKEN_OR,       "\\|", false },
        { TokenType::TOKEN_PLUS,     "\\+", false },
        { TokenType::TOKEN_MINUS,    "-",   false },
        { TokenType::TOKEN_STAR,     "\\*", false },
        { TokenType::TOKEN_SLASH,    "/",   false },
        { TokenType::TOKEN_CONCAT,   "@",   false },
        { TokenType::TOKEN_LESS_THAN,    "<",   false },
        { TokenType::TOKEN_GREATER_THAN, ">",   false },
        { TokenType::TOKEN_EQUAL,    "=",  false },
        { TokenType::TOKEN_ARROW,    "=>", false },
        { TokenType::TOKEN_LPAREN,   "\\(", false },
        { TokenType::TOKEN_RPAREN,   "\\)", false },
        { TokenType::TOKEN_L_CURL_BRACK, "\\{", false },
        { TokenType::TOKEN_R_CURL_BRACK, "\\}", false },
        { TokenType::TOKEN_SEMICOLON, ";", false },
        { TokenType::TOKEN_COMMA,    ",", false },
        { TokenType::TOKEN_COLON,    ":", false },
        { TokenType::TOKEN_DOT,      ".", false },

        // Identifiers
        { TokenType::TOKEN_IDENTIFIER, "[a-zA-Z_][a-zA-Z0-9_]*", false },
        { TokenType::TOKEN_WHITESPACE, "[ \t\n\r]+", true },
    };
}

// Helper to get integer from TokenType
inline int token_to_id(TokenType t) { return static_cast<int>(t); }

#endif
