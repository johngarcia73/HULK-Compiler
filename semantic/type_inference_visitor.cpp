#include "type_inference_visitor.hpp"
#include <iostream>
#include <stack>

void TypeInferenceVisitor::infer(ProgramNode* root) {
    // Phase 1: collect all declarations (functions, global variables, etc.)
    collecting = true;
    root->accept(*this);
    if (hasErrors()) {
        return;
    }

    // Phase 2: analyze function bodies in declaration order
    collecting = false;
    for (auto* func : pendingFunctions) {
        func->accept(*this);
        if (hasErrors()) {
            return;
        }
    }

    // Analyze global statements (they may use already-registered functions)
    root->accept(*this);
    reportErrors();
}

void TypeInferenceVisitor::reportErrors() const {
    for (const auto& e : errors) {
        std::cerr << "Semantic error: " << e << "\n";
    }
    if (errors.empty()) {
        std::cout << "Semantic analysis successful.\n";
    }
}

// ============================================================================
// Root and block nodes
// ============================================================================

Type* TypeInferenceVisitor::visit(ProgramNode& node) {
    if (collecting) {
        for (auto* decl : node.decls) decl->accept(*this);
    } else {
        for (auto* stmt : node.stmts) stmt->accept(*this);
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(BlockNode& node) {
    if (collecting) return nullptr;   // Phase 1: ignore

    symTable.enterScope();
    Type* lastType = nullptr;
    for (auto* stmt : node.stmts) {
        lastType = stmt->accept(*this);
    }
    symTable.exitScope();
    node.type = lastType ? lastType : VoidType::instance();
    return node.type;
}

// ============================================================================
// Function declarations
// ============================================================================
Type* TypeInferenceVisitor::visit(FunctionDeclNode& node) {
    if (collecting) {
        // Get return type (default: Void)
        Type* retType = node.returnType ? node.returnType : VoidType::instance();
        node.returnType = retType;  

        FunctionType* funcType = new FunctionType(node.paramTypes, retType);
        SymbolInfo info{node.name, funcType, SemanticSymbolKind::Function, symTable.getCurrentLevel()};
        if (!symTable.insert(node.name, info)) {
            error("Function '" + node.name + "' already declared.");
        }
        depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
        pendingFunctions.push_back(&node);
        return nullptr;
    } else {
        // Analyze function body
        Type* retType = node.returnType ? node.returnType : VoidType::instance();
        returnTypeStack.push(retType);

        DepNode* depNode = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
        currentFuncStack.push_back(depNode);
        symTable.enterScope();

        // Insert parameters
        for (size_t i = 0; i < node.params.size(); ++i) {
            SymbolInfo paramInfo{node.params[i], node.paramTypes[i],
                                 SemanticSymbolKind::Parameter, symTable.getCurrentLevel()};
            if (!symTable.insert(node.params[i], paramInfo)) {
                error("Duplicate parameter: " + node.params[i]);
            }
        }

        if (node.body) {
            node.body->accept(*this);
        } else {
            error("Function '" + node.name + "' has no body");
        }

        symTable.exitScope();
        currentFuncStack.pop_back();
        returnTypeStack.pop();
        return nullptr;
    }
}

Type* TypeInferenceVisitor::visit(ReturnNode& node) {
    node.expr->accept(*this);
    if (!node.expr->type) {
        error("Return expression has no type");
        return nullptr;
    }
    node.type = node.expr->type;
    
    if (returnTypeStack.empty()) {
        error("Return outside function");
        return nullptr;
    }
    Type* expected = returnTypeStack.top();
    if (expected && !expected->equals(node.expr->type)) {
        error("Return type mismatch: expected " + expected->toString() +
              " but got " + node.expr->type->toString());
        return nullptr;
    }// Mark that function has an explicit return
    return node.type;
}

// ============================================================================
// Let
// ============================================================================

Type* TypeInferenceVisitor::visit(LetNode& node) {
    if (collecting) return nullptr;

    Type* initType = node.init->accept(*this);
    if (!initType) initType = UnknownType::instance();

    symTable.enterScope();
    SymbolInfo varInfo{node.name, initType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()};
    if (!symTable.insert(node.name, varInfo)) {
        error("Variable '" + node.name + "' already declared in this scope.");
    }
    Type* bodyType = node.body->accept(*this);
    symTable.exitScope();

    node.type = bodyType;
    return node.type;
}

// ============================================================================
// If
// ============================================================================

Type* TypeInferenceVisitor::visit(IfNode& node) {
    if (collecting) return nullptr;

    // Ensure the condition is boolean
    Type* condType = node.condition->accept(*this);
    if (!condType->equals(BoolType::instance())) {
        error("If condition must be boolean.");
    }

    Type* thenType = node.then_branch->accept(*this);
    Type* elseType = node.else_branch->accept(*this);

    Type* result = thenType;
    if (thenType && elseType && !thenType->equals(elseType)) {
        error("The 'then' and 'else' branches must have the same type.");
        result = UnknownType::instance();
    }
    node.type = result;
    return result;
}

// ============================================================================
// Function call
// ============================================================================

Type* TypeInferenceVisitor::visit(FunctionCallNode& node) {
    // Inferr args types
    for (auto* arg : node.args) {
        arg->accept(*this);
        if (!arg->type) {
            error("Argument has no type");
            return nullptr;
        }
    }
    std::vector<Type*> argTypes;
    for (auto* arg : node.args) argTypes.push_back(arg->type);
    // Look for function
    FunctionType* funcType = symTable.lookupFunction(node.name, argTypes);
    if (!funcType) {
        error("Function '" + node.name + "' not declared or no matching overload");
        return nullptr;
    }
    node.type = funcType->getReturnType();
    return node.type;
}

// ============================================================================
// Variable
// ============================================================================

Type* TypeInferenceVisitor::visit(VariableNode& node) {
    if (collecting) {
        node.type = UnknownType::instance();
        return node.type;
    }

    SymbolInfo* sym = symTable.lookup(node.name);
    if (!sym) {
        error("Variable '" + node.name + "' not declared.");
        node.type = UnknownType::instance();
        return node.type;
    }
    node.type = sym->type;
    return node.type;
}

// ============================================================================
// Literals
// ============================================================================

Type* TypeInferenceVisitor::visit(NumberNode& node) {
    node.type = NumberType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(BoolNode& node) {
    node.type = BoolType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(StringNode& node) {
    node.type = StringType::instance();
    return node.type;
}

// ============================================================================
// Binary operators
// ============================================================================

Type* TypeInferenceVisitor::visit(BinaryOpNode& node) {
    if (collecting) {
        node.type = UnknownType::instance();
        return node.type;
    }

    Type* left = node.left->accept(*this);
    Type* right = node.right->accept(*this);
    if (!left || !right) {
        error("Operand without type in binary expression.");
        node.type = UnknownType::instance();
        return node.type;
    }

    if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error("Arithmetic operator requires Number operands.");
        }
        node.type = NumberType::instance();
    } else if (node.op == "<" || node.op == ">" || node.op == "<=" || node.op == ">=") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error("Relational operator requires Number operands.");
        }
        node.type = BoolType::instance();
    } else if (node.op == "==") {
        if (!left->equals(right)) {
            error("Operator '==' requires operands of the same type.");
        }
        node.type = BoolType::instance();
    } else if (node.op == "@") {
        if (!left->equals(StringType::instance()) || !right->equals(StringType::instance())) {
            error("Concatenation operator '@' requires String operands.");
        }
        node.type = StringType::instance();
    } else {
        error("Unknown binary operator: " + node.op);
        node.type = UnknownType::instance();
    }
    return node.type;
}

// ============================================================================
// Unary operators
// ============================================================================

Type* TypeInferenceVisitor::visit(UnaryOpNode& node) {
    if (collecting) {
        node.type = UnknownType::instance();
        return node.type;
    }

    Type* operand = node.operand->accept(*this);
    if (!operand) {
        error("Operand without type in unary expression.");
        node.type = UnknownType::instance();
        return node.type;
    }

    if (node.op == "-") {
        if (!operand->equals(NumberType::instance())) {
            error("Unary operator '-' requires Number operand.");
        }
        node.type = NumberType::instance();
    } else if (node.op == "!") {
        if (!operand->equals(BoolType::instance())) {
            error("Unary operator '!' requires Boolean operand.");
        }
        node.type = BoolType::instance();
    } else {
        error("Unknown unary operator: " + node.op);
        node.type = UnknownType::instance();
    }
    return node.type;
}

// ============================================================================
// Expression statement
// ============================================================================

Type* TypeInferenceVisitor::visit(ExprStmtNode& node) {
    if (collecting) return nullptr;
    node.expr->accept(*this);
    node.type = node.expr->type;
    return node.type;
}


Type* TypeInferenceVisitor::visit(TypeNode& node) {
    return node.type;
}

Type* TypeInferenceVisitor::visit(ParamListNode& node) { return nullptr; }
Type* TypeInferenceVisitor::visit(LetBindingNode& node) { return nullptr; }
Type* TypeInferenceVisitor::visit(LetBindingsNode& node) { return nullptr; }
