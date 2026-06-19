#include "ast_node.hpp"
#include "../../semantic/visitor.hpp"
#include <cstdlib>
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
    const std::vector<std::string>& param_type_names,
    Type* return_type) {
    std::ostringstream os;
    os << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) {
            os << ", ";
        }
        os << params[i] << ": ";
        if (i < param_types.size() && param_types[i]) {
            os << param_types[i]->toString();
        } else if (i < param_type_names.size() && !param_type_names[i].empty()) {
            os << param_type_names[i];
        } else {
            os << "<unknown>";
        }
    }
    os << ") -> " << type_label(return_type);
    return os.str();
}

}  // namespace

// ------------------------------------------------------------
// NumberNode
// ------------------------------------------------------------
NumberNode::NumberNode(const std::string& v, NumberKind kind_)
    : value(v), numberKind(kind_) {}

NumberNode::NumberNode(long long v, NumberKind kind_)
    : value(std::to_string(v)), numberKind(kind_) {}

long long NumberNode::asInt() const {
    try { return std::stoll(value); } catch(...) { return 0; }
}

double NumberNode::asDouble() const {
    try { return std::stod(value); } catch(...) { return 0.0; }
}

bool NumberNode::isWellFormed() const {
    if (value.empty()) {
        return false;
    }

    char* end = nullptr;
    switch (numberKind) {
        case NumberKind::Int:
        case NumberKind::Long:
            std::strtoll(value.c_str(), &end, 10);
            break;
        case NumberKind::Float:
            std::strtof(value.c_str(), &end);
            break;
        case NumberKind::Double:
            std::strtod(value.c_str(), &end);
            break;
    }

    return end != nullptr && *end == '\0';
}

std::string NumberNode::kindName() const {
    return numberKindToString(numberKind);
}

void NumberNode::print(std::ostream &o, int indent_n) const {
    indent(o, indent_n);
    o << "Number(" << value;
    o << " : " << kindName();
    o << ")";
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
ParamListNode::ParamListNode(
    std::vector<std::string> p,
    std::vector<Type*> pt,
    std::vector<std::string> ptn)
    : params(std::move(p)), paramTypes(std::move(pt)), paramTypeNames(std::move(ptn)) {}

void ParamListNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "ParamList(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) o << ", ";
        o << params[i] << " : ";
        if (i < paramTypes.size() && paramTypes[i]) {
            o << paramTypes[i]->toString();
        } else if (i < paramTypeNames.size() && !paramTypeNames[i].empty()) {
            o << paramTypeNames[i];
        } else {
            o << "<unknown>";
        }
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
      paramTypeNames(std::vector<std::string>(params.size())),
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
      paramTypeNames(std::vector<std::string>(params.size())),
      returnType(rt),
      declaredReturnType(rt),
      hasExplicitReturnType(rt != nullptr),
      body(nullptr),
      isInline(inlineFlag),
      exprBody(eBody),
      inferredFunctionType(nullptr) {}
      
FunctionDeclNode::~FunctionDeclNode() {
}


void FunctionDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "FunctionDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    std::string mode = isSignatureOnly ? "signature" : (isInline ? "inline" : "block");
    print_line(o, indent_n + 2, "Mode: " + mode);
    print_line(
        o,
        indent_n + 2,
        "DeclaredReturn: " + std::string(hasExplicitReturnType ? type_label(declaredReturnType) : "<implicit>"));
    if (hasExplicitReturnType && !declaredReturnTypeName.empty()) {
        print_line(o, indent_n + 2, "DeclaredReturnName: " + declaredReturnTypeName);
    }
    print_line(o, indent_n + 2, "ResolvedReturn: " + type_label(returnType));
    print_line(
        o,
        indent_n + 2,
        "Signature: " + function_signature(params, paramTypes, paramTypeNames, returnType));
    if (inferredFunctionType) {
        print_line(o, indent_n + 2, "InferredFunctionType: " + inferredFunctionType->toString());
    }
    if (isMethod) {
        print_line(o, indent_n + 2, "MethodOwner: " + (ownerTypeName.empty() ? "<unknown>" : ownerTypeName));
    }
    if (isProtocolMethod) {
        print_line(o, indent_n + 2, "ProtocolOwner: " + (ownerProtocolName.empty() ? "<unknown>" : ownerProtocolName));
    }
    indent(o, indent_n + 2);
    o << "Params";
    if (params.empty()) {
        o << ": <none>\n";
    } else {
        o << ":\n";
        for (size_t i = 0; i < params.size(); ++i) {
            std::string type_text = "<unknown>";
            if (i < paramTypes.size() && paramTypes[i]) {
                type_text = type_label(paramTypes[i]);
            } else if (i < paramTypeNames.size() && !paramTypeNames[i].empty()) {
                type_text = paramTypeNames[i];
            }
            print_line(
                o,
                indent_n + 4,
                params[i] + ": " + type_text);
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
LetNode::LetNode(
    std::string n,
    ASTNodePtr i,
    ASTNodePtr b,
    Type* dt,
    std::string dtn,
    bool explicitType)
    : name(std::move(n)),
      declaredType(dt),
      declaredTypeName(std::move(dtn)),
      hasExplicitType(explicitType),
      init(i),
      body(b) {}

LetNode::~LetNode() {
}

void LetNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Let(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    if (hasExplicitType || declaredType || !declaredTypeName.empty()) {
        print_line(
            o,
            indent_n + 2,
            "DeclaredType: " + (!declaredTypeName.empty() ? declaredTypeName : type_label(declaredType)));
    }
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
FunctionCallNode::FunctionCallNode(std::string n, std::vector<ASTNodePtr> a, ASTNodePtr r)
    : name(std::move(n)), receiver(r), args(std::move(a)) {}

FunctionCallNode::~FunctionCallNode() {
}

void FunctionCallNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "FunctionCall(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, receiver ? "Receiver:" : "Receiver: <none>");
    if (receiver) {
        receiver->print(o, indent_n + 4);
    }
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
LetBindingNode::LetBindingNode(
    std::string n,
    ASTNodePtr i,
    Type* dt,
    std::string dtn,
    bool explicitType)
    : name(std::move(n)),
      declaredType(dt),
      declaredTypeName(std::move(dtn)),
      hasExplicitType(explicitType),
      init(i) {}

LetBindingNode::~LetBindingNode() {
}

void LetBindingNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "LetBinding(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    if (hasExplicitType || declaredType || !declaredTypeName.empty()) {
        print_line(
            o,
            indent_n + 2,
            "DeclaredType: " + (!declaredTypeName.empty() ? declaredTypeName : type_label(declaredType)));
    }
    if (init) {
        print_line(o, indent_n + 2, "Init:");
        init->print(o, indent_n + 4);
    } else {
        print_line(o, indent_n + 2, "Init: <none>");
    }
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
TypeNode::TypeNode(Type* t)
    : typeName(t ? t->toString() : "") {
    type = t;
}

TypeNode::TypeNode(std::string name, Type* t)
    : typeName(std::move(name)) {
    type = t;
}

void TypeNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Type(" << (!typeName.empty() ? typeName : type_label(type)) << ")";
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
    : target(new VariableNode(std::move(t))), value(v) {}

AssignmentNode::AssignmentNode(ASTNodePtr t, ASTNodePtr v)
    : target(t), value(v) {}

AssignmentNode::~AssignmentNode() {
}

void AssignmentNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Assignment";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, target ? "Target:" : "Target: <none>");
    if (target) target->print(o, indent_n + 4);
    print_line(o, indent_n + 2, value ? "Value:" : "Value: <none>");
    if (value) value->print(o, indent_n + 4);
}

Type* AssignmentNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// MemberAccessNode
// ------------------------------------------------------------
MemberAccessNode::MemberAccessNode(ASTNodePtr b, std::string m)
    : base(b), member(std::move(m)) {}

MemberAccessNode::~MemberAccessNode() {
}

void MemberAccessNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "MemberAccess(" << member << ")";
    print_metadata(o, *this);
    o << "\n";
    if (base) base->print(o, indent_n + 2);
}

Type* MemberAccessNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// NewNode
// ------------------------------------------------------------
NewNode::NewNode(std::string t, std::vector<ASTNodePtr> a)
    : typeName(std::move(t)), args(std::move(a)) {}

NewNode::~NewNode() {
}

void NewNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "New(" << typeName << ")";
    print_metadata(o, *this);
    o << "\n";
    for (auto* a : args) a->print(o, indent_n + 2);
}

Type* NewNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// LambdaNode
// ------------------------------------------------------------
LambdaNode::LambdaNode(
    std::vector<std::string> p,
    std::vector<Type*> pt,
    std::vector<std::string> ptn,
    Type* rt,
    std::string rtn,
    bool explicitReturn,
    ASTNodePtr b)
    : params(std::move(p)),
      paramTypes(std::move(pt)),
      paramTypeNames(std::move(ptn)),
      declaredReturnType(rt),
      declaredReturnTypeName(std::move(rtn)),
      hasExplicitReturnType(explicitReturn),
      body(b) {}

LambdaNode::~LambdaNode() {
}

void LambdaNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "Lambda";
    print_metadata(o, *this);
    o << "\n";
    print_line(
        o,
        indent_n + 2,
        "Signature: " + function_signature(params, paramTypes, paramTypeNames, declaredReturnType));
    if (functionType) {
        print_line(o, indent_n + 2, "InferredFunctionType: " + functionType->toString());
    }
    print_line(o, indent_n + 2, body ? "Body:" : "Body: <none>");
    if (body) body->print(o, indent_n + 4);
}

Type* LambdaNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// VectorLiteralNode
// ------------------------------------------------------------
VectorLiteralNode::VectorLiteralNode(std::vector<ASTNodePtr> e)
    : elements(std::move(e)) {}

VectorLiteralNode::~VectorLiteralNode() {
}

void VectorLiteralNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "VectorLiteral";
    print_metadata(o, *this);
    o << "\n";
    if (elements.empty()) {
        print_line(o, indent_n + 2, "Elements: <empty>");
        return;
    }
    print_line(o, indent_n + 2, "Elements:");
    for (auto* element : elements) element->print(o, indent_n + 4);
}

Type* VectorLiteralNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// VectorComprehensionNode
// ------------------------------------------------------------
VectorComprehensionNode::VectorComprehensionNode(ASTNodePtr expr, std::string it, ASTNodePtr iter)
    : expression(expr), iterator(std::move(it)), iterable(iter) {}

VectorComprehensionNode::~VectorComprehensionNode() {
}

void VectorComprehensionNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "VectorComprehension(" << iterator << ")";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, iterable ? "Iterable:" : "Iterable: <none>");
    if (iterable) iterable->print(o, indent_n + 4);
    print_line(o, indent_n + 2, expression ? "Expression:" : "Expression: <none>");
    if (expression) expression->print(o, indent_n + 4);
}

Type* VectorComprehensionNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// IndexAccessNode
// ------------------------------------------------------------
IndexAccessNode::IndexAccessNode(ASTNodePtr b, ASTNodePtr i)
    : base(b), index(i) {}

IndexAccessNode::~IndexAccessNode() {
}

void IndexAccessNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "IndexAccess";
    print_metadata(o, *this);
    o << "\n";
    print_line(o, indent_n + 2, base ? "Base:" : "Base: <none>");
    if (base) base->print(o, indent_n + 4);
    print_line(o, indent_n + 2, index ? "Index:" : "Index: <none>");
    if (index) index->print(o, indent_n + 4);
}

Type* IndexAccessNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// WhileNode
// ------------------------------------------------------------
WhileNode::WhileNode(ASTNodePtr c, ASTNodePtr b)
    : condition(c), body(b) {}

WhileNode::~WhileNode() {
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
    return v.visit(*this);
}

// ------------------------------------------------------------
// ForNode
// ------------------------------------------------------------
ForNode::ForNode(std::string it, ASTNodePtr iter, ASTNodePtr b)
    : iterator(std::move(it)), iterable(iter), body(b) {}

ForNode::~ForNode() {
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
    return v.visit(*this);
}

// ------------------------------------------------------------
// AttributeDeclNode
// ------------------------------------------------------------
AttributeDeclNode::AttributeDeclNode(
    std::string n,
    ASTNodePtr i,
    Type* dt,
    std::string dtn,
    bool explicitType)
    : name(std::move(n)),
      declaredType(dt),
      declaredTypeName(std::move(dtn)),
      hasExplicitType(explicitType),
      init(i) {}

AttributeDeclNode::~AttributeDeclNode() {
}

void AttributeDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "AttributeDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    if (hasExplicitType || declaredType || !declaredTypeName.empty()) {
        print_line(
            o,
            indent_n + 2,
            "DeclaredType: " + (!declaredTypeName.empty() ? declaredTypeName : type_label(declaredType)));
    }
    print_line(o, indent_n + 2, init ? "Init:" : "Init: <none>");
    if (init) init->print(o, indent_n + 4);
}

Type* AttributeDeclNode::accept(Visitor& v) {
    return v.visit(*this);
}

// ------------------------------------------------------------
// TypeDeclNode
// ------------------------------------------------------------
TypeDeclNode::TypeDeclNode(
    std::string n,
    std::vector<std::string> cp,
    std::vector<Type*> cpt,
    std::vector<std::string> cptn,
    std::string pt,
    std::vector<ASTNodePtr> pa,
    std::vector<ASTNodePtr> m,
    bool explicitParentArgs)
    : name(std::move(n)),
      ctorParams(std::move(cp)),
      ctorParamTypes(std::move(cpt)),
      ctorParamTypeNames(std::move(cptn)),
      parentType(std::move(pt)),
      parentArgs(std::move(pa)),
      hasExplicitParentArgs(explicitParentArgs),
      members(std::move(m)) {}

TypeDeclNode::~TypeDeclNode() {
}

void TypeDeclNode::print(std::ostream& o, int indent_n) const {
    indent(o, indent_n);
    o << "TypeDecl(" << name << ")";
    print_metadata(o, *this);
    o << "\n";
    indent(o, indent_n + 2);
    o << "CtorParams:";
    if (ctorParams.empty()) {
        o << " <none>\n";
    } else {
        o << "\n";
        for (size_t i = 0; i < ctorParams.size(); ++i) {
            std::string type_text = "<unknown>";
            if (i < ctorParamTypes.size() && ctorParamTypes[i]) {
                type_text = type_label(ctorParamTypes[i]);
            } else if (i < ctorParamTypeNames.size() && !ctorParamTypeNames[i].empty()) {
                type_text = ctorParamTypeNames[i];
            }
            print_line(o, indent_n + 4, ctorParams[i] + ": " + type_text);
        }
    }
    indent(o, indent_n + 2);
    o << "ParentType: " << parentType << "\n";
    print_line(
        o,
        indent_n + 2,
        std::string("ParentArgsMode: ") + (hasExplicitParentArgs ? "explicit" : "implicit"));
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
    return v.visit(*this);
}

// ------------------------------------------------------------
// ProtocolDeclNode
// ------------------------------------------------------------
ProtocolDeclNode::ProtocolDeclNode(std::string n, std::string ep, std::vector<ASTNodePtr> m)
    : name(std::move(n)), extendedProtocol(std::move(ep)), methods(std::move(m)) {}

ProtocolDeclNode::~ProtocolDeclNode() {
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
    return v.visit(*this);
}


ReturnNode::ReturnNode(ASTNodePtr e) : expr(e) {}
ReturnNode::~ReturnNode() {}
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
