#pragma once

#include <string>
#include <vector>
#include "dependency_graph.hpp"
#include "diagnostics.hpp"
#include "symbol_table.hpp"
#include "visitor.hpp"

class TypeInferenceVisitor : public Visitor {
    SemanticSymbolTable& symTable;
    DependencyGraph& depGraph;
    SemanticContext& context;
    std::vector<FunctionDeclNode*> pendingFunctions;
    std::vector<Type*> returnTypeStack;
    bool collecting = false;
    bool collectingDependencies = false;
    std::string currentFunctionName;

    void error(
        SemanticPhase phase,
        const ASTNode& node,
        const std::string& message,
        std::vector<std::string> notes = {});
    void error(
        SemanticPhase phase,
        const SourceSpan& span,
        const std::string& message,
        std::vector<std::string> notes = {});
    void traceInference(const std::string& message);
    static Type* coerceUnknown(Type* type);

public:
    TypeInferenceVisitor(
        SemanticSymbolTable& table,
        DependencyGraph& graph,
        SemanticContext& semanticContext)
        : symTable(table), depGraph(graph), context(semanticContext) {}

    void collectDeclarations(ProgramNode* root);
    void analyzeFunction(FunctionDeclNode* function);
    void analyzeGlobals(ProgramNode* root);
    const std::vector<FunctionDeclNode*>& getPendingFunctions() const { return pendingFunctions; }
    bool hasErrors() const { return context.hasErrors(); }

    Type* visit(ProgramNode& node) override;
    Type* visit(BlockNode& node) override;
    Type* visit(FunctionDeclNode& node) override;
    Type* visit(ReturnNode& node) override;
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
    Type* visit(AssignmentNode& node) override;
    Type* visit(WhileNode& node) override;
    Type* visit(ForNode& node) override;
    Type* visit(ParamListNode& node) override;
    Type* visit(LetBindingNode& node) override;
    Type* visit(LetBindingsNode& node) override;
    Type* visit(TypeNode& node) override;
};
