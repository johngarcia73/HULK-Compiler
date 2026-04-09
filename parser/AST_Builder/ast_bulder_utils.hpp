#include <cassert>
#include <iostream>
#include <algorithm>

// Helper macros for safe access to the RHS vector
#define RHS(i) (rhs[i].node)
#define TOKEN(i) (rhs[i].token_text.value_or(""))
#define INT_VAL(i) (rhs[i].int_value.value_or(0))

// Common construction patterns
// Pass the first child as the result (e.g., expr -> relational)
#define PASS() return RHS(0)

// Empty production -> return an empty BlockNode
#define EMPTY_BLOCK() return new BlockNode({})

// Standard binary operator: children at indices 0 and 2, operator given
#define BIN_OP(op) return new BinaryOpNode(op, RHS(0), RHS(2))

// Binary operator with custom indices
#define BIN_OP_IDX(op, l, r) return new BinaryOpNode(op, RHS(l), RHS(r))

// Unary operator: operand at index 1
#define UNARY_OP(op) return new UnaryOpNode(op, RHS(1))

// Build a BlockNode from two AST nodes, flattening if they are already BlockNodes
#define BUILD_BLOCK(left, right) \
    do { \
        std::vector<ASTNode*> items; \
        if (auto* lb = dynamic_cast<BlockNode*>(left)) { \
            items = std::move(lb->stmts); \
            delete lb; \
        } else if (left) items.push_back(left); \
        if (auto* rb = dynamic_cast<BlockNode*>(right)) { \
            for (auto* s : rb->stmts) items.push_back(s); \
            rb->stmts.clear(); \
            delete rb; \
        } else if (right) items.push_back(right); \
        return new BlockNode(std::move(items)); \
    } while(0)

// Build an argument list (as a BlockNode) from a node that may be a BlockNode or a single expr
#define BUILD_ARG_LIST(argNode) \
    do { \
        std::vector<ASTNode*> args; \
        if (auto* argBlock = dynamic_cast<BlockNode*>(argNode)) { \
            args = std::move(argBlock->stmts); \
            delete argBlock; \
        } else if (argNode) args.push_back(argNode); \
        return new BlockNode(std::move(args)); \
    } while(0)
