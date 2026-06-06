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
    auto* promoted_sum = new BinaryOpNode(
        "+",
        new NumberNode("1", NumberKind::Int),
        new NumberNode("2.5", NumberKind::Double));
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
    auto* promoted_stmt = new ExprStmtNode(promoted_sum);
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
        {concat_stmt, promoted_stmt, elif_stmt, stmt, while_expr_stmt, for_stmt});

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
        EXPECT_TRUE(r, modulo_expr->type->equals(NumberType::instance(NumberKind::Int)));
    }
    EXPECT_TRUE(r, promoted_sum->type != nullptr);
    if (promoted_sum->type) {
        EXPECT_TRUE(r, promoted_sum->type->equals(NumberType::instance(NumberKind::Double)));
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
        EXPECT_TRUE(r, while_loop->type->equals(NumberType::instance(NumberKind::Int)));
    }
    EXPECT_TRUE(r, for_expr->type != nullptr);
    if (for_expr->type) {
        EXPECT_TRUE(r, for_expr->type->equals(VoidType::instance()));
    }

    delete prog;

    std::cout << "[test_semantic] Analizando tipos nominales, herencia y casts\n";

    auto* pointAttrX = new AttributeDeclNode(
        "x",
        new VariableNode("x"),
        NumberType::instance(),
        "Number",
        true);
    auto* pointAttrY = new AttributeDeclNode(
        "y",
        new VariableNode("y"),
        NumberType::instance(),
        "Number",
        true);
    auto* pointDescribe = new FunctionDeclNode(
        "describe",
        {},
        {},
        StringType::instance(),
        new BinaryOpNode(
            "@",
            new BinaryOpNode(
                "@",
                new StringNode("Point("),
                new FunctionCallNode("getX", {}, new VariableNode("self"))),
            new StringNode(")")),
        true);
    pointDescribe->isMethod = true;
    pointDescribe->ownerTypeName = "Point";
    pointDescribe->declaredReturnType = StringType::instance();
    pointDescribe->declaredReturnTypeName = "String";
    pointDescribe->hasExplicitReturnType = true;

    auto* pointGetX = new FunctionDeclNode(
        "getX",
        {},
        {},
        NumberType::instance(),
        new MemberAccessNode(new VariableNode("self"), "x"),
        true);
    pointGetX->isMethod = true;
    pointGetX->ownerTypeName = "Point";
    pointGetX->declaredReturnType = NumberType::instance();
    pointGetX->declaredReturnTypeName = "Number";
    pointGetX->hasExplicitReturnType = true;

    auto* pointGetY = new FunctionDeclNode(
        "getY",
        {},
        {},
        NumberType::instance(),
        new MemberAccessNode(new VariableNode("self"), "y"),
        true);
    pointGetY->isMethod = true;
    pointGetY->ownerTypeName = "Point";
    pointGetY->declaredReturnType = NumberType::instance();
    pointGetY->declaredReturnTypeName = "Number";
    pointGetY->hasExplicitReturnType = true;

    auto* pointDecl = new TypeDeclNode(
        "Point",
        {"x", "y"},
        {NumberType::instance(), NumberType::instance()},
        {"Number", "Number"},
        "Object",
        {},
        {pointAttrX, pointAttrY, pointGetX, pointGetY, pointDescribe},
        false);

    auto* labeledAttr = new AttributeDeclNode(
        "label",
        new VariableNode("label"),
        StringType::instance(),
        "String",
        true);
    auto* labeledDescribe = new FunctionDeclNode(
        "describe",
        {},
        {},
        StringType::instance(),
        new BinaryOpNode(
            "@",
            new FunctionCallNode("base", {}),
            new BinaryOpNode(
                "@",
                new StringNode(" "),
                new MemberAccessNode(new VariableNode("self"), "label"))),
        true);
    labeledDescribe->isMethod = true;
    labeledDescribe->ownerTypeName = "LabeledPoint";
    labeledDescribe->declaredReturnType = StringType::instance();
    labeledDescribe->declaredReturnTypeName = "String";
    labeledDescribe->hasExplicitReturnType = true;

    auto* labeledDecl = new TypeDeclNode(
        "LabeledPoint",
        {"x", "y", "label"},
        {NumberType::instance(), NumberType::instance(), StringType::instance()},
        {"Number", "Number", "String"},
        "Point",
        {new VariableNode("x"), new VariableNode("y")},
        {labeledAttr, labeledDescribe},
        true);

    auto* newLabeled = new NewNode(
        "LabeledPoint",
        {new NumberNode("3"), new NumberNode("4"), new StringNode("A")});
    auto* describeCall = new FunctionCallNode("describe", {}, new VariableNode("lp"));
    auto* describeStmt = new ExprStmtNode(
        new LetNode(
            "lp",
            newLabeled,
            describeCall,
            nullptr,
            "LabeledPoint",
            true));

    auto* castExpr = new BinaryOpNode("as", new VariableNode("obj"), new TypeNode("LabeledPoint"));
    auto* isExpr = new BinaryOpNode("is", new VariableNode("obj"), new TypeNode("LabeledPoint"));
    auto* downcastDescribe = new FunctionCallNode("describe", {}, castExpr);
    auto* typeFlowStmt = new ExprStmtNode(
        new LetNode(
            "obj",
            new NewNode("LabeledPoint", {new NumberNode("1"), new NumberNode("2"), new StringNode("B")}),
            new IfNode(isExpr, downcastDescribe, new StringNode("none")),
            nullptr,
            "Point",
            true));

    ProgramNode* nominalProg = new ProgramNode(
        {pointDecl, labeledDecl},
        {describeStmt, typeFlowStmt});

    SemanticSymbolTable nominalTable;
    SemanticAnalyzer nominalAnalyzer(nominalTable);
    SemanticDebugOptions nominalOptions;
    nominalOptions.trace_pipeline = r.verbose;
    nominalOptions.trace_inference = r.verbose;
    nominalOptions.trace_overloads = r.verbose;
    SemanticAnalysisResult nominalRes = nominalAnalyzer.analyze(nominalProg, nominalOptions);
    if ((r.verbose || !nominalRes.success()) &&
        (!nominalRes.traces.empty() || !nominalRes.diagnostics.empty())) {
        std::cout << "Nominal semantic report:\n";
        nominalAnalyzer.reportErrors(std::cout);
    }
    print_annotated_ast(nominalProg);

    EXPECT_TRUE(r, nominalRes.success());
    EXPECT_TRUE(r, pointDecl->type != nullptr);
    EXPECT_TRUE(r, labeledDecl->type != nullptr);
    EXPECT_TRUE(r, pointDecl->type && labeledDecl->type);
    if (pointDecl->type && labeledDecl->type) {
        EXPECT_TRUE(r, typeConforms(labeledDecl->type, pointDecl->type));
    }
    EXPECT_TRUE(r, labeledDescribe->returnType != nullptr);
    if (labeledDescribe->returnType) {
        EXPECT_TRUE(r, labeledDescribe->returnType->equals(StringType::instance()));
    }
    EXPECT_TRUE(r, describeCall->type != nullptr);
    if (describeCall->type) {
        EXPECT_TRUE(r, describeCall->type->equals(StringType::instance()));
    }
    EXPECT_TRUE(r, downcastDescribe->type != nullptr);
    if (downcastDescribe->type) {
        EXPECT_TRUE(r, downcastDescribe->type->equals(StringType::instance()));
    }
    EXPECT_TRUE(r, isExpr->type != nullptr);
    if (isExpr->type) {
        EXPECT_TRUE(r, isExpr->type->equals(BoolType::instance()));
    }

    delete nominalProg;

    std::cout << "[test_semantic] Analizando protocolos, extension y conformidad estructural\n";

    auto* hashSig = new FunctionDeclNode(
        "hash",
        {},
        {},
        NumberType::instance(),
        static_cast<ASTNode*>(nullptr));
    hashSig->isSignatureOnly = true;
    hashSig->isProtocolMethod = true;
    hashSig->ownerProtocolName = "Hashable";
    hashSig->declaredReturnType = NumberType::instance();
    hashSig->declaredReturnTypeName = "Number";
    hashSig->hasExplicitReturnType = true;
    auto* hashableProtocol = new ProtocolDeclNode("Hashable", "", {hashSig});

    auto* describeSig = new FunctionDeclNode(
        "describe",
        {},
        {},
        StringType::instance(),
        static_cast<ASTNode*>(nullptr));
    describeSig->isSignatureOnly = true;
    describeSig->isProtocolMethod = true;
    describeSig->ownerProtocolName = "Named";
    describeSig->declaredReturnType = StringType::instance();
    describeSig->declaredReturnTypeName = "String";
    describeSig->hasExplicitReturnType = true;
    auto* namedProtocol = new ProtocolDeclNode("Named", "Hashable", {describeSig});

    auto* aliasHashSig = new FunctionDeclNode(
        "hash",
        {},
        {},
        NumberType::instance(),
        static_cast<ASTNode*>(nullptr));
    aliasHashSig->isSignatureOnly = true;
    aliasHashSig->isProtocolMethod = true;
    aliasHashSig->ownerProtocolName = "AliasHashable";
    aliasHashSig->declaredReturnType = NumberType::instance();
    aliasHashSig->declaredReturnTypeName = "Number";
    aliasHashSig->hasExplicitReturnType = true;
    auto* aliasHashableProtocol = new ProtocolDeclNode("AliasHashable", "", {aliasHashSig});

    auto* personNameAttr = new AttributeDeclNode(
        "name",
        new VariableNode("name"),
        StringType::instance(),
        "String",
        true);
    auto* personHash = new FunctionDeclNode(
        "hash",
        {},
        {},
        NumberType::instance(),
        new NumberNode("1"),
        true);
    personHash->isMethod = true;
    personHash->ownerTypeName = "Person";
    personHash->declaredReturnType = NumberType::instance();
    personHash->declaredReturnTypeName = "Number";
    personHash->hasExplicitReturnType = true;
    auto* personDescribe = new FunctionDeclNode(
        "describe",
        {},
        {},
        StringType::instance(),
        new MemberAccessNode(new VariableNode("self"), "name"),
        true);
    personDescribe->isMethod = true;
    personDescribe->ownerTypeName = "Person";
    personDescribe->declaredReturnType = StringType::instance();
    personDescribe->declaredReturnTypeName = "String";
    personDescribe->hasExplicitReturnType = true;
    auto* personDecl = new TypeDeclNode(
        "Person",
        {"name"},
        {StringType::instance()},
        {"String"},
        "Object",
        {},
        {personNameAttr, personHash, personDescribe},
        false);

    auto* showNamedDecl = new FunctionDeclNode(
        "showNamed",
        {"item"},
        {new ProtocolType("Named")},
        StringType::instance(),
        new FunctionCallNode("describe", {}, new VariableNode("item")),
        true);
    showNamedDecl->paramTypeNames = {"Named"};
    showNamedDecl->declaredReturnType = StringType::instance();
    showNamedDecl->declaredReturnTypeName = "String";
    showNamedDecl->hasExplicitReturnType = true;

    auto* hashValueDecl = new FunctionDeclNode(
        "hashValue",
        {"item"},
        {new ProtocolType("Hashable")},
        NumberType::instance(),
        new FunctionCallNode("hash", {}, new VariableNode("item")),
        true);
    hashValueDecl->paramTypeNames = {"Hashable"};
    hashValueDecl->declaredReturnType = NumberType::instance();
    hashValueDecl->declaredReturnTypeName = "Number";
    hashValueDecl->hasExplicitReturnType = true;

    auto* showStmt = new ExprStmtNode(
        new LetNode(
            "person",
            new NewNode("Person", {new StringNode("Ada")}),
            new FunctionCallNode("showNamed", {new VariableNode("person")}),
            nullptr,
            "Named",
            true));

    auto* hashStmt = new ExprStmtNode(
        new LetNode(
            "alias",
            new NewNode("Person", {new StringNode("Grace")}),
            new FunctionCallNode("hashValue", {new VariableNode("alias")}),
            nullptr,
            "AliasHashable",
            true));

    ProgramNode* protocolProg = new ProgramNode(
        {hashableProtocol, namedProtocol, aliasHashableProtocol, personDecl, showNamedDecl, hashValueDecl},
        {showStmt, hashStmt});

    SemanticSymbolTable protocolTable;
    SemanticAnalyzer protocolAnalyzer(protocolTable);
    SemanticAnalysisResult protocolRes = protocolAnalyzer.analyze(protocolProg, nominalOptions);
    if ((r.verbose || !protocolRes.success()) &&
        (!protocolRes.traces.empty() || !protocolRes.diagnostics.empty())) {
        std::cout << "Protocol semantic report:\n";
        protocolAnalyzer.reportErrors(std::cout);
    }
    print_annotated_ast(protocolProg);

    EXPECT_TRUE(r, protocolRes.success());
    EXPECT_TRUE(r, showNamedDecl->paramTypes.size() == (size_t)1);
    if (showNamedDecl->paramTypes.size() == 1) {
        EXPECT_TRUE(r, dynamic_cast<ProtocolType*>(showNamedDecl->paramTypes[0]) != nullptr);
    }
    auto* showCall = dynamic_cast<FunctionCallNode*>(showStmt->expr ? dynamic_cast<LetNode*>(showStmt->expr)->body : nullptr);
    EXPECT_TRUE(r, showCall != nullptr);
    if (showCall) {
        EXPECT_TRUE(r, showCall->type != nullptr);
        if (showCall->type) {
            EXPECT_TRUE(r, showCall->type->equals(StringType::instance()));
        }
    }
    auto* hashCall = dynamic_cast<FunctionCallNode*>(hashStmt->expr ? dynamic_cast<LetNode*>(hashStmt->expr)->body : nullptr);
    EXPECT_TRUE(r, hashCall != nullptr);
    if (hashCall) {
        EXPECT_TRUE(r, hashCall->type != nullptr);
        if (hashCall->type) {
            EXPECT_TRUE(r, hashCall->type->equals(NumberType::instance()));
        }
    }

    delete protocolProg;

    std::cout << "[test_semantic] Analizando inferencia local de parametros sin anotacion\n";

    auto* twiceDecl = new FunctionDeclNode(
        "twice",
        {"n"},
        {nullptr},
        nullptr,
        new BinaryOpNode("+", new VariableNode("n"), new VariableNode("n")),
        true);
    auto* twiceCall = new FunctionCallNode("twice", {new NumberNode("21")});
    auto* inferredPrint = new FunctionCallNode("print", {twiceCall});
    auto* inferredStmt = new ExprStmtNode(inferredPrint);

    auto* counterCurrent = new AttributeDeclNode(
        "current",
        new BinaryOpNode("+", new VariableNode("seed"), new NumberNode("1")));
    auto* counterValue = new FunctionDeclNode(
        "value",
        {},
        {},
        NumberType::instance(),
        new MemberAccessNode(new VariableNode("self"), "current"),
        true);
    counterValue->isMethod = true;
    counterValue->ownerTypeName = "Counter";
    counterValue->declaredReturnType = NumberType::instance();
    counterValue->declaredReturnTypeName = "Number";
    counterValue->hasExplicitReturnType = true;
    auto* counterDecl = new TypeDeclNode(
        "Counter",
        {"seed"},
        {nullptr},
        {""},
        "Object",
        {},
        {counterCurrent, counterValue},
        false);
    auto* counterStmt = new ExprStmtNode(
        new LetNode(
            "counter",
            new NewNode("Counter", {new NumberNode("4")}),
            new FunctionCallNode("value", {}, new VariableNode("counter"))));

    ProgramNode* inferenceProg = new ProgramNode(
        {twiceDecl, counterDecl},
        {inferredStmt, counterStmt});

    SemanticSymbolTable inferenceTable;
    SemanticAnalyzer inferenceAnalyzer(inferenceTable);
    SemanticAnalysisResult inferenceRes = inferenceAnalyzer.analyze(inferenceProg, nominalOptions);
    if ((r.verbose || !inferenceRes.success()) &&
        (!inferenceRes.traces.empty() || !inferenceRes.diagnostics.empty())) {
        std::cout << "Inference semantic report:\n";
        inferenceAnalyzer.reportErrors(std::cout);
    }
    print_annotated_ast(inferenceProg);

    EXPECT_TRUE(r, inferenceRes.success());
    EXPECT_TRUE(r, twiceDecl->paramTypes.size() == (size_t)1);
    if (twiceDecl->paramTypes.size() == 1) {
        EXPECT_TRUE(r, twiceDecl->paramTypes[0] != nullptr);
        if (twiceDecl->paramTypes[0]) {
            EXPECT_TRUE(r, twiceDecl->paramTypes[0]->equals(NumberType::instance()));
        }
    }
    EXPECT_TRUE(r, counterDecl->ctorParamTypes.size() == (size_t)1);
    if (counterDecl->ctorParamTypes.size() == 1) {
        EXPECT_TRUE(r, counterDecl->ctorParamTypes[0] != nullptr);
        if (counterDecl->ctorParamTypes[0]) {
            EXPECT_TRUE(r, counterDecl->ctorParamTypes[0]->equals(NumberType::instance()));
        }
    }
    EXPECT_TRUE(r, counterCurrent->type != nullptr);
    if (counterCurrent->type) {
        EXPECT_TRUE(r, counterCurrent->type->equals(NumberType::instance()));
    }

    delete inferenceProg;

    std::cout << "[test_semantic] Comprobando error por asignacion ilegal a self\n";

    auto* badMethod = new FunctionDeclNode(
        "bad",
        {},
        {},
        NumberType::instance(),
        new AssignmentNode(
            new VariableNode("self"),
            new NewNode("Broken", {})),
        true);
    badMethod->isMethod = true;
    badMethod->ownerTypeName = "Broken";
    badMethod->declaredReturnType = NumberType::instance();
    badMethod->declaredReturnTypeName = "Number";
    badMethod->hasExplicitReturnType = true;

    auto* brokenDecl = new TypeDeclNode(
        "Broken",
        {},
        {},
        {},
        "Object",
        {},
        {badMethod},
        false);
    ProgramNode* brokenProg = new ProgramNode({brokenDecl}, {});

    SemanticSymbolTable brokenTable;
    SemanticAnalyzer brokenAnalyzer(brokenTable);
    SemanticAnalysisResult brokenRes = brokenAnalyzer.analyze(brokenProg, nominalOptions);
    EXPECT_TRUE(r, !brokenRes.success());
    EXPECT_TRUE(r, brokenAnalyzer.hasErrors());

    delete brokenProg;
}
