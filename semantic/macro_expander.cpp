#include "macro_expander.hpp"

#include <utility>

namespace {

template <typename NodeT>
NodeT* copy_with_span(NodeT* node, const SourceSpan& span) {
    if (node) {
        node->span = span;
    }
    return node;
}

}  // namespace

bool MacroExpander::expandProgram(ProgramNode& program) {
    collectMacros(program);
    if (context_.hasErrors()) {
        return false;
    }

    if (!macros_.empty()) {
        context_.tracePipeline(
            "Expanding " + std::to_string(macros_.size()) + " macro declaration(s).");
    }

    std::vector<MacroDeclNode*> macroNodes;
    std::vector<ASTNode*> decls;
    decls.reserve(program.decls.size());
    for (auto* decl : program.decls) {
        if (auto* macro = dynamic_cast<MacroDeclNode*>(decl)) {
            macroNodes.push_back(macro);
            continue;
        }
        if (ASTNode* expanded = expandNode(decl)) {
            decls.push_back(expanded);
        }
    }

    std::vector<ASTNode*> stmts;
    stmts.reserve(program.stmts.size());
    for (auto* stmt : program.stmts) {
        if (ASTNode* expanded = expandNode(stmt)) {
            stmts.push_back(expanded);
        }
    }

    program.decls = std::move(decls);
    program.stmts = std::move(stmts);

    for (auto* macro : macroNodes) {
        delete macro;
    }
    macros_.clear();
    return !context_.hasErrors();
}

void MacroExpander::collectMacros(ProgramNode& program) {
    macros_.clear();
    for (auto* decl : program.decls) {
        auto* macro = dynamic_cast<MacroDeclNode*>(decl);
        if (!macro) {
            continue;
        }
        auto [it, inserted] = macros_.emplace(macro->name, macro);
        if (!inserted) {
            context_.error(
                SemanticPhase::MacroExpansion,
                "Duplicate macro declaration for '" + macro->name + "'.",
                macro->span);
            continue;
        }
        context_.tracePipeline("Registered macro '" + macro->name + "'.");
    }
}

ASTNode* MacroExpander::expandNode(ASTNode* node) {
    if (!node) {
        return nullptr;
    }

    if (auto* program = dynamic_cast<ProgramNode*>(node)) {
        expandProgram(*program);
        return program;
    }
    if (auto* block = dynamic_cast<BlockNode*>(node)) {
        for (auto*& stmt : block->stmts) {
            stmt = expandNode(stmt);
        }
        return block;
    }
    if (auto* exprStmt = dynamic_cast<ExprStmtNode*>(node)) {
        exprStmt->expr = expandNode(exprStmt->expr);
        return exprStmt;
    }
    if (auto* letNode = dynamic_cast<LetNode*>(node)) {
        letNode->init = expandNode(letNode->init);
        letNode->body = expandNode(letNode->body);
        return letNode;
    }
    if (auto* ifNode = dynamic_cast<IfNode*>(node)) {
        ifNode->condition = expandNode(ifNode->condition);
        ifNode->then_branch = expandNode(ifNode->then_branch);
        ifNode->else_branch = expandNode(ifNode->else_branch);
        return ifNode;
    }
    if (auto* binary = dynamic_cast<BinaryOpNode*>(node)) {
        binary->left = expandNode(binary->left);
        binary->right = expandNode(binary->right);
        return binary;
    }
    if (auto* unary = dynamic_cast<UnaryOpNode*>(node)) {
        unary->operand = expandNode(unary->operand);
        return unary;
    }
    if (auto* assignment = dynamic_cast<AssignmentNode*>(node)) {
        assignment->target = expandNode(assignment->target);
        assignment->value = expandNode(assignment->value);
        return assignment;
    }
    if (auto* member = dynamic_cast<MemberAccessNode*>(node)) {
        member->base = expandNode(member->base);
        return member;
    }
    if (auto* newNode = dynamic_cast<NewNode*>(node)) {
        for (auto*& arg : newNode->args) {
            arg = expandNode(arg);
        }
        return newNode;
    }
    if (auto* whileNode = dynamic_cast<WhileNode*>(node)) {
        whileNode->condition = expandNode(whileNode->condition);
        whileNode->body = expandNode(whileNode->body);
        return whileNode;
    }
    if (auto* forNode = dynamic_cast<ForNode*>(node)) {
        forNode->iterable = expandNode(forNode->iterable);
        forNode->body = expandNode(forNode->body);
        return forNode;
    }
    if (auto* attr = dynamic_cast<AttributeDeclNode*>(node)) {
        attr->init = expandNode(attr->init);
        return attr;
    }
    if (auto* typeDecl = dynamic_cast<TypeDeclNode*>(node)) {
        for (auto*& parentArg : typeDecl->parentArgs) {
            parentArg = expandNode(parentArg);
        }
        for (auto*& memberNode : typeDecl->members) {
            memberNode = expandNode(memberNode);
        }
        return typeDecl;
    }
    if (auto* protocolDecl = dynamic_cast<ProtocolDeclNode*>(node)) {
        for (auto*& method : protocolDecl->methods) {
            method = expandNode(method);
        }
        return protocolDecl;
    }
    if (auto* function = dynamic_cast<FunctionDeclNode*>(node)) {
        function->body = expandNode(function->body);
        function->exprBody = expandNode(function->exprBody);
        return function;
    }
    if (auto* lambda = dynamic_cast<LambdaNode*>(node)) {
        lambda->body = expandNode(lambda->body);
        return lambda;
    }
    if (auto* vectorLiteral = dynamic_cast<VectorLiteralNode*>(node)) {
        for (auto*& element : vectorLiteral->elements) {
            element = expandNode(element);
        }
        return vectorLiteral;
    }
    if (auto* vectorComp = dynamic_cast<VectorComprehensionNode*>(node)) {
        vectorComp->expression = expandNode(vectorComp->expression);
        vectorComp->iterable = expandNode(vectorComp->iterable);
        return vectorComp;
    }
    if (auto* index = dynamic_cast<IndexAccessNode*>(node)) {
        index->base = expandNode(index->base);
        index->index = expandNode(index->index);
        return index;
    }
    if (auto* ret = dynamic_cast<ReturnNode*>(node)) {
        ret->expr = expandNode(ret->expr);
        return ret;
    }
    if (auto* functionCall = dynamic_cast<FunctionCallNode*>(node)) {
        functionCall->receiver = expandNode(functionCall->receiver);
        if (!functionCall->receiver && macros_.count(functionCall->name)) {
            for (auto*& arg : functionCall->args) {
                if (auto* symbolic = dynamic_cast<MacroSymbolRefNode*>(arg)) {
                    symbolic->target = expandNode(symbolic->target);
                } else {
                    arg = expandNode(arg);
                }
            }
            ASTNode* expanded =
                expandMacroCall(functionCall->name, functionCall->args, nullptr, functionCall->span);
            delete functionCall;
            return expanded;
        }
        for (auto*& arg : functionCall->args) {
            arg = expandNode(arg);
        }
        for (auto* arg : functionCall->args) {
            if (dynamic_cast<MacroSymbolRefNode*>(arg)) {
                context_.error(
                    SemanticPhase::MacroExpansion,
                    "Symbolic arguments can only be passed to macros.",
                    arg->span);
            }
        }
        return functionCall;
    }
    if (auto* macroCall = dynamic_cast<MacroCallNode*>(node)) {
        for (auto*& arg : macroCall->args) {
            if (auto* symbolic = dynamic_cast<MacroSymbolRefNode*>(arg)) {
                symbolic->target = expandNode(symbolic->target);
            } else {
                arg = expandNode(arg);
            }
        }
        macroCall->bodyArg = expandNode(macroCall->bodyArg);
        ASTNode* expanded =
            expandMacroCall(macroCall->name, macroCall->args, macroCall->bodyArg, macroCall->span);
        delete macroCall;
        return expanded;
    }
    if (auto* symbolRef = dynamic_cast<MacroSymbolRefNode*>(node)) {
        symbolRef->target = expandNode(symbolRef->target);
        return symbolRef;
    }
    if (auto* matchNode = dynamic_cast<MatchNode*>(node)) {
        matchNode->subject = expandNode(matchNode->subject);
        for (auto*& matchCase : matchNode->cases) {
            matchCase = dynamic_cast<MatchCaseNode*>(expandNode(matchCase));
        }
        matchNode->defaultExpr = expandNode(matchNode->defaultExpr);
        context_.error(
            SemanticPhase::MacroExpansion,
            "Compile-time match expression can only appear inside a macro expansion.",
            matchNode->span);
        return matchNode;
    }
    if (auto* matchCase = dynamic_cast<MatchCaseNode*>(node)) {
        matchCase->result = expandNode(matchCase->result);
        return matchCase;
    }
    if (dynamic_cast<MacroDeclNode*>(node) ||
        dynamic_cast<MacroParamNode*>(node) ||
        dynamic_cast<MacroParamListNode*>(node) ||
        dynamic_cast<MatchCasesNode*>(node) ||
        dynamic_cast<PatternCaptureNode*>(node) ||
        dynamic_cast<ParamListNode*>(node) ||
        dynamic_cast<TypeNode*>(node) ||
        dynamic_cast<VariableNode*>(node) ||
        dynamic_cast<NumberNode*>(node) ||
        dynamic_cast<StringNode*>(node) ||
        dynamic_cast<BoolNode*>(node) ||
        dynamic_cast<LetBindingNode*>(node) ||
        dynamic_cast<LetBindingsNode*>(node)) {
        return node;
    }

    return node;
}

ASTNode* MacroExpander::expandMacroCall(
    const std::string& name,
    const std::vector<ASTNode*>& args,
    ASTNode* bodyArg,
    const SourceSpan& callSpan) {
    auto it = macros_.find(name);
    if (it == macros_.end()) {
        context_.error(
            SemanticPhase::MacroExpansion,
            "Unknown macro '" + name + "'.",
            callSpan);
        return nullptr;
    }
    if (expansionDepth_ >= kMaxExpansionDepth) {
        context_.error(
            SemanticPhase::MacroExpansion,
            "Macro expansion depth exceeded while expanding '" + name + "'.",
            callSpan);
        return nullptr;
    }

    MacroDeclNode* macro = it->second;
    ExpansionEnv env;
    env.macroName = name;
    pushScope(env);

    std::size_t positionalExpected = 0;
    std::size_t bodyParams = 0;
    for (auto* param : macro->params) {
        if (!param) {
            continue;
        }
        if (param->kind == MacroParamKind::Body) {
            ++bodyParams;
        } else {
            ++positionalExpected;
        }
        if (param->kind == MacroParamKind::Placeholder) {
            env.placeholderParams.insert(param->name);
        }
    }

    if (args.size() != positionalExpected) {
        context_.error(
            SemanticPhase::MacroExpansion,
            "Macro '" + name + "' expects " + std::to_string(positionalExpected) +
                " argument(s), but received " + std::to_string(args.size()) + ".",
            callSpan);
        return nullptr;
    }
    if ((bodyArg != nullptr) != (bodyParams == 1)) {
        context_.error(
            SemanticPhase::MacroExpansion,
            bodyParams == 1
                ? "Macro '" + name + "' requires a trailing body argument."
                : "Macro '" + name + "' does not accept a trailing body argument.",
            callSpan);
        return nullptr;
    }
    if (bodyParams > 1) {
        context_.error(
            SemanticPhase::MacroExpansion,
            "Macro '" + name + "' declares more than one body parameter.",
            macro->span);
        return nullptr;
    }

    std::size_t argIndex = 0;
    for (auto* param : macro->params) {
        if (!param) {
            continue;
        }

        Binding binding;
        if (param->kind == MacroParamKind::Body) {
            binding.kind = Binding::Kind::Ast;
            binding.ast = bodyArg;
            env.bindings[param->name] = binding;
            continue;
        }

        ASTNode* actual = argIndex < args.size() ? args[argIndex++] : nullptr;
        if (param->kind == MacroParamKind::Symbolic) {
            auto* symbolic = dynamic_cast<MacroSymbolRefNode*>(actual);
            if (!symbolic) {
                context_.error(
                    SemanticPhase::MacroExpansion,
                    "Macro parameter '" + param->name +
                        "' expects a symbolic argument written as @symbol.",
                    actual ? actual->span : callSpan);
                return nullptr;
            }
            binding.kind = Binding::Kind::Ast;
            binding.ast = symbolic->target;
            env.bindings[param->name] = binding;
            continue;
        }
        if (param->kind == MacroParamKind::Placeholder) {
            auto* placeholder = dynamic_cast<VariableNode*>(actual);
            if (!placeholder) {
                context_.error(
                    SemanticPhase::MacroExpansion,
                    "Macro parameter '" + param->name +
                        "' expects a placeholder symbol name.",
                    actual ? actual->span : callSpan);
                return nullptr;
            }
            binding.kind = Binding::Kind::Placeholder;
            binding.placeholderName = placeholder->name;
            env.bindings[param->name] = std::move(binding);
            continue;
        }
        binding.kind = Binding::Kind::Ast;
        binding.ast = actual;
        env.bindings[param->name] = binding;
    }

    ++expansionDepth_;
    ASTNode* expanded = cloneForExpansion(macro->body, env);
    --expansionDepth_;

    if (expanded) {
        expanded->span = callSpan;
        expanded = expandNode(expanded);
    }
    context_.tracePipeline("Expanded macro '" + name + "'.");
    return expanded;
}

ASTNode* MacroExpander::cloneDetached(ASTNode* node) {
    return cloneNode(node, nullptr);
}

ASTNode* MacroExpander::cloneForExpansion(ASTNode* node, ExpansionEnv& env) {
    return cloneNode(node, &env);
}

Type* MacroExpander::cloneType(Type* type) const {
    if (!type) {
        return nullptr;
    }
    if (type->equals(NumberType::instance())) {
        return NumberType::instance();
    }
    if (const auto* number = asNumberType(type)) {
        return number->isGeneric() ? NumberType::instance() : NumberType::instance(number->kind());
    }
    if (type->equals(BoolType::instance())) {
        return BoolType::instance();
    }
    if (type->equals(StringType::instance())) {
        return StringType::instance();
    }
    if (type->equals(ObjectType::instance())) {
        return ObjectType::instance();
    }
    if (type->equals(VoidType::instance())) {
        return VoidType::instance();
    }
    if (type->equals(UnknownType::instance())) {
        return UnknownType::instance();
    }
    if (const auto* nominal = asNominalType(type)) {
        return new NominalType(nominal->name(), nominal->parent());
    }
    if (const auto* protocol = asProtocolType(type)) {
        return new ProtocolType(protocol->name());
    }
    if (const auto* iterable = asIterableType(type)) {
        return new IterableType(cloneType(iterable->elementType()));
    }
    if (const auto* enumerable = asEnumerableType(type)) {
        return new EnumerableType(cloneType(enumerable->elementType()));
    }
    if (const auto* vector = asVectorType(type)) {
        return new VectorType(cloneType(vector->elementType()));
    }
    if (const auto* function = dynamic_cast<FunctionType*>(type)) {
        std::vector<Type*> params;
        params.reserve(function->getParamTypes().size());
        for (auto* param : function->getParamTypes()) {
            params.push_back(cloneType(param));
        }
        return new FunctionType(std::move(params), cloneType(function->getReturnType()));
    }
    return type;
}

std::string MacroExpander::freshName(const std::string& base) {
    return "__macro_" + base + "_" + std::to_string(++uniqueCounter_);
}

void MacroExpander::pushScope(ExpansionEnv& env) {
    env.renameScopes.emplace_back();
}

void MacroExpander::popScope(ExpansionEnv& env) {
    if (!env.renameScopes.empty()) {
        env.renameScopes.pop_back();
    }
}

void MacroExpander::bindLocal(
    ExpansionEnv& env,
    const std::string& original,
    const std::string& renamed) {
    if (env.renameScopes.empty()) {
        pushScope(env);
    }
    env.renameScopes.back()[original] = renamed;
}

const std::string* MacroExpander::lookupLocal(
    const ExpansionEnv& env,
    const std::string& name) const {
    for (auto it = env.renameScopes.rbegin(); it != env.renameScopes.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

const MacroExpander::Binding* MacroExpander::lookupBinding(
    const ExpansionEnv& env,
    const std::string& name) const {
    auto it = env.bindings.find(name);
    return it == env.bindings.end() ? nullptr : &it->second;
}

ASTNode* MacroExpander::cloneActualBinding(const Binding& binding) {
    if (binding.kind == Binding::Kind::Placeholder) {
        return new VariableNode(binding.placeholderName);
    }
    if (!binding.ast) {
        return nullptr;
    }
    ASTNode* clone = cloneDetached(binding.ast);
    return expandNode(clone);
}

bool MacroExpander::bindPattern(
    ASTNode* pattern,
    ASTNode* value,
    std::unordered_map<std::string, Binding>& captures) {
    if (!pattern || !value) {
        return pattern == value;
    }

    if (auto* capture = dynamic_cast<PatternCaptureNode*>(pattern)) {
        Binding binding;
        binding.kind = Binding::Kind::Ast;
        binding.ast = value;
        captures[capture->name] = binding;
        return true;
    }
    if (auto* numberPattern = dynamic_cast<NumberNode*>(pattern)) {
        auto* numberValue = dynamic_cast<NumberNode*>(value);
        return numberValue &&
               numberPattern->numberKind == numberValue->numberKind &&
               numberPattern->value == numberValue->value;
    }
    if (auto* stringPattern = dynamic_cast<StringNode*>(pattern)) {
        auto* stringValue = dynamic_cast<StringNode*>(value);
        return stringValue && stringPattern->value == stringValue->value;
    }
    if (auto* boolPattern = dynamic_cast<BoolNode*>(pattern)) {
        auto* boolValue = dynamic_cast<BoolNode*>(value);
        return boolValue && boolPattern->value == boolValue->value;
    }
    if (auto* binaryPattern = dynamic_cast<BinaryOpNode*>(pattern)) {
        auto* binaryValue = dynamic_cast<BinaryOpNode*>(value);
        return binaryValue &&
               binaryPattern->op == binaryValue->op &&
               bindPattern(binaryPattern->left, binaryValue->left, captures) &&
               bindPattern(binaryPattern->right, binaryValue->right, captures);
    }
    if (auto* unaryPattern = dynamic_cast<UnaryOpNode*>(pattern)) {
        auto* unaryValue = dynamic_cast<UnaryOpNode*>(value);
        return unaryValue &&
               unaryPattern->op == unaryValue->op &&
               bindPattern(unaryPattern->operand, unaryValue->operand, captures);
    }
    return false;
}

ASTNode* MacroExpander::cloneNode(ASTNode* node, ExpansionEnv* env) {
    if (!node) {
        return nullptr;
    }

    if (auto* number = dynamic_cast<NumberNode*>(node)) {
        return copy_with_span(new NumberNode(number->value, number->numberKind), node->span);
    }
    if (auto* boolean = dynamic_cast<BoolNode*>(node)) {
        return copy_with_span(new BoolNode(boolean->value), node->span);
    }
    if (auto* string = dynamic_cast<StringNode*>(node)) {
        return copy_with_span(new StringNode(string->value), node->span);
    }
    if (auto* variable = dynamic_cast<VariableNode*>(node)) {
        if (env) {
            if (const std::string* renamed = lookupLocal(*env, variable->name)) {
                return copy_with_span(new VariableNode(*renamed), node->span);
            }
            if (const Binding* binding = lookupBinding(*env, variable->name)) {
                ASTNode* replacement = cloneActualBinding(*binding);
                if (replacement) {
                    replacement->span = node->span.isValid() ? node->span : replacement->span;
                }
                return replacement;
            }
        }
        return copy_with_span(new VariableNode(variable->name), node->span);
    }
    if (auto* binary = dynamic_cast<BinaryOpNode*>(node)) {
        return copy_with_span(
            new BinaryOpNode(
                binary->op,
                cloneNode(binary->left, env),
                cloneNode(binary->right, env)),
            node->span);
    }
    if (auto* unary = dynamic_cast<UnaryOpNode*>(node)) {
        return copy_with_span(
            new UnaryOpNode(unary->op, cloneNode(unary->operand, env)),
            node->span);
    }
    if (auto* exprStmt = dynamic_cast<ExprStmtNode*>(node)) {
        return copy_with_span(new ExprStmtNode(cloneNode(exprStmt->expr, env)), node->span);
    }
    if (auto* block = dynamic_cast<BlockNode*>(node)) {
        std::vector<ASTNode*> stmts;
        stmts.reserve(block->stmts.size());
        for (auto* stmt : block->stmts) {
            stmts.push_back(cloneNode(stmt, env));
        }
        return copy_with_span(new BlockNode(std::move(stmts)), node->span);
    }
    if (auto* program = dynamic_cast<ProgramNode*>(node)) {
        std::vector<ASTNode*> decls;
        decls.reserve(program->decls.size());
        for (auto* decl : program->decls) {
            decls.push_back(cloneNode(decl, env));
        }
        std::vector<ASTNode*> stmts;
        stmts.reserve(program->stmts.size());
        for (auto* stmt : program->stmts) {
            stmts.push_back(cloneNode(stmt, env));
        }
        return copy_with_span(new ProgramNode(std::move(decls), std::move(stmts)), node->span);
    }
    if (auto* params = dynamic_cast<ParamListNode*>(node)) {
        std::vector<Type*> paramTypes;
        paramTypes.reserve(params->paramTypes.size());
        for (auto* type : params->paramTypes) {
            paramTypes.push_back(cloneType(type));
        }
        return copy_with_span(
            new ParamListNode(params->params, std::move(paramTypes), params->paramTypeNames),
            node->span);
    }
    if (auto* function = dynamic_cast<FunctionDeclNode*>(node)) {
        std::vector<std::string> params = function->params;
        std::vector<Type*> paramTypes;
        paramTypes.reserve(function->paramTypes.size());
        for (auto* type : function->paramTypes) {
            paramTypes.push_back(cloneType(type));
        }
        ASTNode* body = nullptr;
        ASTNode* exprBody = nullptr;
        std::vector<std::string> renamedParams;
        if (env) {
            pushScope(*env);
            renamedParams.reserve(params.size());
            for (const auto& param : params) {
                std::string fresh = freshName(param);
                bindLocal(*env, param, fresh);
                renamedParams.push_back(fresh);
            }
            params = renamedParams;
        }
        body = cloneNode(function->body, env);
        exprBody = cloneNode(function->exprBody, env);
        if (env) {
            popScope(*env);
        }
        FunctionDeclNode* copy =
            function->isInline
                ? new FunctionDeclNode(
                      function->name,
                      std::move(params),
                      std::move(paramTypes),
                      cloneType(function->returnType),
                      exprBody,
                      true)
                : new FunctionDeclNode(
                      function->name,
                      std::move(params),
                      std::move(paramTypes),
                      cloneType(function->returnType),
                      body);
        copy->paramTypeNames = function->paramTypeNames;
        copy->declaredReturnType = cloneType(function->declaredReturnType);
        copy->declaredReturnTypeName = function->declaredReturnTypeName;
        copy->hasExplicitReturnType = function->hasExplicitReturnType;
        copy->isMethod = function->isMethod;
        copy->ownerTypeName = function->ownerTypeName;
        copy->isSignatureOnly = function->isSignatureOnly;
        copy->isProtocolMethod = function->isProtocolMethod;
        copy->ownerProtocolName = function->ownerProtocolName;
        return copy_with_span(copy, node->span);
    }
    if (auto* letNode = dynamic_cast<LetNode*>(node)) {
        ASTNode* init = cloneNode(letNode->init, env);
        std::string boundName = letNode->name;
        bool usePlaceholder = env && env->placeholderParams.count(letNode->name) > 0;
        if (env) {
            pushScope(*env);
            if (usePlaceholder) {
                const Binding* binding = lookupBinding(*env, letNode->name);
                boundName = (binding && binding->kind == Binding::Kind::Placeholder)
                    ? binding->placeholderName
                    : letNode->name;
            } else {
                boundName = freshName(letNode->name);
            }
            bindLocal(*env, letNode->name, boundName);
        }
        ASTNode* body = cloneNode(letNode->body, env);
        if (env) {
            popScope(*env);
        }
        return copy_with_span(
            new LetNode(
                boundName,
                init,
                body,
                cloneType(letNode->declaredType),
                letNode->declaredTypeName,
                letNode->hasExplicitType),
            node->span);
    }
    if (auto* ifNode = dynamic_cast<IfNode*>(node)) {
        return copy_with_span(
            new IfNode(
                cloneNode(ifNode->condition, env),
                cloneNode(ifNode->then_branch, env),
                cloneNode(ifNode->else_branch, env)),
            node->span);
    }
    if (auto* functionCall = dynamic_cast<FunctionCallNode*>(node)) {
        ASTNode* receiver = cloneNode(functionCall->receiver, env);
        std::vector<ASTNode*> args;
        args.reserve(functionCall->args.size());
        for (auto* arg : functionCall->args) {
            args.push_back(cloneNode(arg, env));
        }

        std::string name = functionCall->name;
        if (!receiver && env) {
            if (const std::string* renamed = lookupLocal(*env, name)) {
                name = *renamed;
            } else if (const Binding* binding = lookupBinding(*env, name)) {
                if (binding->kind == Binding::Kind::Placeholder) {
                    name = binding->placeholderName;
                } else if (auto* actualVar = dynamic_cast<VariableNode*>(binding->ast)) {
                    name = actualVar->name;
                } else {
                    ASTNode* actualReceiver = cloneActualBinding(*binding);
                    auto* invokeCall =
                        copy_with_span(new FunctionCallNode("invoke", std::move(args), actualReceiver), node->span);
                    return expandNode(invokeCall);
                }
            }
        }

        auto* call = copy_with_span(new FunctionCallNode(name, std::move(args), receiver), node->span);
        if (!receiver && macros_.count(name)) {
            ASTNode* expanded = expandMacroCall(name, call->args, nullptr, call->span);
            delete call;
            return expanded;
        }
        return call;
    }
    if (auto* macroParam = dynamic_cast<MacroParamNode*>(node)) {
        return copy_with_span(
            new MacroParamNode(
                macroParam->kind,
                macroParam->name,
                cloneType(macroParam->declaredType),
                macroParam->declaredTypeName,
                macroParam->hasExplicitType),
            node->span);
    }
    if (auto* macroParams = dynamic_cast<MacroParamListNode*>(node)) {
        std::vector<MacroParamNode*> params;
        params.reserve(macroParams->params.size());
        for (auto* param : macroParams->params) {
            params.push_back(dynamic_cast<MacroParamNode*>(cloneNode(param, env)));
        }
        return copy_with_span(new MacroParamListNode(std::move(params)), node->span);
    }
    if (auto* patternCapture = dynamic_cast<PatternCaptureNode*>(node)) {
        return copy_with_span(
            new PatternCaptureNode(
                patternCapture->name,
                cloneType(patternCapture->annotatedType),
                patternCapture->annotatedTypeName,
                patternCapture->hasExplicitType),
            node->span);
    }
    if (auto* matchCase = dynamic_cast<MatchCaseNode*>(node)) {
        return copy_with_span(
            new MatchCaseNode(
                cloneNode(matchCase->pattern, env),
                cloneNode(matchCase->result, env)),
            node->span);
    }
    if (auto* matchCases = dynamic_cast<MatchCasesNode*>(node)) {
        std::vector<MatchCaseNode*> cases;
        cases.reserve(matchCases->cases.size());
        for (auto* matchCase : matchCases->cases) {
            cases.push_back(dynamic_cast<MatchCaseNode*>(cloneNode(matchCase, env)));
        }
        return copy_with_span(new MatchCasesNode(std::move(cases)), node->span);
    }
    if (auto* matchNode = dynamic_cast<MatchNode*>(node)) {
        if (env) {
            ASTNode* subject = cloneNode(matchNode->subject, env);
            for (auto* matchCase : matchNode->cases) {
                std::unordered_map<std::string, Binding> captures;
                if (!matchCase || !bindPattern(matchCase->pattern, subject, captures)) {
                    continue;
                }
                ExpansionEnv branch = *env;
                for (const auto& [captureName, binding] : captures) {
                    branch.bindings[captureName] = binding;
                }
                ASTNode* result = cloneNode(matchCase->result, &branch);
                delete subject;
                return result;
            }
            ASTNode* fallback = cloneNode(matchNode->defaultExpr, env);
            delete subject;
            return fallback;
        }

        std::vector<MatchCaseNode*> cases;
        cases.reserve(matchNode->cases.size());
        for (auto* matchCase : matchNode->cases) {
            cases.push_back(dynamic_cast<MatchCaseNode*>(cloneNode(matchCase, env)));
        }
        return copy_with_span(
            new MatchNode(
                cloneNode(matchNode->subject, env),
                std::move(cases),
                cloneNode(matchNode->defaultExpr, env)),
            node->span);
    }
    if (auto* macroDecl = dynamic_cast<MacroDeclNode*>(node)) {
        std::vector<MacroParamNode*> params;
        params.reserve(macroDecl->params.size());
        for (auto* param : macroDecl->params) {
            params.push_back(dynamic_cast<MacroParamNode*>(cloneNode(param, env)));
        }
        return copy_with_span(
            new MacroDeclNode(
                macroDecl->name,
                std::move(params),
                cloneType(macroDecl->declaredReturnType),
                macroDecl->declaredReturnTypeName,
                cloneNode(macroDecl->body, env),
                macroDecl->bodyIsBlock,
                macroDecl->hasExplicitReturnType),
            node->span);
    }
    if (auto* symbolRef = dynamic_cast<MacroSymbolRefNode*>(node)) {
        return copy_with_span(new MacroSymbolRefNode(cloneNode(symbolRef->target, env)), node->span);
    }
    if (auto* macroCall = dynamic_cast<MacroCallNode*>(node)) {
        std::vector<ASTNode*> args;
        args.reserve(macroCall->args.size());
        for (auto* arg : macroCall->args) {
            args.push_back(cloneNode(arg, env));
        }
        ASTNode* bodyArg = cloneNode(macroCall->bodyArg, env);
        if (env) {
            return expandMacroCall(macroCall->name, args, bodyArg, node->span);
        }
        return copy_with_span(new MacroCallNode(macroCall->name, std::move(args), bodyArg), node->span);
    }
    if (auto* lambda = dynamic_cast<LambdaNode*>(node)) {
        std::vector<std::string> params = lambda->params;
        std::vector<Type*> paramTypes;
        paramTypes.reserve(lambda->paramTypes.size());
        for (auto* type : lambda->paramTypes) {
            paramTypes.push_back(cloneType(type));
        }
        if (env) {
            pushScope(*env);
            for (auto& param : params) {
                std::string fresh = freshName(param);
                bindLocal(*env, param, fresh);
                param = fresh;
            }
        }
        ASTNode* body = cloneNode(lambda->body, env);
        if (env) {
            popScope(*env);
        }
        auto* copy = new LambdaNode(
            std::move(params),
            std::move(paramTypes),
            lambda->paramTypeNames,
            cloneType(lambda->declaredReturnType),
            body,
            lambda->bodyIsBlock);
        copy->declaredReturnType = cloneType(lambda->declaredReturnType);
        copy->declaredReturnTypeName = lambda->declaredReturnTypeName;
        copy->hasExplicitReturnType = lambda->hasExplicitReturnType;
        return copy_with_span(copy, node->span);
    }
    if (auto* vectorLiteral = dynamic_cast<VectorLiteralNode*>(node)) {
        std::vector<ASTNode*> elements;
        elements.reserve(vectorLiteral->elements.size());
        for (auto* element : vectorLiteral->elements) {
            elements.push_back(cloneNode(element, env));
        }
        return copy_with_span(new VectorLiteralNode(std::move(elements)), node->span);
    }
    if (auto* vectorComp = dynamic_cast<VectorComprehensionNode*>(node)) {
        std::string iteratorName = vectorComp->iterator;
        ASTNode* iterable = cloneNode(vectorComp->iterable, env);
        if (env) {
            pushScope(*env);
            std::string fresh = freshName(iteratorName);
            bindLocal(*env, iteratorName, fresh);
            iteratorName = fresh;
        }
        ASTNode* expression = cloneNode(vectorComp->expression, env);
        if (env) {
            popScope(*env);
        }
        return copy_with_span(
            new VectorComprehensionNode(expression, iteratorName, iterable),
            node->span);
    }
    if (auto* index = dynamic_cast<IndexAccessNode*>(node)) {
        return copy_with_span(
            new IndexAccessNode(cloneNode(index->base, env), cloneNode(index->index, env)),
            node->span);
    }
    if (auto* assignment = dynamic_cast<AssignmentNode*>(node)) {
        return copy_with_span(
            new AssignmentNode(cloneNode(assignment->target, env), cloneNode(assignment->value, env)),
            node->span);
    }
    if (auto* member = dynamic_cast<MemberAccessNode*>(node)) {
        return copy_with_span(new MemberAccessNode(cloneNode(member->base, env), member->member), node->span);
    }
    if (auto* newNode = dynamic_cast<NewNode*>(node)) {
        std::vector<ASTNode*> args;
        args.reserve(newNode->args.size());
        for (auto* arg : newNode->args) {
            args.push_back(cloneNode(arg, env));
        }
        return copy_with_span(new NewNode(newNode->typeName, std::move(args)), node->span);
    }
    if (auto* whileNode = dynamic_cast<WhileNode*>(node)) {
        return copy_with_span(
            new WhileNode(cloneNode(whileNode->condition, env), cloneNode(whileNode->body, env)),
            node->span);
    }
    if (auto* forNode = dynamic_cast<ForNode*>(node)) {
        ASTNode* iterable = cloneNode(forNode->iterable, env);
        std::string iteratorName = forNode->iterator;
        if (env) {
            pushScope(*env);
            if (env->placeholderParams.count(iteratorName) > 0) {
                const Binding* binding = lookupBinding(*env, iteratorName);
                iteratorName = (binding && binding->kind == Binding::Kind::Placeholder)
                    ? binding->placeholderName
                    : iteratorName;
            } else {
                iteratorName = freshName(iteratorName);
            }
            bindLocal(*env, forNode->iterator, iteratorName);
        }
        ASTNode* body = cloneNode(forNode->body, env);
        if (env) {
            popScope(*env);
        }
        return copy_with_span(new ForNode(iteratorName, iterable, body), node->span);
    }
    if (auto* attr = dynamic_cast<AttributeDeclNode*>(node)) {
        return copy_with_span(
            new AttributeDeclNode(
                attr->name,
                cloneNode(attr->init, env),
                cloneType(attr->declaredType),
                attr->declaredTypeName,
                attr->hasExplicitType),
            node->span);
    }
    if (auto* typeDecl = dynamic_cast<TypeDeclNode*>(node)) {
        std::vector<Type*> ctorTypes;
        ctorTypes.reserve(typeDecl->ctorParamTypes.size());
        for (auto* type : typeDecl->ctorParamTypes) {
            ctorTypes.push_back(cloneType(type));
        }
        std::vector<ASTNode*> parentArgs;
        parentArgs.reserve(typeDecl->parentArgs.size());
        for (auto* parentArg : typeDecl->parentArgs) {
            parentArgs.push_back(cloneNode(parentArg, env));
        }
        std::vector<ASTNode*> members;
        members.reserve(typeDecl->members.size());
        for (auto* member : typeDecl->members) {
            members.push_back(cloneNode(member, env));
        }
        return copy_with_span(
            new TypeDeclNode(
                typeDecl->name,
                typeDecl->ctorParams,
                std::move(ctorTypes),
                typeDecl->ctorParamTypeNames,
                typeDecl->parentType,
                std::move(parentArgs),
                std::move(members),
                typeDecl->hasExplicitParentArgs),
            node->span);
    }
    if (auto* protocolDecl = dynamic_cast<ProtocolDeclNode*>(node)) {
        std::vector<ASTNode*> methods;
        methods.reserve(protocolDecl->methods.size());
        for (auto* method : protocolDecl->methods) {
            methods.push_back(cloneNode(method, env));
        }
        return copy_with_span(
            new ProtocolDeclNode(
                protocolDecl->name,
                protocolDecl->extendedProtocol,
                std::move(methods)),
            node->span);
    }
    if (auto* letBinding = dynamic_cast<LetBindingNode*>(node)) {
        return copy_with_span(
            new LetBindingNode(
                letBinding->name,
                cloneNode(letBinding->init, env),
                cloneType(letBinding->declaredType),
                letBinding->declaredTypeName,
                letBinding->hasExplicitType),
            node->span);
    }
    if (auto* letBindings = dynamic_cast<LetBindingsNode*>(node)) {
        std::vector<LetBindingNode*> bindings;
        bindings.reserve(letBindings->bindings.size());
        for (auto* binding : letBindings->bindings) {
            bindings.push_back(dynamic_cast<LetBindingNode*>(cloneNode(binding, env)));
        }
        return copy_with_span(new LetBindingsNode(std::move(bindings)), node->span);
    }
    if (auto* typeNode = dynamic_cast<TypeNode*>(node)) {
        return copy_with_span(new TypeNode(typeNode->typeName, cloneType(typeNode->type)), node->span);
    }
    if (auto* returnNode = dynamic_cast<ReturnNode*>(node)) {
        return copy_with_span(new ReturnNode(cloneNode(returnNode->expr, env)), node->span);
    }

    return nullptr;
}
