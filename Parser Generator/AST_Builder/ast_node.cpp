#include "visitor.hpp"
#include "ast_node.hpp"

void NumberNode::accept(Visitor& v) {
    v.visit(*this);
}

void BinaryOpNode::accept(Visitor& v) {
    v.visit(*this);
}