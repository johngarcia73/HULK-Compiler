#include "ast_builder.hpp"
#include "ast_bulder_utils.hpp"
#include <algorithm>
#include <cassert>
#include <initializer_list>
#include <iostream>
#include <limits>

namespace {

std::pair<std::string, NumberKind> parse_number(const std::optional<std::string>& s) {
    if (!s) {
        return {"0", NumberKind::Int};
    }

    std::string raw = *s;
    if (raw.find('.') != std::string::npos ||
        raw.find('e') != std::string::npos ||
        raw.find('E') != std::string::npos) {
        return {raw, NumberKind::Double};
    }

    try {
        long long numericValue = std::stoll(raw);
        if (numericValue < std::numeric_limits<int>::min() ||
            numericValue > std::numeric_limits<int>::max()) {
            return {raw, NumberKind::Long};
        }
    } catch (...) {
        return {raw, NumberKind::Long};
    }
    return {raw, NumberKind::Int};
}

Type* builtin_type_for_name(const std::string& name) {
    if (name == "Number") {
        return NumberType::instance();
    }
    if (name == "Bool") {
        return BoolType::instance();
    }
    if (name == "String") {
        return StringType::instance();
    }
    if (name == "Object") {
        return ObjectType::instance();
    }
    return nullptr;
}

static SourceSpan merged_rhs_span(const std::vector<Value>& rhs) {
    SourceSpan combined;
    for (const auto& value : rhs) {
        combined = SourceSpan::merge(combined, value_span(value));
    }
    return combined;
}

std::vector<ASTNode*> take_block_items(ASTNode* node) {
    std::vector<ASTNode*> items;
    if (!node) {
        return items;
    }

    if (auto* block = dynamic_cast<BlockNode*>(node)) {
        items = std::move(block->stmts);
        block->stmts.clear();
        delete block;
        return items;
    }

    items.push_back(node);
    return items;
}

std::vector<ASTNode*> take_arg_items(ASTNode* node) {
    return take_block_items(node);
}

std::vector<TypeNode*> take_type_items(ASTNode* node) {
    std::vector<TypeNode*> types;
    if (!node) {
        return types;
    }
    if (auto* block = dynamic_cast<BlockNode*>(node)) {
        for (auto* item : block->stmts) {
            if (auto* type = dynamic_cast<TypeNode*>(item)) {
                types.push_back(type);
            } else {
                delete item;
            }
        }
        block->stmts.clear();
        delete block;
        return types;
    }
    if (auto* type = dynamic_cast<TypeNode*>(node)) {
        types.push_back(type);
        return types;
    }
    delete node;
    return types;
}

ParamListNode* empty_param_list() {
    return new ParamListNode({}, {}, {});
}

ASTNode* build_nested_let(
    LetBindingsNode* bindings_node,
    ASTNode* body,
    const SourceSpan& full_span) {
    if (!bindings_node) {
        return attach_span(body, full_span);
    }

    ASTNode* result = body;
    for (auto it = bindings_node->bindings.rbegin(); it != bindings_node->bindings.rend(); ++it) {
        auto* binding = *it;
        if (!binding) {
            continue;
        }

        ASTNode* init = binding->init;
        binding->init = nullptr;

        SourceSpan let_span = SourceSpan::merge(binding->span, node_span(result));
        if (!let_span.isValid()) {
            let_span = full_span;
        }

        result = attach_span(
            new LetNode(
                binding->name,
                init,
                result,
                binding->declaredType,
                binding->declaredTypeName,
                binding->hasExplicitType),
            let_span);
    }

    delete bindings_node;
    return attach_span(result, full_span);
}

ASTNode* pass_with_span(ASTNode* node, const SourceSpan& span) {
    return attach_span(node, span);
}

FunctionDeclNode* build_function_like(
    const std::string& name,
    ParamListNode* paramsNode,
    TypeNode* returnTypeNode,
    ASTNode* bodyOrExpr,
    bool isInline,
    bool isMethod,
    const SourceSpan& span) {
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramTypeNames;
    if (paramsNode) {
        params = std::move(paramsNode->params);
        paramTypes = std::move(paramsNode->paramTypes);
        paramTypeNames = std::move(paramsNode->paramTypeNames);
        delete paramsNode;
    }

    bool hasExplicitReturnType = returnTypeNode != nullptr;
    Type* returnType = returnTypeNode ? returnTypeNode->type : nullptr;
    std::string declaredReturnName = returnTypeNode ? returnTypeNode->typeName : "";
    delete returnTypeNode;

    FunctionDeclNode* node = nullptr;
    if (isInline) {
        node = new FunctionDeclNode(name, std::move(params), std::move(paramTypes), returnType, bodyOrExpr, true);
    } else {
        node = new FunctionDeclNode(name, std::move(params), std::move(paramTypes), returnType, bodyOrExpr);
    }

    node->paramTypeNames = std::move(paramTypeNames);
    node->declaredReturnType = returnType;
    node->declaredReturnTypeName = declaredReturnName;
    node->hasExplicitReturnType = hasExplicitReturnType || !declaredReturnName.empty();
    node->isMethod = isMethod;
    return attach_span(node, span);
}

FunctionDeclNode* build_protocol_method(
    const std::string& name,
    ParamListNode* paramsNode,
    TypeNode* returnTypeNode,
    const SourceSpan& span) {
    FunctionDeclNode* node = build_function_like(
        name,
        paramsNode,
        returnTypeNode,
        nullptr,
        false,
        false,
        span);
    node->isSignatureOnly = true;
    node->isProtocolMethod = true;
    return node;
}

TypeNode* take_type_annotation(ASTNode* node) {
    return dynamic_cast<TypeNode*>(node);
}

LambdaNode* build_lambda(
    ParamListNode* paramsNode,
    TypeNode* returnTypeNode,
    ASTNode* body,
    const SourceSpan& span) {
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramTypeNames;
    if (paramsNode) {
        params = std::move(paramsNode->params);
        paramTypes = std::move(paramsNode->paramTypes);
        paramTypeNames = std::move(paramsNode->paramTypeNames);
        delete paramsNode;
    }
    Type* returnType = returnTypeNode ? returnTypeNode->type : nullptr;
    std::string returnTypeName = returnTypeNode ? returnTypeNode->typeName : "";
    bool hasExplicitReturn = returnTypeNode != nullptr;
    delete returnTypeNode;
    return attach_span(
        new LambdaNode(
            std::move(params),
            std::move(paramTypes),
            std::move(paramTypeNames),
            returnType,
            std::move(returnTypeName),
            hasExplicitReturn,
            body),
        span);
}

bool production_matches(
    const Grammar& grammar,
    size_t pid,
    const char* lhs,
    std::initializer_list<const char*> rhs_names) {
    const auto& production = grammar.get_productions()[pid];
    if (grammar.symtab[production.lhs].name != lhs) {
        return false;
    }
    if (production.rhs.size() != rhs_names.size()) {
        return false;
    }

    size_t index = 0;
    for (const char* rhs_name : rhs_names) {
        if (grammar.symtab[production.rhs[index]].name != rhs_name) {
            return false;
        }
        ++index;
    }
    return true;
}

}  // namespace

ASTNode* ASTBuilder::build(size_t pid, const std::vector<Value>& rhs) {
    if (!grammar) {
        throw std::runtime_error("ASTBuilder requires a grammar to dispatch productions.");
    }

    auto match = [&](const char* lhs, std::initializer_list<const char*> rhs_names) {
        return production_matches(*grammar, pid, lhs, rhs_names);
    };

    // Program structure
    if (match("program", {"top_level_items"})) {
        if (auto* block = dynamic_cast<BlockNode*>(RHS(0))) {
            auto items = std::move(block->stmts);
            SourceSpan items_span = block->span;
            block->stmts.clear();
            delete block;

            std::vector<ASTNode*> decls;
            std::vector<ASTNode*> stmts;
            for (auto* item : items) {
                if (dynamic_cast<FunctionDeclNode*>(item) ||
                    dynamic_cast<TypeDeclNode*>(item) ||
                    dynamic_cast<ProtocolDeclNode*>(item)) {
                    decls.push_back(item);
                } else {
                    stmts.push_back(item);
                }
            }
            return attach_span(new ProgramNode(std::move(decls), std::move(stmts)), items_span);
        }
        return attach_span(new ProgramNode({}, {}), merged_rhs_span(rhs));
    }
    if (match("top_level_items", {"top_level_item"})) {
        std::vector<ASTNode*> items;
        items.push_back(RHS(0));
        return attach_span(new BlockNode(std::move(items)), merged_rhs_span(rhs));
    }
    if (match("top_level_items", {"top_level_items", "top_level_item"})) {
        BUILD_BLOCK(RHS(0), RHS(1));
    }

    // top_level_item
    if (match("top_level_item", {"declaration"})) { PASS(); }
    if (match("top_level_item", {"statement"}))   { PASS(); }
    if (match("top_level_item", {"expr"}))        { PASS(); }
    if (match("declaration", {"function_decl"}) ||
        match("declaration", {"type_decl"}) ||
        match("declaration", {"protocol_decl"})) {
        PASS();
    }

    // Function declarations
    if (match("function_decl", {"FUNCTION", "IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "block"})) {
        return build_function_like(TOKEN(1), dynamic_cast<ParamListNode*>(RHS(3)), nullptr, RHS(5), false, false, merged_rhs_span(rhs));
    }
    if (match("function_decl", {"FUNCTION", "IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "block"})) {
        return build_function_like(TOKEN(1), dynamic_cast<ParamListNode*>(RHS(3)), dynamic_cast<TypeNode*>(RHS(6)), RHS(7), false, false, merged_rhs_span(rhs));
    }
    if (match("function_decl", {"FUNCTION", "IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "ARROW", "expr", "SEMICOLON"})) {
        return build_function_like(TOKEN(1), dynamic_cast<ParamListNode*>(RHS(3)), nullptr, RHS(6), true, false, merged_rhs_span(rhs));
    }
    if (match("function_decl", {"FUNCTION", "IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "ARROW", "expr", "SEMICOLON"})) {
        return build_function_like(TOKEN(1), dynamic_cast<ParamListNode*>(RHS(3)), dynamic_cast<TypeNode*>(RHS(6)), RHS(8), true, false, merged_rhs_span(rhs));
    }

    // Function expression for lambdas
    if (match("function_expr", {"FUNCTION", "L_PAREN", "param_list_opt", "R_PAREN", "TYPE_ARROW", "expr"})) {
        return build_lambda(
            dynamic_cast<ParamListNode*>(RHS(2)),
            nullptr,
            RHS(5),
            merged_rhs_span(rhs));
    }
    if (match("function_expr", {"FUNCTION", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "TYPE_ARROW", "expr"})) {
        return build_lambda(
            dynamic_cast<ParamListNode*>(RHS(2)),
            dynamic_cast<TypeNode*>(RHS(5)),
            RHS(7),
            merged_rhs_span(rhs));
    }
    if (match("function_expr", {"FUNCTION", "L_PAREN", "param_list_opt", "R_PAREN", "block"})) {
        return build_lambda(
            dynamic_cast<ParamListNode*>(RHS(2)),
            nullptr,
            RHS(4),
            merged_rhs_span(rhs));
    }
    if (match("function_expr", {"FUNCTION", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "block"})) {
        return build_lambda(
            dynamic_cast<ParamListNode*>(RHS(2)),
            dynamic_cast<TypeNode*>(RHS(5)),
            RHS(6),
            merged_rhs_span(rhs));
    }


    // Type declarations
    if (match("type_decl", {"TYPE", "IDENTIFIER", "ctor_param_clause_opt", "inheritance_clause", "type_body"})) {
        auto* ctorParams = dynamic_cast<ParamListNode*>(RHS(2));
        auto* inheritanceType = dynamic_cast<TypeNode*>(RHS(3));
        auto* inheritanceCall = dynamic_cast<NewNode*>(RHS(3));
        auto* membersBlock = dynamic_cast<BlockNode*>(RHS(4));

        std::vector<std::string> ctorParamNames;
        std::vector<Type*> ctorParamTypes;
        std::vector<std::string> ctorParamTypeNames;
        if (ctorParams) {
            ctorParamNames = std::move(ctorParams->params);
            ctorParamTypes = std::move(ctorParams->paramTypes);
            ctorParamTypeNames = std::move(ctorParams->paramTypeNames);
            delete ctorParams;
        }

        std::string parentType = "Object";
        std::vector<ASTNode*> parentArgs;
        bool hasExplicitParentArgs = false;
        if (inheritanceType) {
            parentType = inheritanceType->typeName.empty() ? "Object" : inheritanceType->typeName;
            delete inheritanceType;
        } else if (inheritanceCall) {
            parentType = inheritanceCall->typeName;
            parentArgs = std::move(inheritanceCall->args);
            inheritanceCall->args.clear();
            hasExplicitParentArgs = true;
            delete inheritanceCall;
        }

        std::vector<ASTNode*> members;
        if (membersBlock) {
            members = std::move(membersBlock->stmts);
            membersBlock->stmts.clear();
            delete membersBlock;
        }

        for (auto* member : members) {
            if (auto* method = dynamic_cast<FunctionDeclNode*>(member)) {
                method->isMethod = true;
                method->ownerTypeName = TOKEN(1);
            }
        }

        return attach_span(
            new TypeDeclNode(
                TOKEN(1),
                std::move(ctorParamNames),
                std::move(ctorParamTypes),
                std::move(ctorParamTypeNames),
                parentType,
                std::move(parentArgs),
                std::move(members),
                hasExplicitParentArgs),
            merged_rhs_span(rhs));
    }
    if (match("protocol_decl", {"PROTOCOL", "IDENTIFIER", "protocol_extension_clause", "protocol_body"})) {
        auto* extension = dynamic_cast<TypeNode*>(RHS(2));
        auto* membersBlock = dynamic_cast<BlockNode*>(RHS(3));

        std::string extendedProtocol;
        if (extension) {
            extendedProtocol = extension->typeName;
            delete extension;
        }

        std::vector<ASTNode*> methods;
        if (membersBlock) {
            methods = std::move(membersBlock->stmts);
            membersBlock->stmts.clear();
            delete membersBlock;
        }

        for (auto* method : methods) {
            if (auto* function = dynamic_cast<FunctionDeclNode*>(method)) {
                function->isSignatureOnly = true;
                function->isProtocolMethod = true;
                function->ownerProtocolName = TOKEN(1);
            }
        }

        return attach_span(
            new ProtocolDeclNode(TOKEN(1), std::move(extendedProtocol), std::move(methods)),
            merged_rhs_span(rhs));
    }
    if (match("ctor_param_clause_opt", {})) {
        return attach_span(empty_param_list(), merged_rhs_span(rhs));
    }
    if (match("ctor_param_clause_opt", {"L_PAREN", "param_list_opt", "R_PAREN"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("inheritance_clause", {})) {
        return nullptr;
    }
    if (match("inheritance_clause", {"INHERITS", "IDENTIFIER"})) {
        return attach_span(new TypeNode(TOKEN(1), builtin_type_for_name(TOKEN(1))), merged_rhs_span(rhs));
    }
    if (match("inheritance_clause", {"INHERITS", "IDENTIFIER", "L_PAREN", "arg_list_opt", "R_PAREN"})) {
        return attach_span(new NewNode(TOKEN(1), take_arg_items(RHS(3))), merged_rhs_span(rhs));
    }
    if (match("protocol_extension_clause", {})) {
        return nullptr;
    }
    if (match("protocol_extension_clause", {"EXTENDS", "IDENTIFIER"})) {
        return attach_span(new TypeNode(TOKEN(1), builtin_type_for_name(TOKEN(1))), merged_rhs_span(rhs));
    }
    if (match("type_body", {"L_CURL_BRACK", "type_members", "R_CURL_BRACK"})) {
        if (auto* block = dynamic_cast<BlockNode*>(RHS(1))) {
            return attach_span(new BlockNode(take_block_items(block)), merged_rhs_span(rhs));
        }
        return attach_span(new BlockNode({}), merged_rhs_span(rhs));
    }
    if (match("protocol_body", {"L_CURL_BRACK", "protocol_methods", "R_CURL_BRACK"})) {
        if (auto* block = dynamic_cast<BlockNode*>(RHS(1))) {
            return attach_span(new BlockNode(take_block_items(block)), merged_rhs_span(rhs));
        }
        return attach_span(new BlockNode({}), merged_rhs_span(rhs));
    }
    if (match("type_members", {})) {
        return attach_span(new BlockNode({}), {});
    }
    if (match("type_members", {"type_members", "type_member"})) {
        BUILD_BLOCK(RHS(0), RHS(1));
    }
    if (match("protocol_methods", {})) {
        return attach_span(new BlockNode({}), {});
    }
    if (match("protocol_methods", {"protocol_methods", "protocol_method_decl"})) {
        BUILD_BLOCK(RHS(0), RHS(1));
    }
    if (match("type_member", {"attribute_decl"}) || match("type_member", {"method_decl"})) {
        PASS();
    }
    if (match("attribute_decl", {"IDENTIFIER", "opt_type_annotation", "EQUAL", "expr", "SEMICOLON"})) {
        auto* typeNode = take_type_annotation(RHS(1));
        bool hasExplicitType = typeNode != nullptr;
        Type* declaredType = typeNode ? typeNode->type : nullptr;
        std::string declaredTypeName = typeNode ? typeNode->typeName : "";
        delete typeNode;
        return attach_span(
            new AttributeDeclNode(TOKEN(0), RHS(3), declaredType, declaredTypeName, hasExplicitType || !declaredTypeName.empty()),
            merged_rhs_span(rhs));
    }
    if (match("method_decl", {"IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "block"})) {
        return build_function_like(TOKEN(0), dynamic_cast<ParamListNode*>(RHS(2)), nullptr, RHS(4), false, true, merged_rhs_span(rhs));
    }
    if (match("method_decl", {"IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "block"})) {
        return build_function_like(TOKEN(0), dynamic_cast<ParamListNode*>(RHS(2)), dynamic_cast<TypeNode*>(RHS(5)), RHS(6), false, true, merged_rhs_span(rhs));
    }
    if (match("method_decl", {"IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "ARROW", "expr", "SEMICOLON"})) {
        return build_function_like(TOKEN(0), dynamic_cast<ParamListNode*>(RHS(2)), nullptr, RHS(5), true, true, merged_rhs_span(rhs));
    }
    if (match("method_decl", {"IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "ARROW", "expr", "SEMICOLON"})) {
        return build_function_like(TOKEN(0), dynamic_cast<ParamListNode*>(RHS(2)), dynamic_cast<TypeNode*>(RHS(5)), RHS(7), true, true, merged_rhs_span(rhs));
    }
    if (match("protocol_method_decl", {"IDENTIFIER", "L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "SEMICOLON"})) {
        return build_protocol_method(TOKEN(0), dynamic_cast<ParamListNode*>(RHS(2)), dynamic_cast<TypeNode*>(RHS(5)), merged_rhs_span(rhs));
    }

    // Parameters and annotations
    if (match("param_list_opt", {})) {
        return attach_span(empty_param_list(), merged_rhs_span(rhs));
    }
    if (match("param_list_opt", {"param_list"})) {
        PASS();
    }
    if (match("param_list", {"param_decl"})) {
        PASS();
    }
    if (match("param_list", {"param_list", "COMMA", "param_decl"})) {
        auto* left = dynamic_cast<ParamListNode*>(RHS(0));
        auto* right = dynamic_cast<ParamListNode*>(RHS(2));
        std::vector<std::string> params;
        std::vector<Type*> types;
        std::vector<std::string> typeNames;
        if (left) {
            params = std::move(left->params);
            types = std::move(left->paramTypes);
            typeNames = std::move(left->paramTypeNames);
            delete left;
        }
        if (right) {
            params.insert(params.end(), right->params.begin(), right->params.end());
            types.insert(types.end(), right->paramTypes.begin(), right->paramTypes.end());
            typeNames.insert(typeNames.end(), right->paramTypeNames.begin(), right->paramTypeNames.end());
            right->params.clear();
            right->paramTypes.clear();
            right->paramTypeNames.clear();
            delete right;
        }
        return attach_span(new ParamListNode(std::move(params), std::move(types), std::move(typeNames)), merged_rhs_span(rhs));
    }
    if (match("param_decl", {"IDENTIFIER", "opt_type_annotation"})) {
        auto* typeNode = take_type_annotation(RHS(1));
        std::vector<Type*> types;
        std::vector<std::string> typeNames;
        types.push_back(typeNode ? typeNode->type : nullptr);
        typeNames.push_back(typeNode ? typeNode->typeName : "");
        delete typeNode;
        return attach_span(new ParamListNode({TOKEN(0)}, std::move(types), std::move(typeNames)), merged_rhs_span(rhs));
    }
    if (match("opt_type_annotation", {})) {
        return nullptr;
    }
    if (match("opt_type_annotation", {"COLON", "type"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("type", {"type_atom"})) {
        PASS();
    }
    if (match("type", {"type", "STAR"})) {
        auto* element = dynamic_cast<TypeNode*>(RHS(0));
        Type* elementType = element ? element->type : nullptr;
        std::string elementName = element ? element->typeName : "";
        delete element;
        return attach_span(
            new TypeNode(elementName + "*", new IterableType(elementType ? elementType : ObjectType::instance())),
            merged_rhs_span(rhs));
    }
    if (match("type", {"type", "L_SQUARE_BRACK", "R_SQUARE_BRACK"})) {
        auto* element = dynamic_cast<TypeNode*>(RHS(0));
        Type* elementType = element ? element->type : nullptr;
        std::string elementName = element ? element->typeName : "";
        delete element;
        return attach_span(
            new TypeNode(elementName + "[]", new VectorType(elementType ? elementType : ObjectType::instance())),
            merged_rhs_span(rhs));
    }
    if (match("type", {"L_PAREN", "type_list_opt", "R_PAREN", "TYPE_ARROW", "type"})) {
        auto paramNodes = take_type_items(RHS(1));
        auto* returnTypeNode = dynamic_cast<TypeNode*>(RHS(4));
        std::vector<Type*> params;
        std::string name = "(";
        for (size_t i = 0; i < paramNodes.size(); ++i) {
            if (i > 0) {
                name += ", ";
            }
            params.push_back(paramNodes[i]->type ? paramNodes[i]->type : ObjectType::instance());
            name += paramNodes[i]->typeName;
            delete paramNodes[i];
        }
        Type* returnType = returnTypeNode && returnTypeNode->type ? returnTypeNode->type : ObjectType::instance();
        std::string returnName = returnTypeNode ? returnTypeNode->typeName : "Object";
        delete returnTypeNode;
        name += ") -> " + returnName;
        return attach_span(new TypeNode(name, new FunctionType(params, returnType)), merged_rhs_span(rhs));
    }
    if (match("type_atom", {"NUMBER_TYPE"})) {
        return attach_span(new TypeNode("Number", NumberType::instance()), rhs[0].span);
    }
    if (match("type_atom", {"BOOL_TYPE"})) {
        return attach_span(new TypeNode("Bool", BoolType::instance()), rhs[0].span);
    }
    if (match("type_atom", {"STRING_TYPE"})) {
        return attach_span(new TypeNode("String", StringType::instance()), rhs[0].span);
    }
    if (match("type_atom", {"IDENTIFIER"})) {
        return attach_span(new TypeNode(TOKEN(0), builtin_type_for_name(TOKEN(0))), rhs[0].span);
    }
    if (match("type_list_opt", {})) {
        return attach_span(new BlockNode({}), {});
    }
    if (match("type_list_opt", {"type_list"})) {
        PASS();
    }
    if (match("type_list", {"type"})) {
        BUILD_ARG_LIST(RHS(0));
    }
    if (match("type_list", {"type_list", "COMMA", "type"})) {
        BUILD_BLOCK(RHS(0), RHS(2));
    }

    // Statements
    if (match("statement", {"expr", "SEMICOLON"})) {
        return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
    }
    if (match("statement", {"block"}) ||
        match("statement", {"return_stmt"}) ||
        match("statement", {"if_stmt"}) ||
        match("statement", {"while_stmt", "SEMICOLON"}) ||
        match("statement", {"for_stmt", "SEMICOLON"})) {
        PASS();
    }
    if (match("statement", {"block", "SEMICOLON"})) return RHS(0);
    
    if (match("return_stmt", {"RETURN", "expr", "SEMICOLON"})) {
        return attach_span(new ReturnNode(RHS(1)), merged_rhs_span(rhs));
    }
    if (match("return_inline", {"RETURN", "expr"})) {
        return attach_span(new ReturnNode(RHS(1)), merged_rhs_span(rhs));
    }
    if (match("block", {"L_CURL_BRACK", "statements", "R_CURL_BRACK"})) {
        if (auto* block = dynamic_cast<BlockNode*>(RHS(1))) {
            return attach_span(new BlockNode(take_block_items(block)), merged_rhs_span(rhs));
        }
        return attach_span(new BlockNode({}), merged_rhs_span(rhs));
    }
    if (match("statements", {})) {
        return attach_span(new BlockNode({}), {});
    }
    if (match("statements", {"statements", "statement"})) {
        BUILD_BLOCK(RHS(0), RHS(1));
    }

    // Conditionals and loops
    if (match("if_stmt", {"IF", "L_PAREN", "expr", "R_PAREN", "conditional_stmt_body", "conditional_stmt_tail"})||
        match("if_stmt", {"IF", "L_PAREN", "expr", "R_PAREN", "conditional_inline_stmt_body", "conditional_stmt_tail"})) {
        return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
    }
    if (match("conditional_stmt_tail", {})) {
        return nullptr;
    }
    if (match("conditional_stmt_tail", {"ELSE", "conditional_stmt_body"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("conditional_stmt_tail", {"ELIF", "L_PAREN", "expr", "R_PAREN", "conditional_stmt_body", "conditional_stmt_tail"})) {
        return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
    }
    if (match("conditional_stmt_tail", {"ELSE", "conditional_inline_stmt_body"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("conditional_stmt_tail", {"ELIF", "L_PAREN", "expr", "R_PAREN", "conditional_inline_stmt_body", "conditional_stmt_tail"})) {
        return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
    }

    if (match("conditional_stmt_body", {"block"}) ||
        match("conditional_stmt_body", {"return_stmt"}) ||
        match("conditional_stmt_body", {"if_stmt"}) ||
        match("conditional_stmt_body", {"while_stmt"}) ||
        match("conditional_stmt_body", {"for_stmt"})) {
        PASS();
    }
    if (match("conditional_inline_stmt_body", {"block"}) ||
        match("conditional_inline_stmt_body", {"return_inline"}) ||
        match("conditional_inline_stmt_body", {"if_stmt"}) ||
        match("conditional_inline_stmt_body", {"while_stmt"}) ||
        match("conditional_inline_stmt_body", {"for_stmt"})) {
        PASS();
    }
    if (match("conditional_stmt_body", {"assignment", "SEMICOLON"}) ||
        match("conditional_stmt_body", {"let_expr", "SEMICOLON"})) {
        return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
    }
    if (match("conditional_inline_stmt_body", {"assignment"}) ||
        match("conditional_inline_stmt_body", {"let_expr"})) {
        return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
    }
    if (match("while_stmt", {"WHILE", "L_PAREN", "expr", "R_PAREN", "loop_stmt_body"})) {
        return attach_span(new WhileNode(RHS(2), RHS(4)), merged_rhs_span(rhs));
    }
    if (match("for_stmt", {"FOR", "L_PAREN", "IDENTIFIER", "IN", "expr", "R_PAREN", "loop_stmt_body"})) {
        return attach_span(new ForNode(TOKEN(2), RHS(4), RHS(6)), merged_rhs_span(rhs));
    }
    if (match("loop_stmt_body", {"block"}) ||
        match("loop_stmt_body", {"return_stmt"}) ||
        match("loop_stmt_body", {"if_stmt"}) ||
        match("loop_stmt_body", {"while_stmt", "SEMICOLON"}) ||
        match("loop_stmt_body", {"for_stmt", "SEMICOLON"})) {
        PASS();
    }
    if (match("loop_stmt_body", {"assignment", "SEMICOLON"}) ||
        match("loop_stmt_body", {"let_expr", "SEMICOLON"})) {
        return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
    }

    // Expression forwarding
    if (match("expr", {"assignment"}) ||
        match("expr", {"let_expr"}) ||
        match("expr", {"if_expr"}) ||
        match("expr", {"while_expr"}) ||
        match("expr", {"for_expr"})) {
        PASS();
    }

    // Assignment and binary operators
    if (match("assignment", {"logical_or"})) {
        PASS();
    }
    if (match("assignment", {"assignable", "COLON", "EQUAL", "expr"})) {
        return attach_span(new AssignmentNode(RHS(0), RHS(3)), merged_rhs_span(rhs));
    }
    if (match("assignable", {"IDENTIFIER"})) {
        return attach_span(new VariableNode(TOKEN(0)), rhs[0].span);
    }
    if (match("assignable", {"member_access"})) {
        PASS();
    }
    if (match("logical_or", {"logical_and"}) ||
        match("logical_and", {"equality"}) ||
        match("equality", {"type_relation"}) ||
        match("type_relation", {"relational"}) ||
        match("relational", {"concatenation"}) ||
        match("concatenation", {"additive"}) ||
        match("additive", {"multiplicative"}) ||
        match("multiplicative", {"unary"}) ||
        match("unary", {"primary"})) {
        PASS();
    }
    if (match("logical_or", {"logical_or", "OR", "logical_and"})) {
        BIN_OP("|");
    }
    if (match("logical_and", {"logical_and", "AND", "equality"})) {
        BIN_OP("&");
    }
    if (match("equality", {"type_relation", "EQUALITY", "type_relation"})) {
        BIN_OP("==");
    }
    if (match("equality", {"type_relation", "NOT_EQUAL", "type_relation"})) {
        BIN_OP("!=");
    }
    if (match("type_relation", {"relational", "IS", "type"})) {
        return attach_span(new BinaryOpNode("is", RHS(0), RHS(2)), merged_rhs_span(rhs));
    }
    if (match("type_relation", {"relational", "AS", "type"})) {
        return attach_span(new BinaryOpNode("as", RHS(0), RHS(2)), merged_rhs_span(rhs));
    }
    if (match("relational", {"concatenation", "LESS_THAN", "concatenation"})) {
        BIN_OP("<");
    }
    if (match("relational", {"concatenation", "GREATER_THAN", "concatenation"})) {
        BIN_OP(">");
    }
    if (match("relational", {"concatenation", "LESS_EQUALS", "concatenation"})) {
        BIN_OP("<=");
    }
    if (match("relational", {"concatenation", "GREATER_EQUALS", "concatenation"})) {
        BIN_OP(">=");
    }
    if (match("concatenation", {"concatenation", "CONCAT", "additive"})) {
        BIN_OP("@");
    }
    if (match("concatenation", {"concatenation", "CONCAT_SPACE", "additive"})) {
        BIN_OP("@@");
    }
    if (match("additive", {"additive", "PLUS", "multiplicative"})) {
        BIN_OP("+");
    }
    if (match("additive", {"additive", "MINUS", "multiplicative"})) {
        BIN_OP("-");
    }

    if (match("multiplicative", {"multiplicative", "STAR", "powered"})) { BIN_OP("*"); }
    if (match("multiplicative", {"multiplicative", "SLASH", "powered"})) { BIN_OP("/"); }
    if (match("multiplicative", {"multiplicative", "MODULE", "powered"})) { BIN_OP("%"); }
    if (match("multiplicative", {"powered"})) { PASS(); }
    if (match("powered", {"unary", "POWER", "powered"})) {
        BIN_OP("^");
    }
    if (match("powered", {"unary"})) {
        PASS();
    }
    if (match("unary", {"MINUS", "unary"})) {
        UNARY_OP("-");
    }
    if (match("unary", {"PLUS", "unary"})) {
        UNARY_OP("+");
    }
    if (match("unary", {"NOT", "unary"})) {
        UNARY_OP("!");
    }

    // Primary expressions
    if (match("primary", {"NUMBER"})) {
        auto num = parse_number(rhs[0].token_text);
        return attach_span(new NumberNode(num.first, num.second), rhs[0].span);
    }
    if (match("primary", {"STRING"})) {
        std::string s = TOKEN(0);
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"') {
            s = s.substr(1, s.size() - 2);
        }
        return attach_span(new StringNode(s), rhs[0].span);
    }
    if (match("primary", {"TRUE"})) {
        return attach_span(new BoolNode(true), rhs[0].span);
    }
    if (match("primary", {"FALSE"})) {
        return attach_span(new BoolNode(false), rhs[0].span);
    }
    if (match("primary", {"IDENTIFIER"})) {
        return attach_span(new VariableNode(TOKEN(0)), rhs[0].span);
    }
    if (match("primary", {"global_call"}) ||
        match("primary", {"if_expr"}) ||
        match("primary", {"member_call"}) ||
        match("primary", {"member_access"}) ||
        match("primary", {"index_access"}) ||
        match("primary", {"lambda_expr"}) ||
        match("primary", {"function_expr"}) ||
        match("primary", {"vector_expr"}) ||
        match("primary", {"new_expr"})) {
        PASS();
    }
    if (match("primary", {"L_PAREN", "expr", "R_PAREN"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("global_call", {"IDENTIFIER", "L_PAREN", "arg_list_opt", "R_PAREN"})) {
        return attach_span(new FunctionCallNode(TOKEN(0), take_arg_items(RHS(2))), merged_rhs_span(rhs));
    }
    if (match("member_access", {"access_base", "DOT", "IDENTIFIER"}) ||
        match("member_access", {"member_access", "DOT", "IDENTIFIER"})) {
        return attach_span(new MemberAccessNode(RHS(0), TOKEN(2)), merged_rhs_span(rhs));
    }
    if (match("access_base", {"IDENTIFIER"})) {
        return attach_span(new VariableNode(TOKEN(0)), rhs[0].span);
    }
    if (match("access_base", {"new_expr"})) {
        PASS();
    }
    if (match("access_base", {"L_PAREN", "expr", "R_PAREN"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("member_call", {"access_base", "DOT", "IDENTIFIER", "L_PAREN", "arg_list_opt", "R_PAREN"}) ||
        match("member_call", {"member_access", "DOT", "IDENTIFIER", "L_PAREN", "arg_list_opt", "R_PAREN"})) {
        return attach_span(new FunctionCallNode(TOKEN(2), take_arg_items(RHS(4)), RHS(0)), merged_rhs_span(rhs));
    }
    if (match("index_access", {"access_base", "L_SQUARE_BRACK", "expr", "R_SQUARE_BRACK"}) ||
        match("index_access", {"member_access", "L_SQUARE_BRACK", "expr", "R_SQUARE_BRACK"}) ||
        match("index_access", {"global_call", "L_SQUARE_BRACK", "expr", "R_SQUARE_BRACK"})) {
        return attach_span(new IndexAccessNode(RHS(0), RHS(2)), merged_rhs_span(rhs));
    }
    if (match("new_expr", {"NEW", "IDENTIFIER", "L_PAREN", "arg_list_opt", "R_PAREN"})) {
        return attach_span(new NewNode(TOKEN(1), take_arg_items(RHS(3))), merged_rhs_span(rhs));
    }
    if (match("lambda_expr", {"L_PAREN", "param_list_opt", "R_PAREN", "ARROW", "expr"})) {
        return build_lambda(dynamic_cast<ParamListNode*>(RHS(1)), nullptr, RHS(4), merged_rhs_span(rhs));
    }
    if (match("lambda_expr", {"L_PAREN", "param_list_opt", "R_PAREN", "COLON", "type", "ARROW", "expr"})) {
        return build_lambda(dynamic_cast<ParamListNode*>(RHS(1)), dynamic_cast<TypeNode*>(RHS(4)), RHS(6), merged_rhs_span(rhs));
    }
    if (match("vector_expr", {"L_SQUARE_BRACK", "R_SQUARE_BRACK"})) {
        return attach_span(new VectorLiteralNode({}), merged_rhs_span(rhs));
    }
    if (match("vector_expr", {"L_SQUARE_BRACK", "vector_items", "R_SQUARE_BRACK"})) {
        return attach_span(new VectorLiteralNode(take_arg_items(RHS(1))), merged_rhs_span(rhs));
    }
    if (match("vector_expr", {"L_SQUARE_BRACK", "additive", "OR", "IDENTIFIER", "IN", "expr", "R_SQUARE_BRACK"})) {
        return attach_span(new VectorComprehensionNode(RHS(1), TOKEN(3), RHS(5)), merged_rhs_span(rhs));
    }
    if (match("vector_items", {"expr"})) {
        BUILD_ARG_LIST(RHS(0));
    }
    if (match("vector_items", {"vector_items", "COMMA", "expr"})) {
        BUILD_BLOCK(RHS(0), RHS(2));
    }

    // Let expressions
    if (match("let_expr", {"LET", "let_bindings", "IN", "loop_expr_body"})) {
        return build_nested_let(dynamic_cast<LetBindingsNode*>(RHS(1)), RHS(3), merged_rhs_span(rhs));
    }
    if (match("let_bindings", {"let_binding"})) {
        auto* binding = dynamic_cast<LetBindingNode*>(RHS(0));
        std::vector<LetBindingNode*> bindings;
        if (binding) {
            bindings.push_back(binding);
        }
        return attach_span(new LetBindingsNode(std::move(bindings)), merged_rhs_span(rhs));
    }
    if (match("let_bindings", {"let_bindings", "COMMA", "let_binding"})) {
        auto* left = dynamic_cast<LetBindingsNode*>(RHS(0));
        auto* right = dynamic_cast<LetBindingNode*>(RHS(2));
        std::vector<LetBindingNode*> bindings;
        if (left) {
            bindings = std::move(left->bindings);
            left->bindings.clear();
            delete left;
        }
        if (right) {
            bindings.push_back(right);
        }
        return attach_span(new LetBindingsNode(std::move(bindings)), merged_rhs_span(rhs));
    }
    if (match("let_binding", {"IDENTIFIER", "opt_type_annotation", "EQUAL", "expr"})) {
        bool withInnerLet = match("let_binding", {"LET", "IDENTIFIER", "opt_type_annotation", "EQUAL", "expr"});
        size_t nameIndex = withInnerLet ? 1 : 0;
        size_t typeIndex = withInnerLet ? 2 : 1;
        size_t exprIndex = withInnerLet ? 4 : 3;
        auto* typeNode = take_type_annotation(RHS(typeIndex));
        bool hasExplicitType = typeNode != nullptr;
        Type* declaredType = typeNode ? typeNode->type : nullptr;
        std::string declaredTypeName = typeNode ? typeNode->typeName : "";
        delete typeNode;
        return attach_span(
            new LetBindingNode(
                TOKEN(nameIndex),
                RHS(exprIndex),
                declaredType,
                declaredTypeName,
                hasExplicitType || !declaredTypeName.empty()),
            merged_rhs_span(rhs));
    }

    // If / loop expressions
    if (match("if_expr", {"IF", "L_PAREN", "expr", "R_PAREN", "conditional_expr_body", "conditional_expr_tail"})) {
        return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
    }
    if (match("conditional_expr_tail", {})) {
        return nullptr;
    }
    if (match("conditional_expr_tail", {"ELSE", "conditional_expr_body"})) {
        return pass_with_span(RHS(1), merged_rhs_span(rhs));
    }
    if (match("conditional_expr_tail", {"ELIF", "L_PAREN", "expr", "R_PAREN", "conditional_expr_body", "conditional_expr_tail"})) {
        return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
    }
    if (match("conditional_expr_body", {"block"}) || match("conditional_expr_body", {"expr"})) {
        PASS();
    }
    if (match("while_expr", {"WHILE", "L_PAREN", "expr", "R_PAREN", "loop_expr_body"})) {
        return attach_span(new WhileNode(RHS(2), RHS(4)), merged_rhs_span(rhs));
    }
    if (match("for_expr", {"FOR", "L_PAREN", "IDENTIFIER", "IN", "expr", "R_PAREN", "loop_expr_body"})) {
        return attach_span(new ForNode(TOKEN(2), RHS(4), RHS(6)), merged_rhs_span(rhs));
    }
    if (match("loop_expr_body", {"block"}) || match("loop_expr_body", {"expr"})) {
        PASS();
    }

    // Arguments
    if (match("arg_list_opt", {})) {
        return attach_span(new BlockNode({}), merged_rhs_span(rhs));
    }
    if (match("arg_list_opt", {"arg_list"})) {
        PASS();
    }
    if (match("arg_list", {"expr"})) {
        BUILD_ARG_LIST(RHS(0));
    }
    if (match("arg_list", {"arg_list", "COMMA", "expr"})) {
        BUILD_BLOCK(RHS(0), RHS(2));
    }

    const auto& production = grammar->get_productions()[pid];
    std::cerr << "ASTBuilder: unhandled production " << pid << " ("
              << grammar->symtab[production.lhs].name << " ->";
    for (auto symbol : production.rhs) {
        std::cerr << " " << grammar->symtab[symbol].name;
    }
    std::cerr << ")\n";
    return nullptr;
}