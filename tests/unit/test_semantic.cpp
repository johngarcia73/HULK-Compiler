#include "test_harness.hpp"

#include "../../parser/AST_Builder/ast_node.hpp"
#include "../../semantic/analyzer.hpp"
#include "../../semantic/symbol_table.hpp"
#include "../../semantic/type.hpp"

namespace {

void print_annotated_ast(ProgramNode* program) {
    std::cout << "Annotated AST:\n";
    if (program) {
        program->print(std::cout, 2);
    }
    std::cout << "\n";
}

}  // namespace

void test_semantic(TestRunner& r) {
    std::cout << "[test_semantic] Analizando programa de prueba y mostrando AST anotado\n";

    std::vector<std::string> params = {"x"};
    std::vector<Type*> param_types = {NumberType::instance()};
    auto* square_body =
        new BinaryOpNode("*", new VariableNode("x"), new VariableNode("x"));
    auto* square_decl =
        new FunctionDeclNode("square", params, param_types, nullptr, square_body, true);

    auto* square_call = new FunctionCallNode("square", {new NumberNode("5")});
    auto* print_call = new FunctionCallNode("print", {square_call});
    auto* concat_expr = new BinaryOpNode(
        "@",
        new StringNode("The meaning of life is "),
        new NumberNode("42"));
    auto* modulo_expr = new BinaryOpNode("%", new NumberNode("5"), new NumberNode("2"));
    auto* bool_condition = new BinaryOpNode(
        "&",
        new BoolNode(true),
        new UnaryOpNode("!", new BoolNode(false)));
    auto* elif_like_expr = new IfNode(
        new BinaryOpNode("==", modulo_expr, new NumberNode("1")),
        new StringNode("odd"),
        new IfNode(
            bool_condition,
            new StringNode("bool-ok"),
            new StringNode("fallback")));
    auto* concat_stmt = new ExprStmtNode(concat_expr);
    auto* elif_stmt = new ExprStmtNode(elif_like_expr);
    auto* stmt = new ExprStmtNode(print_call);
    auto* while_loop = new WhileNode(
        new BinaryOpNode(">=", new VariableNode("counter"), new NumberNode("0")),
        new BlockNode({
            new ExprStmtNode(new FunctionCallNode("print", {new VariableNode("counter")})),
            new ExprStmtNode(
                new AssignmentNode(
                    "counter",
                    new BinaryOpNode("-", new VariableNode("counter"), new NumberNode("1"))))
        }));
    auto* while_expr_stmt = new ExprStmtNode(
        new LetNode("counter", new NumberNode("3"), while_loop));
    auto* for_expr = new ForNode(
        "i",
        new FunctionCallNode("range", {new NumberNode("0"), new NumberNode("3")}),
        new FunctionCallNode("print", {new VariableNode("i")}));
    auto* for_stmt = new ExprStmtNode(for_expr);

    ProgramNode* prog = new ProgramNode(
        {square_decl},
        {concat_stmt, elif_stmt, stmt, while_expr_stmt, for_stmt});

    SemanticSymbolTable table;
    SemanticAnalyzer analyzer(table);
    SemanticDebugOptions options;
    options.trace_pipeline = r.verbose;
    options.trace_inference = r.verbose;
    options.trace_overloads = r.verbose;
    SemanticAnalysisResult res = analyzer.analyze(prog, options);

    if ((r.verbose || !res.success()) &&
        (!res.traces.empty() || !res.diagnostics.empty())) {
        std::cout << "Semantic report:\n";
        analyzer.reportErrors(std::cout);
    }
    print_annotated_ast(prog);

    EXPECT_TRUE(r, res.success());
    EXPECT_TRUE(r, !analyzer.hasErrors());
    EXPECT_TRUE(r, square_decl->returnType != nullptr);
    if (square_decl->returnType) {
        EXPECT_TRUE(r, square_decl->returnType->equals(NumberType::instance()));
    }
    EXPECT_TRUE(r, square_decl->inferredFunctionType != nullptr);
    EXPECT_TRUE(r, square_call->type != nullptr);
    if (square_call->type) {
        EXPECT_TRUE(r, square_call->type->equals(NumberType::instance()));
    }
    EXPECT_TRUE(r, concat_expr->type != nullptr);
    if (concat_expr->type) {
        EXPECT_TRUE(r, concat_expr->type->equals(StringType::instance()));
    }
    EXPECT_TRUE(r, modulo_expr->type != nullptr);
    if (modulo_expr->type) {
        EXPECT_TRUE(r, modulo_expr->type->equals(NumberType::instance()));
    }
    EXPECT_TRUE(r, bool_condition->type != nullptr);
    if (bool_condition->type) {
        EXPECT_TRUE(r, bool_condition->type->equals(BoolType::instance()));
    }
    EXPECT_TRUE(r, elif_like_expr->type != nullptr);
    if (elif_like_expr->type) {
        EXPECT_TRUE(r, elif_like_expr->type->equals(StringType::instance()));
    }
    EXPECT_TRUE(r, print_call->resolvedFunctionType != nullptr);
    EXPECT_TRUE(r, print_call->type != nullptr);
    if (print_call->type) {
        EXPECT_TRUE(r, print_call->type->equals(VoidType::instance()));
    }
    EXPECT_TRUE(r, while_loop->type != nullptr);
    if (while_loop->type) {
        EXPECT_TRUE(r, while_loop->type->equals(NumberType::instance()));
    }
    EXPECT_TRUE(r, for_expr->type != nullptr);
    if (for_expr->type) {
        EXPECT_TRUE(r, for_expr->type->equals(VoidType::instance()));
    }

    delete prog;
}
