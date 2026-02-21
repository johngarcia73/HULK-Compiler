#ifndef PREPROCESSOR_HPP
#define PREPROCESSOR_HPP

#include <string>
#include<vector>
#include"tokens.hpp"

bool isOperator(char c);
std::string purgeIgnorable(const std::string& regex);
std::string insertExplicitConcat(const std::string& regex);
std::string concatToPostfix(const std::string& regex);
std::string regexToPostfix(const std::string& regex);
std::vector<RegexToken> regex_to_RegexTokens(const std::string& regex);
std::vector<RegexToken> insert_concat(const std::vector<RegexToken>& in);
std::vector<RegexToken> to_postfix(const std::vector<RegexToken>& infix);
std::vector<RegexToken> regex_to_postfix(const std::string& regex);
inline void specs_to_postfix_and_ids(
    const std::vector<TokenSpec>& specs,
    std::vector<std::string>& postfix_regexes,
    std::vector<int>& token_ids
);

constexpr char LIT_PLUS  = '\x01';
constexpr char LIT_STAR  = '\x02';
constexpr char LIT_QMARK = '\x03';
constexpr char LIT_LPAR  = '\x04';
constexpr char LIT_RPAR  = '\x05';
constexpr char LIT_PIPE  = '\x06';
constexpr char LIT_HASH  = '\x07';

#endif