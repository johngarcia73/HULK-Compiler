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

Type* tokenToType(const Value& v) {
    std::string tokenName = v.token_text.value_or("");
    if (tokenName == "NUMBER_TYPE") return NumberType::instance();
    if (tokenName == "BOOL_TYPE")   return BoolType::instance();
    if (tokenName == "STRING_TYPE") return StringType::instance();
    return nullptr;
}

static long long parse_int(const std::optional<std::string>& s) {
    if (!s) return 0;
    try {
        return std::stoll(*s);
    } catch(...) {
        return 0;
    }
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
            // No hay tipo de retorno explícito, usar Number por defecto
            Type* retType = NumberType::instance();
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
            // Obtener el tipo de retorno del nodo TypeNode
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

        // 10: type : NUMBER_TYPE
        case 10:
            return new TypeNode(NumberType::instance());

        // 11: type : BOOL_TYPE
        case 11:
            return new TypeNode(BoolType::instance());

        // 12: type : STRING_TYPE
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

        // 18: expr : additive
        case 18:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 19: expr : let_expr
        case 19:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 20: expr : if_expr
        case 20:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 21: expr : call_expr
        case 21:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 22: additive : multiplicative
        case 22:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 23: additive : additive PLUS multiplicative
        case 23: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('+', rhs[0].node, rhs[2].node);
        }

        // 24: additive : additive MINUS multiplicative
        case 24: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('-', rhs[0].node, rhs[2].node);
        }

        // 25: multiplicative : primary
        case 25:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 26: multiplicative : multiplicative STAR primary
        case 26: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('*', rhs[0].node, rhs[2].node);
        }

        // 27: multiplicative : multiplicative SLASH primary
        case 27: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('/', rhs[0].node, rhs[2].node);
        }

        // 28: primary : NUMBER
        case 28: {
            CHECK_RHS_IDX(pid, 0);
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }

        // 29: primary : IDENTIFIER
        case 29: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new VariableNode(name);
        }

        // 30: primary : L_PAREN expr R_PAREN
        case 30:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 31: let_expr : LET IDENTIFIER EQUAL expr IN expr
        case 31: {
            CHECK_RHS_IDX(pid, 1);
            CHECK_RHS_IDX(pid, 3);
            CHECK_RHS_IDX(pid, 5);
            std::string name = rhs[1].token_text.value_or("");
            ASTNode* init = rhs[3].node;
            ASTNode* body = rhs[5].node;
            return new LetNode(name, init, body);
        }

        // 32: if_expr : IF L_PAREN expr R_PAREN expr ELSE expr
        case 32: {
            CHECK_RHS_IDX(pid, 2);
            CHECK_RHS_IDX(pid, 4);
            CHECK_RHS_IDX(pid, 6);
            ASTNode* cond = rhs[2].node;
            ASTNode* then_branch = rhs[4].node;
            ASTNode* else_branch = rhs[6].node;
            return new IfNode(cond, then_branch, else_branch);
        }

        // 33: call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN
        case 33: {
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

        // 34: arg_list_opt : /* empty */
        case 34:
            return new BlockNode({});

        // 35: arg_list_opt : arg_list
        case 35:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 36: arg_list : expr
        case 36: {
            CHECK_RHS_IDX(pid, 0);
            std::vector<ASTNode*> v;
            v.push_back(rhs[0].node);
            return new BlockNode(std::move(v));
        }

        // 37: arg_list : arg_list COMMA expr
        case 37: {
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

        // 38: program' : program (augmented production)
        case 38:
            std::cerr << "ASTBuilder: producción aumentada 38 no debería ser invocada\n";
            return nullptr;

        default:
            std::cerr << "ASTBuilder: producción no manejada: " << pid << "\n";
            return nullptr;
    }
}