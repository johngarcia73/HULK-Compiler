#include "ast_builder.hpp"
#include "ast_bulder_utils.hpp"
#include <cassert>
#include <iostream>
#include <algorithm>

// Utility functions
static long long parse_int(const std::optional<std::string>& s) {
    if (!s) return 0;
    try { return std::stoll(*s); }
    catch(...) { return 0; }
}

static Type* tokenToType(const Value& v) {
    std::string name = v.token_text.value_or("");
    if (name == "NUMBER_TYPE") return NumberType::instance();
    if (name == "BOOL_TYPE")   return BoolType::instance();
    if (name == "STRING_TYPE") return StringType::instance();
    return nullptr;
}

// ASTBuilder::build - production dispatcher
// ----------------------------------------------------------------------
ASTNode* ASTBuilder::build(size_t pid, const std::vector<Value>& rhs) {
    switch (pid) {
        // ========== Program structure ==========
        case 0: { // program : declarations statements
            std::vector<ASTNode*> decls;
            if (auto* block = dynamic_cast<BlockNode*>(RHS(0))) {
                decls = std::move(block->stmts);
                delete block;
            } else if (RHS(0)) decls.push_back(RHS(0));
            std::vector<ASTNode*> stmts;
            if (auto* block = dynamic_cast<BlockNode*>(RHS(1))) {
                stmts = std::move(block->stmts);
                delete block;
            } else if (RHS(1)) stmts.push_back(RHS(1));
            return new ProgramNode(std::move(decls), std::move(stmts));
        }

        case 1: EMPTY_BLOCK();                      // declarations : ε
        case 2: BUILD_BLOCK(RHS(0), RHS(1));       // declarations : declarations declaration
        case 3: PASS();                             // declaration : function_decl

        // ========== Function declarations ==========
        case 4: { // function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            Type* retType = NumberType::instance();
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, RHS(5));
        }

        case 5: { // function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
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
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, RHS(7));
        }

        case 6: { // inline sin tipo: FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN ARROW expr SEMICOLON
            std::string name = TOKEN(1);
            auto* paramsNode = dynamic_cast<ParamListNode*>(RHS(3));
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            Type* retType = nullptr; // se inferirá después
            ASTNode* exprBody = RHS(5);
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, exprBody, true);
        }

        case 7: { // inline con tipo: FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type ARROW expr SEMICOLON
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
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, exprBody, true);
        }

        // ========== Parameters ==========
        case 8:  return new ParamListNode({}, {});               // param_list_opt : ε
        case 9:  PASS();                                          // param_list_opt : param_list
        case 10: { // param_list : IDENTIFIER COLON type
            std::string name = TOKEN(0);
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(2));
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            return new ParamListNode({name}, {type});
        }
        case 11: { // param_list : param_list COMMA IDENTIFIER COLON type
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
            return new ParamListNode(std::move(params), std::move(types));
        }

        // ========== Types ==========
        case 12: return new TypeNode(NumberType::instance());
        case 13: return new TypeNode(BoolType::instance());
        case 14: return new TypeNode(StringType::instance());

        // ========== Statements and blocks ==========
        case 15: EMPTY_BLOCK();                               // statements : ε
        case 16: BUILD_BLOCK(RHS(0), RHS(1));                // statements : statements statement
        case 17: return new ExprStmtNode(RHS(0));            // statement : expr SEMICOLON
        case 18: PASS();                                      // statement : block
        case 19: { // block : L_CURL_BRACK statements R_CURL_BRACK
            if (auto* b = dynamic_cast<BlockNode*>(RHS(1))) {
                auto v = std::move(b->stmts);
                delete b;
                return new BlockNode(std::move(v));
            } else {
                return new BlockNode(RHS(1) ? std::vector{RHS(1)} : std::vector<ASTNode*>{});
            }
        }

        // ========== Expression forwarding ==========
        case 20: // expr -> relational
        case 21: // expr -> let_expr
        case 22: // expr -> if_expr
        case 23: // expr -> call_expr
        case 24: // relational -> concatenation
        case 30: // concatenation -> additive
        case 32: // additive -> multiplicative
        case 35: // multiplicative -> unary
        case 40: // unary -> primary
            PASS();

        // ========== Binary operators ==========
        case 25: BIN_OP("<");   // relational -> concatenation LESS_THAN concatenation
        case 26: BIN_OP(">");
        case 27: BIN_OP("<=");
        case 28: BIN_OP(">=");
        case 29: BIN_OP("==");
        case 31: BIN_OP("@");   // concatenation -> concatenation CONCAT additive
        case 33: BIN_OP("+");   // additive -> additive PLUS multiplicative
        case 34: BIN_OP("-");
        case 36: BIN_OP("*");   // multiplicative -> multiplicative STAR unary
        case 37: BIN_OP("/");

        // ========== Unary operators ==========
        case 38: UNARY_OP("-");
        case 39: UNARY_OP("+");

        // ========== Primaries ==========
        case 41: { // NUMBER
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }
        case 42: { // STRING
            std::string s = TOKEN(0);
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return new StringNode(s);
        }
        case 43: // IDENTIFIER
            return new VariableNode(TOKEN(0));
        case 44: // L_PAREN expr R_PAREN
            return RHS(1);

        // ========== Let expression ==========
        case 45: // LET IDENTIFIER EQUAL expr IN expr
            return new LetNode(TOKEN(1), RHS(3), RHS(5));

        // ========== If expression ==========
        case 46: // IF L_PAREN expr R_PAREN expr ELSE expr
            return new IfNode(RHS(2), RHS(4), RHS(6));

        // ========== Function call ==========
        case 47: { // call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return new FunctionCallNode(name, std::move(args));
        }

        // ========== Argument lists ==========
        case 48: EMPTY_BLOCK();          // arg_list_opt : ε
        case 49: PASS();                 // arg_list_opt : arg_list
        case 50: BUILD_ARG_LIST(RHS(0)); // arg_list -> expr
        case 51: BUILD_BLOCK(RHS(0), RHS(2)); // arg_list -> arg_list COMMA expr

        // ========== Dummy production ==========
        case 52: // program' -> program
            return RHS(0);

        default:
            std::cerr << "ASTBuilder: unhandled production " << pid << "\n";
            return nullptr;
    }
}