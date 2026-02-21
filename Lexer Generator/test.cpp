#include<stdio.h>
#include<iostream>
#include<string>
#include<vector>
#include <assert.h>
#include"tokens.hpp"
#include"preprocessor.hpp"
#include"nfa.hpp"
#include"lexer.hpp"

int main(int argc, char const *argv[])
{
    const std::string regex = "(0|1|2|3|4|5|6|7|8|9)+";

    //std::vector<RegexToken> p = regex_to_postfix(regex);
    //std::string postfix = regexToPostfix(regex);
    //int state_counter = 0;
    //NFA nfa = build_lexer_nfa(default_token_specs(), state_counter);

    //int new_state = 0;
    //NFA nfa = build_lexer_nfa(default_token_specs(), new_state);

    Lexer lexer(default_token_specs());
    std::vector<Token> tokens = lexer.tokenize("9+2");
    for(Token t: tokens){
        std::cout<<"Token with lexeme: "<< t.lexeme<< " and ID: "<< t.token_id<<" ";
    }
    return 0;
}
