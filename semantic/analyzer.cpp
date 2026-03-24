#include "analyzer.hpp"
#include <iostream>

void SemanticAnalyzer::analyze(ProgramNode* root) {
    std::cerr << "SemanticAnalyzer::analyze: entering global scope\n";
    symTable.enterScope();
    std::cerr << "SemanticAnalyzer::analyze: calling root->accept\n";

    if (!root) {
        std::cerr << "SemanticAnalyzer::analyze: root is null!\n";
        return;
    }
    std::cerr << "SemanticAnalyzer::analyze: root = " << root << "\n";
    root->accept(*this);
    std::cerr << "SemanticAnalyzer::analyze: exiting global scope\n";
    symTable.exitScope();
    reportErrors();

}

void SemanticAnalyzer::reportErrors() const {
    for (const auto& e : errors) {
        std::cerr << "Semantic error: " << e << "\n";
    }
    if (errors.empty()) {
        std::cout << "Semantic analysis successful.\n";
    }
}

Type* SemanticAnalyzer::visit(ProgramNode& node) {
    std::cerr << "Visiting ProgramNode\n";

    for (auto* decl : node.decls) decl->accept(*this); 
    for (auto* stmt : node.stmts) stmt->accept(*this); 
    return nullptr;
}

Type* SemanticAnalyzer::visit(BlockNode& node) {
    symTable.enterScope();
    for (auto* stmt : node.stmts) stmt->accept(*this); 
    symTable.exitScope();
    return nullptr;
}

Type* SemanticAnalyzer::visit(FunctionDeclNode& node) {
    if (symTable.lookupCurrent(node.name)) {
        error("Función '" + node.name + "' ya declarada en este ámbito.");
        return nullptr;
    }
    FunctionType* funcType = new FunctionType(node.paramTypes, node.returnType);
    SymbolInfo info{node.name, funcType, SemanticSymbolKind::Function, symTable.getCurrentLevel()};
    if (!symTable.insert(node.name, info)) {
        error("Función '" + node.name + "' no se pudo insertar.");
        delete funcType;
        return nullptr;
    }
    symTable.enterScope();
    for (size_t i = 0; i < node.params.size(); ++i) {
        SymbolInfo paramInfo{node.params[i], node.paramTypes[i], SemanticSymbolKind::Parameter, symTable.getCurrentLevel()};
        if (!symTable.insert(node.params[i], paramInfo)) {
            error("Parámetro duplicado: " + node.params[i]);
        }
    }
    Type* oldReturn = currentFunctionReturnType;
    currentFunctionReturnType = node.returnType;
    node.body->accept(*this);
    currentFunctionReturnType = oldReturn;
    symTable.exitScope();
    return nullptr;
}

Type* SemanticAnalyzer::visit(LetNode& node) {
    Type* initType = node.init->accept(*this);  
    if (!initType) {
        error("No se pudo determinar tipo de inicialización de '" + node.name + "'.");
        return nullptr;
    }
    symTable.enterScope();
    SymbolInfo varInfo{node.name, initType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()};
    if (!symTable.insert(node.name, varInfo)) {
        error("Variable '" + node.name + "' ya declarada en este ámbito.");
    }
    Type* bodyType = node.body->accept(*this);  
    symTable.exitScope();
    return bodyType;
}

Type* SemanticAnalyzer::visit(IfNode& node) {
    Type* condType = node.condition->accept(*this);  
    if (!condType->equals(BoolType::instance())) {
        error("Condición de 'if' debe ser booleana.");
    }
    Type* thenType = node.then_branch->accept(*this);  
    Type* elseType = node.else_branch->accept(*this);  
    if (thenType && elseType && !thenType->equals(elseType)) {
        error("Las ramas 'then' y 'else' deben tener el mismo tipo.");
    }
    return thenType;
}

Type* SemanticAnalyzer::visit(FunctionCallNode& node) {
    const SymbolInfo* sym = symTable.lookup(node.name);
    if (!sym) {
        error("Función '" + node.name + "' no declarada.");
        return nullptr;
    }
    if (sym->kind != SemanticSymbolKind::Function) {
        error("'" + node.name + "' no es una función.");
        return nullptr;
    }
    FunctionType* funcType = dynamic_cast<FunctionType*>(sym->type);
    if (!funcType) {
        error("Tipo incorrecto para función '" + node.name + "'.");
        return nullptr;
    }
    if (node.args.size() != funcType->getParamTypes().size()) {
        error("Número incorrecto de argumentos para '" + node.name +
              "'. Esperaba " + std::to_string(funcType->getParamTypes().size()) +
              ", recibió " + std::to_string(node.args.size()));
        return nullptr;
    }
    for (size_t i = 0; i < node.args.size(); ++i) {
        Type* argType = node.args[i]->accept(*this);  
        if (!argType->equals(funcType->getParamTypes()[i])) {
            error("Argumento " + std::to_string(i+1) + " de '" + node.name +
                  "' esperaba tipo " + funcType->getParamTypes()[i]->toString() +
                  ", pero recibió " + argType->toString());
        }
    }
    return funcType->getReturnType();
}

Type* SemanticAnalyzer::visit(VariableNode& node) {
    const SymbolInfo* sym = symTable.lookup(node.name);
    if (!sym) {
        error("Variable '" + node.name + "' no declarada.");
        return nullptr;
    }
    if (sym->kind != SemanticSymbolKind::Variable && sym->kind != SemanticSymbolKind::Parameter) {
        error("'" + node.name + "' no es una variable.");
        return nullptr;
    }
    return sym->type;
}

Type* SemanticAnalyzer::visit(NumberNode& node) {
    return NumberType::instance();
}

Type* SemanticAnalyzer::visit(StringNode& node) {
    return StringType::instance();
}

Type* SemanticAnalyzer::visit(BoolNode& node) {
    return BoolType::instance();
}


Type* SemanticAnalyzer::visit(BinaryOpNode& node) {
    Type* left = node.left->accept(*this);  
    Type* right = node.right->accept(*this);
    if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
        error("Operadores aritméticos requieren operandos enteros.");
    }
    return NumberType::instance();
}

Type* SemanticAnalyzer::visit(ExprStmtNode& node) {
    node.expr->accept(*this);  
    return nullptr;
}

Type* SemanticAnalyzer::visit(ParamListNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(LetBindingNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(LetBindingsNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(UnaryOpNode& node) {
    node.operand->accept(*this);
    return NumberType::instance();
}

Type* SemanticAnalyzer::visit(TypeNode& node) { return node.type; }