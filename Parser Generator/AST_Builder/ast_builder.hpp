#pragma once
#include <cstdint>
#include <vector>
#include "../../common/token.hpp"
#include "ast_node.hpp"

class ASTBuilder {
public:
    virtual ~ASTBuilder() = default;
    virtual void onShift(uint32_t symbol, const Token& token, ASTNode*& result) = 0;
    virtual void onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) = 0;
};


class ExprASTBuilder : public ASTBuilder {
public:
    void onShift(uint32_t symbol, const Token& token, ASTNode*& result) override;
    void onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) override;
};