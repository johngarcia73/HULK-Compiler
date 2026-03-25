#include "ast_builder.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <algorithm>

//---------------------Utils--------------------------------
#define CHECK_RHS_IDX(pid, idx) do { \
    if (rhs.size() <= (idx)) { \
        std::cerr << "ASTBuilder: producción " << (pid) << " requiere índice " << (idx) \
                  << " pero rhs.size() = " << rhs.size() << "\n"; \
        return nullptr; \
    } \
} while(0)

static long long parse_int(const std::optional<std::string>& s) {
    if (!s) return 0;
    try {
        return std::stoll(*s);
    } catch(...) {
        return 0;
    }
}

Type* tokenToType(const Value& v) {
    std::string tokenName = v.token_text.value_or("");
    if (tokenName == "NUMBER_TYPE") return NumberType::instance();
    if (tokenName == "BOOL_TYPE")   return BoolType::instance();
    if (tokenName == "STRING_TYPE") return StringType::instance();
    return nullptr;
}
//---------------------------------------------------------

ASTNode* ASTBuilder::build(size_t pid, const std::vector<Value>& rhs) {
    switch (pid) {
        // 0: program : declarations statements
        case 0: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 1);
            std::vector<ASTNode*> decls;
            std::vector<ASTNode*> stmts;
            BlockNode* bn = dynamic_cast<BlockNode*>(rhs[0].node);
            if (bn) {
                for (auto p : bn->stmts) decls.push_back(p);
                bn->stmts.clear();
                delete bn;
            } else if (rhs[0].node) decls.push_back(rhs[0].node);
            BlockNode* bn2 = dynamic_cast<BlockNode*>(rhs[1].node);
            if (bn2) {
                for (auto p : bn2->stmts) stmts.push_back(p);
                bn2->stmts.clear();
                delete bn2;
            } else if (rhs[1].node) stmts.push_back(rhs[1].node);
            return new ProgramNode(std::move(decls), std::move(stmts));
        }

        // 1: declarations : /* empty */
        case 1:
            return new BlockNode({});

        // 2: declarations : declarations declaration
        case 2: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 1);
            BlockNode* left = dynamic_cast<BlockNode*>(rhs[0].node);
            std::vector<ASTNode*> items;
            if (left) {
                items = std::move(left->stmts);
                delete left;
            }
            if (rhs[1].node) items.push_back(rhs[1].node);
            return new BlockNode(std::move(items));
        }

        // 3: declaration : function_decl
        case 3:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 4: function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN block
        case 4: {
            CHECK_RHS_IDX(pid, 1); // IDENTIFIER
            CHECK_RHS_IDX(pid, 3); // param_list_opt
            CHECK_RHS_IDX(pid, 5); // block
            std::string name = rhs[1].token_text.value_or("");
            ParamListNode* paramsNode = dynamic_cast<ParamListNode*>(rhs[3].node);
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            Type* retType = NumberType::instance(); // por defecto
            ASTNode* body = rhs[5].node;
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, body);
        }

        // 5: function_decl : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN COLON type block
        case 5: {
            CHECK_RHS_IDX(pid, 1); // IDENTIFIER
            CHECK_RHS_IDX(pid, 3); // param_list_opt
            CHECK_RHS_IDX(pid, 6); // type
            CHECK_RHS_IDX(pid, 7); // block
            std::string name = rhs[1].token_text.value_or("");
            ParamListNode* paramsNode = dynamic_cast<ParamListNode*>(rhs[3].node);
            std::vector<std::string> params;
            std::vector<Type*> paramTypes;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                paramTypes = std::move(paramsNode->paramTypes);
                delete paramsNode;
            }
            TypeNode* typeNode = dynamic_cast<TypeNode*>(rhs[6].node);
            Type* retType = typeNode ? typeNode->type : NumberType::instance();
            if (typeNode) delete typeNode;
            ASTNode* body = rhs[7].node;
            return new FunctionDeclNode(name, std::move(params), std::move(paramTypes), retType, body);
        }

        // 6: param_list_opt : /* empty */
        case 6:
            return new ParamListNode({}, {});

        // 7: param_list_opt : param_list
        case 7:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 8: param_list : IDENTIFIER COLON type
        case 8: {
            CHECK_RHS_IDX(pid, 0); // IDENTIFIER
            CHECK_RHS_IDX(pid, 2); // type
            std::string name = rhs[0].token_text.value_or("");
            TypeNode* typeNode = dynamic_cast<TypeNode*>(rhs[2].node);
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            return new ParamListNode({name}, {type});
        }

        // 9: param_list : param_list COMMA IDENTIFIER COLON type
        case 9: {
            CHECK_RHS_IDX(pid, 0); // param_list
            CHECK_RHS_IDX(pid, 2); // IDENTIFIER
            CHECK_RHS_IDX(pid, 4); // type
            ParamListNode* left = dynamic_cast<ParamListNode*>(rhs[0].node);
            std::string name = rhs[2].token_text.value_or("");
            TypeNode* typeNode = dynamic_cast<TypeNode*>(rhs[4].node);
            Type* type = typeNode ? typeNode->type : NumberType::instance();
            delete typeNode;
            std::vector<std::string> params = std::move(left->params);
            std::vector<Type*> types = std::move(left->paramTypes);
            delete left;
            params.push_back(name);
            types.push_back(type);
            return new ParamListNode(std::move(params), std::move(types));
        }

        // 10: type -> NUMBER_TYPE
        case 10:
            return new TypeNode(NumberType::instance());

        // 11: type -> BOOL_TYPE
        case 11:
            return new TypeNode(BoolType::instance());

        // 12: type -> STRING_TYPE
        case 12:
            return new TypeNode(StringType::instance());

        // 13: statements : /* empty */
        case 13:
            return new BlockNode({});

        // 14: statements : statements statement
        case 14: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 1);
            BlockNode* left = dynamic_cast<BlockNode*>(rhs[0].node);
            std::vector<ASTNode*> items;
            if (left) {
                items = std::move(left->stmts);
                delete left;
            }
            if (rhs[1].node) items.push_back(rhs[1].node);
            return new BlockNode(std::move(items));
        }

        // 15: statement : expr SEMICOLON
        case 15:
            CHECK_RHS_IDX(pid, 0);
            return new ExprStmtNode(rhs[0].node);

        // 16: statement : block
        case 16:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 17: block : L_CURL_BRACK statements R_CURL_BRACK
        case 17: {
            CHECK_RHS_IDX(pid, 1);
            BlockNode* b = dynamic_cast<BlockNode*>(rhs[1].node);
            if (b) {
                auto v = std::move(b->stmts);
                delete b;
                return new BlockNode(std::move(v));
            } else {
                std::vector<ASTNode*> v;
                if (rhs[1].node) v.push_back(rhs[1].node);
                return new BlockNode(std::move(v));
            }
        }

        // 18: expr -> relational
        case 18:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 19: expr -> let_expr
        case 19:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 20: expr -> if_expr
        case 20:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 21: expr -> call_expr
        case 21:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 22: relational -> concatenation
        case 22:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 23: relational -> concatenation LESS_THAN concatenation
        case 23: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("<", rhs[0].node, rhs[2].node);
        }

        // 24: relational -> concatenation GREATER_THAN concatenation
        case 24: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode(">", rhs[0].node, rhs[2].node);
        }

        // 25: relational -> concatenation LESS_EQUALS concatenation
        case 25: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("<=", rhs[0].node, rhs[2].node);
        }

        // 26: relational -> concatenation GREATER_EQUALS concatenation
        case 26: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode(">=", rhs[0].node, rhs[2].node);
        }

        // 27: relational -> concatenation EQUALITY concatenation
        case 27: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("==", rhs[0].node, rhs[2].node);
        }

        // 28: concatenation -> additive
        case 28:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 29: concatenation -> concatenation CONCAT additive
        case 29: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("@", rhs[0].node, rhs[2].node);
        }

        // 30: additive -> multiplicative
        case 30:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 31: additive -> additive PLUS multiplicative
        case 31: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("+", rhs[0].node, rhs[2].node);
        }

        // 32: additive -> additive MINUS multiplicative
        case 32: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("-", rhs[0].node, rhs[2].node);
        }

        // 33: multiplicative -> unary
        case 33:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 34: multiplicative -> multiplicative STAR unary
        case 34: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("*", rhs[0].node, rhs[2].node);
        }

        // 35: multiplicative -> multiplicative SLASH unary
        case 35: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode("/", rhs[0].node, rhs[2].node);
        }

        // 36: unary -> MINUS unary
        case 36: {
            CHECK_RHS_IDX(pid, 1);
            return new UnaryOpNode("-", rhs[1].node);
        }

        // 37: unary -> PLUS unary
        case 37: {
            CHECK_RHS_IDX(pid, 1);
            return new UnaryOpNode("+", rhs[1].node);
        }

        // 38: unary -> primary
        case 38:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 39: primary -> NUMBER
        case 39: {
            CHECK_RHS_IDX(pid, 0);
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }

        // 40: primary -> STRING
        case 40: {
            CHECK_RHS_IDX(pid, 0);
            std::string s = rhs[0].token_text.value_or("");
            // Quitar comillas si existen (depende del lexer)
            if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
                s = s.substr(1, s.size() - 2);
            return new StringNode(s);
        }

        // 41: primary -> IDENTIFIER
        case 41: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new VariableNode(name);
        }

        // 42: primary -> L_PAREN expr R_PAREN
        case 42:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 43: let_expr -> LET IDENTIFIER EQUAL expr IN expr
        case 43: {
            CHECK_RHS_IDX(pid, 1);
            CHECK_RHS_IDX(pid, 3);
            CHECK_RHS_IDX(pid, 5);
            std::string name = rhs[1].token_text.value_or("");
            ASTNode* init = rhs[3].node;
            ASTNode* body = rhs[5].node;
            return new LetNode(name, init, body);
        }

        // 44: if_expr -> IF L_PAREN expr R_PAREN expr ELSE expr
        case 44: {
            CHECK_RHS_IDX(pid, 2);
            CHECK_RHS_IDX(pid, 4);
            CHECK_RHS_IDX(pid, 6);
            ASTNode* cond = rhs[2].node;
            ASTNode* then_branch = rhs[4].node;
            ASTNode* else_branch = rhs[6].node;
            return new IfNode(cond, then_branch, else_branch);
        }

        // 45: call_expr -> IDENTIFIER L_PAREN arg_list_opt R_PAREN
        case 45: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            std::string name = rhs[0].token_text.value_or("");
            BlockNode* argBlock = dynamic_cast<BlockNode*>(rhs[2].node);
            std::vector<ASTNode*> args;
            if (argBlock) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            }
            return new FunctionCallNode(name, std::move(args));
        }

        // 46: arg_list_opt : /* empty */
        case 46:
            return new BlockNode({});

        // 47: arg_list_opt : arg_list
        case 47:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 48: arg_list -> expr
        case 48: {
            CHECK_RHS_IDX(pid, 0);
            std::vector<ASTNode*> v;
            v.push_back(rhs[0].node);
            return new BlockNode(std::move(v));
        }

        // 49: arg_list -> arg_list COMMA expr
        case 49: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            BlockNode* left = dynamic_cast<BlockNode*>(rhs[0].node);
            std::vector<ASTNode*> items;
            if (left) {
                items = std::move(left->stmts);
                delete left;
            }
            items.push_back(rhs[2].node);
            return new BlockNode(std::move(items));
        }

        // 50: program' -> program
        case 50:
            return nullptr;

        default:
            std::cerr << "ASTBuilder: producción no manejada: " << pid << "\n";
            return nullptr;
    }
}