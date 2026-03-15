// lalr_algorithms.cpp  (parche mínimo)
#include "lalr_builder.hpp"
#include "../utils/First_Comp/first.hpp"
#include <algorithm>
#include <queue>
#include <sstream>
#include <unordered_set>
#include <cassert>
#include <stdexcept>

inline uint64_t pack_key(uint32_t state, uint32_t symbol) {
    return (uint64_t(state) << 32) | uint64_t(symbol);
}

inline std::string fingerprint_lr1(const LR1State& state) {
    std::vector<std::tuple<uint32_t, uint32_t, uint32_t>> triples;
    triples.reserve(128);
    
    for (auto& kv : state.core_la) {
        const ItemCore& core = kv.first;
        const SymbolSet& lookaheads = kv.second;
        for (uint32_t la : lookaheads.indices()) {
            triples.emplace_back(core.prod, core.dot, la);
        }
    }
    
    std::sort(triples.begin(), triples.end());
    
    std::ostringstream os;
    for (auto& t : triples) {
        os << std::get<0>(t) << ':' << std::get<1>(t) << ':' << std::get<2>(t) << ';';
    }
    return os.str();
}

// --- closure_lr1: ahora recibe la tabla de producciones que debe usar
LR1State closure_lr1(
    const LR1State& start,
    const Grammar& grammar,
    const FirstResult& first,
    const std::vector<Production>& productions)   // <- nuevo parámetro
{
    LR1State closure = start;
    std::queue<ItemCore> work;
    
    for (auto& kv : closure.core_la) {
        work.push(kv.first);
    }
    
    while (!work.empty()) {
        ItemCore core = work.front();
        work.pop();
        
        auto it_la = closure.core_la.find(core);
        assert(it_la != closure.core_la.end());
        SymbolSet lookaheads = it_la->second;
        
        if (core.prod >= productions.size()) {
            throw std::runtime_error("closure_lr1: core.prod out of range");
        }
        const Production& prod = productions[core.prod];
        
        if (core.dot >= prod.rhs.size()) continue;
        
        uint32_t next_sym = prod.rhs[core.dot];
        if (grammar.symtab[next_sym].kind != SymbolKind::NonTerminal) continue;
        
        // For each production of next_sym (these indices refer to original grammar productions)
        for (uint32_t prod_id : grammar.productions_of(next_sym)) {
            // prod_id must be valid relative to productions (we copied grammar.get_productions() earlier)
            if (prod_id >= productions.size()) {
                throw std::runtime_error("closure_lr1: prod_id out of range relative to local productions");
            }
            // Compute FIRST(beta a) and add la where needed
            SymbolSet beta_first(first.FIRST.size());
            
            // Compute FIRST(beta) once (beta = symbols after the nonterminal in prod)
            SymbolSet first_of_beta(first.FIRST.size());
            bool beta_all_nullable = true;
            for (size_t k = core.dot + 1; k < prod.rhs.size(); ++k) {
                uint32_t sym = prod.rhs[k];
                if (grammar.symtab[sym].kind == SymbolKind::Terminal || grammar.symtab[sym].kind == SymbolKind::End) {
                    first_of_beta.insert(sym);
                    beta_all_nullable = false;
                    break;
                }
                first_of_beta.unite(first.FIRST[sym]);
                if (!first.nullable[sym]) {
                    beta_all_nullable = false;
                    break;
                }
            }
            
            // For each lookahead a, compute FIRST(beta a)
            for (uint32_t la : lookaheads.indices()) {
                SymbolSet addset = first_of_beta;
                if (beta_all_nullable) addset.insert(la);
                // unite into beta_first (we can accumulate across la)
                beta_first.unite(addset);
            }
            
            ItemCore new_core{prod_id, 0};
            auto& la_ref = closure.core_la[new_core];
            bool changed = la_ref.unite(beta_first);
            
            if (changed) {
                work.push(new_core);
            }
        }
    }
    
    return closure;
}

// --- goto_lr1: ahora recibe la tabla de producciones que debe usar
LR1State goto_lr1(
    const LR1State& state,
    uint32_t symbol,
    const Grammar& grammar,
    const FirstResult& first,
    const std::vector<Production>& productions)   // <- nuevo parámetro
{
    LR1State next_state;
    next_state.core_la.reserve(32);
    
    for (auto& kv : state.core_la) {
        const ItemCore& core = kv.first;
        const SymbolSet& lookaheads = kv.second;
        
        if (core.prod >= productions.size()) {
            throw std::runtime_error("goto_lr1: core.prod out of range");
        }
        const Production& prod = productions[core.prod];
        
        if (core.dot < prod.rhs.size() && prod.rhs[core.dot] == symbol) {
            ItemCore advanced{core.prod, core.dot + 1};
            auto& la = next_state.core_la[advanced];
            la.unite(lookaheads);
        }
    }
    
    return closure_lr1(next_state, grammar, first, productions);
}

// --- build_canonical_lr1_impl: crea local_prods, añade aumentada, y pasa local_prods
CanonicalLR1 build_canonical_lr1_impl(
    const Grammar& grammar,
    const FirstResult& first,
    uint32_t dollar_symbol)
{
    CanonicalLR1 result;
    
    
    const auto& productions = grammar.get_productions();
    
    uint32_t start_prod = static_cast<uint32_t>(productions.size() - 1); // index of augmented prod
    
    LR1State initial;
    ItemCore core0{start_prod, 0};
    initial.core_la[core0].insert(dollar_symbol);
    
    initial = closure_lr1(initial, grammar, first, productions);
    
    std::unordered_map<std::string, uint32_t> fingerprint_to_id;
    std::queue<LR1State> work_queue;
    
    initial.id = 0;
    result.states.push_back(initial);
    fingerprint_to_id[fingerprint_lr1(initial)] = 0;
    work_queue.push(initial);
    
    while (!work_queue.empty()) {
        LR1State current = work_queue.front();
        work_queue.pop();
        
        uint32_t current_id = current.id;
        std::unordered_set<uint32_t> symbols_after_dot;
        
        for (auto& kv : current.core_la) {
            const ItemCore& core = kv.first;
            if (core.prod >= productions.size()) {
                throw std::runtime_error("build_canonical_lr1_impl: core.prod out of range during symbol collection");
            }
            const Production& prod = productions[core.prod];
            
            if (core.dot < prod.rhs.size()) {
                symbols_after_dot.insert(prod.rhs[core.dot]);
            }
        }
        
        for (uint32_t sym : symbols_after_dot) {
            LR1State next = goto_lr1(current, sym, grammar, first, productions);
            
            if (next.core_la.empty()) continue;
            
            std::string fp = fingerprint_lr1(next);
            auto it = fingerprint_to_id.find(fp);
            
            uint32_t next_id;
            if (it == fingerprint_to_id.end()) {
                next_id = static_cast<uint32_t>(result.states.size());
                next.id = next_id;
                result.states.push_back(next);
                fingerprint_to_id[fp] = next_id;
                work_queue.push(next);
            } else {
                next_id = it->second;
            }
            
            result.trans[pack_key(current_id, sym)] = next_id;
        }
    }
    
    return result;
}

// --- merge_to_lalr_impl unchanged (keeps original behaviour) ---
LALRAutomaton merge_to_lalr_impl(
    const CanonicalLR1& canonical,
    const Grammar& grammar)
{
    std::unordered_map<std::string, std::vector<uint32_t>> core_groups;
    
    // Group states by their cores (without lookaheads)
    for (uint32_t s = 0; s < canonical.states.size(); ++s) {
        std::vector<std::pair<uint32_t, uint32_t>> cores_vec;
        for (const auto& kv : canonical.states[s].core_la) {
            cores_vec.emplace_back(kv.first.prod, kv.first.dot);
        }
        std::sort(cores_vec.begin(), cores_vec.end());
        
        std::ostringstream os;
        for (auto& pr : cores_vec) {
            os << pr.first << ':' << pr.second << ';';
        }
        core_groups[os.str()].push_back(s);
    }
    
    // Find the group key that contains the 0 state
    std::string initial_key;
    for (const auto& grp : core_groups) {
        if (std::find(grp.second.begin(), grp.second.end(), 0) != grp.second.end()) {
            initial_key = grp.first;
            break;
        }
    }
    
    LALRAutomaton result;
    result.states.reserve(core_groups.size());
    
    std::vector<uint32_t> old_to_new(canonical.states.size(), (uint32_t)-1);
    
    // First, process the initial group
    if (!initial_key.empty()) {
        auto it = core_groups.find(initial_key);
        if (it != core_groups.end()) {
            LR1State merged;
            merged.id = static_cast<uint32_t>(result.states.size());
            
            for (uint32_t old_id : it->second) {
                for (const auto& kv : canonical.states[old_id].core_la) {
                    auto& la = merged.core_la[kv.first];
                    la.unite(kv.second);
                }
            }
            
            result.states.push_back(std::move(merged));
            uint32_t new_id = static_cast<uint32_t>(result.states.size() - 1);
            
            for (uint32_t old_id : it->second) {
                old_to_new[old_id] = new_id;
            }
            
            core_groups.erase(it);
        }
    }
    
    // Process remaining groups
    for (auto& grp : core_groups) {
        LR1State merged;
        merged.id = static_cast<uint32_t>(result.states.size());
        
        for (uint32_t old_id : grp.second) {
            for (const auto& kv : canonical.states[old_id].core_la) {
                auto& la = merged.core_la[kv.first];
                la.unite(kv.second);
            }
        }
        
        result.states.push_back(std::move(merged));
        uint32_t new_id = static_cast<uint32_t>(result.states.size() - 1);
        
        for (uint32_t old_id : grp.second) {
            old_to_new[old_id] = new_id;
        }
    }
    
    // Reassign transitions
    for (const auto& kv : canonical.trans) {
        uint64_t key = kv.first;
        uint32_t old_from = static_cast<uint32_t>(key >> 32);
        uint32_t sym = static_cast<uint32_t>(key & 0xffffffff);
        uint32_t old_to = kv.second;
        
        uint32_t new_from = old_to_new[old_from];
        uint32_t new_to = old_to_new[old_to];
        
        if (new_from != (uint32_t)-1 && new_to != (uint32_t)-1) {
            result.trans[pack_key(new_from, sym)] = new_to;
        }
    }
    
    return result;
}