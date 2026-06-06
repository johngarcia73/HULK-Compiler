#pragma once

#include<stdio.h>
#include <set>
#include <map>
#include <string>
#include<vector>
#include "../common/token.hpp"
#include"tokens.hpp"
#include"./Lexer_Generator/dfa.hpp"

class Lexer {
private:
    DFA dfa;
    bool built = false;
    std::vector<bool> skip_table;
public:
    Lexer(const std::vector<TokenSpec>& specs);
    Lexer(DFA cached_dfa, std::vector<bool> cached_skip_table);

    const DFA& get_dfa() const;
    const std::vector<bool>& get_skip_table() const;

    std::vector<Token> tokenize(const std::string& input);
};
