#include "test_harness.hpp"

#include "../../parser/utils/Grammar/grammar.hpp"
#include "../../parser/utils/Symbol/symbol.hpp"
#include "../../parser/Parser_Generator/genparser.hpp"
#include "../../parser/Parser_Generator/parser_interface.hpp"
#include "../../parser/AST_Builder/ast_builder.hpp"
#include "../../parser/AST_Builder/ast_node.hpp"
#include "../../lexer/tokens.hpp"
#include "../../common/token.hpp"
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace {

ParseResult parse_with_project_grammar(const std::vector<Token>& raw_tokens) {
    Grammar grammar = build_grammar_from_file("../parser/Parser_Generator/grammar.y");

    std::unordered_map<std::string, uint32_t> name_to_id;
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        name_to_id[grammar.symtab[i].name] = i;
    }

    LALRParser parser = LALRParser::from_grammar(grammar, name_to_id.at("$"));
    parser.set_builder(std::make_unique<ASTBuilder>());

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
    std::cout << "[test_parser] Parseando gramatica real para if sin else y let multiple\n";

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
                                EXPECT_EQ(r, number_node->kind, std::string("int"));
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
                            EXPECT_EQ(r, outer_init->kind, std::string("int"));
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
                                    EXPECT_EQ(r, right_num->kind, std::string("int"));
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
}
