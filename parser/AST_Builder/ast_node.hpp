#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../../semantic/type.hpp"
#include "visitor.hpp"


struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(std::ostream &o, int indent = 0) const = 0;
    virtual Type* accept(Visitor& v) = 0;   // método para el patrón Visitor
};

using ASTNodePtr = ASTNode*;

inline void indent(std::ostream &o, int n) {
    for (int i = 0; i < n; ++i) o.put(' ');
}

// ============================================================================
// Nodos básicos
// ============================================================================

struct NumberNode : ASTNode {
    long long value;
    NumberNode(long long v) : value(v) {}
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Number(" << value << ")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct BoolNode : ASTNode {
    bool value;
    BoolNode(bool v) : value(v) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Bool(" << (value ? "true" : "false") << ")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct StringNode : ASTNode {
    std::string value;
    StringNode(const std::string& v) : value(v) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "String(\"" << value << "\")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct VariableNode : ASTNode {
    std::string name;
    VariableNode(std::string n) : name(std::move(n)) {}
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Variable(" << name << ")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct BinaryOpNode : ASTNode {
    char op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOpNode(char op_, ASTNodePtr l, ASTNodePtr r) : op(op_), left(l), right(r) {}
    ~BinaryOpNode() { delete left; delete right; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "BinaryOp(" << op << ")\n";
        if (left) left->print(o, indent_n + 2);
        if (right) right->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct UnaryOpNode : ASTNode {
    char op;
    ASTNodePtr operand;
    UnaryOpNode(char op_, ASTNodePtr operand_) : op(op_), operand(operand_) {}
    ~UnaryOpNode() { delete operand; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "UnaryOp(" << op << ")\n";
        if (operand) operand->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct ExprStmtNode : ASTNode {
    ASTNodePtr expr;
    ExprStmtNode(ASTNodePtr e) : expr(e) {}
    ~ExprStmtNode() { delete expr; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "ExprStmt\n";
        if (expr) expr->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct BlockNode : ASTNode {
    std::vector<ASTNodePtr> stmts;
    BlockNode(std::vector<ASTNodePtr> s) : stmts(std::move(s)) {}
    ~BlockNode() { for (auto p : stmts) delete p; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Block\n";
        for (auto s : stmts) s->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct ProgramNode : ASTNode {
    std::vector<ASTNodePtr> decls;
    std::vector<ASTNodePtr> stmts;
    void test() { std::cerr << "ProgramNode::test called\n"; }  //Testing purpose


    ProgramNode(std::vector<ASTNodePtr> d, std::vector<ASTNodePtr> s)
        : decls(std::move(d)), stmts(std::move(s)) {}
    ~ProgramNode() { for (auto p : decls) delete p; for (auto p : stmts) delete p; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Program\n";
        if (!decls.empty()) {
            indent(o, indent_n + 2);
            o << "Declarations\n";
            for (auto d : decls) d->print(o, indent_n + 4);
        }
        if (!stmts.empty()) {
            indent(o, indent_n + 2);
            o << "Statements\n";
            for (auto s : stmts) s->print(o, indent_n + 4);
        }
    }
    Type* accept(Visitor& v) override { std::cerr << "ProgramNode::accept called\n";
    return v.visit(*this); }
};

// ============================================================================
// Nodos de la gramática reducida
// ============================================================================

struct ParamListNode : ASTNode {
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    ParamListNode(std::vector<std::string> p, std::vector<Type*> pt)
        : params(std::move(p)), paramTypes(std::move(pt)) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "ParamList(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) o << ", ";
            o << params[i] << " : " << paramTypes[i]->toString();
        }
        o << ")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct FunctionDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;   // nuevo
    Type* returnType;                // nuevo
    ASTNodePtr body;
    FunctionDeclNode(std::string n, std::vector<std::string> p, std::vector<Type*> pt, Type* rt, ASTNodePtr b)
        : name(std::move(n)), params(std::move(p)), paramTypes(std::move(pt)), returnType(rt), body(b) {}
    ~FunctionDeclNode() { delete body; }

    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "FunctionDecl(" << name << ")\n";
        indent(o, indent_n + 2);
        o << "Params:";
        for (auto& p : params) o << " " << p;
        o << "\n";
        if (body) body->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct LetNode : ASTNode {
    std::string name;
    ASTNodePtr init;
    ASTNodePtr body;
    LetNode(std::string n, ASTNodePtr i, ASTNodePtr b)
        : name(std::move(n)), init(i), body(b) {}
    ~LetNode() { delete init; delete body; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Let(" << name << ")\n";
        if (init) init->print(o, indent_n + 2);
        if (body) body->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct IfNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr then_branch;
    ASTNodePtr else_branch;
    IfNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e)
        : condition(c), then_branch(t), else_branch(e) {}
    ~IfNode() { delete condition; delete then_branch; delete else_branch; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "If\n";
        indent(o, indent_n + 2); o << "Condition:\n";
        if (condition) condition->print(o, indent_n + 4);
        indent(o, indent_n + 2); o << "Then:\n";
        if (then_branch) then_branch->print(o, indent_n + 4);
        indent(o, indent_n + 2); o << "Else:\n";
        if (else_branch) else_branch->print(o, indent_n + 4);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct FunctionCallNode : ASTNode {
    std::string name;
    std::vector<ASTNodePtr> args;
    FunctionCallNode(std::string n, std::vector<ASTNodePtr> a)
        : name(std::move(n)), args(std::move(a)) {}
    ~FunctionCallNode() { for (auto* a : args) delete a; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "FunctionCall(" << name << ")\n";
        for (auto* a : args) a->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

// ============================================================================
// Nodos auxiliares (para el let con múltiples bindings, si se usan)
// ============================================================================

struct LetBindingNode : ASTNode {
    std::string name;
    ASTNodePtr init;
    LetBindingNode(std::string n, ASTNodePtr i) : name(std::move(n)), init(i) {}
    ~LetBindingNode() { delete init; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "LetBinding(" << name << ")\n";
        if (init) init->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct LetBindingsNode : ASTNode {
    std::vector<LetBindingNode*> bindings;
    LetBindingsNode() = default;
    LetBindingsNode(std::vector<LetBindingNode*> b) : bindings(std::move(b)) {}
    ~LetBindingsNode() { for (auto* b : bindings) delete b; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "LetBindings\n";
        for (auto* b : bindings) b->print(o, indent_n + 2);
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};

struct TypeNode : ASTNode {
    Type* type;
    TypeNode(Type* t) : type(t) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Type(" << type->toString() << ")\n";
    }
    Type* accept(Visitor& v) override { return v.visit(*this); }
};