#include "type_inference_visitor.hpp"

#include <sstream>

namespace {

std::string safeTypeName(Type* type) {
    return type ? type->toString() : std::string("<null>");
}

std::string safeSpanName(const SourceSpan& span) {
    return span.isValid() ? span.toString() : std::string("<unknown>");
}

std::string targetLabel(ASTNode* target) {
    if (!target) {
        return "<invalid-target>";
    }
    if (auto* variable = dynamic_cast<VariableNode*>(target)) {
        return variable->name;
    }
    if (auto* member = dynamic_cast<MemberAccessNode*>(target)) {
        return targetLabel(member->base) + "." + member->member;
    }
    return "<invalid-target>";
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

Type* TypeInferenceVisitor::resolveDeclaredType(
    Type* builtinType,
    const std::string& typeName,
    const ASTNode& node) {
    if (builtinType) {
        return builtinType;
    }
    if (typeName.empty()) {
        return nullptr;
    }

    Type* resolved = typeRegistry.resolveTypeName(typeName);
    if (!resolved) {
        error(
            SemanticPhase::Declarations,
            node,
            "Unknown type '" + typeName + "'.");
        return UnknownType::instance();
    }
    return resolved;
}

bool TypeInferenceVisitor::checkTypeConformance(
    Type* actual,
    Type* expected,
    const ASTNode& node,
    const std::string& message,
    std::vector<std::string> notes) {
    actual = coerceUnknown(actual);
    expected = coerceUnknown(expected);
    if (actual->equals(UnknownType::instance()) || expected->equals(UnknownType::instance())) {
        return true;
    }
    if (typeRegistry.conforms(actual, expected)) {
        return true;
    }
    notes.push_back("Expected: " + expected->toString());
    notes.push_back("Got: " + actual->toString());
    error(SemanticPhase::Inference, node, message, std::move(notes));
    return false;
}

bool TypeInferenceVisitor::hasExplicitParameterType(const FunctionDeclNode& node, size_t index) const {
    return index < node.paramTypeNames.size() && !node.paramTypeNames[index].empty();
}

Type* TypeInferenceVisitor::normalizeConstraintType(Type* type, bool preferGenericNumber) const {
    type = coerceUnknown(type);
    if (preferGenericNumber && isNumberType(type)) {
        return NumberType::instance();
    }
    return type;
}

void TypeInferenceVisitor::refineUnknownExpression(ASTNode* node, Type* inferredType, bool preferGenericNumber) {
    if (!node || !inferredType) {
        return;
    }

    Type* candidate = normalizeConstraintType(inferredType, preferGenericNumber);
    if (auto* variable = dynamic_cast<VariableNode*>(node)) {
        SymbolInfo* symbol = symTable.lookup(variable->name);
        if (!symbol || !symbol->type) {
            return;
        }
        if (symbol->type->equals(UnknownType::instance()) && !candidate->equals(UnknownType::instance())) {
            symbol->type = candidate;
            traceInference(
                "Refined '" + variable->name + "' to " + candidate->toString() +
                " from contextual use at " + safeSpanName(node->span));
        }
    }
}

bool TypeInferenceVisitor::mergeInferredType(
    Type*& current,
    Type* candidate,
    const ASTNode& node,
    const std::string& label) {
    candidate = normalizeConstraintType(candidate);
    current = coerceUnknown(current);
    candidate = coerceUnknown(candidate);

    if (candidate->equals(UnknownType::instance())) {
        return true;
    }
    if (current->equals(UnknownType::instance())) {
        current = candidate;
        return true;
    }
    if (current->equals(candidate)) {
        return true;
    }
    if (typeRegistry.conforms(candidate, current)) {
        current = candidate;
        return true;
    }
    if (typeRegistry.conforms(current, candidate)) {
        return true;
    }

    error(
        SemanticPhase::Inference,
        node,
        "Failed to infer a unique type for " + label + ".",
        {
            "Existing constraint: " + current->toString(),
            "New constraint: " + candidate->toString()
        });
    current = UnknownType::instance();
    return false;
}

void TypeInferenceVisitor::captureFunctionParameterTypes(FunctionDeclNode& node) {
    if (node.paramTypes.size() < node.params.size()) {
        node.paramTypes.resize(node.params.size(), UnknownType::instance());
    }

    for (size_t i = 0; i < node.params.size(); ++i) {
        SymbolInfo* symbol = symTable.lookup(node.params[i]);
        Type* inferred = symbol && symbol->type ? coerceUnknown(symbol->type) : UnknownType::instance();
        node.paramTypes[i] = inferred;
        if (node.inferredFunctionType) {
            node.inferredFunctionType->setParamType(i, inferred);
        }
        if (!hasExplicitParameterType(node, i) && inferred->equals(UnknownType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Could not infer the type of parameter '" + node.params[i] +
                    "' in '" + node.name + "'.",
                {"Add an explicit type annotation for this parameter."});
        }
    }
}

void TypeInferenceVisitor::captureCtorParameterTypes(
    const ASTNode& ownerNode,
    const std::string& ownerLabel,
    const std::vector<std::string>& names,
    const std::vector<std::string>& declaredNames,
    std::vector<Type*>& types) {
    if (types.size() < names.size()) {
        types.resize(names.size(), UnknownType::instance());
    }

    for (size_t i = 0; i < names.size(); ++i) {
        SymbolInfo* symbol = symTable.lookup(names[i]);
        Type* inferred = symbol && symbol->type ? coerceUnknown(symbol->type) : UnknownType::instance();
        mergeInferredType(types[i], inferred, ownerNode, ownerLabel + " parameter '" + names[i] + "'");
    }

    for (size_t i = 0; i < names.size(); ++i) {
        bool hasExplicitType = i < declaredNames.size() && !declaredNames[i].empty();
        if (!hasExplicitType && coerceUnknown(types[i])->equals(UnknownType::instance())) {
            error(
                SemanticPhase::Inference,
                ownerNode,
                "Could not infer the type of " + ownerLabel + " parameter '" + names[i] + "'.",
                {"Add an explicit type annotation for this parameter."});
        }
    }
}

Type* TypeInferenceVisitor::inferAssignmentTargetType(AssignmentNode& node, bool* isSelfTarget) {
    if (isSelfTarget) {
        *isSelfTarget = false;
    }

    if (auto* variable = dynamic_cast<VariableNode*>(node.target)) {
        SymbolInfo* symbol = symTable.lookup(variable->name);
        if (!symbol) {
            error(
                SemanticPhase::Inference,
                node,
                "Cannot assign to undeclared symbol '" + variable->name + "'.");
            return UnknownType::instance();
        }
        if (symbol->kind == SemanticSymbolKind::Function) {
            error(
                SemanticPhase::Inference,
                node,
                "Cannot assign to function '" + variable->name + "'.");
            return coerceUnknown(symbol->type);
        }
        if (symbol->kind == SemanticSymbolKind::Self) {
            if (isSelfTarget) {
                *isSelfTarget = true;
            }
            error(
                SemanticPhase::Inference,
                node,
                "'self' is not a valid assignment target.");
            return coerceUnknown(symbol->type);
        }
        return coerceUnknown(symbol->type);
    }

    if (auto* member = dynamic_cast<MemberAccessNode*>(node.target)) {
        Type* baseType = member->base ? coerceUnknown(member->base->accept(*this)) : UnknownType::instance();
        const RegisteredAttribute* attribute = typeRegistry.findDeclaredAttribute(baseType, member->member);
        if (!attribute) {
            error(
                SemanticPhase::Inference,
                node,
                "Cannot assign to member '" + member->member + "' because it is not a writable attribute of '" +
                    safeTypeName(baseType) + "'.");
            return UnknownType::instance();
        }
        if (!currentMethod || currentOwnerTypeName != attribute->ownerTypeName) {
            error(
                SemanticPhase::Inference,
                node,
                "Attribute '" + member->member + "' is private to type '" + attribute->ownerTypeName + "'.");
        }
        return coerceUnknown(attribute->type);
    }

    error(
        SemanticPhase::Inference,
        node,
        "Invalid assignment target '" + targetLabel(node.target) + "'.");
    return UnknownType::instance();
}

void TypeInferenceVisitor::collectDeclarations(ProgramNode* root) {
    collecting = true;
    collectingDependencies = true;
    currentFunctionName.clear();
    currentOwnerTypeName.clear();
    currentMethod = nullptr;
    pendingFunctions.clear();
    pendingMethods.clear();
    typeRegistry.clear();

    if (!root) {
        return;
    }

    for (auto* decl : root->decls) {
        if (auto* typeDecl = dynamic_cast<TypeDeclNode*>(decl)) {
            typeRegistry.registerType(*typeDecl, context);
        } else if (auto* protocolDecl = dynamic_cast<ProtocolDeclNode*>(decl)) {
            typeRegistry.registerProtocol(*protocolDecl, context);
        }
    }

    root->accept(*this);
    typeRegistry.resolveParentsAndConstructors(context);
}

void TypeInferenceVisitor::analyzeFunction(FunctionDeclNode* function) {
    collecting = false;
    collectingDependencies = false;
    if (function) {
        function->accept(*this);
    }
}

void TypeInferenceVisitor::analyzeTypes() {
    collecting = false;
    collectingDependencies = false;
    for (auto* record : typeRegistry.declaredTypesInOrder()) {
        if (record && record->declaration) {
            record->declaration->accept(*this);
        }
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
        std::vector<Type*> resolvedParamTypes;
        resolvedParamTypes.reserve(node.params.size());
        if (node.paramTypeNames.size() < node.params.size()) {
            node.paramTypeNames.resize(node.params.size());
        }
        if (node.paramTypes.size() < node.params.size()) {
            node.paramTypes.resize(node.params.size(), nullptr);
        }
        for (size_t i = 0; i < node.params.size(); ++i) {
            Type* resolved = resolveDeclaredType(
                i < node.paramTypes.size() ? node.paramTypes[i] : nullptr,
                i < node.paramTypeNames.size() ? node.paramTypeNames[i] : "",
                node);
            resolvedParamTypes.push_back(resolved ? resolved : UnknownType::instance());
        }
        node.paramTypes = resolvedParamTypes;

        Type* initialReturnType = node.hasExplicitReturnType
            ? resolveDeclaredType(node.declaredReturnType, node.declaredReturnTypeName, node)
            : UnknownType::instance();
        node.declaredReturnType = node.hasExplicitReturnType ? initialReturnType : nullptr;
        node.returnType = initialReturnType;

        auto* functionType = new FunctionType(node.paramTypes, initialReturnType);
        node.inferredFunctionType = functionType;
        node.type = functionType;

        if (node.isMethod) {
            typeRegistry.registerMethod(node.ownerTypeName, node, functionType, context);
            pendingMethods.push_back(&node);
            traceInference(
                "Collected method '" + node.ownerTypeName + "." + node.name + "' as " +
                functionType->toString() + " at " + safeSpanName(node.span));
        } else {
            SymbolInfo* existing = symTable.lookupCurrent(node.name);
            if (existing && existing->kind == SemanticSymbolKind::Function) {
                error(
                    SemanticPhase::Declarations,
                    node,
                    "Function '" + node.name + "' is already declared in the current scope.");
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
        }

        if (collectingDependencies) {
            std::string previousFunction = currentFunctionName;
            currentFunctionName = node.isMethod ? node.ownerTypeName + "." + node.name : node.name;
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

    std::string previousOwnerTypeName = currentOwnerTypeName;
    FunctionDeclNode* previousMethod = currentMethod;

    symTable.enterScope();
    if (node.isMethod) {
        currentOwnerTypeName = node.ownerTypeName;
        currentMethod = &node;
        Type* selfType = typeRegistry.resolveTypeName(node.ownerTypeName);
        symTable.insert(
            "self",
            SymbolInfo(
                "self",
                selfType ? selfType : UnknownType::instance(),
                SemanticSymbolKind::Self,
                symTable.getCurrentLevel()));
    }

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
    captureFunctionParameterTypes(node);

    if (!node.hasExplicitReturnType || expectedReturnType->equals(UnknownType::instance())) {
        functionType->setReturnType(actualReturnType);
        node.returnType = actualReturnType;
        traceInference(
            "Refined function '" + node.name + "' to return " + actualReturnType->toString());
        if (actualReturnType->equals(UnknownType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Could not infer the return type of '" + node.name + "'.",
                {"Add an explicit return type annotation or make the body more type-constrained."});
        }
    } else {
        checkTypeConformance(
            actualReturnType,
            expectedReturnType,
            node,
            "Return type mismatch in function '" + node.name + "'.");
        node.returnType = expectedReturnType;
    }

    node.type = functionType;

    symTable.exitScope();
    symTable.exitScope();
    currentOwnerTypeName = previousOwnerTypeName;
    currentMethod = previousMethod;
    returnTypeStack.pop_back();
    return node.type;
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
        !typeRegistry.conforms(expressionType, expected)) {
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

    node.declaredType = resolveDeclaredType(node.declaredType, node.declaredTypeName, node);
    Type* initType = node.init ? coerceUnknown(node.init->accept(*this)) : UnknownType::instance();
    Type* boundType = node.declaredType ? node.declaredType : initType;

    if (node.declaredType) {
        checkTypeConformance(
            initType,
            node.declaredType,
            node,
            "Let binding for '" + node.name + "' does not conform to its declared type.");
    } else if (boundType->equals(UnknownType::instance())) {
        error(
            SemanticPhase::Inference,
            node,
            "Could not infer the type of let binding '" + node.name + "'.",
            {"Add an explicit type annotation for this binding."});
    }

    symTable.enterScope();
    if (!symTable.insert(
            node.name,
            SymbolInfo(node.name, boundType, SemanticSymbolKind::Variable, symTable.getCurrentLevel()))) {
        error(
            SemanticPhase::Inference,
            node,
            "Variable '" + node.name + "' is already declared in this scope.");
    }
    traceInference(
        "Let binding '" + node.name + "' inferred as " + safeTypeName(boundType) +
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

    refineUnknownExpression(node.condition, BoolType::instance(), false);
    refineUnknownExpression(node.condition, BoolType::instance(), false);
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
    Type* joinType = typeRegistry.lowestCommonAncestor(thenType, elseType);
    if (joinType->equals(UnknownType::instance()) &&
        !thenType->equals(UnknownType::instance()) &&
        !elseType->equals(UnknownType::instance())) {
        error(
            SemanticPhase::Inference,
            node,
            "The 'then' and 'else' branches are not type-compatible.",
            {
                "Then branch type: " + thenType->toString(),
                "Else branch type: " + elseType->toString()
            });
    }

    node.type = joinType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(FunctionCallNode& node) {
    if (collecting) {
        if (collectingDependencies && !currentFunctionName.empty() && !node.receiver && node.name != "base") {
            auto* from = depGraph.getOrCreateNode(currentFunctionName, DepNodeKind::Function, nullptr);
            auto* to = depGraph.getOrCreateNode(node.name, DepNodeKind::Function, nullptr);
            depGraph.addDependency(from, to);
        }
        if (node.receiver) {
            node.receiver->accept(*this);
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

    auto validate_function_like = [&](FunctionType* type, const std::string& displayName) -> bool {
        if (!type) {
            error(
                SemanticPhase::Inference,
                node,
                "Callable '" + displayName + "' has no type information.");
            return false;
        }
        const auto& params = type->getParamTypes();
        if (params.size() != argumentTypes.size()) {
            error(
                SemanticPhase::Inference,
                node,
                "Call to '" + displayName + "' has the wrong number of arguments.",
                {
                    "Expected " + std::to_string(params.size()) + " argument(s).",
                    "Got " + std::to_string(argumentTypes.size()) + " argument(s)."
                });
            return false;
        }

        bool ok = true;
        for (size_t i = 0; i < params.size(); ++i) {
            if (!typeRegistry.conforms(argumentTypes[i], params[i])) {
                ok = false;
                error(
                    SemanticPhase::Inference,
                    node,
                    "Argument " + std::to_string(i + 1) + " of '" + displayName + "' does not conform to the parameter type.",
                    {
                        "Expected: " + safeTypeName(params[i]),
                        "Got: " + safeTypeName(argumentTypes[i])
                    });
            }
        }
        return ok;
    };

    if (node.receiver) {
        Type* receiverType = coerceUnknown(node.receiver->accept(*this));
        const RegisteredMethod* method = typeRegistry.findMethod(receiverType, node.name);
        if (!method) {
            error(
                SemanticPhase::Inference,
                node,
                "Type '" + safeTypeName(receiverType) + "' has no method '" + node.name + "'.");
            node.type = UnknownType::instance();
            return node.type;
        }
        node.resolvedFunctionType = method->type;
        node.overloadStatus = "resolved";
        node.overloadNotes = {"Selected method: " + method->ownerTypeName + "." + method->type->toString()};
        const auto& params = method->type->getParamTypes();
        for (size_t i = 0; i < params.size() && i < node.args.size(); ++i) {
            refineUnknownExpression(node.args[i], params[i]);
            argumentTypes[i] = node.args[i] ? coerceUnknown(node.args[i]->accept(*this)) : UnknownType::instance();
        }
        validate_function_like(method->type, method->ownerTypeName + "." + node.name);
        node.type = coerceUnknown(method->type->getReturnType());
        return node.type;
    }

    if (node.name == "base") {
        if (!currentMethod) {
            error(
                SemanticPhase::Inference,
                node,
                "'base()' can only be used inside a method.");
            node.type = UnknownType::instance();
            return node.type;
        }
        const RegisteredMethod* baseMethod = typeRegistry.findBaseMethod(currentOwnerTypeName, currentMethod->name);
        if (!baseMethod) {
            error(
                SemanticPhase::Inference,
                node,
                "Method '" + currentMethod->name + "' has no base implementation.");
            node.type = UnknownType::instance();
            return node.type;
        }

        node.resolvedFunctionType = baseMethod->type;
        node.overloadStatus = "resolved";
        node.overloadNotes = {"Selected base method: " + baseMethod->ownerTypeName + "." + baseMethod->type->toString()};
        const auto& params = baseMethod->type->getParamTypes();
        for (size_t i = 0; i < params.size() && i < node.args.size(); ++i) {
            refineUnknownExpression(node.args[i], params[i]);
            argumentTypes[i] = node.args[i] ? coerceUnknown(node.args[i]->accept(*this)) : UnknownType::instance();
        }
        validate_function_like(baseMethod->type, "base");
        node.type = coerceUnknown(baseMethod->type->getReturnType());
        return node.type;
    }

    OverloadResolutionResult resolution = symTable.resolveFunction(
        node.name,
        argumentTypes,
        [&](Type* actual, Type* expected) { return typeRegistry.conforms(actual, expected); });
    node.resolvedFunctionType = resolution.selected;
    node.overloadStatus = resolution.statusString();
    node.overloadNotes = resolution.notesFor(node.name);

    if (resolution.selected) {
        const auto& params = resolution.selected->getParamTypes();
        for (size_t i = 0; i < params.size() && i < node.args.size(); ++i) {
            refineUnknownExpression(node.args[i], params[i]);
        }
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
        node.type = NumberType::instance(node.numberKind);
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

    if (node.op == "is" || node.op == "as") {
        Type* left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
        auto* rightTypeNode = dynamic_cast<TypeNode*>(node.right);
        if (!rightTypeNode) {
            error(
                SemanticPhase::Inference,
                node,
                "Operator '" + node.op + "' expects a type on the right-hand side.");
            node.type = UnknownType::instance();
            return node.type;
        }

        rightTypeNode->type = resolveDeclaredType(rightTypeNode->type, rightTypeNode->typeName, *rightTypeNode);
        if (node.op == "is") {
            if (left->equals(VoidType::instance())) {
                error(
                    SemanticPhase::Inference,
                    node,
                    "Operator 'is' cannot be applied to Void.");
            }
            node.type = BoolType::instance();
            return node.type;
        }

        Type* targetType = coerceUnknown(rightTypeNode->type);
        if (!left->equals(UnknownType::instance()) &&
            !targetType->equals(UnknownType::instance()) &&
            !typeRegistry.conforms(left, targetType) &&
            !typeRegistry.conforms(targetType, left)) {
            error(
                SemanticPhase::Inference,
                node,
                "Invalid downcast with 'as'.",
                {
                    "Source type: " + left->toString(),
                    "Target type: " + targetType->toString()
                });
        }
        node.type = targetType;
        return node.type;
    }

    Type* left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
    Type* right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();

    if (node.op == "+" || node.op == "-" || node.op == "*" || node.op == "/" || node.op == "%") {
        refineUnknownExpression(node.left, NumberType::instance());
        refineUnknownExpression(node.right, NumberType::instance());
        left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
        right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();
        if (left->equals(UnknownType::instance()) || right->equals(UnknownType::instance())) {
            node.type = UnknownType::instance();
            return node.type;
        }
        if (!isNumberType(left) || !isNumberType(right)) {
            error(
                SemanticPhase::Inference,
                node,
                "Arithmetic operator '" + node.op + "' requires Number operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
            node.type = NumberType::instance();
            return node.type;
        }
        node.type = commonNumberType(left, right);
        return node.type;
    }

    if (node.op == "<" || node.op == ">" || node.op == "<=" || node.op == ">=") {
        refineUnknownExpression(node.left, NumberType::instance());
        refineUnknownExpression(node.right, NumberType::instance());
        left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
        right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();
        if (!isNumberType(left) || !isNumberType(right)) {
            error(
                SemanticPhase::Inference,
                node,
                "Relational operator '" + node.op + "' requires Number operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    if (node.op == "==" || node.op == "!=") {
        if (left->equals(UnknownType::instance()) && !right->equals(UnknownType::instance())) {
            refineUnknownExpression(node.left, right, isNumberType(right));
            left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
        } else if (right->equals(UnknownType::instance()) && !left->equals(UnknownType::instance())) {
            refineUnknownExpression(node.right, left, isNumberType(left));
            right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();
        }
        if (!(isNumberType(left) && isNumberType(right)) &&
            !typeRegistry.conforms(left, right) &&
            !typeRegistry.conforms(right, left)) {
            error(
                SemanticPhase::Inference,
                node,
                "Operator '" + node.op + "' requires comparable operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    if (node.op == "&" || node.op == "|") {
        refineUnknownExpression(node.left, BoolType::instance(), false);
        refineUnknownExpression(node.right, BoolType::instance(), false);
        left = node.left ? coerceUnknown(node.left->accept(*this)) : UnknownType::instance();
        right = node.right ? coerceUnknown(node.right->accept(*this)) : UnknownType::instance();
        if (!left->equals(BoolType::instance()) || !right->equals(BoolType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Boolean operator '" + node.op + "' requires Bool operands.",
                {"Left operand: " + left->toString(), "Right operand: " + right->toString()});
        }
        node.type = BoolType::instance();
        return node.type;
    }

    if (node.op == "@") {
        auto is_concat_operand = [](Type* type) {
            return type->equals(StringType::instance()) || isNumberType(type);
        };

        if (left->equals(UnknownType::instance()) || right->equals(UnknownType::instance())) {
            node.type = UnknownType::instance();
            return node.type;
        }

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

    if (node.op == "-" || node.op == "+") {
        refineUnknownExpression(node.operand, NumberType::instance());
        operandType = node.operand ? coerceUnknown(node.operand->accept(*this)) : UnknownType::instance();
        if (operandType->equals(UnknownType::instance())) {
            node.type = UnknownType::instance();
            return node.type;
        }
        if (!isNumberType(operandType)) {
            error(
                SemanticPhase::Inference,
                node,
                "Unary operator '" + node.op + "' requires a Number operand.",
                {"Operand type: " + operandType->toString()});
            node.type = NumberType::instance();
            return node.type;
        }
        node.type = operandType;
        return node.type;
    }

    if (node.op == "!") {
        refineUnknownExpression(node.operand, BoolType::instance(), false);
        operandType = node.operand ? coerceUnknown(node.operand->accept(*this)) : UnknownType::instance();
        if (operandType->equals(UnknownType::instance())) {
            node.type = UnknownType::instance();
            return node.type;
        }
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

Type* TypeInferenceVisitor::visit(AssignmentNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.target) {
                node.target->accept(*this);
            }
            if (node.value) {
                node.value->accept(*this);
            }
        }
        return nullptr;
    }

    Type* valueType = node.value ? coerceUnknown(node.value->accept(*this)) : UnknownType::instance();
    bool isSelfTarget = false;
    Type* targetType = inferAssignmentTargetType(node, &isSelfTarget);

    if (isSelfTarget) {
        node.type = valueType;
        return node.type;
    }

    if (!targetType->equals(UnknownType::instance()) &&
        !valueType->equals(UnknownType::instance()) &&
        !typeRegistry.conforms(valueType, targetType)) {
        error(
            SemanticPhase::Inference,
            node,
            "Assignment type mismatch for '" + targetLabel(node.target) + "'.",
            {
                "Target type: " + targetType->toString(),
                "Assigned value type: " + valueType->toString()
            });
    }

    if (auto* variable = dynamic_cast<VariableNode*>(node.target)) {
        SymbolInfo* symbol = symTable.lookup(variable->name);
        if (symbol && symbol->type &&
            symbol->type->equals(UnknownType::instance()) &&
            !valueType->equals(UnknownType::instance())) {
            symbol->type = valueType;
            traceInference(
                "Assignment refined '" + variable->name + "' to " + valueType->toString() +
                " at " + safeSpanName(node.span));
        }
    }

    node.type = valueType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(MemberAccessNode& node) {
    if (collecting) {
        if (collectingDependencies && node.base) {
            node.base->accept(*this);
        }
        return nullptr;
    }

    Type* baseType = node.base ? coerceUnknown(node.base->accept(*this)) : UnknownType::instance();
    const RegisteredAttribute* attribute = typeRegistry.findDeclaredAttribute(baseType, node.member);
    if (attribute) {
        if (!currentMethod || currentOwnerTypeName != attribute->ownerTypeName) {
            error(
                SemanticPhase::Inference,
                node,
                "Attribute '" + node.member + "' is private to type '" + attribute->ownerTypeName + "'.");
        }
        node.type = coerceUnknown(attribute->type);
        return node.type;
    }

    const RegisteredMethod* method = typeRegistry.findMethod(baseType, node.member);
    if (method) {
        node.type = method->type;
        return node.type;
    }

    error(
        SemanticPhase::Inference,
        node,
        "Type '" + safeTypeName(baseType) + "' has no member '" + node.member + "'.");
    node.type = UnknownType::instance();
    return node.type;
}

Type* TypeInferenceVisitor::visit(NewNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            for (auto* arg : node.args) {
                if (arg) {
                    arg->accept(*this);
                }
            }
        }
        return nullptr;
    }

    if (node.typeName == "Number" || node.typeName == "String" || node.typeName == "Bool" || node.typeName == "Boolean") {
        error(
            SemanticPhase::Inference,
            node,
            "Builtin type '" + node.typeName + "' cannot be instantiated with 'new'.");
        node.type = UnknownType::instance();
        return node.type;
    }

    Type* instanceType = typeRegistry.resolveTypeName(node.typeName);
    if (!instanceType) {
        error(
            SemanticPhase::Inference,
            node,
            "Unknown type '" + node.typeName + "' in constructor call.");
        node.type = UnknownType::instance();
        return node.type;
    }
    if (isProtocolType(instanceType)) {
        error(
            SemanticPhase::Inference,
            node,
            "Protocol '" + node.typeName + "' cannot be instantiated with 'new'.");
        node.type = UnknownType::instance();
        return node.type;
    }

    const std::vector<Type*>* expectedTypes = typeRegistry.constructorParamTypes(instanceType);
    const std::vector<std::string>* expectedNames = typeRegistry.constructorParamNames(instanceType);
    std::vector<Type*> actualTypes;
    for (auto* arg : node.args) {
        actualTypes.push_back(arg ? coerceUnknown(arg->accept(*this)) : UnknownType::instance());
    }

    size_t expectedCount = expectedTypes ? expectedTypes->size() : 0;
    if (actualTypes.size() != expectedCount) {
        error(
            SemanticPhase::Inference,
            node,
            "Constructor for type '" + node.typeName + "' expects " + std::to_string(expectedCount) +
                " argument(s), got " + std::to_string(actualTypes.size()) + ".",
            expectedNames ? std::vector<std::string>{} : std::vector<std::string>{});
    } else if (expectedTypes) {
        for (size_t i = 0; i < expectedTypes->size(); ++i) {
            refineUnknownExpression(node.args[i], (*expectedTypes)[i]);
            actualTypes[i] = node.args[i] ? coerceUnknown(node.args[i]->accept(*this)) : UnknownType::instance();
            if (!typeRegistry.conforms(actualTypes[i], (*expectedTypes)[i])) {
                error(
                    SemanticPhase::Inference,
                    node,
                    "Constructor argument " + std::to_string(i + 1) + " does not conform to the parameter type.",
                    {
                        "Expected: " + safeTypeName((*expectedTypes)[i]),
                        "Got: " + safeTypeName(actualTypes[i])
                    });
            }
        }
    }

    node.type = instanceType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(WhileNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.condition) node.condition->accept(*this);
            if (node.body) node.body->accept(*this);
        }
        return nullptr;
    }

    Type* conditionType = node.condition ? coerceUnknown(node.condition->accept(*this)) : UnknownType::instance();
    if (!conditionType->equals(BoolType::instance()) &&
        !conditionType->equals(UnknownType::instance())) {
        error(
            SemanticPhase::Inference,
            node.condition ? node.condition->span : node.span,
            "While condition must be boolean.",
            {"Condition type: " + conditionType->toString()});
    }

    Type* bodyType = node.body ? coerceUnknown(node.body->accept(*this)) : VoidType::instance();
    node.type = bodyType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(ForNode& node) {
    if (collecting) {
        if (collectingDependencies) {
            if (node.iterable) {
                node.iterable->accept(*this);
            }
            if (node.body) {
                node.body->accept(*this);
            }
        }
        return nullptr;
    }

    Type* iterableType = node.iterable ? coerceUnknown(node.iterable->accept(*this)) : UnknownType::instance();
    if (iterableType->equals(VoidType::instance())) {
        error(
            SemanticPhase::Inference,
            node.iterable ? node.iterable->span : node.span,
            "For loop iterable cannot be Void.");
    }

    Type* iteratorType = UnknownType::instance();
    if (auto* call = dynamic_cast<FunctionCallNode*>(node.iterable)) {
        if (call->name == "range") {
            iteratorType = NumberType::instance();
        }
    }
    if (iteratorType->equals(UnknownType::instance())) {
        if (const RegisteredMethod* currentMethodInfo = typeRegistry.findMethod(iterableType, "current")) {
            iteratorType = currentMethodInfo->type ? currentMethodInfo->type->getReturnType() : UnknownType::instance();
        }
    }

    symTable.enterScope();
    if (!symTable.insert(
            node.iterator,
            SymbolInfo(
                node.iterator,
                iteratorType,
                SemanticSymbolKind::Variable,
                symTable.getCurrentLevel()))) {
        error(
            SemanticPhase::Inference,
            node,
            "Loop variable '" + node.iterator + "' is already declared in this scope.");
    }

    traceInference(
        "For iterator '" + node.iterator + "' inferred as " + safeTypeName(iteratorType) +
        " at " + safeSpanName(node.span));

    Type* bodyType = node.body ? coerceUnknown(node.body->accept(*this)) : VoidType::instance();
    symTable.exitScope();

    node.type = bodyType;
    return node.type;
}

Type* TypeInferenceVisitor::visit(AttributeDeclNode& node) {
    if (collecting) {
        if (collectingDependencies && node.init) {
            node.init->accept(*this);
        }
        return nullptr;
    }

    node.declaredType = resolveDeclaredType(node.declaredType, node.declaredTypeName, node);
    Type* initType = node.init ? coerceUnknown(node.init->accept(*this)) : UnknownType::instance();
    if (node.declaredType) {
        checkTypeConformance(
            initType,
            node.declaredType,
            node,
            "Attribute '" + node.name + "' does not conform to its declared type.");
        node.type = node.declaredType;
    } else {
        node.type = initType;
        if (node.type->equals(UnknownType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Could not infer the type of attribute '" + node.name + "'.",
                {"Add an explicit type annotation for this attribute."});
        }
    }
    return node.type;
}

Type* TypeInferenceVisitor::visit(TypeDeclNode& node) {
    node.type = resolveDeclaredType(node.type, node.name, node);

    if (collecting) {
        for (auto* member : node.members) {
            if (auto* attribute = dynamic_cast<AttributeDeclNode*>(member)) {
                attribute->declaredType = resolveDeclaredType(attribute->declaredType, attribute->declaredTypeName, *attribute);
                typeRegistry.registerAttribute(
                    node.name,
                    *attribute,
                    attribute->declaredType,
                    attribute->declaredTypeName,
                    context);
            } else if (auto* method = dynamic_cast<FunctionDeclNode*>(member)) {
                method->ownerTypeName = node.name;
                method->isMethod = true;
                method->accept(*this);
            }
        }
        return node.type;
    }

    RegisteredType* record = typeRegistry.findType(node.type);
    if (!record) {
        error(
            SemanticPhase::Inference,
            node,
            "Internal error: missing type record for '" + node.name + "'.");
        return node.type ? node.type : UnknownType::instance();
    }

    // Validate explicit parent constructor arguments in the child's constructor scope.
    if (record->hasExplicitParentArgs) {
        symTable.enterScope();
        for (size_t i = 0; i < record->ctorParamNames.size(); ++i) {
            symTable.insert(
                record->ctorParamNames[i],
                SymbolInfo(
                    record->ctorParamNames[i],
                    i < record->ctorParamTypes.size() && record->ctorParamTypes[i]
                        ? record->ctorParamTypes[i]
                        : UnknownType::instance(),
                    SemanticSymbolKind::Parameter,
                    symTable.getCurrentLevel()));
        }

        const std::vector<Type*>* parentParamTypes = typeRegistry.constructorParamTypes(record->parent);
        if (record->parentArgs.size() != (parentParamTypes ? parentParamTypes->size() : 0)) {
            error(
                SemanticPhase::Inference,
                node,
                "Parent constructor invocation for type '" + node.name + "' has the wrong arity.",
                {
                    "Expected: " + std::to_string(parentParamTypes ? parentParamTypes->size() : 0),
                    "Got: " + std::to_string(record->parentArgs.size())
                });
        } else if (parentParamTypes) {
            for (size_t i = 0; i < record->parentArgs.size(); ++i) {
                refineUnknownExpression(record->parentArgs[i], (*parentParamTypes)[i]);
                Type* argType = record->parentArgs[i]
                    ? coerceUnknown(record->parentArgs[i]->accept(*this))
                    : UnknownType::instance();
                if (!typeRegistry.conforms(argType, (*parentParamTypes)[i])) {
                    error(
                        SemanticPhase::Inference,
                        node,
                        "Parent constructor argument " + std::to_string(i + 1) + " does not conform to the expected type.",
                        {
                            "Expected: " + safeTypeName((*parentParamTypes)[i]),
                            "Got: " + safeTypeName(argType)
                        });
                }
            }
        }
        captureCtorParameterTypes(
            node,
            "type '" + node.name + "' constructor",
            record->ctorParamNames,
            record->ctorParamTypeNames,
            record->ctorParamTypes);
        symTable.exitScope();
    }

    // Analyze attributes in a scope with only ctor parameters plus globals.
    for (auto* member : node.members) {
        auto* attribute = dynamic_cast<AttributeDeclNode*>(member);
        if (!attribute) {
            continue;
        }

        symTable.enterScope();
        for (size_t i = 0; i < record->ctorParamNames.size(); ++i) {
            symTable.insert(
                record->ctorParamNames[i],
                SymbolInfo(
                    record->ctorParamNames[i],
                    i < record->ctorParamTypes.size() && record->ctorParamTypes[i]
                        ? record->ctorParamTypes[i]
                        : UnknownType::instance(),
                    SemanticSymbolKind::Parameter,
                    symTable.getCurrentLevel()));
        }
        attribute->accept(*this);
        record->attributes[attribute->name].type = attribute->type;
        record->attributes[attribute->name].declaredTypeName = attribute->declaredTypeName;
        captureCtorParameterTypes(
            node,
            "type '" + node.name + "' constructor",
            record->ctorParamNames,
            record->ctorParamTypeNames,
            record->ctorParamTypes);
        symTable.exitScope();
    }

    node.ctorParamTypes = record->ctorParamTypes;
    for (size_t i = 0; i < node.ctorParams.size(); ++i) {
        bool hasExplicitType = i < node.ctorParamTypeNames.size() && !node.ctorParamTypeNames[i].empty();
        Type* inferred = i < node.ctorParamTypes.size() ? coerceUnknown(node.ctorParamTypes[i]) : UnknownType::instance();
        if (!hasExplicitType && inferred->equals(UnknownType::instance())) {
            error(
                SemanticPhase::Inference,
                node,
                "Could not infer the type of type parameter '" + node.ctorParams[i] +
                    "' in '" + node.name + "'.",
                {"Add an explicit type annotation for this type parameter."});
        }
    }

    // Analyze methods after attributes are known.
    for (auto* member : node.members) {
        if (auto* method = dynamic_cast<FunctionDeclNode*>(member)) {
            analyzeFunction(method);
        }
    }

    return node.type;
}

Type* TypeInferenceVisitor::visit(ProtocolDeclNode& node) {
    node.type = resolveDeclaredType(node.type, node.name, node);

    if (!collecting) {
        return node.type ? node.type : UnknownType::instance();
    }

    RegisteredProtocol* record = typeRegistry.findProtocol(node.type);
    if (!record) {
        error(
            SemanticPhase::Declarations,
            node,
            "Internal error: missing protocol record for '" + node.name + "'.");
        return UnknownType::instance();
    }

    for (auto* methodNode : node.methods) {
        auto* method = dynamic_cast<FunctionDeclNode*>(methodNode);
        if (!method) {
            continue;
        }

        if (!method->hasExplicitReturnType) {
            error(
                SemanticPhase::Declarations,
                *method,
                "Protocol method '" + method->name + "' must declare an explicit return type.");
        }

        if (method->paramTypes.size() < method->params.size()) {
            method->paramTypes.resize(method->params.size(), nullptr);
        }
        if (method->paramTypeNames.size() < method->params.size()) {
            method->paramTypeNames.resize(method->params.size());
        }

        std::vector<Type*> resolvedParamTypes;
        resolvedParamTypes.reserve(method->params.size());
        for (size_t i = 0; i < method->params.size(); ++i) {
            if (!hasExplicitParameterType(*method, i)) {
                error(
                    SemanticPhase::Declarations,
                    *method,
                    "Protocol method '" + method->name + "' must fully type parameter '" +
                        method->params[i] + "'.");
            }
            Type* resolved = resolveDeclaredType(
                i < method->paramTypes.size() ? method->paramTypes[i] : nullptr,
                i < method->paramTypeNames.size() ? method->paramTypeNames[i] : "",
                *method);
            resolvedParamTypes.push_back(coerceUnknown(resolved));
        }

        method->paramTypes = resolvedParamTypes;
        method->declaredReturnType = resolveDeclaredType(
            method->declaredReturnType,
            method->declaredReturnTypeName,
            *method);
        method->returnType = coerceUnknown(method->declaredReturnType);
        method->inferredFunctionType = new FunctionType(method->paramTypes, method->returnType);
        method->type = method->inferredFunctionType;
        method->isProtocolMethod = true;
        method->isSignatureOnly = true;
        method->ownerProtocolName = node.name;

        typeRegistry.registerProtocolMethod(
            node.name,
            *method,
            method->inferredFunctionType,
            context);
    }

    return node.type;
}

Type* TypeInferenceVisitor::visit(TypeNode& node) {
    node.type = resolveDeclaredType(node.type, node.typeName, node);
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
