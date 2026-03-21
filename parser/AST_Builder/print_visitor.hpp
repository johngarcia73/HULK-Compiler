// PrintVisitor.hpp
#include "ast_node.hpp"
#include "visitor.hpp"
#include <iostream>

class PrintVisitor : public Visitor {
public:
    void visit(NumberNode& node) override {
        std::cout << node.getValue();
    }

    void visit(BinaryOpNode& node) override {
        std::cout << "(";
        node.getLeft()->accept(*this);
        std::cout << " " << node.getOp() << " ";
        node.getRight()->accept(*this);
        std::cout << ")";
    }
};