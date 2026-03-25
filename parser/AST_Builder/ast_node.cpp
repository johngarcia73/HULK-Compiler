#include "ast_node.hpp"
#include "../../semantic/visitor.hpp" 

// ------------------------------------------------------------
// NumberNode
// ------------------------------------------------------------
NumberNode::NumberNode(long long v) : value(v) {}

void NumberNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Number(" << value << ")\n";
}

Type* NumberNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// BoolNode
// ------------------------------------------------------------
BoolNode::BoolNode(bool v) : value(v) {}

void BoolNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Bool(" << (value ? "true" : "false") << ")\n";
}

Type* BoolNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// StringNode
// ------------------------------------------------------------
StringNode::StringNode(const std::string& v) : value(v) {}

void StringNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "String(\"" << value << "\")\n";
}

Type* StringNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// VariableNode
// ------------------------------------------------------------
VariableNode::VariableNode(std::string n) : name(std::move(n)) {}

void VariableNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Variable(" << name << ")\n";
}

Type* VariableNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// BinaryOpNode
// ------------------------------------------------------------
BinaryOpNode::BinaryOpNode(std::string op_, ASTNodePtr l, ASTNodePtr r)
    : op(op_), left(l), right(r) {}

BinaryOpNode::~BinaryOpNode() {
    delete left;
    delete right;
}

void BinaryOpNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "BinaryOp(" << op << ")\n";
    if (left) left->print(o, indent_n + 2);
    if (right) right->print(o, indent_n + 2);
}

Type* BinaryOpNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// UnaryOpNode
// ------------------------------------------------------------
UnaryOpNode::UnaryOpNode(std::string op_, ASTNodePtr operand_)
    : op(op_), operand(operand_) {}

UnaryOpNode::~UnaryOpNode() {
    delete operand;
}

void UnaryOpNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "UnaryOp(" << op << ")\n";
    if (operand) operand->print(o, indent_n + 2);
}

Type* UnaryOpNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// ExprStmtNode
// ------------------------------------------------------------
ExprStmtNode::ExprStmtNode(ASTNodePtr e) : expr(e) {}

ExprStmtNode::~ExprStmtNode() {
    delete expr;
}

void ExprStmtNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "ExprStmt\n";
    if (expr) expr->print(o, indent_n + 2);
}

Type* ExprStmtNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// BlockNode
// ------------------------------------------------------------
BlockNode::BlockNode(std::vector<ASTNodePtr> s) : stmts(std::move(s)) {}

BlockNode::~BlockNode() {
    for (auto p : stmts) delete p;
}

void BlockNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Block\n";
    for (auto s : stmts) s->print(o, indent_n + 2);
}

Type* BlockNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// ProgramNode
// ------------------------------------------------------------
ProgramNode::ProgramNode(std::vector<ASTNodePtr> d, std::vector<ASTNodePtr> s)
    : decls(std::move(d)), stmts(std::move(s)) {}

ProgramNode::~ProgramNode() {
    for (auto p : decls) delete p;
    for (auto p : stmts) delete p;
}

void ProgramNode::test() const {
    std::cerr << "ProgramNode::test called\n";
}

void ProgramNode::print(std::ostream &o, int indent_n) const {
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

Type* ProgramNode::accept(Visitor& v) {
    std::cerr << "ProgramNode::accept called\n";
    return v.visit(*this);
}

// ------------------------------------------------------------
// ParamListNode
// ------------------------------------------------------------
ParamListNode::ParamListNode(std::vector<std::string> p, std::vector<Type*> pt)
    : params(std::move(p)), paramTypes(std::move(pt)) {}

void ParamListNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "ParamList(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) o << ", ";
        o << params[i] << " : " << paramTypes[i]->toString();
    }
    o << ")\n";
}

Type* ParamListNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// FunctionDeclNode
// ------------------------------------------------------------
FunctionDeclNode::FunctionDeclNode(std::string n, std::vector<std::string> p,
                                   std::vector<Type*> pt, Type* rt, ASTNodePtr b)
    : name(std::move(n)), params(std::move(p)), paramTypes(std::move(pt)), returnType(rt), body(b) {}

FunctionDeclNode::~FunctionDeclNode() {
    delete body;
}

void FunctionDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "FunctionDecl(" << name << ")\n";
    indent(o, indent_n + 2);
    o << "Params:";
    for (auto& p : params) o << " " << p;
    o << "\n";
    if (body) body->print(o, indent_n + 2);
}

Type* FunctionDeclNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// LetNode
// ------------------------------------------------------------
LetNode::LetNode(std::string n, ASTNodePtr i, ASTNodePtr b)
    : name(std::move(n)), init(i), body(b) {}

LetNode::~LetNode() {
    delete init;
    delete body;
}

void LetNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Let(" << name << ")\n";
    if (init) init->print(o, indent_n + 2);
    if (body) body->print(o, indent_n + 2);
}

Type* LetNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// IfNode
// ------------------------------------------------------------
IfNode::IfNode(ASTNodePtr c, ASTNodePtr t, ASTNodePtr e)
    : condition(c), then_branch(t), else_branch(e) {}

IfNode::~IfNode() {
    delete condition;
    delete then_branch;
    delete else_branch;
}

void IfNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "If\n";
    indent(o, indent_n + 2); o << "Condition:\n";
    if (condition) condition->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Then:\n";
    if (then_branch) then_branch->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Else:\n";
    if (else_branch) else_branch->print(o, indent_n + 4);
}

Type* IfNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// FunctionCallNode
// ------------------------------------------------------------
FunctionCallNode::FunctionCallNode(std::string n, std::vector<ASTNodePtr> a)
    : name(std::move(n)), args(std::move(a)) {}

FunctionCallNode::~FunctionCallNode() {
    for (auto* a : args) delete a;
}

void FunctionCallNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "FunctionCall(" << name << ")\n";
    for (auto* a : args) a->print(o, indent_n + 2);
}

Type* FunctionCallNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// LetBindingNode
// ------------------------------------------------------------
LetBindingNode::LetBindingNode(std::string n, ASTNodePtr i)
    : name(std::move(n)), init(i) {}

LetBindingNode::~LetBindingNode() {
    delete init;
}

void LetBindingNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "LetBinding(" << name << ")\n";
    if (init) init->print(o, indent_n + 2);
}

Type* LetBindingNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// LetBindingsNode
// ------------------------------------------------------------
LetBindingsNode::LetBindingsNode() = default;
LetBindingsNode::LetBindingsNode(std::vector<LetBindingNode*> b)
    : bindings(std::move(b)) {}

LetBindingsNode::~LetBindingsNode() {
    for (auto* b : bindings) delete b;
}

void LetBindingsNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "LetBindings\n";
    for (auto* b : bindings) b->print(o, indent_n + 2);
}

Type* LetBindingsNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// TypeNode
// ------------------------------------------------------------
TypeNode::TypeNode(Type* t) : type(t) {}

void TypeNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Type(" << type->toString() << ")\n";
}

Type* TypeNode::accept(Visitor& v) {
    return v.visit(*this);
}