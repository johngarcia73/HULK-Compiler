#pragma once

#include <cstdint>
#include <vector>
#include <memory>
#include "../common/token.hpp"

class Visitor;

class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(Visitor& v) = 0;
};

class ExprNode : public ASTNode {
public:
};

class NumberNode : public ExprNode {
    int value;
public:
    NumberNode(int v) : value(v) {}
    int getValue() const { return value; }
    void accept(Visitor& v) override;
};

class BinaryOpNode : public ExprNode {
    char op;
    std::unique_ptr<ExprNode> left;
    std::unique_ptr<ExprNode> right;
public:
    BinaryOpNode(char o, ExprNode* l, ExprNode* r) : op(o), left(l), right(r) {}
    char getOp() const { return op; }
    ExprNode* getLeft() const { return left.get(); }
    ExprNode* getRight() const { return right.get(); }
    void accept(Visitor& v) override;
};