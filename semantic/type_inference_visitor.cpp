#include "type_inference_visitor.hpp"
#include <iostream>

void TypeInferenceVisitor::infer(ProgramNode* root) {
    // Phase 1: collect all declarations (functions, global variables, etc.)
    collecting = true;
    root->accept(*this);
    if (hasErrors()) {
        reportErrors();
        return;
    }

    // Phase 2: analyze function bodies in declaration order
    collecting = false;
    for (auto* func : pendingFunctions) {
        func->accept(*this);
        if (hasErrors()) {
            reportErrors();
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
        // Register the function in the symbol table
        Type* retType = node.returnType ? node.returnType : VoidType::instance();
        FunctionType* funcType = new FunctionType(node.paramTypes, retType);
        SymbolInfo info{node.name, funcType, SemanticSymbolKind::Function, symTable.getCurrentLevel()};
        if (!symTable.insert(node.name, info)) {
            error("Function '" + node.name + "' already declared.");
        }
        depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
        pendingFunctions.push_back(&node);
        return nullptr;
    } else {
        // Analyze the function body
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

        node.body->accept(*this);

        symTable.exitScope();
        currentFuncStack.pop_back();
        return nullptr;
    }
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
    if (collecting) {
        node.type = UnknownType::instance();
        return node.type;
    }

    SymbolInfo* funcInfo = symTable.lookup(node.name);
    if (!funcInfo || funcInfo->kind != SemanticSymbolKind::Function) {
        error("Function '" + node.name + "' not declared or is not a function.");
        node.type = UnknownType::instance();
        return node.type;
    }
    FunctionType* funcType = dynamic_cast<FunctionType*>(funcInfo->type);
    if (!funcType) {
        error("'" + node.name + "' is not a function.");
        node.type = UnknownType::instance();
        return node.type;
    }
    if (node.args.size() != funcType->getParamTypes().size()) {
        error("Incorrect number of arguments for '" + node.name +
              "'. Expected " + std::to_string(funcType->getParamTypes().size()) +
              ", got " + std::to_string(node.args.size()));
        node.type = UnknownType::instance();
        return node.type;
    }
    for (size_t i = 0; i < node.args.size(); ++i) {
        Type* argType = node.args[i]->accept(*this);
        if (!argType->equals(funcType->getParamTypes()[i])) {
            error("Argument " + std::to_string(i+1) + " of '" + node.name +
                  "' expected type " + funcType->getParamTypes()[i]->toString() +
                  ", but has type " + argType->toString());
        }
    }
    node.type = funcType->getReturnType();

    // Registrar dependencia en el grafo (solo si estamos dentro de una función)
    if (!currentFuncStack.empty()) {
        DepNode* calledNode = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, nullptr);
        depGraph.addDependency(currentFuncStack.back(), calledNode);
    }
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