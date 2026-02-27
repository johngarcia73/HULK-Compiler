#include <iostream>
#include <vector>
#include <stack>
#include <stdexcept>

enum class Token {
    ID,
    PLUS,
    END
};

struct Production {
    int lhs;                 // no terminal
    int rhs_len;             // |β|
};

// No terminales
constexpr int S = 0;
constexpr int E = 1;

// Producciones:
// (0) S → E
// (1) E → E + id
// (2) E → id
const std::vector<Production> productions = {
    {S, 1},
    {E, 3},
    {E, 1}
};