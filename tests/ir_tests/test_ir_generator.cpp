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
                makeString("Outer scope variable", StringType::instance()),
                buildBlock()
                    .addExpression(
                        buildFunctionCall("print")
                            .addArgument(makeVariable("x", StringType::instance()))
                            .setType(VoidType::instance())
                            .build()
                    )
                    .addExpression(
                        makeLetExpression(
                            "x",
                            makeString("Inner scope variable", StringType::instance()),
                            buildFunctionCall("print")
                                .addArgument(makeVariable("x", StringType::instance()))
                                .setType(VoidType::instance())
                                .build(),
                            VoidType::instance()
                        )
                    )
                    .addExpression(
                        buildFunctionCall("print")
                            .addArgument(makeVariable("x", StringType::instance()))
                            .setType(VoidType::instance())
                            .build()
                    )
                    .setType(VoidType::instance())
                    .build(),
                VoidType::instance()
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
                    makeNumber("5", NumberType::instance()),
                    makeNumber("10", NumberType::instance()),
                    BoolType::instance()
                ),
                buildFunctionCall("print")
                    .addArgument(makeString("Five is greather", StringType::instance()))
                    .setType(VoidType::instance())
                    .build(),
                buildFunctionCall("print")
                    .addArgument(makeString("Ten is greather", StringType::instance()))
                    .setType(VoidType::instance())
                    .build(),
                VoidType::instance()
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
                .addParameter("a", NumberType::instance())
                .addParameter("b", NumberType::instance())
                .setReturnType(NumberType::instance())
                .setInlineBody(
                    makeBinaryOperation("+", makeVariable("a", NumberType::instance()), makeVariable("b", NumberType::instance()), NumberType::instance())
                )
                .build()
        )
        .addStatement(
                    buildFunctionCall("print").
                    addArgument(
                        buildFunctionCall("add")
                            .addArgument(makeNumber("5", NumberType::instance()))
                            .addArgument(makeNumber("10", NumberType::instance()))
                            .setType(NumberType::instance())
                            .build()
                    )
                    .setType(NumberType::instance())
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
                            makeString("Hello, ", StringType::instance()),
                            makeString("World", StringType::instance()),
                            StringType::instance()
                        ),
                        makeString("!", StringType::instance()),
                        StringType::instance()
                    )
                )
                .setType(VoidType::instance())
                .build()
        )
        .build();
    return program;
}

ProgramNode* test_while_loop()
{
    auto let_exp =makeLetExpression(
                "i",
                makeNumber("0", NumberType::instance()),
                buildBlock()
                    .addExpression(
                        makeWhileLoop(
                            makeBinaryOperation("<", makeVariable("i", NumberType::instance()), makeNumber("5", NumberType::instance()), BoolType::instance()),
                            buildBlock()
                                .addExpression(
                                    makeAssignment(
                                        "i",
                                        makeBinaryOperation("+", makeVariable("i", NumberType::instance()), makeNumber("1", NumberType::instance()), NumberType::instance()),
                                        NumberType::instance()
                                    )
                                )
                                .setType(NumberType::instance())
                                .build(),
                            NumberType::instance()
                        )
                    )
                    .setType(NumberType::instance())
                    .build(),
                NumberType::instance()
            );
    auto program = buildProgram()
        .addStatement(
            buildFunctionCall("print")
            .addArgument(let_exp)
            .setType(NumberType::instance())
            .build()
        )
        .build();
    return program;
}

ProgramNode* test_point_type()
{
    auto numType = NumberType::instance();
    auto stringType = StringType::instance();
    auto voidType = VoidType::instance();
    auto pointTypePtr = new NominalType("Point");

    auto makeSelf = [pointTypePtr]() {
        auto v = makeVariable("self");
        v->type = pointTypePtr;
        return v;
    };

    auto pointType = buildTypeDeclaration("Point")
        .addCtorParam("x", numType)
        .addCtorParam("y", numType)
        .addAttribute("x", makeVariable("x", numType), numType)
        .addAttribute("y", makeVariable("y", numType), numType)
        .addMethod(
            buildFunction("getX")
                .setReturnType(numType)
                .setInlineBody(makeMemberAccess(makeSelf(), "x", numType))
                .build()
        )
        .addMethod(
            buildFunction("getY")
                .setReturnType(numType)
                .setInlineBody(makeMemberAccess(makeSelf(), "y", numType))
                .build()
        )
        .addMethod(
            buildFunction("setY")
                .addParameter("val", numType)
                .setReturnType(voidType)
                .setInlineBody(makeAssignment(makeMemberAccess(makeSelf(), "y", numType), makeVariable("val", numType), numType))
                .build()
        )
        .addMethod(
            buildFunction("describe")
                .setReturnType(stringType)
                .setInlineBody(
                    makeBinaryOperation("@",
                        makeBinaryOperation("@",
                            makeBinaryOperation("@",
                                makeString("Point(", stringType),
                                buildFunctionCall("getX")
                                    .setReceiver(makeSelf())
                                    .setType(numType)
                                    .build(),
                                stringType
                            ),
                            makeString(", ", stringType),
                            stringType
                        ),
                        makeBinaryOperation("@",
                            buildFunctionCall("getY")
                                .setReceiver(makeSelf())
                                .setType(numType)
                                .build(),
                            makeString(")", stringType),
                            stringType
                        ),
                        stringType
                    )
                )
                .build()
        )
        .setType(pointTypePtr)
        .build();

    return buildProgram()
        .addDeclaration(pointType)
        .addStatement(
            makeLetExpression(
                "p",
                makeNewNode("Point", {makeNumber("3", numType), makeNumber("4", numType)}, pointTypePtr),
                buildBlock()
                    .addExpression(
                        buildFunctionCall("setY")
                            .setReceiver([](Type* t) {
                                auto v = makeVariable("p");
                                v->type = t;
                                return v;
                            }(pointTypePtr))
                            .addArgument(makeNumber("100", numType))
                            .setType(voidType)
                            .build()
                    )
                    .addExpression(
                        buildFunctionCall("print")
                            .addArgument(buildFunctionCall("describe")
                                .setReceiver([](Type* t) {
                                    auto v = makeVariable("p");
                                    v->type = t;
                                    return v;
                                }(pointTypePtr))
                                .setType(stringType)
                                .build())
                            .setType(stringType)
                            .build()
                    )
                    .setType(stringType)
                    .build(),
                stringType
            )
        )
        .build();
}

ProgramNode* test_person_type()
{
    auto stringType = StringType::instance();
    auto numType = NumberType::instance();
    auto voidType = VoidType::instance();
    auto personTypePtr = new NominalType("Person");

    auto selfPerson = makeVariable("self");
    selfPerson->type = personTypePtr;

    auto personType = buildTypeDeclaration("Person")
        .addCtorParam("name", stringType)
        .addAttribute("name", makeVariable("name", stringType), stringType)
        .addMethod(
            buildFunction("hash")
                .setReturnType(numType)
                .setInlineBody(makeNumber("1", numType))
                .build()
        )
        .addMethod(
            buildFunction("describe")
                .setReturnType(stringType)
                .setInlineBody(makeMemberAccess(selfPerson, "name", stringType))
                .build()
        )
        .setType(personTypePtr)
        .build();

    auto personVar = makeVariable("person");
    personVar->type = personTypePtr;

    return buildProgram()
        .addDeclaration(personType)
        .addStatement(
            makeLetExpression(
                "person",
                makeNewNode("Person", {makeString("Ada", stringType)}, personTypePtr),
                buildFunctionCall("print")
                    .addArgument(buildFunctionCall("describe")
                        .setReceiver(personVar)
                        .setType(stringType)
                        .build())
                    .setType(stringType)
                    .build(),
                stringType
            )
        )
        .build();
}

ProgramNode* test_counter_type()
{
    auto numType = NumberType::instance();
    auto voidType = VoidType::instance();
    auto counterTypePtr = new NominalType("Counter");

    auto selfCounter = makeVariable("self");
    selfCounter->type = counterTypePtr;

    auto counterType = buildTypeDeclaration("Counter")
        .addCtorParam("seed", numType)
        .addAttribute("current", makeBinaryOperation("+", makeVariable("seed", numType), makeNumber("1", numType), numType), numType)
        .addMethod(
            buildFunction("value")
                .setReturnType(numType)
                .setInlineBody(makeMemberAccess(selfCounter, "current", numType))
                .build()
        )
        .setType(counterTypePtr)
        .build();

    auto cVar = makeVariable("c");
    cVar->type = counterTypePtr;

    return buildProgram()
        .addDeclaration(counterType)
        .addStatement(
            makeLetExpression(
                "c",
                makeNewNode("Counter", {makeNumber("4", numType)}, counterTypePtr),
                buildFunctionCall("print")
                    .addArgument(buildFunctionCall("value")
                        .setReceiver(cVar)
                        .setType(numType)
                        .build())
                    .setType(numType)
                    .build(),
                numType
            )
        )
        .build();
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

    // Test 5: While loop
    ProgramNode* p5 = test_while_loop();
    IrGenerator generator5;
    std::string out5 = generator5.generate(*p5);
    EXPECT_SNAPSHOT(runner, out5, "tests/ir_tests/snapshots/while_loop.ssa");
    delete p5;

    // Test 6: Point type operations
    ProgramNode* p6 = test_point_type();
    IrGenerator generator6;
    std::string out6 = generator6.generate(*p6);
    EXPECT_SNAPSHOT(runner, out6, "tests/ir_tests/snapshots/point_type.ssa");
    delete p6;

    // Test 7: Person type operations
    ProgramNode* p7 = test_person_type();
    IrGenerator generator7;
    std::string out7 = generator7.generate(*p7);
    EXPECT_SNAPSHOT(runner, out7, "tests/ir_tests/snapshots/person_type.ssa");
    delete p7;

    // Test 8: Counter type operations
    ProgramNode* p8 = test_counter_type();
    IrGenerator generator8;
    std::string out8 = generator8.generate(*p8);
    EXPECT_SNAPSHOT(runner, out8, "tests/ir_tests/snapshots/counter_type.ssa");
    delete p8;
}

int main() {
    SnapshotRunner runner;
    std::cout << "# Running IR Snapshot Tests\n";
    run_snapshot_tests(runner);
    runner.summary();
    return runner.ok() ? 0 : 1;
}

