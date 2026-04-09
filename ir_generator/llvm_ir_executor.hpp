#pragma once

#include "../parser/AST_Builder/ast_node.hpp"
#include <string>

/// @brief Module that encapsulates LLVM IR generation, file output, and execution.
class LLVMExecutor {
public:
    /// @brief Generate LLVM IR from the AST and write it to a .ll file.
    /// @param ast Root of the semantically annotated AST.
    /// @param llFilename Output filename for the LLVM IR (default "output.ll").
    /// @return true if generation and file writing succeeded, false otherwise.
    bool generateIRToFile(ProgramNode* ast, const std::string& llFilename = "output.ll");

    /// @brief Generate LLVM IR, compile it to an executable using clang, and run it.
    /// @param ast Root of the semantically annotated AST.
    /// @param llFilename Temporary LLVM IR filename (default "output.ll").
    /// @param exeFilename Name of the generated executable (default "program").
    /// @return The exit code of the executed program, or -1 if an error occurred.
    int compileAndRun(ProgramNode* ast, const std::string& llFilename = "output.ll",
                      const std::string& exeFilename = "program");

    /// @brief Get the LLVM IR as a string (for debugging or inspection).
    /// @param ast Root of the semantically annotated AST.
    /// @return The LLVM IR text, or empty string on failure.
    std::string getIRString(ProgramNode* ast);
};