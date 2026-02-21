#include <queue>
#include <algorithm>
#include<stack>
#include"nfa.hpp"
#include "dfa.hpp"
using namespace std;

std::unordered_set<int> epsilon_closure(
    const NFA& nfa,
    const std::unordered_set<int>& states
) {
    std::unordered_set<int> closure = states;
    std::stack<int> st;

    for (int s : states) st.push(s);

    while (!st.empty()) {
        int s = st.top(); st.pop();

        auto it = nfa.transitions.find(s);
        if (it == nfa.transitions.end()) continue;

        for (const NFATransition& t : it->second) {
            if (t.type == SymbolType::Epsilon) {
                if (!closure.count(t.to)) {
                    closure.insert(t.to);
                    st.push(t.to);
                }
            }
        }
    }

    return closure;
}

std::unordered_set<int> move(
    const NFA& nfa,
    const std::unordered_set<int>& states,
    char c
) {
    std::unordered_set<int> result;

    for (int s : states) {
        auto it = nfa.transitions.find(s);
        if (it == nfa.transitions.end()) continue;

        for (const NFATransition& t : it->second) {
            if (t.type == SymbolType::Character && t.c == c)
                result.insert(t.to);

            if (t.type == SymbolType::Any)
                result.insert(t.to);
        }
    }

    return result;
}

DFA nfa_to_dfa(const NFA& nfa) {

    DFA dfa;
    int dfa_state_counter = 0;

    using StateSet = std::unordered_set<int>;

    std::unordered_map<StateSet, int, StateSetHash> subset_to_state;
    std::queue<StateSet> worklist;

    // Start State
    StateSet start_set = epsilon_closure(nfa, {nfa.start});
    subset_to_state[start_set] = dfa_state_counter++;
    dfa.start = 0;
    worklist.push(start_set);

    while (!worklist.empty()) {
        StateSet current_set = worklist.front();
        worklist.pop();

        int current_dfa_state = subset_to_state[current_set];

        // Detect if it's the final state
        for (int s : current_set) {
            if (nfa.accept.count(s)) {
                dfa.accept.insert(current_dfa_state);

                // Token resolution, favors the shortest id
                int token_id = nfa.accept_token.at(s);
                if (!dfa.accept_token.count(current_dfa_state) ||
                    token_id < dfa.accept_token[current_dfa_state])
                {
                    dfa.accept_token[current_dfa_state] = token_id;
                }
            }
        }

        // For each possible ASCII symbol
        for (int c = 0; c < 256; ++c) {

            StateSet moved = move(nfa, current_set, (char)c);
            if (moved.empty()) continue;

            StateSet next = epsilon_closure(nfa, moved);
            if (next.empty()) continue;

            if (!subset_to_state.count(next)) {
                subset_to_state[next] = dfa_state_counter++;
                worklist.push(next);
            }

            int next_dfa_state = subset_to_state[next];
            dfa.transitions[current_dfa_state][(char)c] = next_dfa_state;
        }
    }

    return dfa;
}

