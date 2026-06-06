#pragma once

#include <string>
#include <vector>
#include "dependency_graph.hpp"
#include "diagnostics.hpp"
#include "symbol_table.hpp"
#include "type_registry.hpp"
#include "visitor.hpp"

class TypeInferenceVisitor : public Visitor {
    SemanticSymbolTable& symTable;
    DependencyGraph& depGraph;
    NominalTypeRegistry& typeRegistry;
    SemanticContext& context;
    std::vector<FunctionDeclNode*> pendingFunctions;
    std::vector<FunctionDeclNode*> pendingMethods;
    std::vector<Type*> returnTypeStack;
    bool collecting = false;
    bool collectingDependencies = false;
    std::string currentFunctionName;
    std::string currentOwnerTypeName;
    FunctionDeclNode* currentMethod = nullptr;

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
    Type* resolveDeclaredType(Type* builtinType, const std::string& typeName, const ASTNode& node);
    bool checkTypeConformance(
        Type* actual,
        Type* expected,
        const ASTNode& node,
        const std::string& message,
        std::vector<std::string> notes = {});
    Type* inferAssignmentTargetType(AssignmentNode& node, bool* isSelfTarget = nullptr);
    bool hasExplicitParameterType(const FunctionDeclNode& node, size_t index) const;
    Type* normalizeConstraintType(Type* type, bool preferGenericNumber = true) const;
    void refineUnknownExpression(ASTNode* node, Type* inferredType, bool preferGenericNumber = true);
    bool mergeInferredType(Type*& current, Type* candidate, const ASTNode& node, const std::string& label);
    void captureFunctionParameterTypes(FunctionDeclNode& node);
    void captureCtorParameterTypes(
        const ASTNode& ownerNode,
        const std::string& ownerLabel,
        const std::vector<std::string>& names,
        const std::vector<std::string>& declaredNames,
        std::vector<Type*>& types);
    Type* inferIterableElementType(Type* sourceType, const ASTNode& node);

public:
    TypeInferenceVisitor(
        SemanticSymbolTable& table,
        DependencyGraph& graph,
        NominalTypeRegistry& registry,
        SemanticContext& semanticContext)
        : symTable(table), depGraph(graph), typeRegistry(registry), context(semanticContext) {}

    void collectDeclarations(ProgramNode* root);
    void analyzeFunction(FunctionDeclNode* function);
    void analyzeTypes();
    void analyzeGlobals(ProgramNode* root);
    const std::vector<FunctionDeclNode*>& getPendingFunctions() const { return pendingFunctions; }
    const std::vector<FunctionDeclNode*>& getPendingMethods() const { return pendingMethods; }
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
    Type* visit(TypeNode& node) override;
    Type* visit(LambdaNode& node) override;
    Type* visit(VectorLiteralNode& node) override;
    Type* visit(VectorComprehensionNode& node) override;
    Type* visit(IndexAccessNode& node) override;
};
