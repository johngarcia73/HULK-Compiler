#pragma once

class NumberNode;
class BinaryOpNode;

class Visitor {
public:
    virtual ~Visitor() = default;
    virtual void visit(NumberNode& node) = 0;
    virtual void visit(BinaryOpNode& node) = 0;
};