#include "llvm_ir_generator.hpp"
#include <llvm/IR/Verifier.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/ToolOutputFile.h>
#include <llvm/Support/Casting.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Instructions.h>
#include <iostream>

// ------------------------------------------------------------
// Scope management
// ------------------------------------------------------------
void LLVMIRGenerator::pushScope() {
    varScopes.emplace();
}

void LLVMIRGenerator::popScope() {
    varScopes.pop();
}

void LLVMIRGenerator::declareVariable(const std::string& name, llvm::Value* alloca) {
    if (varScopes.empty()) pushScope();
    varScopes.top()[name] = alloca;
}

llvm::Value* LLVMIRGenerator::lookupVariable(const std::string& name) {
    // Traverse scopes from innermost to outermost
    auto copy = varScopes;
    while (!copy.empty()) {
        auto it = copy.top().find(name);
        if (it != copy.top().end()) return it->second;
        copy.pop();
    }
    std::cerr << "LLVM generation error: variable '" << name << "' not found in scope.\n";
    return nullptr;
}

// ------------------------------------------------------------
// Constructor and runtime setup
// ------------------------------------------------------------
LLVMIRGenerator::LLVMIRGenerator()
    : module(std::make_unique<llvm::Module>("HULK", context)),
      builder(context) {
    declareRuntimeFunctions();
}

void LLVMIRGenerator::declareRuntimeFunctions() {
    // _concat(i8*, i8*) -> i8*
    llvm::FunctionType* concatType = llvm::FunctionType::get(
        llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
        {llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0),
         llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0)},
        false);
    llvm::Function::Create(concatType, llvm::Function::ExternalLinkage,
                           "_concat", module.get());
}

// ------------------------------------------------------------
// Type conversion
// ------------------------------------------------------------
llvm::Type* LLVMIRGenerator::getLLVMType(Type* hulkType) {
    if (hulkType->equals(NumberType::instance()))
        return llvm::Type::getInt32Ty(context);
    if (hulkType->equals(BoolType::instance()))
        return llvm::Type::getInt1Ty(context);
    if (hulkType->equals(StringType::instance()))
        return llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
    // Function type not used as a value
    return llvm::Type::getVoidTy(context);
}

// ------------------------------------------------------------
// String constants
// ------------------------------------------------------------
llvm::Value* LLVMIRGenerator::getStringConstant(const std::string& value) {
    auto it = stringConstants.find(value);
    if (it != stringConstants.end()) {
        // Return a pointer to the first character (i8*) of the global
        llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
        std::vector<llvm::Value*> indices = {zero, zero};
        return builder.CreateInBoundsGEP(it->second->getValueType(), it->second, indices, "strptr");
    }

    // Create new global string constant
    llvm::Constant* strConst = llvm::ConstantDataArray::getString(context, value);
    auto* global = new llvm::GlobalVariable(
        *module, strConst->getType(), true,
        llvm::GlobalValue::PrivateLinkage, strConst, ".str");
    stringConstants[value] = global;

    llvm::Value* zero = llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0);
    std::vector<llvm::Value*> indices = {zero, zero};
    return builder.CreateInBoundsGEP(global->getValueType(), global, indices, "strptr");
}

// ------------------------------------------------------------
// Main generation entry point
// ------------------------------------------------------------
bool LLVMIRGenerator::generate(ProgramNode* root) {
    // First pass: declare all functions (to allow forward calls)
    for (auto* decl : root->decls) {
        if (auto* funcDecl = dynamic_cast<FunctionDeclNode*>(decl)) {
            Type* retType = funcDecl->returnType;
            llvm::Type* llvmRet = getLLVMType(retType);
            std::vector<llvm::Type*> paramTypes;
            for (auto* pt : funcDecl->paramTypes) {
                paramTypes.push_back(getLLVMType(pt));
            }
            llvm::FunctionType* funcType = llvm::FunctionType::get(llvmRet, paramTypes, false);
            llvm::Function::Create(funcType, llvm::Function::ExternalLinkage,
                                   funcDecl->name, module.get());
        }
    }

    // Generate function bodies
    for (auto* decl : root->decls) {
        if (auto* funcDecl = dynamic_cast<FunctionDeclNode*>(decl)) {
            if (!visitFunctionDecl(*funcDecl)) return false;
        }
    }

    // Generate main function that runs global statements
    llvm::FunctionType* mainType = llvm::FunctionType::get(
        llvm::Type::getInt32Ty(context), false);
    llvm::Function* mainFunc = llvm::Function::Create(
        mainType, llvm::Function::ExternalLinkage, "main", module.get());
    llvm::BasicBlock* mainBB = llvm::BasicBlock::Create(context, "entry", mainFunc);
    builder.SetInsertPoint(mainBB);

    for (auto* stmt : root->stmts) {
        if (auto* exprStmt = dynamic_cast<ExprStmtNode*>(stmt)) {
            generateExpr(exprStmt);  // value ignored
        } else {
            std::cerr << "Top-level statement not an ExprStmtNode\n";
        }
    }
    builder.CreateRet(llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0));

    // Verify module
    std::string errStr;
    llvm::raw_string_ostream errStream(errStr);
    if (llvm::verifyModule(*module, &errStream)) {
        std::cerr << "LLVM module verification failed:\n" << errStr << "\n";
        return false;
    }
    return true;
}

// ------------------------------------------------------------
// Output functions
// ------------------------------------------------------------
std::string LLVMIRGenerator::getIRString() const {
    std::string ir;
    llvm::raw_string_ostream os(ir);
    module->print(os, nullptr);
    return ir;
}

bool LLVMIRGenerator::writeToFile(const std::string& filename) const {
    std::error_code EC;
    llvm::ToolOutputFile out(filename, EC, llvm::sys::fs::OF_None);
    if (EC) {
        std::cerr << "Cannot open " << filename << ": " << EC.message() << "\n";
        return false;
    }
    module->print(out.os(), nullptr);
    out.keep();
    return true;
}

// ------------------------------------------------------------
// Expression dispatch
// ------------------------------------------------------------
llvm::Value* LLVMIRGenerator::generateExpr(ASTNode* node) {
    if (auto* n = dynamic_cast<NumberNode*>(node)) return visitNumber(*n);
    if (auto* n = dynamic_cast<BoolNode*>(node)) return visitBool(*n);
    if (auto* n = dynamic_cast<StringNode*>(node)) return visitString(*n);
    if (auto* n = dynamic_cast<VariableNode*>(node)) return visitVariable(*n);
    if (auto* n = dynamic_cast<BinaryOpNode*>(node)) return visitBinaryOp(*n);
    if (auto* n = dynamic_cast<UnaryOpNode*>(node)) return visitUnaryOp(*n);
    if (auto* n = dynamic_cast<LetNode*>(node)) return visitLet(*n);
    if (auto* n = dynamic_cast<IfNode*>(node)) return visitIf(*n);
    if (auto* n = dynamic_cast<FunctionCallNode*>(node)) return visitFunctionCall(*n);
    if (auto* n = dynamic_cast<BlockNode*>(node)) return visitBlock(*n);
    if (auto* n = dynamic_cast<ExprStmtNode*>(node)) return visitExprStmt(*n);
    std::cerr << "Unhandled AST node in generateExpr\n";
    return nullptr;
}

// ------------------------------------------------------------
// Visitors implementation
// ------------------------------------------------------------
llvm::Value* LLVMIRGenerator::visitProgram(ProgramNode& node) {
    // Already handled in generate()
    return nullptr;
}

llvm::Value* LLVMIRGenerator::visitFunctionDecl(FunctionDeclNode& node) {
    llvm::Function* func = module->getFunction(node.name);
    if (!func) {
        std::cerr << "Function " << node.name << " not found in module.\n";
        return nullptr;
    }

    llvm::BasicBlock* bb = llvm::BasicBlock::Create(context, "entry", func);
    builder.SetInsertPoint(bb);
    pushScope();

    // Allocate storage for parameters and store incoming arguments
    auto argIt = func->arg_begin();
    for (size_t i = 0; i < node.params.size(); ++i, ++argIt) {
        llvm::Argument* arg = argIt;
        arg->setName(node.params[i]);
        llvm::AllocaInst* alloca = builder.CreateAlloca(arg->getType(), nullptr, node.params[i]);
        builder.CreateStore(arg, alloca);
        declareVariable(node.params[i], alloca);
    }

    // Generate body (which is a BlockNode)
    generateExpr(node.body);

    // If no explicit return, insert default return
    if (!builder.GetInsertBlock()->getTerminator()) {
        Type* retType = node.returnType;
        if (retType->equals(NumberType::instance())) {
            builder.CreateRet(llvm::ConstantInt::get(getLLVMType(retType), 0));
        } else if (retType->equals(BoolType::instance())) {
            builder.CreateRet(llvm::ConstantInt::get(getLLVMType(retType), 0));
        } else if (retType->equals(StringType::instance())) {
            // Safe way: get pointer type from the known type
            llvm::PointerType* strPtrType = llvm::PointerType::get(llvm::Type::getInt8Ty(context), 0);
            builder.CreateRet(llvm::ConstantPointerNull::get(strPtrType));
        } else {
            builder.CreateRetVoid();
        }
    }

    popScope();
    return nullptr;
}

llvm::Value* LLVMIRGenerator::visitBlock(BlockNode& node) {
    llvm::Value* last = nullptr;
    for (auto* stmt : node.stmts) {
        last = generateExpr(stmt);
    }
    return last;  // return value of last statement (if any)
}

llvm::Value* LLVMIRGenerator::visitExprStmt(ExprStmtNode& node) {
    return generateExpr(node.expr);
}

llvm::Value* LLVMIRGenerator::visitNumber(NumberNode& node) {
    return llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), node.value);
}

llvm::Value* LLVMIRGenerator::visitBool(BoolNode& node) {
    return llvm::ConstantInt::get(llvm::Type::getInt1Ty(context), node.value ? 1 : 0);
}

llvm::Value* LLVMIRGenerator::visitString(StringNode& node) {
    return getStringConstant(node.value);
}

llvm::Value* LLVMIRGenerator::visitVariable(VariableNode& node) {
    llvm::Value* alloca = lookupVariable(node.name);
    if (!alloca) return nullptr;
    // alloca is an AllocaInst*, but we only need to load from it
    llvm::Type* loadedType = llvm::cast<llvm::AllocaInst>(alloca)->getAllocatedType();
    return builder.CreateLoad(loadedType, alloca, node.name);
}

llvm::Value* LLVMIRGenerator::visitBinaryOp(BinaryOpNode& node) {
    llvm::Value* left = generateExpr(node.left);
    llvm::Value* right = generateExpr(node.right);
    if (!left || !right) return nullptr;

    std::string op = node.op;
    // String concatenation
    if (op == "@") {
        llvm::Function* concat = module->getFunction("_concat");
        return builder.CreateCall(concat, {left, right}, "concat");
    }
    // Arithmetic
    if (op == "+") return builder.CreateAdd(left, right, "add");
    if (op == "-") return builder.CreateSub(left, right, "sub");
    if (op == "*") return builder.CreateMul(left, right, "mul");
    if (op == "/") return builder.CreateSDiv(left, right, "div");
    // Relational (produce i1)
    if (op == "<")  return builder.CreateICmpSLT(left, right, "cmp");
    if (op == ">")  return builder.CreateICmpSGT(left, right, "cmp");
    if (op == "<=") return builder.CreateICmpSLE(left, right, "cmp");
    if (op == ">=") return builder.CreateICmpSGE(left, right, "cmp");
    if (op == "==") return builder.CreateICmpEQ(left, right, "cmp");

    std::cerr << "Unsupported binary operator: " << op << "\n";
    return nullptr;
}

llvm::Value* LLVMIRGenerator::visitUnaryOp(UnaryOpNode& node) {
    llvm::Value* operand = generateExpr(node.operand);
    if (!operand) return nullptr;
    if (node.op == "-") return builder.CreateNeg(operand, "neg");
    if (node.op == "!") return builder.CreateNot(operand, "not");
    std::cerr << "Unsupported unary operator: " << node.op << "\n";
    return nullptr;
}

llvm::Value* LLVMIRGenerator::visitLet(LetNode& node) {
    llvm::Value* initVal = generateExpr(node.init);
    if (!initVal) return nullptr;

    llvm::Type* varType = getLLVMType(node.init->type);
    llvm::AllocaInst* alloca = builder.CreateAlloca(varType, nullptr, node.name);
    builder.CreateStore(initVal, alloca);

    pushScope();
    declareVariable(node.name, alloca);
    llvm::Value* bodyVal = generateExpr(node.body);
    popScope();
    return bodyVal;
}

llvm::Value* LLVMIRGenerator::visitIf(IfNode& node) {
    llvm::Value* cond = generateExpr(node.condition);
    if (!cond) return nullptr;

    // Ensure condition is i1
    if (cond->getType()->isIntegerTy(32)) {
        cond = builder.CreateICmpNE(cond, llvm::ConstantInt::get(llvm::Type::getInt32Ty(context), 0), "tobool");
    }

    llvm::Function* parent = builder.GetInsertBlock()->getParent();
    llvm::BasicBlock* thenBB = llvm::BasicBlock::Create(context, "then", parent);
    llvm::BasicBlock* elseBB = llvm::BasicBlock::Create(context, "else", parent);
    llvm::BasicBlock* mergeBB = llvm::BasicBlock::Create(context, "ifcont", parent);

    builder.CreateCondBr(cond, thenBB, elseBB);

    // Then branch
    builder.SetInsertPoint(thenBB);
    llvm::Value* thenVal = generateExpr(node.then_branch);
    if (!thenVal) return nullptr;
    builder.CreateBr(mergeBB);
    thenBB = builder.GetInsertBlock();  // update in case of changes

    // Else branch
    builder.SetInsertPoint(elseBB);
    llvm::Value* elseVal = generateExpr(node.else_branch);
    if (!elseVal) return nullptr;
    builder.CreateBr(mergeBB);
    elseBB = builder.GetInsertBlock();

    // Merge block
    builder.SetInsertPoint(mergeBB);
    llvm::PHINode* phi = builder.CreatePHI(thenVal->getType(), 2, "ifphi");
    phi->addIncoming(thenVal, thenBB);
    phi->addIncoming(elseVal, elseBB);
    return phi;
}

llvm::Value* LLVMIRGenerator::visitFunctionCall(FunctionCallNode& node) {
    llvm::Function* callee = module->getFunction(node.name);
    if (!callee) {
        std::cerr << "Unknown function: " << node.name << "\n";
        return nullptr;
    }
    std::vector<llvm::Value*> args;
    for (auto* argNode : node.args) {
        llvm::Value* arg = generateExpr(argNode);
        if (!arg) return nullptr;
        args.push_back(arg);
    }
    if (args.size() != callee->arg_size()) {
        std::cerr << "Argument count mismatch for call to " << node.name << "\n";
        return nullptr;
    }
    return builder.CreateCall(callee, args, "calltmp");
}