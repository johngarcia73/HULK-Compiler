#pragma once
#include <string>
#include <cstdint>

struct Token {
    uint32_t symbol_id;     // From lexer (matches grammar terminal ID)
    std::string value;      // Token value (e.g., "42", "identifier_name")
    int line;              
    int column;
    
    Token(uint32_t sym, const std::string& val, int l = 0, int c = 0)
        : symbol_id(sym), value(val), line(l), column(c) {}
};