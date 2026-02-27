#pragma once
#include <vector>
#include <cstdint>
#include "grammar.hpp"
#include "symbol.hpp"

class SymbolSet {
public:
    SymbolSet() = default;

    explicit SymbolSet(size_t nbits) {
        reset(nbits);
    }

    void reset(size_t nbits) {
        words.assign((nbits + 63) / 64, 0ULL);
    }

    bool test(size_t idx) const {
        return words[idx / 64] & (1ULL << (idx % 64));
    }

    void set(size_t idx) {
        words[idx / 64] |= (1ULL << (idx % 64));
    }

    bool union_with(const SymbolSet& other) {
        bool changed = false;
        for (size_t i = 0; i < words.size(); ++i) {
            uint64_t before = words[i];
            words[i] |= other.words[i];
            if (words[i] != before)
                changed = true;
        }
        return changed;
    }

private:
    std::vector<uint64_t> words;
};


struct FirstResult {
    std::vector<bool> nullable;
    std::vector<SymbolSet> FIRST;
};

FirstResult compute_nullable_first(const Grammar& G);