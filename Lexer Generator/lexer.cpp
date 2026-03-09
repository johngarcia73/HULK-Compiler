#include<stdio.h>
#include <set>
#include <map>
#include <string>
#include"dfa.hpp"
#include"lexer.hpp"

Lexer::Lexer(const std::vector<TokenSpec>& specs) {

    int state_counter = 0;

    // Builds unified nfa
    NFA nfa = build_lexer_nfa(specs, state_counter);

    dfa = nfa_to_dfa(nfa);

    built = true;
}

std::vector<Token> Lexer::tokenize(const std::string& input) {

    if (!built)
        throw std::runtime_error("Lexer no construido");

    std::vector<Token> tokens;

    size_t pos = 0;
    size_t length = input.size();

    while (pos < length) {

        int state = dfa.start;
        int last_accept_state = -1;
        size_t last_accept_pos = pos;

        size_t i = pos;

        while (i < length) {

            char c = input[i];

            if (!dfa.transitions.count(state) ||
                !dfa.transitions.at(state).count(c))
                break;

            state = dfa.transitions.at(state).at(c);

            if (dfa.accept.count(state)) {
                last_accept_state = state;
                last_accept_pos = i + 1;
            }

            i++;
        }

        if (last_accept_state == -1) {
            throw std::runtime_error(
                "Error léxico en posición " + std::to_string(pos)
            );
        }

        int token_id = dfa.accept_token.at(last_accept_state);
        std::string lexeme = input.substr(pos, last_accept_pos - pos);

        tokens.push_back(Token{static_cast<uint32_t>(token_id), lexeme});

        pos = last_accept_pos;
    }

    return tokens;
}
