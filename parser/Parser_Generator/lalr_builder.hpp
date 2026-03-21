#pragma once

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <queue>
#include "../utils/Grammar/grammar.hpp"
#include "../utils/First_Comp/first.hpp"    
#include "../utils/Symbol/symbol.hpp"

// LALR Item Core (without lookahead)
struct ItemCore {
    uint32_t prod;  // Production ID
    uint32_t dot;   // Position of dot (0 .. rhs.size())
    
    bool operator==(const ItemCore& o) const noexcept { 
        return prod == o.prod && dot == o.dot; 
    }
};

struct ItemCoreHash {
    size_t operator()(const ItemCore& c) const noexcept {
        return (size_t(c.prod) * 1000003) ^ (size_t(c.dot) << 16);
    }
};

// LR(1) State (core + lookahead sets)
struct LR1State {
    uint32_t id;
    std::unordered_map<ItemCore, SymbolSet, ItemCoreHash> core_la;
    
    std::vector<ItemCore> cores() const {
        std::vector<ItemCore> v;
        v.reserve(core_la.size());
        for (auto &kv : core_la) v.push_back(kv.first);
        return v;
    }
};

// Canonical LR(1) Collection
struct CanonicalLR1 {
    std::vector<LR1State> states;
    std::unordered_map<uint64_t, uint32_t> trans;  // (state, symbol) -> state
};

// LALR Automaton (merged states)
struct LALRAutomaton {
    std::vector<LR1State> states;
    std::unordered_map<uint64_t, uint32_t> trans;
};

// Parsing Actions
enum class ActionKind {
    Shift,   // Push state and shift
    Reduce,  // Apply production
    Accept,  // Accept (end of parse)
    Error    // Syntax error
};

struct Action {
    ActionKind kind;
    int value;  // Shift: next state | Reduce: production ID | Accept/Error: unused
};

// Parse Tables (ACTION and GOTO)
struct ParseTables {
    std::vector<std::unordered_map<uint32_t, Action>> ACTION;  // [state][terminal]
    std::vector<std::unordered_map<uint32_t, uint32_t>> GOTO;  // [state][nonterminal]
};

// Conflict Information
struct Conflict {
    uint32_t state;
    uint32_t symbol;
    std::string detail;
};

// LALR Builder Class - Main Interface
class LALRBuilder {
public:
    // Build complete LALR(1) automaton and tables
    static ParseTables build(
        const Grammar& grammar,
        uint32_t dollar_symbol,
        std::vector<Conflict>& conflicts_out
    );

    static CanonicalLR1 build_canonical_lr1(
        const Grammar& grammar,
        const FirstResult& first,
        uint32_t dollar_symbol
    );

    static LALRAutomaton merge_to_lalr(
        const CanonicalLR1& canonical,
        const Grammar& grammar
    );

    static ParseTables build_tables(
        const LALRAutomaton& automaton,
        const Grammar& grammar,
        uint32_t dollar_symbol,
        std::vector<Conflict>& conflicts_out
    );
};
