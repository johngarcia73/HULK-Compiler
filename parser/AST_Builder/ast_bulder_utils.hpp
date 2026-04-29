#include <cassert>
#include <iostream>
#include <algorithm>
#include "../../common/source_span.hpp"

// Helper macros for safe access to the RHS vector
#define RHS(i) (rhs[i].node)
#define TOKEN(i) (rhs[i].token_text.value_or(""))
#define INT_VAL(i) (rhs[i].int_value.value_or(0))

inline SourceSpan node_span(ASTNode* node) {
    return node ? node->span : SourceSpan{};
}

inline SourceSpan value_span(const Value& value) {
    if (value.node && value.node->span.isValid()) {
        return value.node->span;
    }
    return value.span;
}

template <typename NodeT>
NodeT* attach_span(NodeT* node, const SourceSpan& span) {
    if (node) {
        node->span = span;
    }
    return node;
}

// Common construction patterns
// Pass the first child as the result (e.g., expr -> relational)
#define PASS() return RHS(0)

// Empty production -> return an empty BlockNode
#define EMPTY_BLOCK() return attach_span(new BlockNode({}), {})

// Standard binary operator: children at indices 0 and 2, operator given
#define BIN_OP(op) return attach_span(new BinaryOpNode(op, RHS(0), RHS(2)), SourceSpan::merge(node_span(RHS(0)), node_span(RHS(2))))

// Binary operator with custom indices
#define BIN_OP_IDX(op, l, r) return attach_span(new BinaryOpNode(op, RHS(l), RHS(r)), SourceSpan::merge(node_span(RHS(l)), node_span(RHS(r))))

// Unary operator: operand at index 1
#define UNARY_OP(op) return attach_span(new UnaryOpNode(op, RHS(1)), node_span(RHS(1)))

// Build a BlockNode from two AST nodes, flattening if they are already BlockNodes
#define BUILD_BLOCK(left, right) \
    do { \
        std::vector<ASTNode*> items; \
        SourceSpan combined_span = SourceSpan::merge(node_span(left), node_span(right)); \
        if (auto* lb = dynamic_cast<BlockNode*>(left)) { \
            items = std::move(lb->stmts); \
            delete lb; \
        } else if (left) items.push_back(left); \
        if (auto* rb = dynamic_cast<BlockNode*>(right)) { \
            for (auto* s : rb->stmts) items.push_back(s); \
            rb->stmts.clear(); \
            delete rb; \
        } else if (right) items.push_back(right); \
        return attach_span(new BlockNode(std::move(items)), combined_span); \
    } while(0)

// Build an argument list (as a BlockNode) from a node that may be a BlockNode or a single expr
#define BUILD_ARG_LIST(argNode) \
    do { \
        std::vector<ASTNode*> args; \
        if (auto* argBlock = dynamic_cast<BlockNode*>(argNode)) { \
            args = std::move(argBlock->stmts); \
            delete argBlock; \
        } else if (argNode) args.push_back(argNode); \
        return attach_span(new BlockNode(std::move(args)), node_span(argNode)); \
    } while(0)
