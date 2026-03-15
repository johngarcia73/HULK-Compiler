#include "ast_builder.hpp"
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <algorithm>

// Macro para verificar que el índice esté dentro del rango de rhs
#define CHECK_RHS_IDX(pid, idx) do { \
    if (rhs.size() <= (idx)) { \
        std::cerr << "ASTBuilder: producción " << (pid) << " requiere índice " << (idx) \
                  << " pero rhs.size() = " << rhs.size() << "\n"; \
        return nullptr; \
    } \
} while(0)

// Helper: convertir texto de token a entero
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
        // 0: hulk_program : declarations statements
        case 0: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 1);
            std::vector<ASTNode*> decls;
            std::vector<ASTNode*> stmts;
            // declarations (puede ser BlockNode)
            BlockNode* bn = dynamic_cast<BlockNode*>(rhs[0].node);
            if (bn) {
                for (auto p : bn->stmts) decls.push_back(p);
                bn->stmts.clear();
                delete bn;
            } else if (rhs[0].node) {
                decls.push_back(rhs[0].node);
            }
            // statements (puede ser BlockNode)
            BlockNode* bn2 = dynamic_cast<BlockNode*>(rhs[1].node);
            if (bn2) {
                for (auto p : bn2->stmts) stmts.push_back(p);
                bn2->stmts.clear();
                delete bn2;
            } else if (rhs[1].node) {
                stmts.push_back(rhs[1].node);
            }
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

        // 3: declaration : function_declaration
        // 4: declaration : type_declaration
        // 5: declaration : protocol_declaration
        case 3: case 4: case 5:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node; // por ahora solo function_declaration implementada

        // 6: function_declaration : FUNCTION IDENTIFIER L_PAREN param_list_opt R_PAREN function_body
        case 6: {
            CHECK_RHS_IDX(pid, 1); // IDENTIFIER
            CHECK_RHS_IDX(pid, 3); // param_list_opt
            CHECK_RHS_IDX(pid, 5); // function_body
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

        // 7: param_list_opt : /* empty */
        case 7:
            return new ParamListNode({});

        // 8: param_list_opt : param_list
        case 8:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 9: param_list : IDENTIFIER
        case 9: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new ParamListNode({name});
        }

        // 10: param_list : param_list COMMA IDENTIFIER
        case 10: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            ParamListNode* left = dynamic_cast<ParamListNode*>(rhs[0].node);
            std::vector<std::string> params = std::move(left->params);
            delete left;
            params.push_back(rhs[2].token_text.value_or(""));
            return new ParamListNode(std::move(params));
        }

        // 11: function_body : EQ_ARROW expr SEMICOLON
        case 11:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 12: function_body : block
        case 12:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 13: function_body : block SEMICOLON
        case 13:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node; // block

        // 14: statements : /* empty */
        case 14:
            return new BlockNode({});

        // 15: statements : statements statement
        case 15: {
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

        // 16: statement : block
        case 16:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 17: statement : block SEMICOLON
        case 17:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node; // block

        // 18: statement : not_block_expr SEMICOLON
        case 18:
            CHECK_RHS_IDX(pid, 0);
            return new ExprStmtNode(rhs[0].node);

        // 19: block : L_CURL_BRACK statements R_CURL_BRACK
        case 19: {
            CHECK_RHS_IDX(pid, 1); // statements
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

        // 20: not_block_expr : assignment
        case 20:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 21: assignment : logical_or
        case 21:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 22: assignment : IDENTIFIER ASSIGN assignment
        case 22: {
            CHECK_RHS_IDX(pid, 0); // IDENTIFIER
            CHECK_RHS_IDX(pid, 2); // assignment
            std::string name = rhs[0].token_text.value_or("");
            ASTNode* right = rhs[2].node;
            return new AssignmentNode(name, right);
        }

        // 23: logical_or : logical_and
        case 23:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 24: logical_or : logical_or PIPE logical_and
        case 24: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("||", rhs[0].node, rhs[2].node);
        }

        // 25: logical_and : equality
        case 25:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 26: logical_and : logical_and AMPERSAND equality
        case 26: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("&&", rhs[0].node, rhs[2].node);
        }

        // 27: equality : relational
        case 27:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 28: equality : relational EQUAL relational
        case 28: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("==", rhs[0].node, rhs[2].node);
        }

        // 29: equality : relational NOT_EQ relational
        case 29: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("!=", rhs[0].node, rhs[2].node);
        }

        // 30: relational : concatenation
        case 30:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 31: relational : concatenation LESS concatenation
        case 31: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("<", rhs[0].node, rhs[2].node);
        }

        // 32: relational : concatenation GREAT concatenation
        case 32: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode(">", rhs[0].node, rhs[2].node);
        }

        // 33: relational : concatenation GREAT_EQ concatenation
        case 33: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode(">=", rhs[0].node, rhs[2].node);
        }

        // 34: relational : concatenation LESS_EQ concatenation
        case 34: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("<=", rhs[0].node, rhs[2].node);
        }

        // 35: concatenation : additive
        case 35:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 36: concatenation : concatenation ARROBA additive
        case 36: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("@", rhs[0].node, rhs[2].node);
        }

        // 37: concatenation : concatenation DOUBLED_ARROBA additive
        case 37: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpStrNode("@@", rhs[0].node, rhs[2].node);
        }

        // 38: additive : multiplicative
        case 38:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 39: additive : additive PLUS multiplicative
        case 39: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('+', rhs[0].node, rhs[2].node);
        }

        // 40: additive : additive MINUS multiplicative
        case 40: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('-', rhs[0].node, rhs[2].node);
        }

        // 41: multiplicative : unary
        case 41:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 42: multiplicative : multiplicative STAR unary
        case 42: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('*', rhs[0].node, rhs[2].node);
        }

        // 43: multiplicative : multiplicative SLASH unary
        case 43: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('/', rhs[0].node, rhs[2].node);
        }

        // 44: multiplicative : multiplicative PERCENTAGE unary
        case 44: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('%', rhs[0].node, rhs[2].node);
        }

        // 45: unary : MINUS unary %prec UNARY
        case 45: {
            CHECK_RHS_IDX(pid, 1);
            return new UnaryOpNode('-', rhs[1].node);
        }

        // 46: unary : EXCLAMATION unary %prec UNARY
        case 46: {
            CHECK_RHS_IDX(pid, 1);
            return new UnaryOpNode('!', rhs[1].node);
        }

        // 47: unary : power
        case 47:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 48: power : primary
        case 48:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 49: power : primary CARET power
        case 49: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            return new BinaryOpNode('^', rhs[0].node, rhs[2].node);
        }

        // 50: primary : literal
        case 50:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 51: primary : group
        case 51:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 52: primary : command_expr
        case 52:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 53: primary : variable
        case 53:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 54: primary : initialization
        case 54:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 55: literal : NUMBER
        case 55: {
            CHECK_RHS_IDX(pid, 0);
            long long v = parse_int(rhs[0].token_text);
            return new NumberNode(v);
        }

        // 56: literal : STRING
        case 56: {
            CHECK_RHS_IDX(pid, 0);
            std::string s = rhs[0].token_text.value_or("");
            return new StringNode(s);
        }

        // 57: literal : TRUE
        case 57:
            return new BooleanNode(true);

        // 58: literal : FALSE
        case 58:
            return new BooleanNode(false);

        // 59: group : L_PAREN expr R_PAREN
        case 59:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 60: command_expr : function_call
        case 60:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 61: command_expr : variable_declaration
        case 61:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 62: command_expr : if
        case 62:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 63: command_expr : while
        case 63:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 64: command_expr : for
        case 64:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node;

        // 65: function_call : IDENTIFIER L_PAREN arg_list_opt R_PAREN
        case 65: {
            CHECK_RHS_IDX(pid, 0); // IDENTIFIER
            CHECK_RHS_IDX(pid, 2); // arg_list_opt
            std::string name = rhs[0].token_text.value_or("");
            // arg_list_opt es un BlockNode con las expresiones de los argumentos
            BlockNode* argBlock = dynamic_cast<BlockNode*>(rhs[2].node);
            std::vector<ASTNode*> args;
            if (argBlock) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            }
            return new FunctionCallNode(name, std::move(args));
        }

        // 66: arg_list_opt : /* empty */
        case 66:
            return new BlockNode({});

        // 67: arg_list_opt : arg_list
        case 67:
            CHECK_RHS_IDX(pid, 0);
            return rhs[0].node; // arg_list es BlockNode

        // 68: arg_list : expr
        case 68: {
            CHECK_RHS_IDX(pid, 0);
            std::vector<ASTNode*> v;
            v.push_back(rhs[0].node);
            return new BlockNode(std::move(v));
        }

        // 69: arg_list : arg_list COMMA expr
        case 69: {
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

        // 70: variable_declaration : LET let_bindings IN expr
        case 70: {
            CHECK_RHS_IDX(pid, 1); // let_bindings
            CHECK_RHS_IDX(pid, 3); // expr
            LetBindingsNode* binds = dynamic_cast<LetBindingsNode*>(rhs[1].node);
            std::vector<std::pair<std::string, ASTNodePtr>> bindings;
            if (binds) {
                for (auto* lb : binds->bindings) {
                    bindings.emplace_back(lb->name, lb->init);
                    lb->init = nullptr; // transfer ownership
                }
                delete binds;
            }
            ASTNode* body = rhs[3].node;
            return new LetNode(std::move(bindings), body);
        }

        // 71: let_bindings : let_binding
        case 71: {
            CHECK_RHS_IDX(pid, 0);
            LetBindingNode* lb = dynamic_cast<LetBindingNode*>(rhs[0].node);
            std::vector<LetBindingNode*> v;
            if (lb) {
                v.push_back(lb);
            }
            return new LetBindingsNode(std::move(v));
        }

        // 72: let_bindings : let_bindings COMMA let_binding
        case 72: {
            CHECK_RHS_IDX(pid, 0);
            CHECK_RHS_IDX(pid, 2);
            LetBindingsNode* left = dynamic_cast<LetBindingsNode*>(rhs[0].node);
            LetBindingsNode* right = dynamic_cast<LetBindingsNode*>(rhs[2].node);
            std::vector<LetBindingNode*> all = std::move(left->bindings);
            delete left;
            for (auto* b : right->bindings) all.push_back(b);
            right->bindings.clear();
            delete right;
            return new LetBindingsNode(std::move(all));
        }

        // 73: let_binding : IDENTIFIER EQUALS expr
        case 73: {
            CHECK_RHS_IDX(pid, 0); // IDENTIFIER
            CHECK_RHS_IDX(pid, 2); // expr
            std::string name = rhs[0].token_text.value_or("");
            ASTNode* init = rhs[2].node;
            return new LetBindingNode(name, init);
        }

        // 74: if : IF L_PAREN expr R_PAREN expr elif_list else_opt
        case 74: {
            CHECK_RHS_IDX(pid, 2); // expr (cond)
            CHECK_RHS_IDX(pid, 4); // expr (then)
            CHECK_RHS_IDX(pid, 5); // elif_list
            CHECK_RHS_IDX(pid, 6); // else_opt
            ASTNode* cond = rhs[2].node;
            ASTNode* then_branch = rhs[4].node;
            ElifListNode* elif_list = dynamic_cast<ElifListNode*>(rhs[5].node);
            std::vector<ElifNode*> elifs;
            if (elif_list) {
                elifs = std::move(elif_list->elifs);
                delete elif_list;
            }
            ASTNode* else_branch = rhs[6].node; // puede ser nullptr
            return new IfNode(cond, then_branch, std::move(elifs), else_branch);
        }

        // 75: elif_list : /* empty */
        case 75:
            return new ElifListNode();

        // 76: elif_list : elif_list ELIF L_PAREN expr R_PAREN expr
        case 76: {
            CHECK_RHS_IDX(pid, 0); // elif_list
            CHECK_RHS_IDX(pid, 3); // expr (cond)
            CHECK_RHS_IDX(pid, 5); // expr (body)
            ElifListNode* left = dynamic_cast<ElifListNode*>(rhs[0].node);
            ASTNode* cond = rhs[3].node;
            ASTNode* body = rhs[5].node;
            ElifNode* new_elif = new ElifNode(cond, body);
            std::vector<ElifNode*> all = std::move(left->elifs);
            delete left;
            all.push_back(new_elif);
            return new ElifListNode(std::move(all));
        }

        // 77: else_opt : /* empty */
        case 77:
            return nullptr;

        // 78: else_opt : ELSE expr
        case 78:
            CHECK_RHS_IDX(pid, 1);
            return rhs[1].node;

        // 79: while : WHILE L_PAREN expr R_PAREN expr
        case 79: {
            CHECK_RHS_IDX(pid, 2); // expr (cond)
            CHECK_RHS_IDX(pid, 4); // expr (body)
            ASTNode* cond = rhs[2].node;
            ASTNode* body = rhs[4].node;
            return new WhileNode(cond, body);
        }

        // 80: for : FOR L_PAREN IDENTIFIER IN expr R_PAREN expr
        case 80: {
            CHECK_RHS_IDX(pid, 2); // IDENTIFIER
            CHECK_RHS_IDX(pid, 4); // expr (iterable)
            CHECK_RHS_IDX(pid, 6); // expr (body)
            std::string it = rhs[2].token_text.value_or("");
            ASTNode* iter = rhs[4].node;
            ASTNode* body = rhs[6].node;
            return new ForNode(it, iter, body);
        }

        // 81: variable : IDENTIFIER
        case 81: {
            CHECK_RHS_IDX(pid, 0);
            std::string name = rhs[0].token_text.value_or("");
            return new VariableNode(name);
        }

        // 82: initialization : NEW IDENTIFIER L_PAREN arg_list_opt R_PAREN
        case 82: {
            CHECK_RHS_IDX(pid, 1); // IDENTIFIER (type)
            CHECK_RHS_IDX(pid, 3); // arg_list_opt
            std::string type = rhs[1].token_text.value_or("");
            BlockNode* argBlock = dynamic_cast<BlockNode*>(rhs[3].node);
            std::vector<ASTNode*> args;
            if (argBlock) {
                args = std::move(argBlock->stmts);
                delete argBlock;
            }
            return new NewNode(type, std::move(args));
        }

        // La producción aumentada (83) no debería llegar aquí, pero por si acaso
        case 83:
            std::cerr << "ASTBuilder: producción aumentada 83 no debería ser invocada\n";
            return nullptr;

        default:
            std::cerr << "ASTBuilder: producción no manejada: " << pid << "\n";
            return nullptr;
    }
}