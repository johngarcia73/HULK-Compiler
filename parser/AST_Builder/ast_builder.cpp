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
            // Extract declarations BlockNode (or create empty)
            std::vector<ASTNode*> decls;
            if (auto* block = dynamic_cast<BlockNode*>(RHS(0))) {
                decls = std::move(block->stmts);
                delete block;
            } else if (RHS(0)) decls.push_back(RHS(0));
            
            // Extract statements BlockNode
            std::vector<ASTNode*> stmts;
            if (auto* block = dynamic_cast<BlockNode*>(RHS(1))) {
                stmts = std::move(block->stmts);
                delete block;
            } else if (RHS(1)) stmts.push_back(RHS(1));
            
            return new ProgramNode(std::move(decls), std::move(stmts));
        }

        case 1: // declarations : /* empty */
            EMPTY_BLOCK();

        case 2: // declarations : declarations declaration
            BUILD_BLOCK(RHS(0), RHS(1));

        case 3: // declaration : function_decl
            PASS();

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
            // Default return type is Number
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

        // Parameters
        case 6: // param_list_opt : /* empty */
            return new ParamListNode({}, {});
        case 7: // param_list_opt : param_list
            PASS();
        case 8: { // param_list : IDENTIFIER COLON type
            std::string name = TOKEN(0);
            auto* typeNode = dynamic_cast<TypeNode*>(RHS(2));
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            return new ParamListNode({name}, {type});
        }
        case 9: { // param_list : param_list COMMA IDENTIFIER COLON type
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
        case 10: return new TypeNode(NumberType::instance());
        case 11: return new TypeNode(BoolType::instance());
        case 12: return new TypeNode(StringType::instance());

        // ========== Statements and blocks ==========
        case 13: EMPTY_BLOCK();                               // statements : /* empty */
        case 14: BUILD_BLOCK(RHS(0), RHS(1));                // statements : statements statement
        case 15: return new ExprStmtNode(RHS(0));            // statement : expr SEMICOLON
        case 16: PASS();                                      // statement : block
        case 17: { // block : L_CURL_BRACK statements R_CURL_BRACK
            if (auto* b = dynamic_cast<BlockNode*>(RHS(1))) {
                auto v = std::move(b->stmts);
                delete b;
                return new BlockNode(std::move(v));
            } else {
                return new BlockNode(RHS(1) ? std::vector{RHS(1)} : std::vector<ASTNode*>{});
            }
        }

        // ========== Expression forwarding ==========
        case 18: // expr -> relational
        case 19: // expr -> let_expr
        case 20: // expr -> if_expr
        case 21: // expr -> call_expr
        case 22: // relational -> concatenation
        case 28: // concatenation -> additive
        case 30: // additive -> multiplicative
        case 33: // multiplicative -> unary
        case 38: // unary -> primary
            PASS();

        // ========== Binary operators ==========
        case 23: BIN_OP("<");   // concatenation LESS_THAN concatenation
        case 24: BIN_OP(">");
        case 25: BIN_OP("<=");
        case 26: BIN_OP(">=");
        case 27: BIN_OP("==");
        case 29: BIN_OP("@");   // concatenation CONCAT additive
        case 31: BIN_OP("+");   // additive PLUS multiplicative
        case 32: BIN_OP("-");
        case 34: BIN_OP("*");   // multiplicative STAR unary
        case 35: BIN_OP("/");

        // ========== Unary operators ==========
        case 36: UNARY_OP("-");
        case 37: UNARY_OP("+");

        // ========== Primaries ==========
        case 39: { // NUMBER
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }
        case 40: { // STRING
            std::string s = TOKEN(0);
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return new StringNode(s);
        }
        case 41: // IDENTIFIER
            return new VariableNode(TOKEN(0));
        case 42: // L_PAREN expr R_PAREN
            return RHS(1);

        // ========== Let expression ==========
        case 43: // LET IDENTIFIER EQUAL expr IN expr
            return new LetNode(TOKEN(1), RHS(3), RHS(5));

        // ========== If expression ==========
        case 44: // IF L_PAREN expr R_PAREN expr ELSE expr
            return new IfNode(RHS(2), RHS(4), RHS(6));

        // ========== Function call ==========
        case 45: {
            std::string name = TOKEN(0);
            std::vector<ASTNode*> args;
            if (auto* argBlock = dynamic_cast<BlockNode*>(RHS(2))) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            } else if (RHS(2)) args.push_back(RHS(2));
            return new FunctionCallNode(name, std::move(args));
        }

        // ========== Argument lists ==========
        case 46: // arg_list_opt : /* empty */
            EMPTY_BLOCK();
        case 47: // arg_list_opt : arg_list
            PASS();
        case 48: // arg_list -> expr
            BUILD_ARG_LIST(RHS(0));
        case 49: // arg_list -> arg_list COMMA expr
            BUILD_BLOCK(RHS(0), RHS(2));

        // ========== Dummy production (program') ==========
        case 50: // program' -> program
            return RHS(0); 

        default:
            std::cerr << "ASTBuilder: unhandled production " << pid << "\n";
            return nullptr;
    }
}