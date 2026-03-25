#include "type_inference_visitor.hpp"
#include "visitor.hpp"
#include <iostream>

void TypeInferenceVisitor::infer(ProgramNode* root) {
    //symTable.enterScope();
    root->accept(*this);
    //symTable.exitScope();
    reportErrors();
}

void TypeInferenceVisitor::reportErrors() const {
    for (const auto& e : errors) {
        std::cerr << "Type inference error: " << e << "\n";
    }
    if (errors.empty()) {
        std::cout << "Type inference successful.\n";
    }
}

// --- Nodos raíz y bloque ---
Type* TypeInferenceVisitor::visit(ProgramNode& node) {
    for (auto* decl : node.decls) decl->accept(*this);
    for (auto* stmt : node.stmts) stmt->accept(*this);
    node.type = nullptr;  // programa no tiene tipo
    return nullptr;
}

Type* TypeInferenceVisitor::visit(BlockNode& node) {
    symTable.enterScope();
    Type* lastType = nullptr;
    for (auto* stmt : node.stmts) {
        lastType = stmt->accept(*this);
    }
    symTable.exitScope();
    node.type = lastType ? lastType : VoidType::instance();
    return node.type;
}

// --- Funciones ---
Type* TypeInferenceVisitor::visit(FunctionDeclNode& node) {
    // Insert placeholder with Unknown type
    SymbolInfo placeholder;
    placeholder.name = node.name;
    placeholder.type = UnknownType::instance();
    placeholder.kind = SemanticSymbolKind::Function;
    placeholder.scopeLevel = symTable.getCurrentLevel();
    if (!symTable.insert(node.name, placeholder)) {
        error("Función '" + node.name + "' ya declarada.");
        return nullptr;
    }

    symTable.enterScope(); // function scope

    // Insert parameters with their explicit types (or Unknown if not provided)
    for (size_t i = 0; i < node.params.size(); ++i) {
        Type* paramType = node.paramTypes[i];  // already set by AST builder
        SymbolInfo paramInfo{node.params[i], paramType, SemanticSymbolKind::Parameter, symTable.getCurrentLevel()};
        if (!symTable.insert(node.params[i], paramInfo)) {
            error("Parámetro duplicado: " + node.params[i]);
        }
    }

    // Infer body type (still needed for return type if not explicitly given)
    Type* bodyType = node.body->accept(*this);
    if (!bodyType) bodyType = VoidType::instance();

    // Use explicit return type if provided; otherwise use inferred body type
    Type* retType = node.returnType ? node.returnType : bodyType;

    // Update function symbol with correct FunctionType
    FunctionType* funcType = new FunctionType(node.paramTypes, retType);
    SymbolInfo updatedInfo{node.name, funcType, SemanticSymbolKind::Function, symTable.getCurrentLevel() - 1};
    symTable.update(node.name, updatedInfo);

    symTable.exitScope();
    return nullptr;
}

// --- Let ---
Type* TypeInferenceVisitor::visit(LetNode& node) {
    Type* initType = node.init->accept(*this);
    if (!initType) initType = UnknownType::instance();

    symTable.enterScope();                     // new scope for the let body
    SymbolInfo varInfo{node.name, initType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()};
    if (!symTable.insert(node.name, varInfo)) {
        error("Variable '" + node.name + "' ya declarada en este ámbito.");
    }
    Type* bodyType = node.body->accept(*this);
    symTable.exitScope();                      // exit after body
    node.type = bodyType;
    return node.type;
}

// --- If ---
Type* TypeInferenceVisitor::visit(IfNode& node) {
    // Condition: we infer its type (but still do not check if its boolean)
    node.condition->accept(*this);
    Type* thenType = node.then_branch->accept(*this);
    Type* elseType = node.else_branch->accept(*this);

    // If's type is the common type between then and else (if equal)
    Type* result = thenType;
    if (thenType && elseType && !thenType->equals(elseType)) {
        error("Las ramas 'then' y 'else' deben tener el mismo tipo.");
        result = UnknownType::instance();
    }
    node.type = result;
    return result;
}

// --- Function Call---
Type* TypeInferenceVisitor::visit(FunctionCallNode& node) {
    SymbolInfo* funcInfo = symTable.lookup(node.name);
    if (!funcInfo || funcInfo->kind != SemanticSymbolKind::Function) {
        error("Función '" + node.name + "' no declarada o no es función.");
        node.type = UnknownType::instance();
        return node.type;
    }
    FunctionType* funcType = dynamic_cast<FunctionType*>(funcInfo->type);
    if (!funcType) {
        error("Tipo incorrecto para función '" + node.name + "'.");
        node.type = UnknownType::instance();
        return node.type;
    }
    if (node.args.size() != funcType->getParamTypes().size()) {
        error("Número incorrecto de argumentos para '" + node.name + "'.");
        node.type = UnknownType::instance();
        return node.type;
    }
    for (size_t i = 0; i < node.args.size(); ++i) {
        Type* argType = node.args[i]->accept(*this);
        // The inference does not check compatibility here; that is done by the semantic analyzer.
    }
    node.type = funcType->getReturnType();
    return node.type;
}

// --- Variable ---
Type* TypeInferenceVisitor::visit(VariableNode& node) {
    SymbolInfo* sym = symTable.lookup(node.name);
    if (!sym) {
        error("Variable '" + node.name + "' no declarada.");
        node.type = UnknownType::instance();
        return node.type;
    }
    node.type = sym->type;
    return node.type;
}

// --- Literales ---
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

// --- Binary Operators---
Type* TypeInferenceVisitor::visit(BinaryOpNode& node) {
    Type* left = node.left->accept(*this);
    Type* right = node.right->accept(*this);
    if (!left || !right) {
        node.type = UnknownType::instance();
        return node.type;
    }

    if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/") {
        // For arithmetic, both must be Number
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            // The semantic analyzer will catch the mismatch.
        }
        node.type = NumberType::instance();
    } else if (node.op == "<" || node.op == ">" || node.op == "<=" || node.op == ">=" || node.op == "==") {
        // For relational, both must be comparable (same type)
        if (!left->equals(right)) {
        }
        node.type = BoolType::instance();
    } else if (node.op == "@") {
        // Concatenation: if either is String, result is String
        if (left->equals(StringType::instance()) || right->equals(StringType::instance())) {
            node.type = StringType::instance();
        } else {
            node.type = UnknownType::instance();
        }
    } else {
        node.type = UnknownType::instance();
    }
    return node.type;
}

// --- Unary operators---
Type* TypeInferenceVisitor::visit(UnaryOpNode& node) {
    Type* operand = node.operand->accept(*this);
    if (!operand) {
        node.type = UnknownType::instance();
        return node.type;
    }
    if (node.op == "-") {
        node.type = NumberType::instance();
    } else if (node.op == "!") {
        node.type = BoolType::instance();
    } else {
        error("Unary operator not supported.");
        node.type = UnknownType::instance();
    }
    return node.type;
}

// --- Sentences ---
Type* TypeInferenceVisitor::visit(ExprStmtNode& node) {
    Type* exprType = node.expr->accept(*this);
    node.type = exprType;
    return exprType;
}

Type* TypeInferenceVisitor::visit(TypeNode& node) {
    // Type already in node.type;
    node.type = node.type;
    return node.type;
}

// --- Aux nodes (Dont generate types)---
Type* TypeInferenceVisitor::visit(ParamListNode& node) { return nullptr; }
Type* TypeInferenceVisitor::visit(LetBindingNode& node) { return nullptr; }
Type* TypeInferenceVisitor::visit(LetBindingsNode& node) { return nullptr; }