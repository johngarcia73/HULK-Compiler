#pragma once
#include "../semantic/visitor.hpp"
#include "../common/scope_table.hpp"
class LoweringVisitor : public Visitor {
public:
    virtual ~LoweringVisitor() = default;
    ScopeTable scopeTable; 
    ASTNodePtr* parentReference=nullptr;
    std::vector<ASTNodePtr> generatedTypes;
    Type* visit(ProgramNode& node) override;
    Type* visit(BlockNode& node) override;
    Type* visit(FunctionDeclNode& node) override;
    Type* visit(LetNode& node) override;
    Type* visit(IfNode& node) override;
    Type* visit(FunctionCallNode& node) override;
    Type* visit(VariableNode& node) override;
    Type* visit(NumberNode& node) override;
    Type* visit(BinaryOpNode& node) override;
    Type* visit(ExprStmtNode& node) override;
    Type* visit(AssignmentNode& node) override;
    Type* visit(MemberAccessNode& node) override;
    Type* visit(NewNode& node) override;
    Type* visit(WhileNode& node) override;
    Type* visit(ForNode& node) override;
    Type* visit(AttributeDeclNode& node) override;
    Type* visit(TypeDeclNode& node) override;
    Type* visit(ProtocolDeclNode& node) override;
    Type* visit(ParamListNode& node) override;
    Type* visit(LetBindingNode& node) override;
    Type* visit(LetBindingsNode& node) override;
    Type* visit(UnaryOpNode& node) override;
    Type* visit(StringNode& node) override;
    Type* visit(BoolNode& node) override;
    Type* visit(TypeNode& node) override;
    Type* visit(ReturnNode& node) override;
    Type* visit(LambdaNode& node) override;
    Type* visit(VectorLiteralNode& node) override;
    Type* visit(VectorComprehensionNode& node) override;
    Type* visit(IndexAccessNode& node) override;
};

