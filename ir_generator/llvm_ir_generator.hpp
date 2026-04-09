#pragma once

#include "../parser/AST_Builder/ast_node.hpp"
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <memory>
#include <stack>
#include <unordered_map>

class LLVMIRGenerator {
public:
    LLVMIRGenerator();
    ~LLVMIRGenerator() = default;

    /// Generate LLVM IR from the semantically annotated AST.
    /// Returns true on success, false on error (errors printed to stderr).
    bool generate(ProgramNode* root);

    /// Write the generated IR to a .ll file.
    bool writeToFile(const std::string& filename) const;

    /// Get the LLVM module as a string (IR text).
    std::string getIRString() const;

private:
    llvm::LLVMContext context;
    std::unique_ptr<llvm::Module> module;
    llvm::IRBuilder<> builder;

    // Current function being built
    llvm::Function* currentFunction = nullptr;

    // Scope stack: maps variable name to its alloca instruction
    std::stack<std::unordered_map<std::string, llvm::Value*>> varScopes;

    // String literal cache: map content -> global variable (i8*)
    std::unordered_map<std::string, llvm::GlobalVariable*> stringConstants;

    // Helper methods
    void pushScope();
    void popScope();
    void declareVariable(const std::string& name, llvm::Value* alloca);
    llvm::Value* lookupVariable(const std::string& name);

    // Convert HULK type to LLVM type
    llvm::Type* getLLVMType(Type* hulkType);

    // Get or create a global string constant (returns i8*)
    llvm::Value* getStringConstant(const std::string& value);

    // Ensure runtime functions are declared (e.g., _concat)
    void declareRuntimeFunctions();

    // Main dispatch for expressions
    llvm::Value* generateExpr(ASTNode* node);

    // Node visitors (each returns an LLVM value or nullptr)
    llvm::Value* visitProgram(ProgramNode& node);
    llvm::Value* visitFunctionDecl(FunctionDeclNode& node);
    llvm::Value* visitBlock(BlockNode& node);
    llvm::Value* visitExprStmt(ExprStmtNode& node);
    llvm::Value* visitNumber(NumberNode& node);
    llvm::Value* visitBool(BoolNode& node);
    llvm::Value* visitString(StringNode& node);
    llvm::Value* visitVariable(VariableNode& node);
    llvm::Value* visitBinaryOp(BinaryOpNode& node);
    llvm::Value* visitUnaryOp(UnaryOpNode& node);
    llvm::Value* visitLet(LetNode& node);
    llvm::Value* visitIf(IfNode& node);
    llvm::Value* visitFunctionCall(FunctionCallNode& node);

    // Placeholders for unused node types (to satisfy the interface)
    llvm::Value* visitTypeNode(TypeNode& node) { return nullptr; }
    llvm::Value* visitParamList(ParamListNode& node) { return nullptr; }
    llvm::Value* visitLetBinding(LetBindingNode& node) { return nullptr; }
    llvm::Value* visitLetBindings(LetBindingsNode& node) { return nullptr; }
};