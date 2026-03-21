#include <stdio.h>
#include <string>
#include <stdexcept>
#include <iostream>
#include <stack>
#include <algorithm>
#include <cctype>
#include "../tokens.hpp"
#include"preprocessor.hpp"
#include "nfa.hpp"

// optional global counter
int next_state_id = 0;
int new_state_id() { return next_state_id++; }

// ------------------------ helpers --------------------------------

int new_state(int &state_counter) {
    return state_counter++;
}

void add_transition(NFA& nfa, int from, SymbolType type, char c, int to) {
    nfa.transitions[from].push_back(NFATransition{type, c, to});
}

void add_epsilon(NFA& nfa, int from, int to) {
    add_transition(nfa, from, SymbolType::Epsilon, 0, to);
}

void add_char(NFA& nfa, int from, char c, int to) {
    add_transition(nfa, from, SymbolType::Character, c, to);
}

void add_any(NFA& nfa, int from, int to) {
    add_transition(nfa, from, SymbolType::Any, 0, to);
}

//--------------------Thompson Algorithm: postfix (vector<RegexToken>) -> NFA--------------------

NFA regex_postfix_to_nfa(const std::vector<RegexToken>& postfix, int& id_counter, int token_id) {
    std::stack<Fragment> st;
    NFA nfa_local;

    for (size_t i = 0; i < postfix.size(); ++i) {
        const RegexToken& tok = postfix[i];

        switch (tok.type) {
            case RegexTokenType::Literal: {
                int s = new_state(id_counter);
                int t = new_state(id_counter);
                add_char(nfa_local, s, tok.value, t);
                st.push(Fragment{s, t});
                break;
            }

            case RegexTokenType::Concat: {
                if (st.size() < 2) throw std::runtime_error("Concat requiere 2 operandos");
                Fragment B = st.top(); st.pop();
                Fragment A = st.top(); st.pop();
                // connect A.accept -> B.start via epsilon
                add_epsilon(nfa_local, A.accept, B.start);
                st.push(Fragment{A.start, B.accept});
                break;
            }

            case RegexTokenType::Union: {
                if (st.size() < 2) throw std::runtime_error("Union requiere 2 operandos");
                Fragment B = st.top(); st.pop();
                Fragment A = st.top(); st.pop();
                int s = new_state(id_counter);
                int t = new_state(id_counter);
                add_epsilon(nfa_local, s, A.start);
                add_epsilon(nfa_local, s, B.start);
                add_epsilon(nfa_local, A.accept, t);
                add_epsilon(nfa_local, B.accept, t);
                st.push(Fragment{s, t});
                break;
            }

            case RegexTokenType::Star: {
                if (st.empty()) throw std::runtime_error("Star requiere 1 operando");
                Fragment A = st.top(); st.pop();
                int s = new_state(id_counter);
                int t = new_state(id_counter);
                add_epsilon(nfa_local, s, A.start);
                add_epsilon(nfa_local, s, t);
                add_epsilon(nfa_local, A.accept, A.start);
                add_epsilon(nfa_local, A.accept, t);
                st.push(Fragment{s, t});
                break;
            }

            case RegexTokenType::Plus: {
                if (st.empty()) throw std::runtime_error("Plus requiere 1 operando");
                Fragment A = st.top(); st.pop();
                int s = new_state(id_counter);
                int t = new_state(id_counter);
                add_epsilon(nfa_local, s, A.start);
                add_epsilon(nfa_local, A.accept, A.start);
                add_epsilon(nfa_local, A.accept, t);
                st.push(Fragment{s, t});
                break;
            }

            case RegexTokenType::Optional: {
                if (st.empty()) throw std::runtime_error("Optional requiere 1 operando");
                Fragment A = st.top(); st.pop();
                int s = new_state(id_counter);
                int t = new_state(id_counter);
                add_epsilon(nfa_local, s, A.start);
                add_epsilon(nfa_local, s, t);
                add_epsilon(nfa_local, A.accept, t);
                st.push(Fragment{s, t});
                break;
            }

            default:
                throw std::runtime_error("Invalid token in postfix notation");
        }
    }

    if (st.size() != 1) throw std::runtime_error("Postfix regex bad built (stack != 1)");

    Fragment result = st.top();
    nfa_local.start = result.start;
    nfa_local.accept.insert(result.accept);

    if (token_id >= 0) {
        nfa_local.accept_token[result.accept] = token_id;
    }

    return nfa_local;
}

// Copy nfa with offset for states
static NFA copy_with_offset(const NFA& nfa, int offset) {
    NFA out;
    out.start = nfa.start + offset;

    for (int acc : nfa.accept) out.accept.insert(acc + offset);

    for (const auto& kv : nfa.transitions) {
        int from = kv.first + offset;
        for (const NFATransition& t : kv.second) {
            NFATransition nt;
            nt.type = t.type;
            nt.c = t.c;
            nt.to = t.to + offset;
            out.transitions[from].push_back(nt);
        }
    }

    for (const auto& kv : nfa.accept_token) {
        out.accept_token[kv.first + offset] = kv.second;
    }

    return out;
}

// Unifies the NFAs
NFA unite_nfas(const std::vector<NFA>& nfas, int& state_counter) {
    NFA out;
    // New start state
    int new_start = new_state(state_counter);
    out.start = new_start;

    for (const NFA& n : nfas) {
        int offset = new_state(state_counter);
        NFA cpy = copy_with_offset(n, offset);

        // ε from new_start -> cpy.start
        add_epsilon(out, new_start, cpy.start);

        // incorportes transitions and final states
        for (const auto& kv : cpy.transitions) {
            int from = kv.first;
            const auto& vec = kv.second;
            auto &dst = out.transitions[from];
            dst.insert(dst.end(), vec.begin(), vec.end());
        }

        for (int acc : cpy.accept) {
            out.accept.insert(acc);
            auto it = cpy.accept_token.find(acc);
            if (it != cpy.accept_token.end()) out.accept_token[acc] = it->second;
        }
    }

    return out;
}



NFA build_lexer_nfa_from_postfix(
    const std::vector<std::vector<RegexToken>>& postfix_regexes,
    const std::vector<int>& token_ids,
    int& state_counter
) {
    if (postfix_regexes.size() != token_ids.size()) throw std::runtime_error("postfix/token_ids desalineados");

    std::vector<NFA> nfas;
    nfas.reserve(postfix_regexes.size());

    for (size_t i = 0; i < postfix_regexes.size(); ++i) {
        const auto& pf = postfix_regexes[i];
        int tokid = token_ids[i];
        NFA n = regex_postfix_to_nfa(pf, state_counter, tokid);
        nfas.push_back(std::move(n));
    }

    return unite_nfas(nfas, state_counter);
}

// Entire pipeline: specs (infix strings) -> unified NFA
NFA build_lexer_nfa(
    const std::vector<TokenSpec>& specs,
    int& state_counter
) {
    std::vector<std::vector<RegexToken>> postfixes;
    std::vector<int> token_ids;
    postfixes.reserve(specs.size());
    token_ids.reserve(specs.size());

    for (const TokenSpec& s : specs) {
        auto tokens = regex_to_RegexTokens(s.regex_infix);

        auto with_concat = insert_concat(tokens);

        auto postfix = to_postfix(with_concat);

        // debug printing
        // std::cout << "Postfix tokens for " << s.regex_infix << " : ";
        // for (auto &t : postfix) std::cout << t.value << " ";
        // std::cout << std::endl;

        postfixes.push_back(std::move(postfix));
        token_ids.push_back(token_to_id(s.type));
    }

    return build_lexer_nfa_from_postfix(postfixes, token_ids, state_counter);
}
