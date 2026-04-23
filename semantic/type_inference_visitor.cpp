#include "type_inference_visitor.hpp"
#include <iostream>
#include <stack>
#include <unordered_set>

void TypeInferenceVisitor::infer(ProgramNode* root) {
    // Fase 1: recolectar declaraciones y dependencias
    collecting = true;
    collectingDependencies = true;
    root->accept(*this);
    if (hasErrors()) return;

    collecting = false;
    collectingDependencies = false;

    // Ordenar topológicamente
    std::vector<FunctionDeclNode*> orderedFunctions;
    if (!depGraph.topologicalSort()) {
        orderedFunctions = pendingFunctions;
    } else {
        const auto& order = depGraph.getTopologicalOrder();
        std::unordered_set<FunctionDeclNode*> added;
        for (DepNode* depNode : order) {
            if (depNode->kind == DepNodeKind::Function && depNode->ast) {
                if (auto* func = dynamic_cast<FunctionDeclNode*>(depNode->ast)) {
                    orderedFunctions.push_back(func);
                    added.insert(func);
                }
            }
        }
        for (auto* func : pendingFunctions) {
            if (added.find(func) == added.end()) {
                orderedFunctions.push_back(func);
            }
        }
    }

    // Fase 2: analizar cuerpos en orden topológico
    for (auto* func : orderedFunctions) {
        func->accept(*this);
        if (hasErrors()) return;
    }

    // Analizar sentencias globales
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
        Type* retType = node.returnType;
        if (!retType) retType = UnknownType::instance();

        // 🔴 Crear UNA SOLA instancia de FunctionType
        FunctionType* funcType = new FunctionType(node.paramTypes, retType);

        // Guardar referencia en el nodo (IMPORTANTE)
        node.inferredFunctionType = funcType;

        SymbolInfo info{node.name, funcType, SemanticSymbolKind::Function, symTable.getCurrentLevel()};
        if (!symTable.insert(node.name, info)) {
            error("Function '" + node.name + "' already declared.");
        }

        depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
        pendingFunctions.push_back(&node);

        if (collectingDependencies) {
            std::string prev = currentFunctionName;
            currentFunctionName = node.name;

            if (node.isInline) {
                if (node.exprBody) node.exprBody->accept(*this);
            } else {
                if (node.body) node.body->accept(*this);
            }

            currentFunctionName = prev;
        }

        return nullptr;
    }

    // ================= FASE 2 =================

    FunctionType* funcType = node.inferredFunctionType;
    if (!funcType) {
        error("Internal error: missing function type for '" + node.name + "'");
        return nullptr;
    }

    Type* declaredRetType = funcType->getReturnType();

    returnTypeStack.push_back(declaredRetType);

    DepNode* depNode = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
    currentFuncStack.push_back(depNode);

    symTable.enterScope();

    // Insert params
    for (size_t i = 0; i < node.params.size(); ++i) {
        SymbolInfo paramInfo{
            node.params[i],
            node.paramTypes[i],
            SemanticSymbolKind::Parameter,
            symTable.getCurrentLevel()
        };
        symTable.insert(node.params[i], paramInfo);
    }

    Type* actualRetType = nullptr;

    if (node.isInline) {
        actualRetType = node.exprBody ? node.exprBody->accept(*this) : UnknownType::instance();
    } else {
        actualRetType = node.body ? node.body->accept(*this) : VoidType::instance();
    }

    if (!actualRetType) actualRetType = UnknownType::instance();

    // ✅ REFINAR tipo en sitio
    if (declaredRetType->equals(UnknownType::instance())) {
        funcType->setReturnType(actualRetType);
        node.returnType = actualRetType;

        std::cerr << "DEBUG: Refined function '" << node.name
                  << "' to return type " << actualRetType->toString() << "\n";
    } else {
        if (!declaredRetType->equals(actualRetType)) {
            error("Return type mismatch in function '" + node.name + "'");
        }
    }

    symTable.exitScope();
    currentFuncStack.pop_back();
    returnTypeStack.pop_back();

    return nullptr;
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
    std::cerr << "DEBUG: Let binding '" << node.name << "' inserted with type "
              << (initType ? initType->toString() : "null") << " at level "
              << symTable.getCurrentLevel() << "\n";
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
        if (collectingDependencies && !currentFunctionName.empty()) {
            DepNode* from = depGraph.getOrCreateNode(currentFunctionName, DepNodeKind::Function, nullptr);
            DepNode* to   = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, nullptr);
            depGraph.addDependency(from, to);
        }
        return nullptr;
    }

    std::vector<Type*> argTypes;

    for (auto* arg : node.args) {
        Type* t = arg->accept(*this);
        if (!t) t = UnknownType::instance();
        argTypes.push_back(t);
    }

    FunctionType* funcType = symTable.lookupFunction(node.name, argTypes);

    if (!funcType) {
        error("Function '" + node.name + "' not found");
        node.type = UnknownType::instance();
        return node.type;
    }

    node.type = funcType->getReturnType();
    if (!node.type) node.type = UnknownType::instance();

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
    if (!sym->type) {
        error("Variable '" + node.name + "' has null type in symbol table.");
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

    if (!left) left = UnknownType::instance();
    if (!right) right = UnknownType::instance();

    // 🔴 Propagación de Unknown (importante)
    if (left->equals(UnknownType::instance()) ||
        right->equals(UnknownType::instance())) {
        node.type = UnknownType::instance();
        return node.type;
    }

    // =========================
    // ARITHMETIC
    // =========================
    if (node.op == "+" || node.op == "-" ||
        node.op == "*" || node.op == "/") {

        if (!left->equals(NumberType::instance()) ||
            !right->equals(NumberType::instance())) {
            error("Arithmetic operator '" + node.op + "' requires Number operands");
        }

        node.type = NumberType::instance();
    }

    // =========================
    // INEQUALITY
    // =========================
    else if (node.op == "<" || node.op == ">" ||
             node.op == "<=" || node.op == ">=") {

        if (!left->equals(NumberType::instance()) ||
            !right->equals(NumberType::instance())) {
            error("Relational operator '" + node.op + "' requires Number operands");
        }

        node.type = BoolType::instance();   // 🔴 CLAVE
    }

    // =========================
    // EQUALITY
    // =========================
    else if (node.op == "==") {

        if (!left->equals(right)) {
            error("Operator '==' requires operands of same type");
        }

        node.type = BoolType::instance();   // 🔴 CLAVE
    }

    // =========================
    // CONCAT
    // =========================
    else if (node.op == "@") {

        if (!left->equals(StringType::instance()) ||
            !right->equals(StringType::instance())) {
            error("Operator '@' requires String operands");
        }

        node.type = StringType::instance();
    }

    else {
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

Type* TypeInferenceVisitor::visit(ReturnNode& node) {
    if (collecting) return nullptr;

    Type* exprType = node.expr->accept(*this);
    if (!exprType) exprType = UnknownType::instance();

    node.type = exprType;

    if (returnTypeStack.empty()) {
        error("Return outside function");
        return node.type;
    }

    Type* expected = returnTypeStack.back();

    // ⚠️ Permitir Unknown durante inferencia
    if (!expected->equals(UnknownType::instance()) &&
        !exprType->equals(UnknownType::instance()) &&
        !expected->equals(exprType)) {
        error("Return type mismatch: expected " +
              expected->toString() + " but got " +
              exprType->toString());
    }

    return node.type;
}