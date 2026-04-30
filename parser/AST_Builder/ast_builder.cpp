#include "ast_builder.hpp"
#include "ast_bulder_utils.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>

struct ParsedNumber {
    std::string value = "0";
    NumberKind kind = NumberKind::Int;
};

static ParsedNumber parse_number(const std::optional<std::string>& s) {
    if (!s) {
        return {};
    }

    ParsedNumber parsed;
    parsed.value = *s;
    parsed.kind = classify_number_kind(parsed.value);
    return parsed;
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

// ASTBuilder::build - production dispatcher
// ----------------------------------------------------------------------
ASTNode* ASTBuilder::build(size_t pid, const std::vector<Value>& rhs) {
    switch (pid) {
        // ========== Program structure ==========
        case 0: { // program : top_level_items
            if (auto* block = dynamic_cast<BlockNode*>(RHS(0))) {
                auto items = std::move(block->stmts);
                const SourceSpan program_span = block->span;
                delete block;
                std::vector<ASTNode*> decls;
                std::vector<ASTNode*> stmts;
                for (auto* item : items) {
                    if (dynamic_cast<FunctionDeclNode*>(item))
                        decls.push_back(item);
                    else
                        stmts.push_back(item);
                }
                return attach_span(new ProgramNode(std::move(decls), std::move(stmts)), program_span);
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
        case 17: return attach_span(new ExprStmtNode(RHS(0)), merged_rhs_span(rhs)); // statement : expr SEMICOLON
        case 18: PASS();                                      // statement : block
        case 19: PASS();                                      // statement : return_stmt
        case 20: PASS();                                      // statement : if_stmt

        case 21: { // return_stmt : RETURN expr SEMICOLON
            return attach_span(new ReturnNode(RHS(1)), merged_rhs_span(rhs));
        }

        case 22: { // block : L_CURL_BRACK statements R_CURL_BRACK
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

        case 23: EMPTY_BLOCK();                               // statements : ε
        case 24: BUILD_BLOCK(RHS(0), RHS(1));                // statements : statements statement

        // ========== If statement ==========
        case 25: { // if_stmt : IF L_PAREN expr R_PAREN block
            return attach_span(new IfNode(RHS(2), RHS(4), nullptr), merged_rhs_span(rhs));
        }
        case 26: { // if_stmt : IF L_PAREN expr R_PAREN block ELSE block
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(6)), merged_rhs_span(rhs));
        }

        // ========== Expression forwarding ==========
        case 27: // expr -> relational
        case 28: // expr -> let_expr
        case 29: // expr -> if_expr
            PASS();

        // ========== Relational operators ==========
        case 30: PASS(); // relational -> concatenation
        case 31: BIN_OP("<");
        case 32: BIN_OP(">");
        case 33: BIN_OP("<=");
        case 34: BIN_OP(">=");
        case 35: BIN_OP("==");

        // ========== Concatenation ==========
        case 36: PASS(); // concatenation -> additive
        case 37: BIN_OP("@");

        // ========== Additive ==========
        case 38: PASS(); // additive -> multiplicative
        case 39: BIN_OP("+");
        case 40: BIN_OP("-");

        // ========== Multiplicative ==========
        case 41: PASS(); // multiplicative -> unary
        case 42: BIN_OP("*");
        case 43: BIN_OP("/");

        // ========== Unary ==========
        case 44: UNARY_OP("-");
        case 45: UNARY_OP("+");
        case 46: PASS(); // unary -> primary

        // ========== Primary ==========
        case 47: { // NUMBER
            const ParsedNumber number = parse_number(rhs[0].token_text);
            return attach_span(new NumberNode(number.value, number.kind), rhs[0].span);
        }
        case 48: { // STRING
            std::string s = TOKEN(0);
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return attach_span(new StringNode(s), rhs[0].span);
        }
        case 49: // IDENTIFIER
            return attach_span(new VariableNode(TOKEN(0)), rhs[0].span);
        case 50: { // IDENTIFIER L_PAREN arg_list_opt R_PAREN  (llamada a función)
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return attach_span(new FunctionCallNode(name, std::move(args)), merged_rhs_span(rhs));
        }
        case 51: // L_PAREN expr R_PAREN
            return RHS(1);

        // ========== Let expression ==========
        case 52: // LET IDENTIFIER EQUAL expr IN expr
            return attach_span(new LetNode(TOKEN(1), RHS(3), RHS(5)), merged_rhs_span(rhs));

        // ========== If expression ==========
        case 53: // IF L_PAREN expr R_PAREN expr
            return attach_span(new IfNode(RHS(2), RHS(4), nullptr), merged_rhs_span(rhs));
        case 54: // IF L_PAREN expr R_PAREN expr ELSE expr
            return attach_span(new IfNode(RHS(2), RHS(4), RHS(6)), merged_rhs_span(rhs));

        // ========== Call expression (obsolete, kept for compatibility) ==========
        case 55: { // call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return attach_span(new FunctionCallNode(name, std::move(args)), merged_rhs_span(rhs));
        }

        // ========== Argument lists ==========
        case 56: EMPTY_BLOCK();          // arg_list_opt : ε
        case 57: PASS();                 // arg_list_opt : arg_list
        case 58: BUILD_ARG_LIST(RHS(0)); // arg_list -> expr
        case 59: BUILD_BLOCK(RHS(0), RHS(2)); // arg_list -> arg_list COMMA expr

        // ========== Dummy production (program') ==========
        case 60: // program' -> program
            return RHS(0);

        default:
            std::cerr << "ASTBuilder: unhandled production " << pid << "\n";
            return nullptr;
    }
}
