#include "ast_builder.hpp"
#include "ast_bulder_utils.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>

// Utility functions
static std::pair<std::string, std::string> parse_number(const std::optional<std::string>& s) {
    if (!s) return {"0", "int"};
    std::string raw = *s;
    std::string kind = "int";
    if (raw.find('.') != std::string::npos || raw.find('e') != std::string::npos || raw.find('E') != std::string::npos) {
        kind = "float";
    }
    return {raw, kind};
}

static Type* tokenToType(const Value& v) {
    std::string name = v.token_text.value_or("");
    if (name == "NUMBER_TYPE") return NumberType::instance();
    if (name == "BOOL_TYPE")   return BoolType::instance();
    if (name == "STRING_TYPE") return StringType::instance();
    return nullptr;
}

static SourceSpan merged_rhs_span(const std::vector<Value>& rhs) {
    SourceSpan combined;
    for (const auto& value : rhs) {
        combined = SourceSpan::merge(combined, value_span(value));
    }
    return combined;
}

static ASTNode* build_nested_let(
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

        result = attach_span(new LetNode(binding->name, init, result), let_span);
    }

    delete bindings_node;
    return attach_span(result, full_span);
}

static ASTNode* pass_with_span(ASTNode* node, const SourceSpan& span) {
    return attach_span(node, span);
}

// ASTBuilder::build - production dispatcher
// ----------------------------------------------------------------------
ASTNode* ASTBuilder::build(size_t pid, const std::vector<Value>& rhs) {
    switch (pid) {
        // ========== Program structure ==========
        case 0: { // program : top_level_items
            if (auto* block = dynamic_cast<BlockNode*>(RHS(0))) {
                auto items = std::move(block->stmts);
                SourceSpan items_span = block->span;
                delete block;
                std::vector<ASTNode*> decls;
                std::vector<ASTNode*> stmts;
                for (auto* item : items) {
                    if (dynamic_cast<FunctionDeclNode*>(item))
                        decls.push_back(item);
                    else
                        stmts.push_back(item);
                }
                return attach_span(new ProgramNode(std::move(decls), std::move(stmts)), items_span);
            }
            return attach_span(new ProgramNode({}, {}), merged_rhs_span(rhs));
        }

        case 1: EMPTY_BLOCK();                      // top_level_items : ε
        case 2: BUILD_BLOCK(RHS(0), RHS(1));       // top_level_items : top_level_items top_level_item

        case 3:  // top_level_item : declaration
        case 4:  // top_level_item : statement
            PASS();

        case 5: PASS();                             // declaration : function_decl

        // ========== Function declarations ==========
        case 6: { // function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            Type* retType = VoidType::instance();
            auto* node = attach_span(
                new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, RHS(5)),
                merged_rhs_span(rhs));
            node->declaredReturnType = nullptr;
            node->hasExplicitReturnType = false;
            return node;
        }

        case 7: { // function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(6));
            Type* retType = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            return attach_span(
                new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, RHS(7)),
                merged_rhs_span(rhs));
        }

        case 8: { // inline sin tipo: FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN ARROW expr SEMICOLON
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            Type* retType = nullptr;
            ASTNode* exprBody = RHS(6);
            return attach_span(
                new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, exprBody, true),
                merged_rhs_span(rhs));
        }

        case 9: { // inline con tipo: FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type ARROW expr SEMICOLON
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(6));
            Type* retType = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            ASTNode* exprBody = RHS(8);
            return attach_span(
                new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, exprBody, true),
                merged_rhs_span(rhs));
        }

        // ========== Parameters ==========
        case 10: return attach_span(new ParamListNode({}, {}), merged_rhs_span(rhs)); // param_list_opt : ε
        case 11: PASS();                                          // param_list_opt : param_list
        case 12: { // param_list : IDENTIFIER COLON type
            std::string name = TOKEN(0);
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(2));
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            return attach_span(new ParamListNode({name}, {type}), merged_rhs_span(rhs));
        }
        case 13: { // param_list : param_list COMMA IDENTIFIER COLON type
            auto* left = dynamic_cast<ParamListNode*>(RHS(0));
            std::string name = TOKEN(2);
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(4));
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            std::vector<std::string> params = std::move(left->params);
            std::vector<Type*> types = std::move(left->paramTypes);
            delete left;
            params.push_back(name);
            types.push_back(type);
            return attach_span(new ParamListNode(std::move(params), std::move(types)), merged_rhs_span(rhs));
        }

        // ========== Types ==========
        case 14: return attach_span(new TypeNode(NumberType::instance()), rhs[0].span);
        case 15: return attach_span(new TypeNode(BoolType::instance()), rhs[0].span);
        case 16: return attach_span(new TypeNode(StringType::instance()), rhs[0].span);

        // ========== Statements ==========
        case 17: return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs)); // statement : assignment SEMICOLON
        case 18: return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs)); // statement : let_expr SEMICOLON
        case 19: PASS();                                      // statement : block
        case 20: PASS();                                      // statement : return_stmt
        case 21: PASS();                                      // statement : if_stmt
        case 22: PASS();                                      // statement : while_stmt
        case 23: PASS();                                      // statement : for_stmt

        case 24: { // return_stmt : RETURN expr SEMICOLON
            return attach_span(new ReturnNode(RHS(1)), merged_rhs_span(rhs));
        }

        case 25: { // block : L_CURL_BRACK statements R_CURL_BRACK
            if (auto* b = dynamic_cast<BlockNode*>(RHS(1))) {
                auto v = std::move(b->stmts);
                SourceSpan block_span = b->span;
                delete b;
                return attach_span(new BlockNode(std::move(v)), block_span);
            } else {
                return attach_span(
                    new BlockNode(RHS(1) ? std::vector{RHS(1)} : std::vector<ASTNode*>{}),
                    merged_rhs_span(rhs));
            }
        }

        case 26: EMPTY_BLOCK();                               // statements : ε
        case 27: BUILD_BLOCK(RHS(0), RHS(1));                // statements : statements statement

        // ========== If statement ==========
        case 28: { // if_stmt : IF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
        }
        case 29: // conditional_stmt_tail : ε
            return nullptr;
        case 30: // conditional_stmt_tail : ELSE conditional_stmt_body
            return pass_with_span(RHS(1), merged_rhs_span(rhs));
        case 31: { // conditional_stmt_tail : ELIF L_PAREN expr R_PAREN conditional_stmt_body conditional_stmt_tail
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
        }
        case 32: // conditional_stmt_body : block
            PASS();
        case 33: // conditional_stmt_body : assignment SEMICOLON
            return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
        case 34: // conditional_stmt_body : let_expr SEMICOLON
            return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
        case 35: // conditional_stmt_body : return_stmt
        case 36: // conditional_stmt_body : if_stmt
        case 37: // conditional_stmt_body : while_stmt
        case 38: // conditional_stmt_body : for_stmt
            PASS();

        // ========== Loop statements ==========
        case 39: { // while_stmt : WHILE L_PAREN expr R_PAREN loop_stmt_body
            return attach_span(new WhileNode(RHS(2), RHS(4)), merged_rhs_span(rhs));
        }
        case 40: { // for_stmt : FOR L_PAREN IDENTIFIER IN expr R_PAREN loop_stmt_body
            return attach_span(new ForNode(TOKEN(2), RHS(4), RHS(6)), merged_rhs_span(rhs));
        }
        case 41: // loop_stmt_body : block
            PASS();
        case 42: // loop_stmt_body : assignment SEMICOLON
            return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
        case 43: // loop_stmt_body : let_expr SEMICOLON
            return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs));
        case 44: // loop_stmt_body : return_stmt
        case 45: // loop_stmt_body : if_stmt
        case 46: // loop_stmt_body : while_stmt
        case 47: // loop_stmt_body : for_stmt
            PASS();

        // ========== Expression forwarding ==========
        case 48: // expr -> assignment
        case 49: // expr -> let_expr
        case 50: // expr -> if_expr
        case 51: // expr -> while_expr
        case 52: // expr -> for_expr
            PASS();

        // ========== Assignment ==========
        case 53: PASS(); // assignment -> logical_or
        case 54: { // assignment : IDENTIFIER COLON EQUAL expr
            return attach_span(new AssignmentNode(TOKEN(0), RHS(3)), merged_rhs_span(rhs));
        }

        // ========== Boolean operators ==========
        case 55: PASS(); // logical_or -> logical_and
        case 56: BIN_OP("|");
        case 57: PASS(); // logical_and -> equality
        case 58: BIN_OP("&");

        // ========== Equality and relational operators ==========
        case 59: PASS(); // equality -> relational
        case 60: BIN_OP("==");
        case 61: BIN_OP("!=");
        case 62: PASS(); // relational -> concatenation
        case 63: BIN_OP("<");
        case 64: BIN_OP(">");
        case 65: BIN_OP("<=");
        case 66: BIN_OP(">=");

        // ========== Concatenation ==========
        case 67: PASS(); // concatenation -> additive
        case 68: BIN_OP("@");

        // ========== Additive ==========
        case 69: PASS(); // additive -> multiplicative
        case 70: BIN_OP("+");
        case 71: BIN_OP("-");

        // ========== Multiplicative ==========
        case 72: PASS(); // multiplicative -> unary
        case 73: BIN_OP("*");
        case 74: BIN_OP("/");
        case 75: BIN_OP("%");

        // ========== Unary ==========
        case 76: UNARY_OP("-");
        case 77: UNARY_OP("+");
        case 78: UNARY_OP("!");
        case 79: PASS(); // unary -> primary

        // ========== Primary ==========
        case 80: { // NUMBER
            auto num = parse_number(rhs[0].token_text);
            return attach_span(new NumberNode(num.first, num.second), rhs[0].span);
        }
        case 81: { // STRING
            std::string s = TOKEN(0);
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return attach_span(new StringNode(s), rhs[0].span);
        }
        case 82:
            return attach_span(new BoolNode(true), rhs[0].span);
        case 83:
            return attach_span(new BoolNode(false), rhs[0].span);
        case 84: // IDENTIFIER
            return attach_span(new VariableNode(TOKEN(0)), rhs[0].span);
        case 85: { // IDENTIFIER L_PAREN arg_list_opt R_PAREN  (llamada a función)
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return attach_span(new FunctionCallNode(name, std::move(args)), merged_rhs_span(rhs));
        }
        case 86: // L_PAREN expr R_PAREN
            return RHS(1);

        // ========== Let expression ==========
        case 87: { // let_expr : LET let_bindings IN expr
            auto* bindings = dynamic_cast<LetBindingsNode*>(RHS(1));
            return build_nested_let(bindings, RHS(3), merged_rhs_span(rhs));
        }
        case 88: { // let_bindings : let_binding
            auto* binding = dynamic_cast<LetBindingNode*>(RHS(0));
            std::vector<LetBindingNode*> bindings;
            if (binding) {
                bindings.push_back(binding);
            }
            return attach_span(new LetBindingsNode(std::move(bindings)), merged_rhs_span(rhs));
        }
        case 89: { // let_bindings : let_bindings COMMA let_binding
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
        case 90: { // let_binding : IDENTIFIER EQUAL expr
            return attach_span(new LetBindingNode(TOKEN(0), RHS(2)), merged_rhs_span(rhs));
        }
        case 91: { // let_binding : LET IDENTIFIER EQUAL expr
            return attach_span(new LetBindingNode(TOKEN(1), RHS(3)), merged_rhs_span(rhs));
        }

        // ========== If expression ==========
        case 92: { // if_expr : IF L_PAREN expr R_PAREN conditional_expr_body conditional_expr_tail
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
        }
        case 93: // conditional_expr_tail : ε
            return nullptr;
        case 94: // conditional_expr_tail : ELSE conditional_expr_body
            return pass_with_span(RHS(1), merged_rhs_span(rhs));
        case 95: { // conditional_expr_tail : ELIF L_PAREN expr R_PAREN conditional_expr_body conditional_expr_tail
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(5)), merged_rhs_span(rhs));
        }
        case 96: // conditional_expr_body : block
        case 97: // conditional_expr_body : expr
            PASS();

        // ========== Loop expressions ==========
        case 98: { // while_expr : WHILE L_PAREN expr R_PAREN loop_expr_body
            return attach_span(new WhileNode(RHS(2), RHS(4)), merged_rhs_span(rhs));
        }
        case 99: { // for_expr : FOR L_PAREN IDENTIFIER IN expr R_PAREN loop_expr_body
            return attach_span(new ForNode(TOKEN(2), RHS(4), RHS(6)), merged_rhs_span(rhs));
        }
        case 100: // loop_expr_body : block
        case 101: // loop_expr_body : expr
            PASS();

        // ========== Call expression (obsoleta, mantenida por compatibilidad) ==========
        case 102: { // call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return attach_span(new FunctionCallNode(name, std::move(args)), merged_rhs_span(rhs));
        }

        // ========== Argument lists ==========
        case 103: EMPTY_BLOCK();          // arg_list_opt : ε
        case 104: PASS();                 // arg_list_opt : arg_list
        case 105: BUILD_ARG_LIST(RHS(0)); // arg_list -> expr
        case 106: BUILD_BLOCK(RHS(0), RHS(2)); // arg_list -> arg_list COMMA expr

        // ========== Dummy production (program') ==========
        case 107: // program' -> program
            return RHS(0);

        default:
            std::cerr << "ASTBuilder: unhandled production " << pid << "\n";
            return nullptr;
    }
}
