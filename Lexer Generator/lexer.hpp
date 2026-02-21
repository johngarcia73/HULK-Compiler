#include<stdio.h>
#include <set>
#include <map>
#include <string>
#include<vector>
#include"tokens.hpp"
#include"dfa.hpp"

struct Token {
    int token_id;
    std::string lexeme;
};

class Lexer {
private:
    DFA dfa;
    bool built = false;

public:
    Lexer(const std::vector<TokenSpec>& specs);

    std::vector<Token> tokenize(const std::string& input);
};