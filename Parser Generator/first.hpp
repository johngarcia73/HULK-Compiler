#pragma once
#include <vector>
#include <cstdint>
#include "grammar.hpp"
#include "symbol.hpp"




#pragma once
#include <vector>
#include <cstdint>
#include <algorithm>

class SymbolSet {
public:
    using SymbolID = uint32_t;

private:
    std::vector<uint64_t> bits;   // dynamic bitset
    size_t universeSize;          // total of possible symbols

public:
    SymbolSet() : universeSize(0) {}

    explicit SymbolSet(size_t universe)
        : universeSize(universe),
          bits((universe + 63) / 64, 0ULL) {}

    void reset() {
        std::fill(bits.begin(), bits.end(), 0ULL);
    }

    bool insert(SymbolID id) {
        size_t block = id / 64;
        // Grow the bitset if necessary
        if (block >= bits.size()) {
            bits.resize(block + 1, 0ULL);
        }
        uint64_t mask = 1ULL << (id % 64);

        if (!(bits[block] & mask)) {
            bits[block] |= mask;
            return true;   
        }
        return false;      
    }

    bool contains(SymbolID id) const {
        size_t block = id / 64;
        if (block >= bits.size()) return false;
        uint64_t mask = 1ULL << (id % 64);
        return bits[block] & mask;
    }

    bool unite(const SymbolSet& other) {
        bool changed = false;
        // Make sure this set can accommodate all symbols in the other set
        if (bits.size() < other.bits.size()) {
            bits.resize(other.bits.size(), 0ULL);
        }
        for (size_t i = 0; i < bits.size(); ++i) {
            uint64_t before = bits[i];
            uint64_t other_bits = (i < other.bits.size()) ? other.bits[i] : 0ULL;
            bits[i] |= other_bits;
            if (bits[i] != before)
                changed = true;
        }
        return changed;
    }

    bool empty() const {
        for (auto b : bits)
            if (b) return false;
        return true;
    }

    std::vector<SymbolID> indices() const {
        std::vector<SymbolID> result;
        for (size_t block = 0; block < bits.size(); ++block) {
            uint64_t b = bits[block];
            while (b) {
                uint64_t t = b & -b;                  // lowest set bit
                uint32_t r = __builtin_ctzll(b);      // bit index
                result.push_back(block * 64 + r);
                b ^= t;
            }
        }
        return result;
    }

    bool operator==(const SymbolSet& other) const {
        return bits == other.bits;
    }

    bool operator!=(const SymbolSet& other) const {
        return !(*this == other);
    }

    const std::vector<uint64_t>& raw() const {
        return bits;
    }
};


struct FirstResult {
    std::vector<bool> nullable;
    std::vector<SymbolSet> FIRST;
};

FirstResult compute_nullable_first(const Grammar& G);