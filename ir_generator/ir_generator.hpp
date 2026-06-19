#pragma once

#include <string>
#include "../semantic/visitor.hpp"
#include "../common/string_builder.hpp"
#include "scope_table.hpp"
#include "lowering.hpp"
#include "string_pool.hpp"
#include "name_generator.hpp"
#include "type_utils.hpp"
#include "type_register.hpp"
#include <unordered_map>
#include <vector>


class ASTNode;

class IrGenerator : public Visitor {
private:
    TypeUtils::TargetInfo targetInfo = TypeUtils::TargetInfo::getNative();
    StringPool stringPool;
    StringBuilder dataBuilder;
    StringBuilder codeBuilder;
    StringBuilder programBuilder;
    NameManager nameManager;
    std::string nameForCurrentExpression;
    std::string typeForCurrentExpression;
    ScopeTable scopeTable;
    TypeRegister typeRegister;
    
public:
    virtual ~IrGenerator() = default;
    std::string generate(ASTNodePtr node);
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
    Type* visit(ParamListNode& node) override;
    Type* visit(LetBindingNode& node) override;
    Type* visit(LetBindingsNode& node) override;
    Type* visit(UnaryOpNode& node) override;
    Type* visit(StringNode& node) override;
    Type* visit(BoolNode& node) override;
    Type* visit(TypeNode& node) override;
    Type* visit(ReturnNode& node) override;
    Type* visit(AssignmentNode& node) override;
    Type* visit(WhileNode& node) override;
    Type* visit(ForNode& node) override;
    Type* visit(MemberAccessNode& node) override;
    Type* visit(NewNode& node) override;
    Type* visit(AttributeDeclNode& node) override;
    Type* visit(TypeDeclNode& node) override;
    Type* visit(ProtocolDeclNode& node) override;
    Type* visit(LambdaNode& node) override;
    Type* visit(VectorLiteralNode& node) override;
    Type* visit(VectorComprehensionNode& node) override;
    Type* visit(IndexAccessNode &node) override;
};
