#pragma once

#include "../parser/AST_Builder/ast_node.hpp"

#include <memory>
#include <string>
#include <vector>

enum class CompilerErrorKind {
    Lexical = 1,
    Syntactic = 2,
    Semantic = 3
};

struct CompilerError {
    CompilerErrorKind kind;
    int line = 0;
    int column = 0;
    std::string message;
};

struct FrontendPipelineResult {
    std::unique_ptr<ASTNode> ast_owner;
    ProgramNode* ast = nullptr;
    std::vector<CompilerError> errors;

    bool success() const {
        return errors.empty() && ast != nullptr;
    }
};

FrontendPipelineResult run_frontend_pipeline(const std::string& source_path);
int exit_code_for(const FrontendPipelineResult& result);
void print_compiler_errors(const std::vector<CompilerError>& errors);
void write_backend_placeholder_output();
