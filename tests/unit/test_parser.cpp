#include "test_harness.hpp"

#include "../../parser/utils/Grammar/grammar.hpp"
#include "../../parser/utils/Symbol/symbol.hpp"
#include "../../parser/Parser_Generator/genparser.hpp"
#include "../../parser/Parser_Generator/parser_interface.hpp"
#include "../../parser/AST_Builder/ast_builder.hpp"
#include "../../parser/AST_Builder/ast_node.hpp"
#include "../../lexer/tokens.hpp"
#include "../../common/token.hpp"
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace {

std::string resolve_path(std::initializer_list<const char*> candidates) {
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Cannot resolve project test path.");
}

ParseResult parse_with_project_grammar(const std::vector<Token>& raw_tokens) {
    Grammar grammar = build_grammar_from_file(
        resolve_path({"../parser/Parser_Generator/grammar.y", "parser/Parser_Generator/grammar.y"}));

    std::unordered_map<std::string, uint32_t> name_to_id;
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        name_to_id[grammar.symtab[i].name] = i;
    }

    LALRParser parser = LALRParser::from_grammar(grammar, name_to_id.at("$"));
    parser.set_builder(std::make_unique<ASTBuilder>(&grammar));

    std::vector<Token> tokens;
    for (const auto& token : raw_tokens) {
        const std::string token_name =
            token_type_to_string(static_cast<TokenType>(token.symbol_id));
        auto it = name_to_id.find(token_name);
        if (it == name_to_id.end()) {
            throw std::runtime_error("Unknown token in grammar: " + token_name);
        }
        tokens.emplace_back(
            it->second,
            token.value,
            token.line,
            token.column,
            token.end_line,
            token.end_column);
    }

    tokens.emplace_back(name_to_id.at("$"), "$", 0, 0);
    return parser.parse(tokens);
}

Token raw_token(TokenType type, const std::string& value) {
    return Token(static_cast<uint32_t>(token_to_id(type)), value, 1, 1, 1, 1);
}

}  // namespace

void test_parser(TestRunner& r) {
    std::cout << "[test_parser] Parseando gramatica real para if flexibles, elif y let multiple\n";

    Grammar grammar;

    // Add placeholder terminals so their SymbolId == token id (T0..TN)
    int max_tok = static_cast<int>(TokenType::TOKEN_UNKNOWN);
    for (int i = 0; i <= max_tok; ++i) {
        grammar.symtab.add(SymbolKind::Terminal, std::string("T") + std::to_string(i));
    }

    // Non-terminal
    SymbolId S = grammar.symtab.add(SymbolKind::NonTerminal, "S");

    // Production: S -> LET IDENTIFIER EQUAL NUMBER SEMICOLON
    std::vector<SymbolId> rhs = {
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_LET)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_IDENTIFIER)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_EQUAL)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_NUMBER)),
        static_cast<SymbolId>(token_to_id(TokenType::TOKEN_SEMICOLON))
    };

    grammar.add_production(S, rhs);
    grammar.start_symbol = S;
    grammar.build_indices();

    uint32_t dollar = static_cast<uint32_t>(token_to_id(TokenType::TOKEN_EOF));
    LALRParser parser = LALRParser::from_grammar(grammar, dollar);

    // Build a minimal token stream directly.
    std::vector<Token> tokens = {
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_NUMBER, "42"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    };
    tokens.emplace_back(dollar, "$", 0, 0);

    ParseResult res = parser.parse(tokens);
    EXPECT_TRUE(r, res.is_success());

    if (r.verbose && !res.is_success()) {
        std::cout << "Parser error: " << res.error_message << "\n";
    }

    ParseResult if_stmt_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_IF, "if"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_NUMBER, "1"),
        raw_token(TokenType::TOKEN_EQUALITY, "=="),
        raw_token(TokenType::TOKEN_NUMBER, "1"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_NUMBER, "42"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}")
    });
    EXPECT_TRUE(r, if_stmt_res.is_success());
    if (if_stmt_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(if_stmt_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program) {
            EXPECT_EQ(r, program->stmts.size(), (size_t)1);
            if (program->stmts.size() == 1) {
                auto* if_node = dynamic_cast<IfNode*>(program->stmts[0]);
                EXPECT_TRUE(r, if_node != nullptr);
                EXPECT_TRUE(r, if_node && if_node->else_branch == nullptr);
            }
        }
    }

    ParseResult if_expr_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IF, "if"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_NUMBER, "1"),
        raw_token(TokenType::TOKEN_LESS_THAN, "<"),
        raw_token(TokenType::TOKEN_NUMBER, "2"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_NUMBER, "3"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, if_expr_res.is_success());
    if (if_expr_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(if_expr_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program) {
            EXPECT_EQ(r, program->stmts.size(), (size_t)1);
            if (program->stmts.size() == 1) {
                auto* expr_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
                EXPECT_TRUE(r, expr_stmt != nullptr);
                if (expr_stmt) {
                    auto* let_node = dynamic_cast<LetNode*>(expr_stmt->expr);
                    EXPECT_TRUE(r, let_node != nullptr);
                    if (let_node) {
                        auto* if_node = dynamic_cast<IfNode*>(let_node->init);
                        EXPECT_TRUE(r, if_node != nullptr);
                        EXPECT_TRUE(r, if_node && if_node->else_branch == nullptr);
                        if (if_node) {
                            auto* number_node = dynamic_cast<NumberNode*>(if_node->then_branch);
                            EXPECT_TRUE(r, number_node != nullptr);
                            if (number_node) {
                                EXPECT_EQ(r, number_node->value, std::string("3"));
                                EXPECT_EQ(r, number_node->kindName(), std::string("int"));
                            }
                        }
                    }
                }
            }
        }
    }

    ParseResult multi_let_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_NUMBER, "6"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_IDENTIFIER, "b"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_STAR, "*"),
        raw_token(TokenType::TOKEN_NUMBER, "7"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "b"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, multi_let_res.is_success());
    if (multi_let_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(multi_let_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program) {
            EXPECT_EQ(r, program->stmts.size(), (size_t)1);
            if (program->stmts.size() == 1) {
                auto* expr_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
                EXPECT_TRUE(r, expr_stmt != nullptr);
                if (expr_stmt) {
                    auto* outer_let = dynamic_cast<LetNode*>(expr_stmt->expr);
                    EXPECT_TRUE(r, outer_let != nullptr);
                    if (outer_let) {
                        EXPECT_EQ(r, outer_let->name, std::string("a"));
                        auto* outer_init = dynamic_cast<NumberNode*>(outer_let->init);
                        EXPECT_TRUE(r, outer_init != nullptr);
                        if (outer_init) {
                            EXPECT_EQ(r, outer_init->value, std::string("6"));
                            EXPECT_EQ(r, outer_init->kindName(), std::string("int"));
                        }

                        auto* inner_let = dynamic_cast<LetNode*>(outer_let->body);
                        EXPECT_TRUE(r, inner_let != nullptr);
                        if (inner_let) {
                            EXPECT_EQ(r, inner_let->name, std::string("b"));
                            auto* init_expr = dynamic_cast<BinaryOpNode*>(inner_let->init);
                            EXPECT_TRUE(r, init_expr != nullptr);
                            if (init_expr) {
                                EXPECT_EQ(r, init_expr->op, std::string("*"));
                                auto* left_var = dynamic_cast<VariableNode*>(init_expr->left);
                                auto* right_num = dynamic_cast<NumberNode*>(init_expr->right);
                                EXPECT_TRUE(r, left_var != nullptr);
                                EXPECT_TRUE(r, right_num != nullptr);
                                if (left_var) {
                                    EXPECT_EQ(r, left_var->name, std::string("a"));
                                }
                                if (right_num) {
                                    EXPECT_EQ(r, right_num->value, std::string("7"));
                                    EXPECT_EQ(r, right_num->kindName(), std::string("int"));
                                }
                            }

                            auto* result_var = dynamic_cast<VariableNode*>(inner_let->body);
                            EXPECT_TRUE(r, result_var != nullptr);
                            if (result_var) {
                                EXPECT_EQ(r, result_var->name, std::string("b"));
                            }
                        }
                    }
                }
            }
        }
    }

    ParseResult number_kinds_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_NUMBER, "2147483648"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_NUMBER, "123.4"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, number_kinds_res.is_success());
    if (number_kinds_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(number_kinds_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->stmts.size() == 2) {
            auto* first_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
            auto* second_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[1]);
            EXPECT_TRUE(r, first_stmt != nullptr);
            EXPECT_TRUE(r, second_stmt != nullptr);
            if (first_stmt) {
                auto* long_number = dynamic_cast<NumberNode*>(first_stmt->expr);
                EXPECT_TRUE(r, long_number != nullptr);
                if (long_number) {
                    EXPECT_EQ(r, long_number->kindName(), std::string("long"));
                }
            }
            if (second_stmt) {
                auto* double_number = dynamic_cast<NumberNode*>(second_stmt->expr);
                EXPECT_TRUE(r, double_number != nullptr);
                if (double_number) {
                    EXPECT_EQ(r, double_number->kindName(), std::string("double"));
                }
            }
        }
    }

    ParseResult elif_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_NUMBER, "42"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "mod"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_MODULE, "%"),
        raw_token(TokenType::TOKEN_NUMBER, "3"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "print"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IF, "if"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "mod"),
        raw_token(TokenType::TOKEN_EQUALITY, "=="),
        raw_token(TokenType::TOKEN_NUMBER, "0"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_STRING, "\"Magic\""),
        raw_token(TokenType::TOKEN_ELIF, "elif"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "mod"),
        raw_token(TokenType::TOKEN_MODULE, "%"),
        raw_token(TokenType::TOKEN_NUMBER, "3"),
        raw_token(TokenType::TOKEN_EQUALITY, "=="),
        raw_token(TokenType::TOKEN_NUMBER, "1"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_STRING, "\"Woke\""),
        raw_token(TokenType::TOKEN_ELSE, "else"),
        raw_token(TokenType::TOKEN_STRING, "\"Dumb\""),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, elif_res.is_success());
    if (elif_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(elif_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->stmts.size() == 1) {
            auto* expr_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
            EXPECT_TRUE(r, expr_stmt != nullptr);
            if (expr_stmt) {
                auto* outer_let = dynamic_cast<LetNode*>(expr_stmt->expr);
                EXPECT_TRUE(r, outer_let != nullptr);
                if (outer_let) {
                    auto* inner_let = dynamic_cast<LetNode*>(outer_let->body);
                    EXPECT_TRUE(r, inner_let != nullptr);
                    if (inner_let) {
                        auto* print_call = dynamic_cast<FunctionCallNode*>(inner_let->body);
                        EXPECT_TRUE(r, print_call != nullptr);
                        if (print_call && !print_call->args.empty()) {
                            auto* outer_if = dynamic_cast<IfNode*>(print_call->args[0]);
                            EXPECT_TRUE(r, outer_if != nullptr);
                            EXPECT_TRUE(r, outer_if && outer_if->else_branch != nullptr);
                            if (outer_if && outer_if->else_branch) {
                                auto* nested_if = dynamic_cast<IfNode*>(outer_if->else_branch);
                                EXPECT_TRUE(r, nested_if != nullptr);
                            }
                        }
                    }
                }
            }
        }
    }

    ParseResult while_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_NUMBER, "10"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_WHILE, "while"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_GREATER_EQUALS, ">="),
        raw_token(TokenType::TOKEN_NUMBER, "0"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "print"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_MINUS, "-"),
        raw_token(TokenType::TOKEN_NUMBER, "1"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, while_res.is_success());
    if (while_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(while_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->stmts.size() == 1) {
            auto* expr_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
            EXPECT_TRUE(r, expr_stmt != nullptr);
            if (expr_stmt) {
                auto* outer_let = dynamic_cast<LetNode*>(expr_stmt->expr);
                EXPECT_TRUE(r, outer_let != nullptr);
                if (outer_let) {
                    auto* while_node = dynamic_cast<WhileNode*>(outer_let->body);
                    EXPECT_TRUE(r, while_node != nullptr);
                    if (while_node) {
                        auto* block = dynamic_cast<BlockNode*>(while_node->body);
                        EXPECT_TRUE(r, block != nullptr);
                        if (block && block->stmts.size() == 2) {
                            auto* assign_stmt = dynamic_cast<ExprStmtNode*>(block->stmts[1]);
                            EXPECT_TRUE(r, assign_stmt != nullptr);
                            if (assign_stmt) {
                                auto* assign = dynamic_cast<AssignmentNode*>(assign_stmt->expr);
                                EXPECT_TRUE(r, assign != nullptr);
                                if (assign) {
                                    auto* target = dynamic_cast<VariableNode*>(assign->target);
                                    EXPECT_TRUE(r, target != nullptr);
                                    if (target) {
                                        EXPECT_EQ(r, target->name, std::string("a"));
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    ParseResult let_block_body_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "m"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_MODULE, "%"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "b"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "b"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "a"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "m"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, let_block_body_res.is_success());
    if (let_block_body_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(let_block_body_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->stmts.size() == 1) {
            auto* expr_stmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
            EXPECT_TRUE(r, expr_stmt != nullptr);
            if (expr_stmt) {
                auto* let_node = dynamic_cast<LetNode*>(expr_stmt->expr);
                EXPECT_TRUE(r, let_node != nullptr);
                if (let_node) {
                    auto* block = dynamic_cast<BlockNode*>(let_node->body);
                    EXPECT_TRUE(r, block != nullptr);
                    if (block) {
                        EXPECT_EQ(r, block->stmts.size(), (size_t)2);
                    }
                }
            }
        }
    }

    ParseResult for_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_FOR, "for"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "range"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_NUMBER, "0"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_NUMBER, "10"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "print"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, for_res.is_success());
    if (for_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(for_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->stmts.size() == 1) {
            auto* for_node = dynamic_cast<ForNode*>(program->stmts[0]);
            EXPECT_TRUE(r, for_node != nullptr);
            if (for_node) {
                EXPECT_EQ(r, for_node->iterator, std::string("x"));
                auto* iterable = dynamic_cast<FunctionCallNode*>(for_node->iterable);
                EXPECT_TRUE(r, iterable != nullptr);
                auto* body_stmt = dynamic_cast<ExprStmtNode*>(for_node->body);
                EXPECT_TRUE(r, body_stmt != nullptr);
            }
        }
    }

    ParseResult type_decl_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_TYPE, "type"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Point"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_IDENTIFIER, "y"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "y"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "y"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "getX"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_ARROW, "=>"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "self"),
        raw_token(TokenType::TOKEN_DOT, "."),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "pt"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Point"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_NEW, "new"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Point"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_NUMBER, "3"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_NUMBER, "4"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "pt"),
        raw_token(TokenType::TOKEN_DOT, "."),
        raw_token(TokenType::TOKEN_IDENTIFIER, "getX"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, type_decl_res.is_success());
    if (type_decl_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(type_decl_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program) {
            EXPECT_EQ(r, program->decls.size(), (size_t)1);
            EXPECT_EQ(r, program->stmts.size(), (size_t)1);
            if (program->decls.size() == 1) {
                auto* point = dynamic_cast<TypeDeclNode*>(program->decls[0]);
                EXPECT_TRUE(r, point != nullptr);
                if (point) {
                    EXPECT_EQ(r, point->name, std::string("Point"));
                    EXPECT_EQ(r, point->ctorParams.size(), (size_t)2);
                    EXPECT_EQ(r, point->ctorParamTypeNames.size(), (size_t)2);
                    EXPECT_EQ(r, point->ctorParamTypeNames[0], std::string("Number"));
                    EXPECT_EQ(r, point->members.size(), (size_t)3);
                    auto* attrX = dynamic_cast<AttributeDeclNode*>(point->members[0]);
                    auto* getX = dynamic_cast<FunctionDeclNode*>(point->members[2]);
                    EXPECT_TRUE(r, attrX != nullptr);
                    EXPECT_TRUE(r, getX != nullptr);
                    if (attrX) {
                        EXPECT_TRUE(r, attrX->hasExplicitType);
                        EXPECT_EQ(r, attrX->declaredTypeName, std::string("Number"));
                    }
                    if (getX) {
                        EXPECT_TRUE(r, getX->isMethod);
                        EXPECT_EQ(r, getX->ownerTypeName, std::string("Point"));
                        auto* body = dynamic_cast<MemberAccessNode*>(getX->exprBody);
                        EXPECT_TRUE(r, body != nullptr);
                        if (body) {
                            auto* selfVar = dynamic_cast<VariableNode*>(body->base);
                            EXPECT_TRUE(r, selfVar != nullptr);
                            if (selfVar) {
                                EXPECT_EQ(r, selfVar->name, std::string("self"));
                            }
                            EXPECT_EQ(r, body->member, std::string("x"));
                        }
                    }
                }
            }
            if (program->stmts.size() == 1) {
                auto* exprStmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
                EXPECT_TRUE(r, exprStmt != nullptr);
                if (exprStmt) {
                    auto* letNode = dynamic_cast<LetNode*>(exprStmt->expr);
                    EXPECT_TRUE(r, letNode != nullptr);
                    if (letNode) {
                        EXPECT_TRUE(r, letNode->hasExplicitType);
                        EXPECT_EQ(r, letNode->declaredTypeName, std::string("Point"));
                        auto* newNode = dynamic_cast<NewNode*>(letNode->init);
                        auto* callNode = dynamic_cast<FunctionCallNode*>(letNode->body);
                        EXPECT_TRUE(r, newNode != nullptr);
                        EXPECT_TRUE(r, callNode != nullptr);
                        if (newNode) {
                            EXPECT_EQ(r, newNode->typeName, std::string("Point"));
                            EXPECT_EQ(r, newNode->args.size(), (size_t)2);
                        }
                        if (callNode) {
                            EXPECT_EQ(r, callNode->name, std::string("getX"));
                            EXPECT_TRUE(r, callNode->receiver != nullptr);
                        }
                    }
                }
            }
        }
    }

    ParseResult inheritance_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_TYPE, "type"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Point"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_TYPE, "type"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "PolarPoint"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "phi"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_COMMA, ","),
        raw_token(TokenType::TOKEN_IDENTIFIER, "rho"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_INHERITS, "inherits"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Point"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_IDENTIFIER, "rho"),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "rhoValue"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_ARROW, "=>"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "rho"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}")
    });
    EXPECT_TRUE(r, inheritance_res.is_success());
    if (inheritance_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(inheritance_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program && program->decls.size() == 2) {
            auto* polar = dynamic_cast<TypeDeclNode*>(program->decls[1]);
            EXPECT_TRUE(r, polar != nullptr);
            if (polar) {
                EXPECT_EQ(r, polar->parentType, std::string("Point"));
                EXPECT_TRUE(r, polar->hasExplicitParentArgs);
                EXPECT_EQ(r, polar->parentArgs.size(), (size_t)1);
            }
        }
    }

    ParseResult protocol_res = parse_with_project_grammar({
        raw_token(TokenType::TOKEN_PROTOCOL, "protocol"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Hashable"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "hash"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_NUMBER_TYPE, "Number"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_PROTOCOL, "protocol"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Named"),
        raw_token(TokenType::TOKEN_EXTENDS, "extends"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Hashable"),
        raw_token(TokenType::TOKEN_L_CURL_BRACK, "{"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "describe"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_STRING_TYPE, "String"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";"),
        raw_token(TokenType::TOKEN_R_CURL_BRACK, "}"),
        raw_token(TokenType::TOKEN_LET, "let"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_COLON, ":"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "Named"),
        raw_token(TokenType::TOKEN_EQUAL, "="),
        raw_token(TokenType::TOKEN_IDENTIFIER, "source"),
        raw_token(TokenType::TOKEN_IN, "in"),
        raw_token(TokenType::TOKEN_IDENTIFIER, "x"),
        raw_token(TokenType::TOKEN_DOT, "."),
        raw_token(TokenType::TOKEN_IDENTIFIER, "describe"),
        raw_token(TokenType::TOKEN_LPAREN, "("),
        raw_token(TokenType::TOKEN_RPAREN, ")"),
        raw_token(TokenType::TOKEN_SEMICOLON, ";")
    });
    EXPECT_TRUE(r, protocol_res.is_success());
    if (protocol_res.is_success()) {
        auto* program = dynamic_cast<ProgramNode*>(protocol_res.ast.get());
        EXPECT_TRUE(r, program != nullptr);
        if (program) {
            EXPECT_EQ(r, program->decls.size(), (size_t)2);
            EXPECT_EQ(r, program->stmts.size(), (size_t)1);
            if (program->decls.size() == 2) {
                auto* hashable = dynamic_cast<ProtocolDeclNode*>(program->decls[0]);
                auto* named = dynamic_cast<ProtocolDeclNode*>(program->decls[1]);
                EXPECT_TRUE(r, hashable != nullptr);
                EXPECT_TRUE(r, named != nullptr);
                if (hashable) {
                    EXPECT_EQ(r, hashable->name, std::string("Hashable"));
                    EXPECT_EQ(r, hashable->methods.size(), (size_t)1);
                }
                if (named) {
                    EXPECT_EQ(r, named->extendedProtocol, std::string("Hashable"));
                    EXPECT_EQ(r, named->methods.size(), (size_t)1);
                    auto* describe = dynamic_cast<FunctionDeclNode*>(named->methods[0]);
                    EXPECT_TRUE(r, describe != nullptr);
                    if (describe) {
                        EXPECT_TRUE(r, describe->isSignatureOnly);
                        EXPECT_TRUE(r, describe->isProtocolMethod);
                        EXPECT_EQ(r, describe->declaredReturnTypeName, std::string("String"));
                    }
                }
            }
            if (program->stmts.size() == 1) {
                auto* exprStmt = dynamic_cast<ExprStmtNode*>(program->stmts[0]);
                EXPECT_TRUE(r, exprStmt != nullptr);
                if (exprStmt) {
                    auto* letNode = dynamic_cast<LetNode*>(exprStmt->expr);
                    EXPECT_TRUE(r, letNode != nullptr);
                    if (letNode) {
                        EXPECT_EQ(r, letNode->declaredTypeName, std::string("Named"));
                        auto* callNode = dynamic_cast<FunctionCallNode*>(letNode->body);
                        EXPECT_TRUE(r, callNode != nullptr);
                        if (callNode) {
                            EXPECT_EQ(r, callNode->name, std::string("describe"));
                            EXPECT_TRUE(r, callNode->receiver != nullptr);
                        }
                    }
                }
            }
        }
    }
}
