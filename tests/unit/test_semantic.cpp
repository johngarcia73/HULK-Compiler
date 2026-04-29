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

    auto* square_call = new FunctionCallNode("square", {new NumberNode(5)});
    auto* print_call = new FunctionCallNode("print", {square_call});
    auto* stmt = new ExprStmtNode(print_call);

    ProgramNode* prog = new ProgramNode({square_decl}, {stmt});

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
    EXPECT_TRUE(r, print_call->resolvedFunctionType != nullptr);
    EXPECT_TRUE(r, print_call->type != nullptr);
    if (print_call->type) {
        EXPECT_TRUE(r, print_call->type->equals(VoidType::instance()));
    }

    delete prog;
}
