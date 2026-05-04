#include "type_inference_visitor.hpp"

#include <sstream>

namespace {

std::string safeTypeName(Type* type) {
    return type ? type->toString() : std::string("<null>");
}

std::string safeSpanName(const SourceSpan& span) {
    return span.isValid() ? span.toString() : std::string("<unknown>");
}

}  // namespace

void TypeInferenceVisitor::error(
    SemanticPhase phase,
    const ASTNode& node,
    const std::string& message,
    std::vector<std::string> notes) {
    context.error(phase, message, node.span, std::move(notes));
}

void TypeInferenceVisitor::error(
    SemanticPhase phase,
    const SourceSpan& span,
    const std::string& message,
    std::vector<std::string> notes) {
    context.error(phase, message, span, std::move(notes));
}

void TypeInferenceVisitor::traceInference(const std::string& message) {
    context.traceInference(message);
}

Type* TypeInferenceVisitor::coerceUnknown(Type* type) {
    return type ? type : UnknownType::instance();
}

void TypeInferenceVisitor::collectDeclarations(ProgramNode* root) {
    collecting = true;
    collectingDependencies = true;
    currentFunctionName.clear();
    if (root) {
        root->accept(*this);
    }
}

void TypeInferenceVisitor::analyzeFunction(FunctionDeclNode* function) {
    collecting = false;
    collectingDependencies = false;
    if (function) {
        function->accept(*this);
    }
}

void TypeInferenceVisitor::analyzeGlobals(ProgramNode* root) {
    collecting = false;
    collectingDependencies = false;
    if (root) {
        root->accept(*this);
    }
}

Type* TypeInferenceVisitor::visit(ProgramNode& node) {
    if (collecting) {
        for (auto* decl : node.decls) {
            if (decl) {
                decl->accept(*this);
            }
        }
        node.type = VoidType::instance();
        return node.type;
    }

    Type* lastType = VoidType::instance();
    for (auto* stmt : node.stmts) {
        if (stmt) {
            lastType = coerceUnknown(stmt->accept(*this));
        }
    }
    node.type = lastType ? lastType : VoidType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(BlockNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            for (auto* stmt : node.stmts) {
                if (stmt) {
                    stmt->accept(*this);
                }
            }
        }
        return nullptr;
    }

    symTable.enterScope();
    Type* lastType = VoidType::instance();
    for (auto* stmt : node.stmts) {
        if (stmt) {
            lastType = coerceUnknown(stmt->accept(*this));
        }
    }
    symTable.exitScope();

    node.type = lastType ? lastType : VoidType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(FunctionDeclNode& node) {
    if (collecting) {
        Type* initialReturnType = node.hasExplicitReturnType && node.declaredReturnType
            ? node.declaredReturnType
            : UnknownType::instance();
        auto* functionType = new FunctionType(node.paramTypes, initialReturnType);
        node.inferredFunctionType = functionType;
        node.type = functionType;
        node.returnType = initialReturnType;

        SymbolInfo* existing = symTable.lookupCurrent(node.name);
        if (existing && existing->scopeLevel == 0 && existing->kind == SemanticSymbolKind::Function) {
            error(
                SemanticPhase::Declarations,
                node,
                "Cannot redeclare builtin function '" + node.name + "'.");
        } else if (!symTable.insertFunction(node.name, functionType)) {
            error(
                SemanticPhase::Declarations,
                node,
                "Function '" + node.name + "' is already declared with the same signature.");
        }

        depGraph.getOrCreateNode(node.name, DepNodeKind::Function, &node);
        pendingFunctions.push_back(&node);
        traceInference(
            "Collected declaration for '" + node.name + "' as " + functionType->toString() +
            " at " + safeSpanName(node.span));

        if (collectingDependencies) {
            std::string previousFunction = currentFunctionName;
            currentFunctionName = node.name;
            if (node.isInline && node.exprBody) {
                node.exprBody->accept(*this);
            } else if (node.body) {
                node.body->accept(*this);
            }
            currentFunctionName = previousFunction;
        }

        return node.type;
    }

    auto* functionType = node.inferredFunctionType;
    if (!functionType) {
        error(
            SemanticPhase::Inference,
            node,
            "Internal error: missing function type for '" + node.name + "'.");
        node.type = UnknownType::instance();
        return node.type;
    }

    Type* expectedReturnType = node.hasExplicitReturnType && node.declaredReturnType
        ? node.declaredReturnType
        : functionType->getReturnType();
    expectedReturnType = coerceUnknown(expectedReturnType);
    returnTypeStack.push_back(expectedReturnType);

    symTable.enterScope();
    for (size_t i = 0; i < node.params.size(); ++i) {
        symTable.insert(
            node.params[i],
            SymbolInfo(
                node.params[i],
                i < node.paramTypes.size() ? node.paramTypes[i] : UnknownType::instance(),
                SemanticSymbolKind::Parameter,
                symTable.getCurrentLevel()));
    }

    Type* actualReturnType = nullptr;
    if (node.isInline) {
        actualReturnType = node.exprBody ? node.exprBody->accept(*this) : UnknownType::instance();
    } else {
        actualReturnType = node.body ? node.body->accept(*this) : VoidType::instance();
    }
    actualReturnType = coerceUnknown(actualReturnType);

    if (!node.hasExplicitReturnType || expectedReturnType->equals(UnknownType::instance())) {
        functionType->setReturnType(actualReturnType);
        node.returnType = actualReturnType;
        traceInference(
            "Refined function '" + node.name + "' to return " + actualReturnType->toString());
    } else if (!actualReturnType->equals(UnknownType::instance()) &&
               !expectedReturnType->equals(actualReturnType)) {
        error(
            SemanticPhase::Inference,
            node,
            "Return type mismatch in function '" + node.name + "'.",
            {
                "Declared return type: " + expectedReturnType->toString(),
                "Inferred body type: " + actualReturnType->toString()
            });
    }

    node.type = functionType;

    symTable.exitScope();
    returnTypeStack.pop_back();
    return node.type;
}

Type* TypeInferenceVisitor::visit(LetNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.init) {
                node.init->accept(*this);
            }
            if (node.body) {
                node.body->accept(*this);
            }
        }
        return nullptr;
    }

    Type* initType = node.init ? coerceUnknown(node.init->accept(*this)) : UnknownType::instance();

    symTable.enterScope();
    if (!symTable.insert(
            node.name,
            SymbolInfo(node.name, initType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()))) {
        error(
            SemanticPhase::Inference,
            node,
            "Variable '" + node.name + "' is already declared in this scope.");
    }
    traceInference(
        "Let binding '" + node.name + "' inferred as " + safeTypeName(initType) +
        " at " + safeSpanName(node.span));

    Type* bodyType = node.body ? coerceUnknown(node.body->accept(*this)) : VoidType::instance();
    symTable.exitScope();

    node.type = bodyType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(IfNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.condition) node.condition->accept(*this);
            if (node.then_branch) node.then_branch->accept(*this);
            if (node.else_branch) node.else_branch->accept(*this);
        }
        return nullptr;
    }

    Type* conditionType = node.condition ? coerceUnknown(node.condition->accept(*this)) : UnknownType::instance();
    if (!conditionType->equals(BoolType::instance()) &&
        !conditionType->equals(UnknownType::instance())) {
        error(
            SemanticPhase::Inference,
            node.condition ? node.condition->span : node.span,
            "If condition must be boolean.",
            {"Condition type: " + conditionType->toString()});
    }

    Type* thenType = node.then_branch ? coerceUnknown(node.then_branch->accept(*this)) : VoidType::instance();
    if (!node.else_branch) {
        node.type = VoidType::instance();
        return node.type;
    }

    Type* elseType = coerceUnknown(node.else_branch->accept(*this));
    if (!thenType->equals(UnknownType::instance()) &&
        !elseType->equals(UnknownType::instance()) &&
        !thenType->equals(elseType)) {
        error(
            SemanticPhase::Inference,
            node,
            "The 'then' and 'else' branches must have the same type.",
            {
                "Then branch type: " + thenType->toString(),
                "Else branch type: " + elseType->toString()
            });
        node.type = UnknownType::instance();
        return node.type;
    }

    node.type = thenType->equals(UnknownType::instance()) ? elseType : thenType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(FunctionCallNode& node) {
    if (collecting) {
        if (collectingDependencies && !currentFunctionName.empty()) {
            auto* from = depGraph.getOrCreateNode(currentFunctionName, DepNodeKind::Function, nullptr);
            auto* to = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, nullptr);
            depGraph.addDependency(from, to);
        }
        for (auto* arg : node.args) {
            if (arg) {
                arg->accept(*this);
            }
        }
        return nullptr;
    }

    std::vector<Type*> argumentTypes;
    for (auto* arg : node.args) {
        argumentTypes.push_back(arg ? coerceUnknown(arg->accept(*this)) : UnknownType::instance());
    }

    OverloadResolutionResult resolution = symTable.resolveFunction(node.name, argumentTypes);
    node.resolvedFunctionType = resolution.selected;
    node.overloadStatus = resolution.statusString();
    node.overloadNotes = resolution.notesFor(node.name);

    if (resolution.selected) {
        node.type = coerceUnknown(resolution.selected->getReturnType());
        context.traceOverload(
            "Resolved '" + node.name + "' to " + resolution.selected->toString() +
            " at " + safeSpanName(node.span));
        return node.type;
    }

    context.traceOverload(
        "Failed to resolve overload for '" + node.name + "' at " + safeSpanName(node.span) +
        " with status " + resolution.statusString());
    error(
        SemanticPhase::OverloadResolution,
        node,
        resolution.messageFor(node.name),
        node.overloadNotes);
    node.type = UnknownType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(VariableNode& node) {
    if (collecting) {
        return nullptr;
    }

    SymbolInfo* symbol = symTable.lookup(node.name);
    if (!symbol) {
        error(
            SemanticPhase::Inference,
            node,
            "Variable '" + node.name + "' is not declared.");
        node.type = UnknownType::instance();
        return node.type;
    }
    if (!symbol->type) {
        error(
            SemanticPhase::Inference,
            node,
            "Variable '" + node.name + "' has no type in the symbol table.");
        node.type = UnknownType::instance();
        return node.type;
    }

    node.type = symbol->type;
    return node.type;
}

Type* TypeInferenceVisitor::visit(NumberNode& node) {
    if (!collecting) {
        if (!node.isWellFormed()) {
            error(
                SemanticPhase::Inference,
                node,
                "Malformed numeric literal '" + node.value + "'.",
                {"Number kind: " + node.kindName()});
        }
        node.type = NumberType::instance();
        return node.type;
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(BoolNode& node) {
    if (!collecting) {
        node.type = BoolType::instance();
        return node.type;
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(StringNode& node) {
    if (!collecting) {
        node.type = StringType::instance();
        return node.type;
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(BinaryOpNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.left) node.left->accept(*this);
            if (node.right) node.right->accept(*this);
        }
        return nullptr;
    }

    Type* left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
    Type* right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();

    if (left->equals(UnknownType::instance()) || right->equals(UnknownType::instance())) {
        node.type = UnknownType::instance();
        return node.type;
    }

    if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Arithmetic operator '" + node.op + "' requires Number operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = NumberType::instance();
        return node.type;
    }

    if (node.op == "<" || node.op == ">" || node.op == "<=" || node.op == ">=") {
        if (!left->equals(NumberType::instance()) || !right->equals(NumberType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Relational operator '" + node.op + "' requires Number operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    if (node.op == "==") {
        if (!left->equals(right)) {
            error(
                SemanticPhase::Inference,
                node,
                "Operator '==' requires operands of the same type.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    if (node.op == "@") {
        auto is_concat_operand = [](Type* type) {
            return type->equals(StringType::instance()) ||
                   type->equals(NumberType::instance());
        };

        if (!is_concat_operand(left) || !is_concat_operand(right)) {
            error(
                SemanticPhase::Inference,
                node,
                "Operator '@' requires String or Number operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = StringType::instance();
        return node.type;
    }

    error(
        SemanticPhase::Inference,
        node,
        "Unknown binary operator '" + node.op + "'.");
    node.type = UnknownType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(UnaryOpNode& node) {
    if (collecting) {
        if (collectingDependencies && node.operand) {
            node.operand->accept(*this);
        }
        return nullptr;
    }

    Type* operandType = node.operand ? coerceUnknown(node.operand->accept(*this)) : UnknownType::instance();
    if (operandType->equals(UnknownType::instance())) {
        node.type = UnknownType::instance();
        return node.type;
    }

    if (node.op == "-" || node.op == "+") {
        if (!operandType->equals(NumberType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Unary operator '" + node.op + "' requires a Number operand.",
                {"Operand type: " + operandType->toString()});
        }
        node.type = NumberType::instance();
        return node.type;
    }

    if (node.op == "!") {
        if (!operandType->equals(BoolType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Unary operator '!' requires a Boolean operand.",
                {"Operand type: " + operandType->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    error(
        SemanticPhase::Inference,
        node,
        "Unknown unary operator '" + node.op + "'.");
    node.type = UnknownType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(ExprStmtNode& node) {
    if (collecting) {
        if (collectingDependencies && node.expr) {
            node.expr->accept(*this);
        }
        return nullptr;
    }

    node.type = node.expr ? coerceUnknown(node.expr->accept(*this)) : VoidType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(TypeNode& node) {
    return node.type;
}

Type* TypeInferenceVisitor::visit(ParamListNode& node) {
    return nullptr;
}

Type* TypeInferenceVisitor::visit(LetBindingNode& node) {
    if (collecting && collectingDependencies && node.init) {
        node.init->accept(*this);
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(LetBindingsNode& node) {
    if (collecting && collectingDependencies) {
        for (auto* binding : node.bindings) {
            if (binding) {
                binding->accept(*this);
            }
        }
    }
    return nullptr;
}

Type* TypeInferenceVisitor::visit(ReturnNode& node) {
    if (collecting) {
        if (collectingDependencies && node.expr) {
            node.expr->accept(*this);
        }
        return nullptr;
    }

    Type* expressionType = node.expr ? coerceUnknown(node.expr->accept(*this)) : VoidType::instance();
    node.type = expressionType;

    if (returnTypeStack.empty()) {
        error(
            SemanticPhase::Inference,
            node,
            "Return statement is outside of a function.");
        return node.type;
    }

    Type* expected = coerceUnknown(returnTypeStack.back());
    if (!expected->equals(UnknownType::instance()) &&
        !expressionType->equals(UnknownType::instance()) &&
        !expected->equals(expressionType)) {
        error(
            SemanticPhase::Inference,
            node,
            "Return type mismatch.",
            {
                "Expected: " + expected->toString(),
                "Got: " + expressionType->toString()
            });
    }

    return node.type;
}
