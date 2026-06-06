#include "analyzer.hpp"

#include <sstream>
#include <unordered_set>
#include "dependency_graph.hpp"
#include "type_inference_visitor.hpp"

namespace {

std::vector<FunctionDeclNode*> buildOrderedFunctionList(
    const DependencyGraph& graph,
    const std::vector<FunctionDeclNode*>& pendingFunctions) {
    std::vector<FunctionDeclNode*> orderedFunctions;
    const auto& order = graph.getTopologicalOrder();
    std::unordered_set<FunctionDeclNode*> added;

    for (DepNode* depNode : order) {
        if (!depNode || depNode->kind != DepNodeKind::Function || !depNode->ast) {
            continue;
        }
        auto* function = dynamic_cast<FunctionDeclNode*>(depNode->ast);
        if (!function || added.count(function)) {
            continue;
        }
        orderedFunctions.push_back(function);
        added.insert(function);
    }

    for (auto* function : pendingFunctions) {
        if (!function || added.count(function)) {
            continue;
        }
        orderedFunctions.push_back(function);
    }

    return orderedFunctions;
}

}  // namespace

void SemanticAnalyzer::registerBuiltinFunctions(SemanticContext& context) {
    context.tracePipeline("Registering builtin functions.");

    if (symTable.getCurrentLevel() == 0) {
        symTable.enterScope();
    }

    auto insertBuiltin = [&](const std::string& name, FunctionType* type) {
        symTable.insertFunction(name, type);
        context.tracePipeline("Builtin registered: " + name + " : " + type->toString());
    };

    auto mathType = new FunctionType({NumberType::instance()}, NumberType::instance());
    insertBuiltin("sin", mathType);
    insertBuiltin("cos", mathType);
    insertBuiltin("tan", mathType);
    insertBuiltin("abs", mathType);
    insertBuiltin("sqrt", mathType);

    insertBuiltin("input", new FunctionType({}, StringType::instance()));
    insertBuiltin("range", new FunctionType({NumberType::instance(), NumberType::instance()}, AnyType::instance()));
    insertBuiltin(
        "_concat",
        new FunctionType(
            {StringType::instance(), StringType::instance()},
            StringType::instance()));

    insertBuiltin("print", new FunctionType({NumberType::instance()}, VoidType::instance()));
    insertBuiltin("print", new FunctionType({StringType::instance()}, VoidType::instance()));
    insertBuiltin("print", new FunctionType({UnknownType::instance()}, VoidType::instance()));
}

void SemanticAnalyzer::refreshLegacyErrors() {
    errors_.clear();
    for (const auto& diagnostic : lastResult_.diagnostics) {
        if (diagnostic.severity == SemanticDiagnosticSeverity::Error) {
            errors_.push_back(diagnostic.message);
        }
    }
}

SemanticAnalysisResult SemanticAnalyzer::analyze(
    ProgramNode* root,
    SemanticDebugOptions options) {
    symTable.clear();
    lastResult_ = {};
    errors_.clear();

    SemanticContext context(options);
    if (!root) {
        context.error(SemanticPhase::Pipeline, "Cannot analyze a null AST root.");
        lastResult_ = context.buildResult();
        refreshLegacyErrors();
        return lastResult_;
    }

    registerBuiltinFunctions(context);

    DependencyGraph graph;
    NominalTypeRegistry typeRegistry;
    TypeInferenceVisitor inferencer(symTable, graph, typeRegistry, context);

    context.tracePipeline("Collecting declarations and dependencies.");
    inferencer.collectDeclarations(root);

    std::vector<FunctionDeclNode*> orderedFunctions = inferencer.getPendingFunctions();
    if (!graph.topologicalSort()) {
        context.warning(
            SemanticPhase::Dependencies,
            "Dependency cycle detected; falling back to declaration order.",
            {},
            graph.getCycleMessages());
    } else {
        orderedFunctions = buildOrderedFunctionList(graph, inferencer.getPendingFunctions());
    }

    context.tracePipeline(
        "Analyzing " + std::to_string(orderedFunctions.size()) + " function body/bodies.");
    for (auto* function : orderedFunctions) {
        inferencer.analyzeFunction(function);
    }

    context.tracePipeline("Analyzing type declarations.");
    inferencer.analyzeTypes();

    context.tracePipeline("Analyzing global statements.");
    inferencer.analyzeGlobals(root);

    if (options.dump_dependency_graph) {
        std::ostringstream out;
        graph.dump(out);
        context.addDump("dependency-graph", out.str());
    }

    if (options.dump_symbols) {
        std::ostringstream out;
        symTable.dump(out);
        context.addDump("symbols", out.str());
    }

    if (options.dump_ast) {
        std::ostringstream out;
        root->print(out);
        context.addDump("ast", out.str());
    }

    lastResult_ = context.buildResult();
    refreshLegacyErrors();
    return lastResult_;
}

void SemanticAnalyzer::reportErrors(std::ostream& out) const {
    for (const auto& trace : lastResult_.traces) {
        out << trace << "\n";
    }
    for (const auto& diagnostic : lastResult_.diagnostics) {
        out << diagnostic.format() << "\n";
    }
}
