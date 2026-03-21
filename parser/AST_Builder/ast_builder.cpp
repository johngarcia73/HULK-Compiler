#include "ast_builder.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <algorithm>

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
            CHECK_RHS_IDX(pid, 1);
            CHECK_RHS_IDX(pid, 3);
            CHECK_RHS_IDX(pid, 5);
            std::string name = rhs[1].token_text.value_or("");
            ParamListNode* paramsNode = dynamic_cast<ParamListNode*>(rhs[3].node);
            std::vector<std::string> params;
            if (paramsNode) {
                params = std::move(paramsNode->params);
                delete paramsNode;
            }
            ASTNode* body = rhs[5].node;
            return new FunctionDeclNode(name, std::move(params), body);
        }

        // 5: param_list_opt : /* empty */
        case 5:
            return new ParamListNode({});

        // 6: param_list_opt : param_list
        case 6:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 7: param_list : IDENTIFIER
        case 7: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new ParamListNode({name});
        }

        // 8: param_list : param_list COMMA IDENTIFIER
        case 8: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            ParamListNode* left = dynamic_cast<ParamListNode*>(rhs[0].node);
            std::vector<std::string> params = std::move(left->params);
            delete left;
            params.push_back(rhs[2].token_text.value_or(""));
            return new ParamListNode(std::move(params));
        }

        // 9: statements : /* empty */
        case 9:
            return new BlockNode({});   

        // 10: statements : statements statement
        case 10: {
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

        // 11: statement : expr SEMICOLON
        case 11:
            CHECK_RHS_IDX(pid, 0);
            return new ExprStmtNode(rhs[0].node);

        // 12: statement : block
        case 12:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 13: block : L_CURL_BRACK statements R_CURL_BRACK
        case 13: {
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

        // 14: expr : additive
        case 14:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 15: expr : let_expr
        case 15:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 16: expr : if_expr
        case 16:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 17: expr : call_expr
        case 17:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 18: additive : multiplicative
        case 18:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 19: additive : additive PLUS multiplicative
        case 19: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('+', rhs[0].node, rhs[2].node);
        }

        // 20: additive : additive MINUS multiplicative
        case 20: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('-', rhs[0].node, rhs[2].node);
        }

        // 21: multiplicative : primary
        case 21:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 22: multiplicative : multiplicative STAR primary
        case 22: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('*', rhs[0].node, rhs[2].node);
        }

        // 23: multiplicative : multiplicative SLASH primary
        case 23: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('/', rhs[0].node, rhs[2].node);
        }

        // 24: primary : NUMBER
        case 24: {
            CHECK_RHS_IDX(pid, 0);
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }

        // 25: primary : IDENTIFIER
        case 25: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new VariableNode(name);
        }

        // 26: primary : L_PAREN expr R_PAREN
        case 26:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 27: let_expr : LET IDENTIFIER EQUAL expr IN expr
        case 27: {
            CHECK_RHS_IDX(pid, 1);
            CHECK_RHS_IDX(pid, 3);
            CHECK_RHS_IDX(pid, 5);
            std::string name = rhs[1].token_text.value_or("");
            ASTNode* init = rhs[3].node;
            ASTNode* body = rhs[5].node;
            return new LetNode(name, init, body);
        }

        // 28: if_expr : IF L_PAREN expr R_PAREN expr ELSE expr
        case 28: {
            CHECK_RHS_IDX(pid, 2);
            CHECK_RHS_IDX(pid, 4);
            CHECK_RHS_IDX(pid, 6);
            ASTNode* cond = rhs[2].node;
            ASTNode* then_branch = rhs[4].node;
            ASTNode* else_branch = rhs[6].node;
            return new IfNode(cond, then_branch, else_branch);
        }

        // 29: call_expr : IDENTIFIER L_PAREN arg_list_opt R_PAREN
        case 29: {
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

        // 30: arg_list_opt : /* empty */
        case 30:
            return new BlockNode({});

        // 31: arg_list_opt : arg_list
        case 31:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 32: arg_list : expr
        case 32: {
            CHECK_RHS_IDX(pid, 0);
            std::vector<ASTNode*> v;
            v.push_back(rhs[0].node);
            return new BlockNode(std::move(v));
        }

        // 33: arg_list : arg_list COMMA expr
        case 33: {
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

        // 34: program' : program (augmented production)
        case 34:
            std::cerr << "ASTBuilder: producción aumentada 34 no debería ser invocada\n";
            return nullptr;

        default:
            std::cerr << "ASTBuilder: producción no manejada: " << pid << "\n";
            return nullptr;
    }
}