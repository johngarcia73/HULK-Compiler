#include "preprocessor.hpp"
#include"../tokens.hpp"
#include <stack>
#include <map>
#include <cctype>
#include <sstream>
#include <iostream>
using namespace std;
#include <string>
#include <vector>
#include <cctype>
#include <unordered_set>

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

static std::vector<RegexToken> parse_char_class(const std::string& regex, size_t& pos) {
    if (regex[pos] != '[') throw std::runtime_error("Expected '['");
    pos++;
    bool negated = false;
    if (pos < regex.size() && regex[pos] == '^') {
        negated = true;
        pos++;
    }

    std::vector<char> chars;
    while (pos < regex.size() && regex[pos] != ']') {
        char c = regex[pos];
        if (c == '\\') {
            pos++;
            if (pos >= regex.size()) throw std::runtime_error("Incomplete escape in class");
            char esc = regex[pos];
            switch (esc) {
                case 'n': chars.push_back('\n'); break;
                case 'r': chars.push_back('\r'); break;
                case 't': chars.push_back('\t'); break;
                case '\\': chars.push_back('\\'); break;
                case ']': chars.push_back(']'); break;
                default: chars.push_back(esc); break;
            }
            pos++;
        } else if (c == '-' && pos > 0 && regex[pos-1] != '[' && pos+1 < regex.size() && regex[pos+1] != ']') {
            char start = chars.back();
            chars.pop_back();
            char end = regex[pos+1];
            if (start > end) throw std::runtime_error("Invalid range");
            for (char ch = start; ch <= end; ++ch) chars.push_back(ch);
            pos += 2;
        } else {
            chars.push_back(c);
            pos++;
        }
    }
    if (pos >= regex.size() || regex[pos] != ']')
        throw std::runtime_error("Unclosed character class");
    pos++;

    std::vector<char> final_chars;
    if (negated) {
        std::unordered_set<char> exclude(chars.begin(), chars.end());
        for (int ch = 0; ch < 256; ++ch) {
            if (exclude.find((char)ch) == exclude.end())
                final_chars.push_back((char)ch);
        }
    } else {
        final_chars = std::move(chars);
    }

    if (final_chars.empty()) {
        throw std::runtime_error("Empty character class");
    }

    std::vector<RegexToken> result;
    result.push_back({RegexTokenType::LParen, 0});
    for (size_t i = 0; i < final_chars.size(); ++i) {
        if (i > 0) result.push_back({RegexTokenType::Union, 0});
        result.push_back({RegexTokenType::Literal, final_chars[i]});
    }
    result.push_back({RegexTokenType::RParen, 0});
    return result;
}

std::vector<RegexToken> regex_to_RegexTokens(const std::string& regex) {
    std::vector<RegexToken> tokens;
    size_t i = 0;
    while (i < regex.size()) {
        char c = regex[i];
        if (std::isspace(static_cast<unsigned char>(c))) { i++; continue; }

        if (c == '\\') {
            if (i+1 >= regex.size()) throw std::runtime_error("Incomplete escape");
            char esc = regex[++i];
            switch (esc) {
                case 'n': tokens.push_back({RegexTokenType::Literal, '\n'}); break;
                case 'r': tokens.push_back({RegexTokenType::Literal, '\r'}); break;
                case 't': tokens.push_back({RegexTokenType::Literal, '\t'}); break;
                default: tokens.push_back({RegexTokenType::Literal, esc}); break;
            }
            i++;
            continue;
        }

        if (c == '[') {
            auto class_tokens = parse_char_class(regex, i);
            tokens.insert(tokens.end(), class_tokens.begin(), class_tokens.end());
            continue;
        }

        switch (c) {
            case '|': tokens.push_back({RegexTokenType::Union, 0}); break;
            case '*': tokens.push_back({RegexTokenType::Star, 0}); break;
            case '+': tokens.push_back({RegexTokenType::Plus, 0}); break;
            case '?': tokens.push_back({RegexTokenType::Optional, 0}); break;
            case '(': tokens.push_back({RegexTokenType::LParen, 0}); break;
            case ')': tokens.push_back({RegexTokenType::RParen, 0}); break;
            default:  tokens.push_back({RegexTokenType::Literal, c}); break;
        }
        i++;
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
