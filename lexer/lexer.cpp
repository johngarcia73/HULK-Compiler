#include<stdio.h>
#include <set>
#include <map>
#include <string>
#include"./Lexer_Generator/dfa.hpp"
#include"lexer.hpp"
#include"./Lexer_Generator/preprocessor.hpp"
#include <stdexcept>
#include <iostream>
#include <regex>

Lexer::Lexer(const std::vector<TokenSpec>& specs) {

    int state_counter = 0;

    // Builds unified nfa
    NFA nfa = build_lexer_nfa(specs, state_counter);

    dfa = nfa_to_dfa(nfa);
    skip_table = build_skip_table_from_specs(specs);
    built = true;
}
std::vector<Token> Lexer::tokenize(const std::string& input) {
    if (!built)
        throw std::runtime_error("Lexer not built");

    std::vector<Token> tokens;
    size_t pos = 0;
    size_t length = input.size();
    size_t line = 1;
    size_t column = 1;

    while (pos < length) {
        int state = dfa.start;
        int last_accept_state = -1;
        size_t last_accept_pos = pos;
        size_t i = pos;
        size_t token_start_line = line;
        size_t token_start_column = column;

        while (i < length) {
            char c = input[i];
            if (!dfa.transitions.count(state) || !dfa.transitions.at(state).count(c))
                break;
            state = dfa.transitions.at(state).at(c);
            if (dfa.accept.count(state)) {
                last_accept_state = state;
                last_accept_pos = i + 1;
            }
            i++;
        }

        if (last_accept_state == -1) {
            char bad_char = input[pos];
            std::string error = "Lexical error at line " + std::to_string(line) +
                                ", column " + std::to_string(column) +
                                ": unexpected character '" + std::string(1, bad_char) + "'";
            throw std::runtime_error(error);
        }

        int token_id = dfa.accept_token.at(last_accept_state);
        std::string lexeme = input.substr(pos, last_accept_pos - pos);

        // Update line and coumn for the next token
        for (size_t j = pos; j < last_accept_pos; ++j) {
            if (input[j] == '\n') {
                line++;
                column = 1;
            } else {
                column++;
            }
        }

        // If token is ignored, we left it
        if (!skip_table[token_id]) {
            tokens.push_back(Token{
                static_cast<uint32_t>(token_id),
                lexeme,
                static_cast<int>(token_start_line),
                static_cast<int>(token_start_column),
                static_cast<int>(line),
                static_cast<int>(column)
            });
        }

        pos = last_accept_pos;
    }

    return tokens;
}
