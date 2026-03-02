#include "ast_node.hpp"
#include "ast_builder.hpp"
#include <cassert>

void ExprASTBuilder::onShift(uint32_t symbol, const Token& token, ASTNode*& result) {
    // Asumimos que el ID del terminal "number" es 0
    if (symbol == 0) {
        result = new NumberNode(std::stoi(token.value));
    } else {
        result = nullptr; // otros terminales (+, *) no generan nodo
    }
}

void ExprASTBuilder::onReduce(uint32_t prod_id, const std::vector<ASTNode*>& rhs, ASTNode*& result) {
    // 0: E → E + T
    // 1: E → T
    // 2: T → T * F
    // 3: T → F
    // 4: F → number
    // 5: S' → E

    switch (prod_id) {
        case 0: // E → E + T
            // rhs[0] = E, rhs[1] = '+' (nullptr), rhs[2] = T
            result = new BinaryOpNode('+', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2]));
            break;
        case 1: // E → T
        case 3: // T → F
        case 4: // F → number
        case 5: // S' → E
            result = rhs[0];
            break;
        case 2: // T → T * F
            result = new BinaryOpNode('*', static_cast<ExprNode*>(rhs[0]), static_cast<ExprNode*>(rhs[2]));
            break;
        default:
            result = nullptr;
            assert(false && "Producción desconocida");
    }
}