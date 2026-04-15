#include "visitor.hpp"
#include "analyzer.hpp"
#include "dependency_graph.hpp"
#include "type_inference_visitor.hpp"
#include <iostream>


void SemanticAnalyzer::registerBuiltinFunctions() {

    std::cerr << "DEBUG: registerBuiltinFunctions() called\n";
    

    // Ensure theres a global scope 
    if (symTable.getCurrentLevel() == 0 && !symTable.getCurrentLevel())
        symTable.enterScope();

    // Math (Number -> Number)
    auto mathType = new FunctionType({NumberType::instance()}, NumberType::instance());
    symTable.insert("sin", {"sin", mathType, SemanticSymbolKind::Function, 0});
    symTable.insert("cos", {"cos", mathType, SemanticSymbolKind::Function, 0});
    symTable.insert("tan", {"tan", mathType, SemanticSymbolKind::Function, 0});
    symTable.insert("abs", {"abs", mathType, SemanticSymbolKind::Function, 0});
    symTable.insert("sqrt", {"sqrt", mathType, SemanticSymbolKind::Function, 0});

    // input() -> String
    auto inputType = new FunctionType({}, StringType::instance());
    symTable.insert("input", {"input", inputType, SemanticSymbolKind::Function, 0});

    // _concat(String, String) -> String
    auto concatType = new FunctionType({StringType::instance(), StringType::instance()},
                                       StringType::instance());
    symTable.insert("_concat", {"_concat", concatType, SemanticSymbolKind::Function, 0});

    // print (overloads)
    std::vector<FunctionType*> printOverloads;
    // print(Number)
    printOverloads.push_back(new FunctionType({NumberType::instance()}, VoidType::instance()));
    // print(String)
    printOverloads.push_back(new FunctionType({StringType::instance()}, VoidType::instance()));

    SymbolInfo printInfo("print", printOverloads, SemanticSymbolKind::Function, 0);
    symTable.insert("print", printInfo);

    std::cerr << "DEBUG: built-ins registered\n";
}

void SemanticAnalyzer::analyze(ProgramNode* root) {
    registerBuiltinFunctions(); // First populate the symtab wth built-ins

    DependencyGraph graph;
    TypeInferenceVisitor inferencer(symTable, graph);
    inferencer.infer(root);

    if (inferencer.hasErrors()) {
        for (const auto& err : inferencer.getErrors()) {
            error(err);
        }
        // Maybe continue to detect more errors?
    }

    if (!hasErrors()) root->accept(*this);
    
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
    SymbolInfo* existing = symTable.lookup(node.name);
    if (existing && existing->scopeLevel == 0 && existing->kind == SemanticSymbolKind::Function) {
        error("Cannot redeclare built-in function '" + node.name + "'");
        return nullptr;
    }
    
    if (!existing || existing->kind != SemanticSymbolKind::Function) {
        error("Function '" + node.name + "' not found in symbol table");
        return nullptr;
    }

    if (!node.body) {
        error("Function '" + node.name + "' has no body");
        return nullptr;
    }

    // Get the body type
    Type* actualReturnType = node.body->type ? node.body->type : VoidType::instance();
    Type* declaredReturnType = node.returnType ? node.returnType : VoidType::instance();

    if (!declaredReturnType->equals(actualReturnType)) {
        error("Return type mismatch in function '" + node.name + "': declared " +
              declaredReturnType->toString() + " but body returns " +
              actualReturnType->toString());
        return nullptr;
    }

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
    std::vector<Type*> argTypes;
    for (auto* arg : node.args) {
        if (!arg->type) {
            error("Argument has no type");
            return nullptr;
        }
        argTypes.push_back(arg->type);
    }
    FunctionType* funcType = symTable.lookupFunction(node.name, argTypes);
    if (!funcType) {
        error("No matching function for '" + node.name + "'");
        return nullptr;
    }
    node.type = funcType->getReturnType();
    return node.type;
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

Type* SemanticAnalyzer::visit(ReturnNode& node) {
    return nullptr;
}