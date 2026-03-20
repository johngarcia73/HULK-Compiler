#ifndef DFA_HPP
#define DFA_HPP

#include <set>
#include <map>
#include <string>
#include "nfa.hpp"

using State = int;

struct StateSetHash {
    std::size_t operator()(const std::unordered_set<int>& s) const {
        std::size_t h = 0;
        for (int x : s) {
            h ^= std::hash<int>{}(x) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};


struct DFA {
    int start;
    std::unordered_set<int> accept;

    // state -> (char -> next_state)
    std::unordered_map<int, std::unordered_map<char, int>> transitions;

    std::unordered_map<int, int> accept_token; 
};

DFA nfa_to_dfa(const NFA& nfa);

#endif