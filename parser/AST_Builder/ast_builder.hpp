#pragma once
#include "ast_node.hpp"
#include <string>
#include <vector>
#include <optional>
#include "../../common/token.hpp"
#include "../utils/Grammar/grammar.hpp"

struct Value {
    ASTNode* node = nullptr;
    std::optional<std::string> token_text;
    SourceSpan span;

    static Value from_node(ASTNode* n) {
        Value v;
        v.node = n;
        if (n) {
            v.span = n->span;
        }
        return v;
    }
    static Value from_text(const std::string &s) {
        Value v;
        v.token_text = s;
        return v;
    }
    static Value from_token(const Token& token) {
        Value v;
        v.token_text = token.value;
        v.span = token.span();
        return v;
    }
};

struct ASTBuilder {
    const Grammar* grammar = nullptr;

    explicit ASTBuilder(const Grammar* g = nullptr) : grammar(g) {}
    ~ASTBuilder() = default;

    ASTNode* build(size_t production_id, const std::vector<Value>& rhs);
};
