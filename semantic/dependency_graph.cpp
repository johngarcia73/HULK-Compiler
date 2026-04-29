#include "dependency_graph.hpp"
#include <algorithm>
#include <functional>

DepNode* DependencyGraph::getOrCreateNode(const std::string& name, DepNodeKind kind, ASTNode* ast) {
    auto it = nodes.find(name);
    if (it != nodes.end()) {
        return it->second.get();
    }
    auto node = std::make_unique<DepNode>(name, kind, ast);
    DepNode* ptr = node.get();
    nodes[name] = std::move(node);
    return ptr;
}

void DependencyGraph::setNodeAST(const std::string& name, ASTNode* ast) {
    auto it = nodes.find(name);
    if (it != nodes.end()) {
        it->second->ast = ast;
    } else {
        // If doestn exist, It is created with the given AST.
        getOrCreateNode(name, DepNodeKind::Function, ast);
    }
}

void DependencyGraph::addDependency(DepNode* from, DepNode* to) {
    // Avoid duplicates
    for (auto* dep : from->depends) {
        if (dep == to) return;
    }
    from->depends.push_back(to);
    to->dependedBy.push_back(from);
}

bool DependencyGraph::topologicalSort() {
    topologicalOrder.clear();
    cycleMessages.clear();
    // Reset flags
    for (auto& [name, node] : nodes) {
        node->visited = false;
        node->inStack = false;
    }

    bool hasCycle = false;
    std::function<void(DepNode*)> dfs = [&](DepNode* node) {
        if (node->visited) return;
        node->visited = true;
        node->inStack = true;
        for (auto* dep : node->depends) {
            if (!dep->visited) {
                dfs(dep);
            } else if (dep->inStack) {
                // Cycle detected!
                hasCycle = true;
                cycleMessages.push_back("Cycle detected involving " + node->name +
                                        " -> " + dep->name);
            }
        }
        node->inStack = false;
        topologicalOrder.push_back(node);
    };

    for (auto& [name, node] : nodes) {
        if (!node->visited) {
            dfs(node.get());
        }
    }

    // Invert to get topological sorting (nodes without dependency first)
    std::reverse(topologicalOrder.begin(), topologicalOrder.end());
    return !hasCycle;
}

void DependencyGraph::dump(std::ostream& out) const {
    out << "=== Dependency Graph ===\n";
    for (const auto& [name, node] : nodes) {
        out << name << " (";
        switch (node->kind) {
            case DepNodeKind::Function: out << "function"; break;
            case DepNodeKind::GlobalVariable: out << "global var"; break;
            default: out << "unknown";
        }
        out << ")\n";
        if (!node->depends.empty()) {
            out << "  depends on: ";
            for (auto* dep : node->depends) {
                out << dep->name << " ";
            }
            out << "\n";
        }
    }
}
