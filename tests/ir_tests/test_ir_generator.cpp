#include <iostream>
#include <fstream>
#include <vector>
#include "../../parser/AST_Builder/ast_node.hpp"
#include "../../ir_generator/ir_generator.hpp"
#include "snapshot_harness.hpp"
#include "ast_test_builder.hpp"
using namespace ASTTestBuilder;

ProgramNode* test_let_expression()
{
    auto program = buildProgram()
        .addStatement(
            makeLetExpression(
                "x",
                makeString("Outer scope variable"),
                buildBlock()
                    .addExpression(
                        buildFunctionCall("print")
                            .addArgument(makeVariable("x"))
                            .build()
                    )
                    .addExpression(
                        makeLetExpression(
                            "x",
                            makeString("Inner scope variable"),
                            buildFunctionCall("print")
                                .addArgument(makeVariable("x"))
                                .build()
                        )
                    )
                    .addExpression(
                        buildFunctionCall("print")
                            .addArgument(makeVariable("x"))
                            .build()
                    )
                    .build()
            )
        )
        .build();
    return program;
}
ProgramNode* test_conditionals()
{
    auto program = buildProgram()
        .addStatement(
            makeIfExpression(
                makeBinaryOperation(
                    ">",
                    makeNumber(5),
                    makeNumber(10)
                ),
                buildFunctionCall("print")
                    .addArgument(makeString("Five is greather"))
                    .build(),
                buildFunctionCall("print")
                    .addArgument(makeString("Ten is greather"))
                    .build()
            )
        )
        .build();   
        return program;
    
}
ProgramNode* test_function_decl()
{
    auto program = buildProgram()
        .addDeclaration(
            buildFunction("add")
                .addParameter("a")
                .addParameter("b")
                .setInlineBody(
                    makeBinaryOperation("+", makeVariable("a"), makeVariable("b"))
                )
                .build()
        )
        .addStatement(
                    buildFunctionCall("add")
                        .addArgument(makeNumber(5))
                        .addArgument(makeNumber(10))
                    .build()
        )
        .build();
    return program;
}

ProgramNode* test_string_concat()
{
    auto program = buildProgram()
        .addStatement(
            buildFunctionCall("print")
                .addArgument(
                    makeBinaryOperation(
                        "@",
                        makeBinaryOperation(
                            "@",
                            makeString("Hello, "),
                            makeString("World")
                        ),
                        makeString("!")
                    )
                )
                .build()
        )
        .build();
    return program;
}

void run_snapshot_tests(SnapshotRunner& runner) {
    // Test 1: Let Expression
    ProgramNode* p1 = test_let_expression();
    IrGenerator generator1;
    std::string out1 = generator1.generate(*p1);
    EXPECT_SNAPSHOT(runner, out1, "tests/ir_tests/snapshots/let_expression.ssa");
    delete p1;

    // Test 2: Conditionals
    ProgramNode* p2 = test_conditionals();
    IrGenerator generator2;
    std::string out2 = generator2.generate(*p2);
    EXPECT_SNAPSHOT(runner, out2, "tests/ir_tests/snapshots/conditionals.ssa");
    delete p2;

    // Test 3: Function declaration and call
    ProgramNode* p3 = test_function_decl();
    IrGenerator generator3;
    std::string out3 = generator3.generate(*p3);
    EXPECT_SNAPSHOT(runner, out3, "tests/ir_tests/snapshots/function_decl.ssa");
    delete p3;

    // Test 4: String concatenation
    ProgramNode* p4 = test_string_concat();
    IrGenerator generator4;
    std::string out4 = generator4.generate(*p4);
    EXPECT_SNAPSHOT(runner, out4, "tests/ir_tests/snapshots/string_concat.ssa");
    delete p4;
}

int main() {
    SnapshotRunner runner;
    std::cout << "# Running IR Snapshot Tests\n";
    run_snapshot_tests(runner);
    runner.summary();
    return runner.ok() ? 0 : 1;
}

