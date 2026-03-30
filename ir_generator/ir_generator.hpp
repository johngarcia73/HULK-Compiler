// ir_generator.hpp
#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <stack>
#include <unordered_set>
#include "../parser/AST_Builder/ast_node.hpp"

class IRGenerator {
public:
    std::string generate(ProgramNode* root);

private:
    struct CodeGenContext {
        std::vector<std::string> instructions;
        std::unordered_set<std::string> locals;
        std::unordered_set<std::string> params;
        std::stack<std::unordered_map<std::string, std::string>> scopes;
        int tempCounter = 0;
        int labelCounter = 0;
        int varCounter = 0;
        std::string newTemp() {
            return "t" + std::to_string(tempCounter++);
        }
        std::string newLabel() {
            return "L" + std::to_string(labelCounter++);
        }
        std::string newVar(const std::string& base) {
            return "var_" + base + "_" + std::to_string(varCounter++);
        }
        void pushScope() { scopes.emplace(); }
        void popScope() { scopes.pop(); }
        void addMapping(const std::string& original, const std::string& generated, bool isParam) {
            if (scopes.empty()) pushScope();
            scopes.top()[original] = generated;
            if (isParam) params.insert(generated);
            else locals.insert(generated);
        }
        std::string lookup(const std::string& original) {
            auto copy = scopes;
            while (!copy.empty()) {
                auto& map = copy.top();
                auto it = map.find(original);
                if (it != map.end()) return it->second;
                copy.pop();
            }
            return original;
        }
    };

    struct FunctionData {
        std::string name;
        std::vector<std::string> params;
        std::vector<std::string> locals;
        std::vector<std::string> instructions;
    };

    std::unordered_map<std::string, std::string> stringLabels;
    int stringCounter = 0;
    std::string getStringLabel(const std::string& str);

    void collectStrings(ASTNode* node);
    FunctionData generateFunction(FunctionDeclNode* node);
    FunctionData generateEntry(const std::vector<ASTNodePtr>& stmts);
    std::string generateExpr(ASTNode* node, CodeGenContext& ctx);

    // Node-specific generators
    std::string generateNumberNode(NumberNode* node, CodeGenContext& ctx);
    std::string generateBoolNode(BoolNode* node, CodeGenContext& ctx);
    std::string generateStringNode(StringNode* node, CodeGenContext& ctx);
    std::string generateVariableNode(VariableNode* node, CodeGenContext& ctx);
    std::string generateBinaryOpNode(BinaryOpNode* node, CodeGenContext& ctx);
    std::string generateUnaryOpNode(UnaryOpNode* node, CodeGenContext& ctx);
    std::string generateLetNode(LetNode* node, CodeGenContext& ctx);
    std::string generateIfNode(IfNode* node, CodeGenContext& ctx);
    std::string generateFunctionCallNode(FunctionCallNode* node, CodeGenContext& ctx);
    std::string generateBlockNode(BlockNode* node, CodeGenContext& ctx);
    std::string generateExprStmtNode(ExprStmtNode* node, CodeGenContext& ctx);
};