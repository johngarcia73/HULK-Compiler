#pragma once
#include <string>
#include <vector>
#include <iostream>
#include "../../common/source_span.hpp"
#include "../../semantic/type.hpp"

class Visitor;  // forward declaration

struct ASTNode {
    Type* type = nullptr;   // inferred type
    SourceSpan span;
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
    std::string value;    // lexical representation of the number
    NumberKind numberKind;
    NumberNode(const std::string& v, NumberKind kind = NumberKind::Int);
    NumberNode(long long v, NumberKind kind = NumberKind::Int);
    long long asInt() const;
    double asDouble() const;
    bool isWellFormed() const;
    std::string kindName() const;
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
    std::vector<std::string> paramTypeNames;
    ParamListNode(
        std::vector<std::string> p,
        std::vector<Type*> pt,
        std::vector<std::string> ptn = {});
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct FunctionDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramTypeNames;
    Type* returnType;
    Type* declaredReturnType;
    std::string declaredReturnTypeName;
    bool hasExplicitReturnType = false;
    bool isMethod = false;
    std::string ownerTypeName;
    bool isSignatureOnly = false;
    bool isProtocolMethod = false;
    std::string ownerProtocolName;

    ASTNodePtr body;       // nullptr if inline
    bool isInline;
    ASTNodePtr exprBody;

    FunctionType* inferredFunctionType = nullptr;

    FunctionDeclNode(std::string n, std::vector<std::string> p, std::vector<Type*> pt,
                     Type* rt, ASTNodePtr b);

    FunctionDeclNode(std::string n, std::vector<std::string> p, std::vector<Type*> pt,
                     Type* rt, ASTNodePtr eBody, bool inlineFlag);
    ~FunctionDeclNode();

    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};
struct LetNode : ASTNode {
    std::string name;
    Type* declaredType = nullptr;
    std::string declaredTypeName;
    bool hasExplicitType = false;
    ASTNodePtr init;
    ASTNodePtr body;
    LetNode(std::string n, ASTNodePtr i, ASTNodePtr b, Type* dt = nullptr, std::string dtn = "", bool explicitType = false);
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
    ASTNodePtr receiver = nullptr;
    std::vector<ASTNodePtr> args;
    FunctionType* resolvedFunctionType = nullptr;
    std::string overloadStatus;
    std::vector<std::string> overloadNotes;
    FunctionCallNode(std::string n, std::vector<ASTNodePtr> a, ASTNodePtr r = nullptr);
    ~FunctionCallNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct AssignmentNode : ASTNode {
    ASTNodePtr target;
    ASTNodePtr value;
    AssignmentNode(std::string t, ASTNodePtr v);
    AssignmentNode(ASTNodePtr t, ASTNodePtr v);
    ~AssignmentNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct MemberAccessNode : ASTNode {
    ASTNodePtr base;
    std::string member;
    MemberAccessNode(ASTNodePtr b, std::string m);
    ~MemberAccessNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct NewNode : ASTNode {
    std::string typeName;
    std::vector<ASTNodePtr> args;
    NewNode(std::string t, std::vector<ASTNodePtr> a);
    ~NewNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct LambdaNode : ASTNode {
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramTypeNames;
    Type* declaredReturnType = nullptr;
    std::string declaredReturnTypeName;
    bool hasExplicitReturnType = false;
    ASTNodePtr body;
    FunctionType* functionType = nullptr;
    LambdaNode(
        std::vector<std::string> p,
        std::vector<Type*> pt,
        std::vector<std::string> ptn,
        Type* rt,
        std::string rtn,
        bool explicitReturn,
        ASTNodePtr b);
    ~LambdaNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct VectorLiteralNode : ASTNode {
    std::vector<ASTNodePtr> elements;
    explicit VectorLiteralNode(std::vector<ASTNodePtr> e);
    ~VectorLiteralNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct VectorComprehensionNode : ASTNode {
    ASTNodePtr expression;
    std::string iterator;
    ASTNodePtr iterable;
    VectorComprehensionNode(ASTNodePtr expr, std::string it, ASTNodePtr iter);
    ~VectorComprehensionNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct IndexAccessNode : ASTNode {
    ASTNodePtr base;
    ASTNodePtr index;
    IndexAccessNode(ASTNodePtr b, ASTNodePtr i);
    ~IndexAccessNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct WhileNode : ASTNode {
    ASTNodePtr condition;
    ASTNodePtr body;
    WhileNode(ASTNodePtr c, ASTNodePtr b);
    ~WhileNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ForNode : ASTNode {
    std::string iterator;
    ASTNodePtr iterable;
    ASTNodePtr body;
    ForNode(std::string it, ASTNodePtr iter, ASTNodePtr b);
    ~ForNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct AttributeDeclNode : ASTNode {
    std::string name;
    Type* declaredType = nullptr;
    std::string declaredTypeName;
    bool hasExplicitType = false;
    ASTNodePtr init;
    AttributeDeclNode(std::string n, ASTNodePtr i, Type* dt = nullptr, std::string dtn = "", bool explicitType = false);
    ~AttributeDeclNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct TypeDeclNode : ASTNode {
    std::string name;
    std::vector<std::string> ctorParams;
    std::vector<Type*> ctorParamTypes;
    std::vector<std::string> ctorParamTypeNames;
    std::string parentType;
    std::vector<ASTNodePtr> parentArgs;
    bool hasExplicitParentArgs = false;
    std::vector<ASTNodePtr> members;
    TypeDeclNode(
        std::string n,
        std::vector<std::string> cp,
        std::vector<Type*> cpt,
        std::vector<std::string> cptn,
        std::string pt,
        std::vector<ASTNodePtr> pa,
        std::vector<ASTNodePtr> m,
        bool explicitParentArgs = false);
    ~TypeDeclNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ProtocolDeclNode : ASTNode {
    std::string name;
    std::string extendedProtocol;
    std::vector<ASTNodePtr> methods;
    ProtocolDeclNode(std::string n, std::string ep, std::vector<ASTNodePtr> m);
    ~ProtocolDeclNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

// ============================================================================
// Aux nodes
// ============================================================================

struct LetBindingNode : ASTNode {
    std::string name;
    Type* declaredType = nullptr;
    std::string declaredTypeName;
    bool hasExplicitType = false;
    ASTNodePtr init;
    LetBindingNode(std::string n, ASTNodePtr i, Type* dt = nullptr, std::string dtn = "", bool explicitType = false);
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
    std::string typeName;
    TypeNode(Type* t);
    TypeNode(std::string name, Type* t = nullptr);
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};

struct ReturnNode : ASTNode {
    ASTNodePtr expr;
    ReturnNode(ASTNodePtr e);
    ~ReturnNode();
    void print(std::ostream& o, int indent_n = 0) const override;
    Type* accept(Visitor& v) override;
};
