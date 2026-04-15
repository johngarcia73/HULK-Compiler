#pragma once
#include "visitor.hpp"
#include "symbol_table.hpp"
#include "dependency_graph.hpp"   // nuevo
#include <vector>
#include <string>
#include <stack>


class TypeInferenceVisitor : public Visitor {
    SemanticSymbolTable& symTable;
    DependencyGraph& depGraph;
    std::vector<FunctionDeclNode*> pendingFunctions;  // declarations to process
    std::vector<DepNode*> currentFuncStack;          // functions stack (for dependencies)
    bool collecting;                                 // collector phase
    std::vector<std::string> errors;
    

    void error(const std::string& msg) { errors.push_back(msg); }
    void reportErrors() const;

    std::stack<Type*> returnTypeStack;

public:
    TypeInferenceVisitor(SemanticSymbolTable& table, DependencyGraph& graph)
        : symTable(table), depGraph(graph), collecting(true) {}

    void infer(ProgramNode* root);
    bool hasErrors() const { return !errors.empty(); }
    const std::vector<std::string>& getErrors() const { return errors; }

    // visit methods (todos los nodos del AST)
    Type* visit(ProgramNode& node) override;
    Type* visit(BlockNode& node) override;
    Type* visit(FunctionDeclNode& node) override;
    Type *visit(ReturnNode &node);
    Type *visit(LetNode &node) override;
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
    Type* visit(TypeNode& node) override;
};