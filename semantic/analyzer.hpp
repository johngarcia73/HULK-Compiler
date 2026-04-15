#pragma once
#include "visitor.hpp"
#include "symbol_table.hpp"
#include <vector>
#include <string>
#include <stack>

class SemanticAnalyzer : public Visitor {
    SemanticSymbolTable& symTable;
    std::vector<std::string> errors;

    void registerBuiltinFunctions();
    void error(const std::string& msg) { errors.push_back(msg); }

public:
    SemanticAnalyzer(SemanticSymbolTable& table) : symTable(table) {}

    void reportErrors() const;
    void analyze(ProgramNode* root);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }

    const SemanticSymbolTable& getSymbolTable() const { return symTable; }

    // visit Methods(just check)
    Type* visit(ProgramNode& node) override;
    Type* visit(BlockNode& node) override;
    Type* visit(FunctionDeclNode& node) override;
    Type* visit(LetNode& node) override;
    Type* visit(IfNode& node) override;
    Type* visit(FunctionCallNode& node) override;
    Type* visit(VariableNode& node) override;
    Type* visit(NumberNode& node) override;
    Type* visit(BoolNode& node) override;
    Type* visit(StringNode& node) override;
    Type* visit(BinaryOpNode& node) override;
    Type* visit(UnaryOpNode& node) override;
    Type* visit(ExprStmtNode& node) override;
    Type* visit(ParamListNode& node) override;
    Type* visit(LetBindingNode& node) override;
    Type* visit(LetBindingsNode& node) override;
    Type *visit(ReturnNode &node);
    Type *visit(TypeNode &node) override;
};