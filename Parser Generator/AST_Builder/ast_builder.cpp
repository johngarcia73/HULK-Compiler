#include "ast_node.hpp"
#include "ast_builder.hpp"
#include <cassert>

void ExprASTBuilder::onShift(uint32_t symbol, const Token& token, ASTNode*& result) {
    // Terminal 'number' with ID 0
    if (symbol == 0) {
        result = new NumberNode(std::stoi(token.value));
    } else {
        result = nullptr; // other terminals does not generate nodes
    }
}

void ExprASTBuilder::onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) {
    // Productions:
    // 0: E → E + T
    // 1: E → T
    // 2: T → T * F
    // 3: T → F
    // 4: F → number
    // 5: S' → E

    switch (prod_id) {
        case 0: // E → E + T
            { result = new BinaryOpNode('+', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2])); }
            break;
        case 1: // E → T
            { result = rhs[0]; }
            break;
        case 2: // T → T * F
            { result = new BinaryOpNode('*', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2])); }
            break;
        case 3: // T → F
            { result = rhs[0]; }
            break;
        case 4: // F → number
            { result = rhs[0]; }
            break;
        case 5: // S' → E
            { result = rhs[0]; }
            break;
        default:
            result = nullptr;
            assert(false && "Unknown production");
    }
}
