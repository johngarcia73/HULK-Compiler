#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include "diagnostics.hpp"
#include "../parser/AST_Builder/ast_node.hpp"

class MacroExpander {
    struct Binding {
        enum class Kind {
            Ast,
            Placeholder
        };

        Kind kind = Kind::Ast;
        ASTNode* ast = nullptr;  // not owned
        std::string placeholderName;
    };

    struct ExpansionEnv {
        std::unordered_map<std::string, Binding> bindings;
        std::vector<std::unordered_map<std::string, std::string>> renameScopes;
        std::unordered_set<std::string> placeholderParams;
        std::string macroName;
    };

    SemanticContext& context_;
    std::unordered_map<std::string, MacroDeclNode*> macros_;
    std::size_t uniqueCounter_ = 0;
    std::size_t expansionDepth_ = 0;
    static constexpr std::size_t kMaxExpansionDepth = 128;

    void collectMacros(ProgramNode& program);
    ASTNode* expandNode(ASTNode* node);
    ASTNode* expandMacroCall(
        const std::string& name,
        const std::vector<ASTNode*>& args,
        ASTNode* bodyArg,
        const SourceSpan& callSpan);

    ASTNode* cloneDetached(ASTNode* node);
    ASTNode* cloneForExpansion(ASTNode* node, ExpansionEnv& env);
    ASTNode* cloneNode(ASTNode* node, ExpansionEnv* env);
    Type* cloneType(Type* type) const;

    std::string freshName(const std::string& base);
    void pushScope(ExpansionEnv& env);
    void popScope(ExpansionEnv& env);
    void bindLocal(ExpansionEnv& env, const std::string& original, const std::string& renamed);
    const std::string* lookupLocal(const ExpansionEnv& env, const std::string& name) const;
    const Binding* lookupBinding(const ExpansionEnv& env, const std::string& name) const;
    ASTNode* cloneActualBinding(const Binding& binding);

    bool bindPattern(
        ASTNode* pattern,
        ASTNode* value,
        std::unordered_map<std::string, Binding>& captures);

public:
    explicit MacroExpander(SemanticContext& context)
        : context_(context) {}

    bool expandProgram(ProgramNode& program);
};
