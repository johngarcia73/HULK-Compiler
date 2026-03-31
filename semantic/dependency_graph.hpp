#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <iostream>
#include "../parser/AST_Builder/ast_node.hpp"

enum class DepNodeKind {
    Function,
    GlobalVariable,
    Class,          // future extensions
    // ...
};

struct DepNode {
    std::string name;
    DepNodeKind kind;
    ASTNode* ast;                           // pointer to AST node (declaration)
    std::vector<DepNode*> depends;          // nodes it depends of
    std::vector<DepNode*> dependedBy;       // nodes that depends of it
    bool visited;
    bool inStack;
    int order;                              // topological order

    DepNode(const std::string& n, DepNodeKind k, ASTNode* a)
        : name(n), kind(k), ast(a), visited(false), inStack(false), order(-1) {}
};

class DependencyGraph {
    std::unordered_map<std::string, std::unique_ptr<DepNode>> nodes;
    std::vector<DepNode*> topologicalOrder;

public:
    // Gets or creates a node with given name and types.
    DepNode* getOrCreateNode(const std::string& name, DepNodeKind kind, ASTNode* ast = nullptr);

    // Set an existent node AST pointer (úsefull when it was created before declartion).
    void setNodeAST(const std::string& name, ASTNode* ast);

    // Adds a from -> to dependency.
    void addDependency(DepNode* from, DepNode* to);

    // Performs a topological sorting (DFS). Return false if there is a cycle.
    bool topologicalSort();

    // Prints graph for debug.
    void dump() const;

    const std::vector<DepNode*>& getOrder() const { return topologicalOrder; }
};