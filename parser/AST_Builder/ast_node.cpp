#include "ast_node.hpp"
#include "../../semantic/visitor.hpp"
#include <sstream>

namespace {

std::string type_label(const Type* type) {
    return type ? type->toString() : "<untyped>";
}

std::string span_label(const SourceSpan& span) {
    return span.toString();
}

void print_metadata(std::ostream& o, const ASTNode& node) {
    o << " [type=" << type_label(node.type) << ", span=" << span_label(node.span) << "]";
}

void print_line(std::ostream& o, int indent_n, const std::string& text) {
    indent(o, indent_n);
    o << text << "\n";
}

std::string function_signature(
    const std::vector<std::string>& params,
    const std::vector<Type*>& param_types,
    Type* return_type) {
    std::ostringstream os;
    os << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        os << params[i] << ": ";
        os << (i < param_types.size() && param_types[i] ? param_types[i]->toString() : "<unknown>");
    }
    os << ") -> " << type_label(return_type);
    return os.str();
}

bool has_number_suffix(char ch) {
    return ch == 'f' || ch == 'F' || ch == 'd' || ch == 'D';
}

std::string strip_number_suffix(const std::string& literal) {
    if (!literal.empty() && has_number_suffix(literal.back())) {
        return literal.substr(0, literal.size() - 1);
    }
    return literal;
}

}  // namespace

const char* number_kind_name(NumberKind kind) {
    switch (kind) {
        case NumberKind::Int:
            return "int";
        case NumberKind::Float:
            return "float";
        case NumberKind::Double:
            return "double";
    }
    return "int";
}

NumberKind classify_number_kind(const std::string& literal) {
    if (!literal.empty()) {
        if (literal.back() == 'f' || literal.back() == 'F') {
            return NumberKind::Float;
        }
        if (literal.back() == 'd' || literal.back() == 'D') {
            return NumberKind::Double;
        }
    }

    if (literal.find('.') != std::string::npos ||
        literal.find('e') != std::string::npos ||
        literal.find('E') != std::string::npos) {
        return NumberKind::Double;
    }

    return NumberKind::Int;
}

// ------------------------------------------------------------
// NumberNode
// ------------------------------------------------------------
NumberNode::NumberNode(const std::string& v, NumberKind kind_)
    : value(v), kind(kind_) {}

std::string NumberNode::kindName() const {
    return number_kind_name(kind);
}

std::optional<long long> NumberNode::tryAsInt() const {
    if (kind != NumberKind::Int) {
        return std::nullopt;
    }

    const std::string numeric_text = strip_number_suffix(value);
    if (numeric_text.empty()) {
        return std::nullopt;
    }

    try {
        size_t parsed = 0;
        const long long result = std::stoll(numeric_text, &parsed, 10);
        if (parsed != numeric_text.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<double> NumberNode::tryAsDouble() const {
    const std::string numeric_text = strip_number_suffix(value);
    if (numeric_text.empty()) {
        return std::nullopt;
    }

    try {
        size_t parsed = 0;
        const double result = std::stod(numeric_text, &parsed);
        if (parsed != numeric_text.size()) {
            return std::nullopt;
        }
        return result;
    } catch (...) {
        return std::nullopt;
    }
}

bool NumberNode::isWellFormed() const {
    return kind == NumberKind::Int ? tryAsInt().has_value() : tryAsDouble().has_value();
}

long long NumberNode::asInt() const {
    return tryAsInt().value_or(0);
}

double NumberNode::asDouble() const {
    return tryAsDouble().value_or(0.0);
}

void NumberNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Number(" << value << " : " << kindName() << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "Bool(" << (value ? "true" : "false") << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "String(\"" << value << "\")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "Variable(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "BinaryOp(" << op << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "UnaryOp(" << op << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "ExprStmt";
    print_metadata(o, *this);
    o << "\n";
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
    o << "Block";
    print_metadata(o, *this);
    o << "\n";
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
}

void ProgramNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Program";
    print_metadata(o, *this);
    o << "\n";
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
    o << ")";
    print_metadata(o, *this);
    o << "\n";
}

Type* ParamListNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// FunctionDeclNode
// ------------------------------------------------------------
FunctionDeclNode::FunctionDeclNode(
    std::string n,
    std::vector<std::string> p,
    std::vector<Type*> pt,
    Type* rt,
    ASTNodePtr b
)
    : name(std::move(n)),
      params(std::move(p)),
      paramTypes(std::move(pt)),
      returnType(rt),
      declaredReturnType(rt),
      hasExplicitReturnType(rt != nullptr),
      body(b),
      isInline(false),
      exprBody(nullptr),
      inferredFunctionType(nullptr) {}

FunctionDeclNode::FunctionDeclNode(
    std::string n,
    std::vector<std::string> p,
    std::vector<Type*> pt,
    Type* rt,
    ASTNodePtr eBody,
    bool inlineFlag
)
    : name(std::move(n)),
      params(std::move(p)),
      paramTypes(std::move(pt)),
      returnType(rt),
      declaredReturnType(rt),
      hasExplicitReturnType(rt != nullptr),
      body(nullptr),
      isInline(inlineFlag),
      exprBody(eBody),
      inferredFunctionType(nullptr) {}
      
FunctionDeclNode::~FunctionDeclNode() {
    delete body;
    delete exprBody;
}


void FunctionDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "FunctionDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, "Mode: " + std::string(isInline ? "inline" : "block"));
    print_line(
        o,
        indent_n + 2,
        "DeclaredReturn: " + std::string(hasExplicitReturnType ? type_label(declaredReturnType) : "<implicit>"));
    print_line(o, indent_n + 2, "ResolvedReturn: " + type_label(returnType));
    print_line(
        o,
        indent_n + 2,
        "Signature: " + function_signature(params, paramTypes, returnType));
    if (inferredFunctionType) {
        print_line(o, indent_n + 2, "InferredFunctionType: " + inferredFunctionType->toString());
    }
    indent(o, indent_n + 2);
    o << "Params";
    if (params.empty()) {
        o << ": <none>\n";
    } else {
        o << ":\n";
        for (size_t i = 0; i < params.size(); ++i) {
            print_line(
                o,
                indent_n + 4,
                params[i] + ": " + (i < paramTypes.size() ? type_label(paramTypes[i]) : "<unknown>"));
        }
    }
    print_line(o, indent_n + 2, body ? "Body:" : "Body: <none>");
    if (body) {
        body->print(o, indent_n + 4);
    }
    print_line(o, indent_n + 2, exprBody ? "ExprBody:" : "ExprBody: <none>");
    if (exprBody) {
        exprBody->print(o, indent_n + 4);
    }
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
    o << "Let(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, init ? "Init:" : "Init: <none>");
    if (init) init->print(o, indent_n + 4);
    print_line(o, indent_n + 2, body ? "Body:" : "Body: <none>");
    if (body) body->print(o, indent_n + 4);
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
    o << "If";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2); o << "Condition:\n";
    if (condition) condition->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Then:\n";
    if (then_branch) then_branch->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Else:\n";
    if (else_branch) {
        else_branch->print(o, indent_n + 4);
    } else {
        print_line(o, indent_n + 4, "<none>");
    }
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
    o << "FunctionCall(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    print_line(
        o,
        indent_n + 2,
        "ResolvedOverload: " + std::string(resolvedFunctionType ? resolvedFunctionType->toString() : "<none>"));
    if (!overloadStatus.empty()) {
        print_line(o, indent_n + 2, "OverloadStatus: " + overloadStatus);
    }
    if (!overloadNotes.empty()) {
        print_line(o, indent_n + 2, "OverloadNotes:");
        for (const auto& note : overloadNotes) {
            print_line(o, indent_n + 4, note);
        }
    }
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
    o << "LetBinding(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
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
    o << "LetBindings";
    print_metadata(o, *this);
    o << "\n";
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
    o << "Type(" << type->toString() << ")";
    print_metadata(o, *this);
    o << "\n";
}

Type* TypeNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// AssignmentNode
// ------------------------------------------------------------
AssignmentNode::AssignmentNode(std::string t, ASTNodePtr v)
    : target(std::move(t)), value(v) {}

AssignmentNode::~AssignmentNode() {
    delete value;
}

void AssignmentNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Assignment(" << target << ")";
    print_metadata(o, *this);
    o << "\n";
    if (value) value->print(o, indent_n + 2);
}

Type* AssignmentNode::accept(Visitor& v) {
    throw std::runtime_error("AssignmentNode::accept not implemented");
}

// ------------------------------------------------------------
// MemberAccessNode
// ------------------------------------------------------------
MemberAccessNode::MemberAccessNode(ASTNodePtr b, std::string m)
    : base(b), member(std::move(m)) {}

MemberAccessNode::~MemberAccessNode() {
    delete base;
}

void MemberAccessNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "MemberAccess(" << member << ")";
    print_metadata(o, *this);
    o << "\n";
    if (base) base->print(o, indent_n + 2);
}

Type* MemberAccessNode::accept(Visitor& v) {
    throw std::runtime_error("MemberAccessNode::accept not implemented");
}

// ------------------------------------------------------------
// NewNode
// ------------------------------------------------------------
NewNode::NewNode(std::string t, std::vector<ASTNodePtr> a)
    : typeName(std::move(t)), args(std::move(a)) {}

NewNode::~NewNode() {
    for (auto* a : args) delete a;
}

void NewNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "New(" << typeName << ")";
    print_metadata(o, *this);
    o << "\n";
    for (auto* a : args) a->print(o, indent_n + 2);
}

Type* NewNode::accept(Visitor& v) {
    throw std::runtime_error("NewNode::accept not implemented");
}

// ------------------------------------------------------------
// WhileNode
// ------------------------------------------------------------
WhileNode::WhileNode(ASTNodePtr c, ASTNodePtr b)
    : condition(c), body(b) {}

WhileNode::~WhileNode() {
    delete condition;
    delete body;
}

void WhileNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "While";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2); o << "Condition:\n";
    if (condition) condition->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Body:\n";
    if (body) body->print(o, indent_n + 4);
}

Type* WhileNode::accept(Visitor& v) {
    throw std::runtime_error("WhileNode::accept not implemented");
}

// ------------------------------------------------------------
// ForNode
// ------------------------------------------------------------
ForNode::ForNode(std::string it, ASTNodePtr iter, ASTNodePtr b)
    : iterator(std::move(it)), iterable(iter), body(b) {}

ForNode::~ForNode() {
    delete iterable;
    delete body;
}

void ForNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "For(" << iterator << ")";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2); o << "Iterable:\n";
    if (iterable) iterable->print(o, indent_n + 4);
    indent(o, indent_n + 2); o << "Body:\n";
    if (body) body->print(o, indent_n + 4);
}

Type* ForNode::accept(Visitor& v) {
    throw std::runtime_error("ForNode::accept not implemented");
}

// ------------------------------------------------------------
// AttributeDeclNode
// ------------------------------------------------------------
AttributeDeclNode::AttributeDeclNode(std::string n, ASTNodePtr i)
    : name(std::move(n)), init(i) {}

AttributeDeclNode::~AttributeDeclNode() {
    delete init;
}

void AttributeDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "AttributeDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    if (init) init->print(o, indent_n + 2);
}

Type* AttributeDeclNode::accept(Visitor& v) {
    throw std::runtime_error("AttributeDeclNode::accept not implemented");
}

// ------------------------------------------------------------
// TypeDeclNode
// ------------------------------------------------------------
TypeDeclNode::TypeDeclNode(std::string n, std::vector<std::string> cp, std::string pt, 
                           std::vector<ASTNodePtr> pa, std::vector<ASTNodePtr> m)
    : name(std::move(n)), ctorParams(std::move(cp)), parentType(std::move(pt)), 
      parentArgs(std::move(pa)), members(std::move(m)) {}

TypeDeclNode::~TypeDeclNode() {
    for (auto* pa : parentArgs) delete pa;
    for (auto* m : members) delete m;
}

void TypeDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "TypeDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2);
    o << "CtorParams:";
    for (auto& p : ctorParams) o << " " << p;
    o << "\n";
    indent(o, indent_n + 2);
    o << "ParentType: " << parentType << "\n";
    if (!parentArgs.empty()) {
        indent(o, indent_n + 2);
        o << "ParentArgs:\n";
        for (auto* pa : parentArgs) pa->print(o, indent_n + 4);
    }
    if (!members.empty()) {
        indent(o, indent_n + 2);
        o << "Members:\n";
        for (auto* m : members) m->print(o, indent_n + 4);
    }
}

Type* TypeDeclNode::accept(Visitor& v) {
    throw std::runtime_error("TypeDeclNode::accept not implemented");
}

// ------------------------------------------------------------
// ProtocolDeclNode
// ------------------------------------------------------------
ProtocolDeclNode::ProtocolDeclNode(std::string n, std::string ep, std::vector<ASTNodePtr> m)
    : name(std::move(n)), extendedProtocol(std::move(ep)), methods(std::move(m)) {}

ProtocolDeclNode::~ProtocolDeclNode() {
    for (auto* m : methods) delete m;
}

void ProtocolDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "ProtocolDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2);
    o << "Extends: " << extendedProtocol << "\n";
    if (!methods.empty()) {
        indent(o, indent_n + 2);
        o << "Methods:\n";
        for (auto* m : methods) m->print(o, indent_n + 4);
    }
}

Type* ProtocolDeclNode::accept(Visitor& v) {
    throw std::runtime_error("ProtocolDeclNode::accept not implemented");
}


ReturnNode::ReturnNode(ASTNodePtr e) : expr(e) {}
ReturnNode::~ReturnNode() { delete expr; }
void ReturnNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Return";
    print_metadata(o, *this);
    o << "\n";
    if (expr) {
        expr->print(o, indent_n + 2);
    } else {
        print_line(o, indent_n + 2, "<none>");
    }
}

Type* ReturnNode::accept(Visitor& v) {
    return v.visit(*this);
}
