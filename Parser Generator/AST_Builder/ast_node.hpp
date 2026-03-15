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

// --- Nodos existentes (se mantienen) ---
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

struct VarDeclNode : ASTNode {
    std::string name;
    ASTNodePtr init;
    VarDeclNode(std::string n, ASTNodePtr i): name(std::move(n)), init(i) {}
    ~VarDeclNode() { delete init; }
    void print(std::ostream &o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "VarDecl(" << name << ")\n";
        if (init) init->print(o, indent_n + 2);
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

// --- Nuevos nodos para Hulk ---

// Lista de parámetros formales (para funciones)
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

// Binding individual de let
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
};

// Lista de bindings de let
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
};

// Declaración de función
struct FunctionDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    ASTNodePtr body;  // puede ser BlockNode o cualquier expresión
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

// Asignación (variable := expr)
struct AssignmentNode : ASTNode {
    std::string name;
    ASTNodePtr right;
    AssignmentNode(std::string n, ASTNodePtr r) : name(std::move(n)), right(r) {}
    ~AssignmentNode() { delete right; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Assignment(" << name << ")\n";
        if (right) right->print(o, indent_n + 2);
    }
};

// Operador binario con string (para operadores de varios caracteres)
struct BinaryOpStrNode : ASTNode {
    std::string op;
    ASTNodePtr left;
    ASTNodePtr right;
    BinaryOpStrNode(std::string o, ASTNodePtr l, ASTNodePtr r) : op(std::move(o)), left(l), right(r) {}
    ~BinaryOpStrNode() { delete left; delete right; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "BinaryOp(" << op << ")\n";
        if (left) left->print(o, indent_n + 2);
        if (right) right->print(o, indent_n + 2);
    }
};

// Llamada a función
struct FunctionCallNode : ASTNode {
    std::string name;
    std::vector<ASTNodePtr> args;
    FunctionCallNode(std::string n, std::vector<ASTNodePtr> a) : name(std::move(n)), args(std::move(a)) {}
    ~FunctionCallNode() { for (auto* a : args) delete a; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "FunctionCall(" << name << ")\n";
        for (auto* a : args) a->print(o, indent_n + 2);
    }
};

// Expresión let
struct LetNode : ASTNode {
    std::vector<std::pair<std::string, ASTNodePtr>> bindings;
    ASTNodePtr body;
    LetNode(std::vector<std::pair<std::string, ASTNodePtr>> b, ASTNodePtr body_)
        : bindings(std::move(b)), body(body_) {}
    ~LetNode() {
        for (auto& p : bindings) delete p.second;
        delete body;
    }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Let\n";
        for (auto& b : bindings) {
            indent(o, indent_n + 2);
            o << "Binding(" << b.first << ")\n";
            if (b.second) b.second->print(o, indent_n + 4);
        }
        if (body) body->print(o, indent_n + 2);
    }
};

// Nodo para una cláusula elif (condición y cuerpo)
struct ElifNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;
    ElifNode(ASTNodePtr c, ASTNodePtr b) : condition(c), body(b) {}
    ~ElifNode() { delete condition; delete body; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Elif\n";
        indent(o, indent_n + 2); o << "Condition:\n";
        if (condition) condition->print(o, indent_n + 4);
        indent(o, indent_n + 2); o << "Body:\n";
        if (body) body->print(o, indent_n + 4);
    }
};

// Lista de cláusulas elif
struct ElifListNode : ASTNode {
    std::vector<ElifNode*> elifs;
    ElifListNode() = default;
    ElifListNode(std::vector<ElifNode*> e) : elifs(std::move(e)) {}
    ~ElifListNode() { for (auto* e : elifs) delete e; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "ElifList (" << elifs.size() << ")\n";
        for (auto* e : elifs) e->print(o, indent_n + 2);
    }
};

// Expresión if (con elifs y else opcional)
struct IfNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr then_branch;
    std::vector<ElifNode*> elifs;  // almacenamos los nodos directamente
    ASTNodePtr else_branch;
    IfNode(ASTNodePtr cond, ASTNodePtr then,
           std::vector<ElifNode*> e, ASTNodePtr else_)
        : condition(cond), then_branch(then), elifs(std::move(e)), else_branch(else_) {}
    ~IfNode() {
        delete condition;
        delete then_branch;
        for (auto* e : elifs) delete e;
        delete else_branch;
    }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "If\n";
        indent(o, indent_n + 2); o << "Condition:\n";
        if (condition) condition->print(o, indent_n + 4);
        indent(o, indent_n + 2); o << "Then:\n";
        if (then_branch) then_branch->print(o, indent_n + 4);
        for (size_t i = 0; i < elifs.size(); ++i) {
            indent(o, indent_n + 2); o << "Elif " << i << ":\n";
            if (elifs[i]) elifs[i]->print(o, indent_n + 4);
        }
        if (else_branch) {
            indent(o, indent_n + 2); o << "Else:\n";
            else_branch->print(o, indent_n + 4);
        }
    }
};

// While
struct WhileNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;
    WhileNode(ASTNodePtr c, ASTNodePtr b) : condition(c), body(b) {}
    ~WhileNode() { delete condition; delete body; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "While\n";
        indent(o, indent_n + 2); o << "Condition:\n";
        if (condition) condition->print(o, indent_n + 4);
        indent(o, indent_n + 2); o << "Body:\n";
        if (body) body->print(o, indent_n + 4);
    }
};

// For
struct ForNode : ASTNode {
    std::string iterator;
    ASTNodePtr iterable;
    ASTNodePtr body;
    ForNode(std::string it, ASTNodePtr iter, ASTNodePtr b)
        : iterator(std::move(it)), iterable(iter), body(b) {}
    ~ForNode() { delete iterable; delete body; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "For(" << iterator << " in)\n";
        if (iterable) iterable->print(o, indent_n + 2);
        if (body) body->print(o, indent_n + 2);
    }
};

// String literal
struct StringNode : ASTNode {
    std::string value;
    StringNode(std::string v) : value(std::move(v)) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "String(\"" << value << "\")\n";
    }
};

// Boolean literal
struct BooleanNode : ASTNode {
    bool value;
    BooleanNode(bool v) : value(v) {}
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "Boolean(" << (value ? "true" : "false") << ")\n";
    }
};

// Inicialización (new)
struct NewNode : ASTNode {
    std::string type_name;
    std::vector<ASTNodePtr> args;
    NewNode(std::string t, std::vector<ASTNodePtr> a) : type_name(std::move(t)), args(std::move(a)) {}
    ~NewNode() { for (auto* a : args) delete a; }
    void print(std::ostream& o, int indent_n = 0) const override {
        indent(o, indent_n);
        o << "New(" << type_name << ")\n";
        for (auto* a : args) a->print(o, indent_n + 2);
    }
};