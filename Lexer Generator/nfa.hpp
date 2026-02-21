#ifndef NFA_HPP
#define NFA_HPP

#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <map>
#include "tokens.hpp" 
#include"preprocessor.hpp"

extern int next_state_id;
int new_state_id();

struct Fragment {
    int start;
    int accept;
};

enum class SymbolType {
    Character,
    Epsilon,
    Any
};

struct NFATransition {
    SymbolType type;
    char c;    // Valid only if type == Character
    int to;
};

struct NFA {
    int start = -1;
    std::unordered_set<int> accept;

    std::unordered_map<int, std::vector<NFATransition>> transitions;

    std::unordered_map<int, int> accept_token;
};

// Building up helpers
int new_state(int &state_counter);
void add_transition(NFA& nfa, int from, SymbolType type, char c, int to);
void add_epsilon(NFA& nfa, int from, int to);
void add_char(NFA& nfa, int from, char c, int to);
void add_any(NFA& nfa, int from, int to);

NFA regex_postfix_to_nfa(
    const std::vector<RegexToken>& postfix,
    int& id_counter,
    int token_id = -1
);

NFA unite_nfas(const std::vector<NFA>& nfas, int& state_counter);

NFA build_lexer_nfa_from_postfix(
    const std::vector<std::vector<RegexToken>>& postfix_regexes,
    const std::vector<int>& token_ids,
    int& state_counter
);

NFA build_lexer_nfa(
    const std::vector<TokenSpec>& specs,
    int& state_counter
);

#endif
