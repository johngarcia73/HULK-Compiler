#pragma once
#include <string>
#include <cstdint>
#include "source_span.hpp"

struct Token {
    uint32_t symbol_id;     // From lexer (matches grammar terminal ID)
    std::string value;      // Token value (e.g., "42", "identifier_name")
    int line;
    int column;
    int end_line;
    int end_column;

    Token(
        uint32_t sym,
        const std::string& val,
        int l = 0,
        int c = 0,
        int el = 0,
        int ec = 0)
        : symbol_id(sym),
          value(val),
          line(l),
          column(c),
          end_line(el ? el : l),
          end_column(ec ? ec : c) {}

    SourceSpan span() const {
        return {{line, column}, {end_line, end_column}};
    }
};
