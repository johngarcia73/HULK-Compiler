#include "visitor.hpp"
#include "analyzer.hpp"
#include <iostream>

void SemanticAnalyzer::analyze(ProgramNode* root) {
    root->accept(*this);
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
    // Verify function is already in the table
    SymbolInfo* info = symTable.lookup(node.name);
    if (!info || info->kind != SemanticSymbolKind::Function) {
        error("Función '" + node.name + "' no encontrada en tabla.");
        return nullptr;
    }
    FunctionType* funcType = dynamic_cast<FunctionType*>(info->type);
    if (!funcType) {
        error("Tipo incorrecto para función '" + node.name + "'.");
        return nullptr;
    }
    // Check params number
    if (node.params.size() != funcType->getParamTypes().size()) {
        error("Declaración de función '" + node.name +
              "' tiene número incorrecto de parámetros.");
        return nullptr;
    }
    // Function scope
    symTable.enterScope();
    // Insert parameters and their already inferred types
    for (size_t i = 0; i < node.params.size(); ++i) {
        SymbolInfo paramInfo;
        paramInfo.name = node.params[i];
        paramInfo.type = funcType->getParamTypes()[i];
        paramInfo.kind = SemanticSymbolKind::Parameter;
        paramInfo.scopeLevel = symTable.getCurrentLevel();
        if (!symTable.insert(node.params[i], paramInfo)) {
            error("Parámetro duplicado: " + node.params[i]);
        }
    }
    // Check body
    node.body->accept(*this);
    symTable.exitScope();
    return nullptr;
}

Type* SemanticAnalyzer::visit(LetNode& node) {
    // New scope for let body
    symTable.enterScope();

    // Get the variable type from initialization (already inferred)
    Type* varType = node.init->type;
    if (!varType) {
        error("Variable '" + node.name + "' no tiene tipo inferido.");
    } else {
        SymbolInfo varInfo{node.name, varType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()};
        if (!symTable.insert(node.name, varInfo)) {
            error("Variable '" + node.name + "' ya declarada en este ámbito.");
        }
    }

    // Check the body
    node.body->accept(*this);

    // Exit body scope
    symTable.exitScope();

    return nullptr;
}

Type* SemanticAnalyzer::visit(IfNode& node) {
    Type* condType = node.condition->type;
    if (!condType->equals(BoolType::instance())) {
        error("If condition must be boolean.");
    }
    node.then_branch->accept(*this);
    node.else_branch->accept(*this);
    return nullptr;
}

Type* SemanticAnalyzer::visit(FunctionCallNode& node) {
    SymbolInfo* funcInfo = symTable.lookup(node.name);
    if (!funcInfo || funcInfo->kind != SemanticSymbolKind::Function) {
        error("Function '" + node.name + "' not declared.");
        return nullptr;
    }
    FunctionType* funcType = dynamic_cast<FunctionType*>(funcInfo->type);
    if (!funcType) {
        error("Wrong type for function '" + node.name + "'.");
        return nullptr;
    }
    if (node.args.size() != funcType->getParamTypes().size()) {
        error("Wrong params numbers for '" + node.name +
              "'. Waited " + std::to_string(funcType->getParamTypes().size()) +
              ", got " + std::to_string(node.args.size()));
        return nullptr;
    }
    for (size_t i = 0; i < node.args.size(); ++i) {
        Type* argType = node.args[i]->type;
        if (!argType->equals(funcType->getParamTypes()[i])) {
            error("Argument " + std::to_string(i+1) + " de '" + node.name +
                  "' Waited type " + funcType->getParamTypes()[i]->toString() +
                  ", but got " + argType->toString());
        }
    }
    return nullptr;
}

Type* SemanticAnalyzer::visit(VariableNode& node) {
    // Variable must b in table (already verified at inference)
    SymbolInfo* sym = symTable.lookup(node.name);
    if (!sym) {
        error("Variable '" + node.name + "' not declared.");
    }
    return nullptr;
}

Type* SemanticAnalyzer::visit(BinaryOpNode& node) {
    // Verify operands have the correct type acoording to operator
    Type* left = node.left->type;
    Type* right = node.right->type;
    if (!left || !right) {
        error("Typeless operand (inference error).");
        return nullptr;
    }
    if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error("arithmetic operadrands require Number operands.");
        }
    } else if (node.op == "<" || node.op == ">" || node.op == "<=" || node.op == ">=") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error("relactional operands require Number operands.");
        }
    } else if (node.op == "==") {
        if (!left->equals(right)) {
            error("Operador '==' requires same type operands.");
        }
    } else if (node.op == "@") {
        if (!left->equals(StringType::instance()) && !right->equals(StringType::instance())) {
            error("Operator '@' requires one String operand.");
        }
    } else {
        error("Binary operator not supported.");
    }
    return nullptr;
}

Type* SemanticAnalyzer::visit(UnaryOpNode& node) {
    Type* operand = node.operand->type;
    if (!operand) {
        error("Typeless operand (inference error).");
        return nullptr;
    }
    if (node.op == "-") {
        if (!operand->equals(NumberType::instance())) {
            error("Unary operator '!' requires Number operands.");
        }
    } else if (node.op == "!") {
        if (!operand->equals(BoolType::instance())) {
            error("Unary operator '!' requires Boolean operands.");
        }
    } else {
        error("Unary operator not supported");
    }
    return nullptr;
}

Type* SemanticAnalyzer::visit(ExprStmtNode& node) {
    node.expr->accept(*this);
    return nullptr;
}

Type* SemanticAnalyzer::visit(TypeNode& node) {
    return nullptr;
}

// Aux nodes
Type* SemanticAnalyzer::visit(NumberNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(BoolNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(StringNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(ParamListNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(LetBindingNode& node) { return nullptr; }
Type* SemanticAnalyzer::visit(LetBindingsNode& node) { return nullptr; }