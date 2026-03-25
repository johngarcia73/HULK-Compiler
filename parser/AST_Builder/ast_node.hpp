#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../../semantic/type.hpp"

class Visitor;  // forward declaration

struct ASTNode {
    Type* type = nullptr;   // inferred type
    virtual ~ASTNode() = default;
    virtual void print(std::ostream &o, int indent = 0) const = 0;
    virtual Type* accept(Visitor& v) = 0; 
};

using ASTNodePtr = ASTNode*;

inline void indent(std::ostream &o, int n) {
    for (int i = 0; i < n; ++i) o.put(' ');
}

// ============================================================================
// Basic nodes
// ============================================================================

struct NumberNode : ASTNode {
    long long value;
    NumberNode(long long v);
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct BoolNode : ASTNode {
    bool value;
    BoolNode(bool v);
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct StringNode : ASTNode {
    std::string value;
    StringNode(const std::string& v);
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct VariableNode : ASTNode {
    std::string name;
    VariableNode(std::string n);
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct BinaryOpNode : ASTNode {
    std::string op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOpNode(std::string op_, ASTNodePtr l, ASTNodePtr r);
    ~BinaryOpNode();
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct UnaryOpNode : ASTNode {
    std::string op;
    ASTNodePtr operand;
    UnaryOpNode(std::string op_, ASTNodePtr operand_);
    ~UnaryOpNode();
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ExprStmtNode : ASTNode {
    ASTNodePtr expr;
    ExprStmtNode(ASTNodePtr e);
    ~ExprStmtNode();
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct BlockNode : ASTNode {
    std::vector<ASTNodePtr> stmts;
    BlockNode(std::vector<ASTNodePtr> s);
    ~BlockNode();
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ProgramNode : ASTNode {
    std::vector<ASTNodePtr> decls;
    std::vector<ASTNodePtr> stmts;
    void test() const;  // Testing purpose

    ProgramNode(std::vector<ASTNodePtr> d, std::vector<ASTNodePtr> s);
    ~ProgramNode();
    void print(std::ostream &o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ParamListNode : ASTNode {
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    ParamListNode(std::vector<std::string> p, std::vector<Type*> pt);
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct FunctionDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    Type* returnType;
    ASTNodePtr body;
    FunctionDeclNode(std::string n, std::vector<std::string> p, std::vector<Type*> pt, Type* rt, ASTNodePtr b);
    ~FunctionDeclNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct LetNode : ASTNode {
    std::string name;
    ASTNodePtr init;
    ASTNodePtr body;
    LetNode(std::string n, ASTNodePtr i, ASTNodePtr b);
    ~LetNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct IfNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr then_branch;
    ASTNodePtr else_branch;
    IfNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e);
    ~IfNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct FunctionCallNode : ASTNode {
    std::string name;
    std::vector<ASTNodePtr> args;
    FunctionCallNode(std::string n, std::vector<ASTNodePtr> a);
    ~FunctionCallNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

// ============================================================================
// Aux nodes
// ============================================================================

struct LetBindingNode : ASTNode {
    std::string name;
    ASTNodePtr init;
    LetBindingNode(std::string n, ASTNodePtr i);
    ~LetBindingNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct LetBindingsNode : ASTNode {
    std::vector<LetBindingNode*> bindings;
    LetBindingsNode();
    LetBindingsNode(std::vector<LetBindingNode*> b);
    ~LetBindingsNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct TypeNode : ASTNode {
    Type* type;
    TypeNode(Type* t);
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};