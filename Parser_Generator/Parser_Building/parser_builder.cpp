#include<iostream>
#include "lalr_builder.hpp"
#include "../utils/First_Comp/first.hpp"
#include <sstream>
#include <cassert>

using namespace std;        
extern LR1State closure_lr1(
    const LR1State& start,
    const Grammar& grammar,
    const FirstResult& first);

extern CanonicalLR1 build_canonical_lr1_impl(
    const Grammar& grammar,
    const FirstResult& first,
    uint32_t dollar_symbol);

extern LALRAutomaton merge_to_lalr_impl(
    const CanonicalLR1& canonical,
    const Grammar& grammar);


CanonicalLR1 LALRBuilder::build_canonical_lr1(
    const Grammar& grammar,
    const FirstResult& first,
    uint32_t dollar_symbol)
{
    return build_canonical_lr1_impl(grammar, first, dollar_symbol);
}

LALRAutomaton LALRBuilder::merge_to_lalr(
    const CanonicalLR1& canonical,
    const Grammar& grammar)
{
    return merge_to_lalr_impl(canonical, grammar);
}

// Parse Table Generation
ParseTables LALRBuilder::build_tables(
    const LALRAutomaton& automaton,
    const Grammar& grammar,
    uint32_t dollar_symbol,
    std::vector<Conflict>& conflicts_out)
{
    ParseTables tables;
    size_t num_states = automaton.states.size();
    
    tables.ACTION.resize(num_states);
    tables.GOTO.resize(num_states);
    conflicts_out.clear();
    
    auto set_action = [&](uint32_t state, uint32_t symbol, const Action& action) {
        auto& action_map = tables.ACTION[state];
        auto it = action_map.find(symbol);
        
        if (it == action_map.end()) {
            action_map[symbol] = action;
        } else {
            Action existing = it->second;
            bool is_conflict = false;
            
            if (existing.kind == ActionKind::Shift && action.kind == ActionKind::Reduce) {
                is_conflict = true;
            } else if (existing.kind == ActionKind::Reduce && action.kind == ActionKind::Shift) {
                is_conflict = true;
            } else if (existing.kind == ActionKind::Reduce && action.kind == ActionKind::Reduce && 
                       existing.value != action.value) {
                is_conflict = true;
            }
            
            if (is_conflict) {
                Conflict c;
                c.state = state;
                c.symbol = symbol;
                
                std::ostringstream os;
                os << "Existing: ";
                switch (existing.kind) {
                    case ActionKind::Shift: os << "Shift " << existing.value; break;
                    case ActionKind::Reduce: os << "Reduce prod " << existing.value; break;
                    case ActionKind::Accept: os << "Accept"; break;
                    case ActionKind::Error: os << "Error"; break;
                }
                os << " ; New: ";
                switch (action.kind) {
                    case ActionKind::Shift: os << "Shift " << action.value; break;
                    case ActionKind::Reduce: os << "Reduce prod " << action.value; break;
                    case ActionKind::Accept: os << "Accept"; break;
                    case ActionKind::Error: os << "Error"; break;
                }
                
                c.detail = os.str();
                conflicts_out.push_back(std::move(c));
            }
        }
    };
    
    // Shift actions and GOTO entries from transitions
    for (const auto& kv : automaton.trans) {
        uint64_t key = kv.first;
        uint32_t from = (uint32_t)(key >> 32);
        uint32_t sym = (uint32_t)(key & 0xffffffff);
        uint32_t to = kv.second;
        
        if (grammar.symtab[sym].kind == SymbolKind::Terminal || 
            grammar.symtab[sym].kind == SymbolKind::End) {
            set_action(from, sym, Action{ActionKind::Shift, (int)to});
        } else {
            tables.GOTO[from][sym] = to;
        }
    }
    
    // Reduce and Accept actions
    const std::vector<Production>& productions = grammar.get_productions();
    
    for (uint32_t s = 0; s < num_states; ++s) {
        const LR1State& state = automaton.states[s];
        
        for (const auto& kv : state.core_la) {
            const ItemCore& core = kv.first;
            const SymbolSet& lookaheads = kv.second;
            const Production& prod = productions[core.prod];
            
            if (core.dot == prod.rhs.size()) {
                if (prod.lhs == grammar.start_symbol && lookaheads.contains(dollar_symbol)) {
                    set_action(s, dollar_symbol, Action{ActionKind::Accept, 0});
                } else {
                    for (uint32_t la : lookaheads.indices()) {
                        set_action(s, la, Action{ActionKind::Reduce, (int)core.prod});
                    }
                }
            }
        }
    }
    
    return tables;
}

// Main Build Function
ParseTables LALRBuilder::build(
    const Grammar& grammar,
    uint32_t dollar_symbol,
    std::vector<Conflict>& conflicts_out)
{
    FirstResult first = compute_nullable_first(grammar);
    CanonicalLR1 canonical = build_canonical_lr1_impl(grammar, first, dollar_symbol);
    
    std::cout << "=== Canonical LR(1) state 0 ===\n";
    for (const auto& kv : canonical.states[0].core_la) {
        std::cout << "  Prod " << kv.first.prod << " dot " << kv.first.dot << " lookaheads:";
        for (uint32_t la : kv.second.indices()) std::cout << " " << la;
        std::cout << "\n";
    }


    LALRAutomaton lalr = merge_to_lalr_impl(canonical, grammar);
    
    std::cout << "=== LALR state 0 ===\n";
    for (const auto& kv : lalr.states[0].core_la) {
        std::cout << "  Prod " << kv.first.prod << " dot " << kv.first.dot << " lookaheads:";
        for (uint32_t la : kv.second.indices()) std::cout << " " << la;
        std::cout << "\n";
    }
    
    ParseTables tables = build_tables(lalr, grammar, dollar_symbol, conflicts_out);
    
    return tables;
}
