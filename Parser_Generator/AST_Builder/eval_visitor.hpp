// EvalVisitor.hpp
#include "ast_node.hpp"
#include "visitor.hpp"
#include <stack>

class EvalVisitor : public Visitor {
    std::stack<int> evalStack;
public:
    int getResult() const { return evalStack.top(); }

    void visit(NumberNode& node) override {
        evalStack.push(node.getValue());
    }

    void visit(BinaryOpNode& node) override {
        node.getLeft()->accept(*this);
        node.getRight()->accept(*this);
        
        int right = evalStack.top(); evalStack.pop();
        int left  = evalStack.top(); evalStack.pop();
        
        switch (node.getOp()) {
            case '+': evalStack.push(left + right); break;
            case '*': evalStack.push(left * right); break;
            default: evalStack.push(0); break;
        }
    }
};