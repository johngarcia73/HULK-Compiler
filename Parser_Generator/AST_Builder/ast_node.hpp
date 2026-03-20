#pragma once
#include <string>
#include <vector>
#include <memory>
#include <iostream>

struct ASTNode {
    virtual ~ASTNode() = default;
    virtual void print(std::ostream &o, int indent = 0) const = 0;
};

using ASTNodePtr = ASTNode*;

inline void indent(std::ostream &o, int n) {
    for (int i = 0; i < n; ++i) o.put(' ');
}

// Nodos básicos
struct NumberNode : ASTNode {
    long long value;
    NumberNode(long long v) : value(v) {}
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Number(" << value << ")\n";
    }
};

struct VariableNode : ASTNode {
    std::string name;
    VariableNode(std::string n): name(std::move(n)) {}
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Variable(" << name << ")\n";
    }
};

struct BinaryOpNode : ASTNode {
    char op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOpNode(char op_, ASTNodePtr l, ASTNodePtr r): op(op_), left(l), right(r) {}
    ~BinaryOpNode() { delete left; delete right; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "BinaryOp(" << op << ")\n";
        if (left) left->print(o, indent_n + 2);
        if (right) right->print(o, indent_n + 2);
    }
};

struct UnaryOpNode : ASTNode {
    char op;
    ASTNodePtr operand;
    UnaryOpNode(char op_, ASTNodePtr operand_): op(op_), operand(operand_) {}
    ~UnaryOpNode() { delete operand; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "UnaryOp(" << op << ")\n";
        if (operand) operand->print(o, indent_n + 2);
    }
};

struct ExprStmtNode : ASTNode {
    ASTNodePtr expr;
    ExprStmtNode(ASTNodePtr e): expr(e) {}
    ~ExprStmtNode() { delete expr; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "ExprStmt\n";
        if (expr) expr->print(o, indent_n + 2);
    }
};

struct BlockNode : ASTNode {
    std::vector<ASTNodePtr> stmts;
    BlockNode(std::vector<ASTNodePtr> s): stmts(std::move(s)) {}
    ~BlockNode() { for (auto p : stmts) delete p; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Block\n";
        for (auto s : stmts) s->print(o, indent_n + 2);
    }
};

struct ProgramNode : ASTNode {
    std::vector<ASTNodePtr> decls;
    std::vector<ASTNodePtr> stmts;
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
};

// Nodos específicos de la gramática reducida
struct ParamListNode : ASTNode {
    std::vector<std::string> params;
    ParamListNode(std::vector<std::string> p) : params(std::move(p)) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "ParamList(";
        for (size_t i = 0; i < params.size(); ++i) {
            if (i > 0) o << ", ";
            o << params[i];
        }
        o << ")\n";
    }
};

struct FunctionDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    ASTNodePtr body;
    FunctionDeclNode(std::string n, std::vector<std::string> p, ASTNodePtr b)
        : name(std::move(n)), params(std::move(p)), body(b) {}
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
};