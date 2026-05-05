#pragma once

#include <string>
#include "../semantic/visitor.hpp"
#include "../common/string_builder.hpp"
#include "../ir_generator/scope_table.hpp"
#include "string_pool.hpp"
#include "name_generator.hpp"
#include "target_info.hpp"
#include <unordered_map>
#include <vector>

class ASTNode;

class IrGenerator : public Visitor {
private:
    TargetInfo targetInfo=TargetInfo::getNative();
    StringPool stringPool;
    StringBuilder dataBuilder;
    StringBuilder codeBuilder;
    NameGenerator nameGenerator;
    std::string nameForCurrentExpression;
    std::string typeForCurrentExpression;
    ScopeTable scopeTable;


public:
    virtual ~IrGenerator() = default;

    std::string generate(ASTNode& node);

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
};
