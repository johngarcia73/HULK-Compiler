#include "preprocessor.hpp"
#include"tokens.hpp"
#include <stack>
#include <map>
#include <cctype>
#include <sstream>
#include <iostream>
using namespace std;
#include <string>
#include <vector>
#include <cctype>

bool isLiteralChar(char ch) {
    return std::isalnum(static_cast<unsigned char>(ch)) || ch == '.';
}

bool isUnaryPostfix(char ch) {
    return ch == '*' || ch == '+' || ch == '?';
}

bool isIgnorable(char ch) {
    return ch == ' ' || ch == '\n' || ch == '\t';
}

std::string purgeIgnorable(const std::string& input) {
    std::string output;
    for(char ch: input){
        if(!isIgnorable(ch)){
            output.push_back(ch);
        }
    }
    return output;
}


std::vector<RegexToken> regex_to_RegexTokens(const std::string& regex) {
    std::vector<RegexToken> tokens;

    for (size_t i = 0; i < regex.size(); ++i) {
        char c = regex[i];

        if (std::isspace(static_cast<unsigned char>(c)))
            continue;

        if (c == '\\') {
            if (i + 1 >= regex.size())
                throw std::runtime_error("Incomplete escape in regex");

            tokens.push_back({RegexTokenType::Literal, regex[++i]});
            continue;
        }

        switch (c) {
            case '|': tokens.push_back({RegexTokenType::Union, 0}); break;
            case '*': tokens.push_back({RegexTokenType::Star, 0}); break;
            case '+': tokens.push_back({RegexTokenType::Plus, 0}); break;
            case '?': tokens.push_back({RegexTokenType::Optional, 0}); break;
            case '(': tokens.push_back({RegexTokenType::LParen, 0}); break;
            case ')': tokens.push_back({RegexTokenType::RParen, 0}); break;
            default:
                tokens.push_back({RegexTokenType::Literal, c});
        }
    }

    return tokens;
}

bool can_end(const RegexToken& t) {
    return t.type == RegexTokenType::Literal ||
           t.type == RegexTokenType::RParen ||
           t.type == RegexTokenType::Star ||
           t.type == RegexTokenType::Plus ||
           t.type == RegexTokenType::Optional;
}

bool can_start(const RegexToken& t) {
    return t.type == RegexTokenType::Literal ||
           t.type == RegexTokenType::LParen;
}

std::vector<RegexToken>
insert_concat(const std::vector<RegexToken>& in) {
    std::vector<RegexToken> out;

    for (size_t i = 0; i < in.size(); ++i) {
        if (i > 0 && can_end(in[i - 1]) && can_start(in[i])) {
            out.push_back({RegexTokenType::Concat, 0});
        }
        out.push_back(in[i]);
    }

    return out;
}

int precedence(RegexTokenType t) {
    switch (t) {
        case RegexTokenType::Star:
        case RegexTokenType::Plus:
        case RegexTokenType::Optional:
            return 3;
        case RegexTokenType::Concat:
            return 2;
        case RegexTokenType::Union:
            return 1;
        default:
            return 0;
    }
}

std::vector<RegexToken>
to_postfix(const std::vector<RegexToken>& infix) {
    std::vector<RegexToken> output;
    std::vector<RegexToken> stack;

    for (const auto& tok : infix) {
        switch (tok.type) {

            case RegexTokenType::Literal:
                output.push_back(tok);
                break;

            case RegexTokenType::LParen:
                stack.push_back(tok);
                break;

            case RegexTokenType::RParen:
                while (!stack.empty() &&
                       stack.back().type != RegexTokenType::LParen) {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                if (stack.empty())
                    throw std::runtime_error("Paréntesis desbalanceados");
                stack.pop_back(); // erases '('
                break;

            default: // operator
                while (!stack.empty() &&
                       precedence(stack.back().type) >= precedence(tok.type)) {
                    output.push_back(stack.back());
                    stack.pop_back();
                }
                stack.push_back(tok);
        }
    }

    while (!stack.empty()) {
        if (stack.back().type == RegexTokenType::LParen)
            throw std::runtime_error("Paréntesis desbalanceados");
        output.push_back(stack.back());
        stack.pop_back();
    }

    
    return output;
}

std::vector<RegexToken> regex_to_postfix(const std::string& regex) {
    auto tokens = regex_to_RegexTokens(regex);
    auto with_concat = insert_concat(tokens);
    return to_postfix(with_concat);
}









inline void specs_to_postfix_and_ids(
    const std::vector<TokenSpec>& specs,
    std::vector<std::string>& postfix_regexes,
    std::vector<int>& token_ids
) {
    postfix_regexes.clear();
    token_ids.clear();
    postfix_regexes.reserve(specs.size());
    token_ids.reserve(specs.size());

    for (const auto &spec : specs) {
        // regexToPostfix debe estar definida en preprocessor.hpp
        std::string pf = regexToPostfix(spec.regex_infix);
        // puedes validar pf vacío como indicio de error en tu tokenizer
        if (pf.empty()) {
            throw std::runtime_error("regexToPostfix returned empty postfix for pattern: " + spec.regex_infix);
        }
        postfix_regexes.push_back(std::move(pf));
        token_ids.push_back(token_to_id(spec.type));
    }
}

std::vector<bool> build_skip_table_from_specs(const std::vector<TokenSpec>& specs) {
    // max token id:
    int max_id = -1;
    for (auto &s: specs) max_id = std::max(max_id, token_to_id(s.type));
    std::vector<bool> skip_table(max_id + 1, false);
    for (auto &s: specs) skip_table[token_to_id(s.type)] = s.skip;
    return skip_table;
}
