#include "../common/token.hpp"
#include "../parser/utils/Grammar/grammar.hpp"
#include "../parser/Parser_Generator/lalr_builder.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "../parser/AST_Builder/ast_builder.hpp"
#include "../parser/Parser_Generator/genparser.hpp"
#include "../lexer/lexer.hpp"
#include "../semantic/analyzer.hpp"
#include "../compiler/frontend_cache.hpp"
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static std::string resolve_path(std::initializer_list<const char*> candidates) {
    for (const char* candidate : candidates) {
        if (std::filesystem::exists(candidate)) {
            return candidate;
        }
    }
    throw std::runtime_error("Cannot resolve project path for pipeline test.");
}

struct RunnerOptions {
    std::string program_path = resolve_path({"program.hulk", "tests/program.hulk"});
    bool trace_pipeline = true;
    bool trace_inference = true;
    bool trace_overloads = true;
    bool dump_ast = true;
    bool dump_symbols = false;
    bool dump_deps = false;
    bool dump_tokens = true;
    bool force_frontend_rebuild = false;
    std::string frontend_cache_dir = ".hulk_cache";
};

static void print_section(const std::string& title) {
    std::cout << "\n=== " << title << " ===" << std::endl;
}

static void print_stage(const std::string& message) {
    std::cout << "[stage] " << message << std::endl;
}

static void print_success(const std::string& message) {
    std::cout << "[ok] " << message << std::endl;
}

static void print_annotated_ast(ProgramNode* program) {
    if (!program) {
        return;
    }
    print_section("Annotated AST");
    program->print(std::cout);
    std::cout << std::endl;
}

static bool is_focused_call(const FunctionCallNode* call) {
    if (!call) {
        return false;
    }
    return call->name == "range"
        || call->name == "sum"
        || call->name == "mean"
        || call->name == "count_when"
        || call->name == "filter"
        || call->name == "invoke"
        || call->name == "size"
        || call->name == "iter"
        || call->name == "next"
        || call->name == "current";
}

static bool is_focused_ast_node(const ASTNode* node) {
    return dynamic_cast<const ForNode*>(node)
        || dynamic_cast<const VectorLiteralNode*>(node)
        || dynamic_cast<const VectorComprehensionNode*>(node)
        || dynamic_cast<const IndexAccessNode*>(node)
        || dynamic_cast<const LambdaNode*>(node)
        || is_focused_call(dynamic_cast<const FunctionCallNode*>(node));
}

static void collect_focused_ast_nodes(ASTNode* node, std::vector<ASTNode*>& focused) {
    if (!node) {
        return;
    }

    if (is_focused_ast_node(node)) {
        focused.push_back(node);
    }

    if (auto* program = dynamic_cast<ProgramNode*>(node)) {
        for (auto* decl : program->decls) collect_focused_ast_nodes(decl, focused);
        for (auto* stmt : program->stmts) collect_focused_ast_nodes(stmt, focused);
    } else if (auto* function = dynamic_cast<FunctionDeclNode*>(node)) {
        collect_focused_ast_nodes(function->body, focused);
        collect_focused_ast_nodes(function->exprBody, focused);
    } else if (auto* exprStmt = dynamic_cast<ExprStmtNode*>(node)) {
        collect_focused_ast_nodes(exprStmt->expr, focused);
    } else if (auto* block = dynamic_cast<BlockNode*>(node)) {
        for (auto* stmt : block->stmts) collect_focused_ast_nodes(stmt, focused);
    } else if (auto* binary = dynamic_cast<BinaryOpNode*>(node)) {
        collect_focused_ast_nodes(binary->left, focused);
        collect_focused_ast_nodes(binary->right, focused);
    } else if (auto* unary = dynamic_cast<UnaryOpNode*>(node)) {
        collect_focused_ast_nodes(unary->operand, focused);
    } else if (auto* let = dynamic_cast<LetNode*>(node)) {
        collect_focused_ast_nodes(let->init, focused);
        collect_focused_ast_nodes(let->body, focused);
    } else if (auto* conditional = dynamic_cast<IfNode*>(node)) {
        collect_focused_ast_nodes(conditional->condition, focused);
        collect_focused_ast_nodes(conditional->then_branch, focused);
        collect_focused_ast_nodes(conditional->else_branch, focused);
    } else if (auto* call = dynamic_cast<FunctionCallNode*>(node)) {
        collect_focused_ast_nodes(call->receiver, focused);
        for (auto* arg : call->args) collect_focused_ast_nodes(arg, focused);
    } else if (auto* assignment = dynamic_cast<AssignmentNode*>(node)) {
        collect_focused_ast_nodes(assignment->target, focused);
        collect_focused_ast_nodes(assignment->value, focused);
    } else if (auto* member = dynamic_cast<MemberAccessNode*>(node)) {
        collect_focused_ast_nodes(member->base, focused);
    } else if (auto* newExpr = dynamic_cast<NewNode*>(node)) {
        for (auto* arg : newExpr->args) collect_focused_ast_nodes(arg, focused);
    } else if (auto* lambda = dynamic_cast<LambdaNode*>(node)) {
        collect_focused_ast_nodes(lambda->body, focused);
    } else if (auto* vector = dynamic_cast<VectorLiteralNode*>(node)) {
        for (auto* element : vector->elements) collect_focused_ast_nodes(element, focused);
    } else if (auto* comprehension = dynamic_cast<VectorComprehensionNode*>(node)) {
        collect_focused_ast_nodes(comprehension->iterable, focused);
        collect_focused_ast_nodes(comprehension->expression, focused);
    } else if (auto* index = dynamic_cast<IndexAccessNode*>(node)) {
        collect_focused_ast_nodes(index->base, focused);
        collect_focused_ast_nodes(index->index, focused);
    } else if (auto* loop = dynamic_cast<WhileNode*>(node)) {
        collect_focused_ast_nodes(loop->condition, focused);
        collect_focused_ast_nodes(loop->body, focused);
    } else if (auto* loop = dynamic_cast<ForNode*>(node)) {
        collect_focused_ast_nodes(loop->iterable, focused);
        collect_focused_ast_nodes(loop->body, focused);
    } else if (auto* attr = dynamic_cast<AttributeDeclNode*>(node)) {
        collect_focused_ast_nodes(attr->init, focused);
    } else if (auto* typeDecl = dynamic_cast<TypeDeclNode*>(node)) {
        for (auto* arg : typeDecl->parentArgs) collect_focused_ast_nodes(arg, focused);
        for (auto* member : typeDecl->members) collect_focused_ast_nodes(member, focused);
    } else if (auto* protocol = dynamic_cast<ProtocolDeclNode*>(node)) {
        for (auto* method : protocol->methods) collect_focused_ast_nodes(method, focused);
    } else if (auto* ret = dynamic_cast<ReturnNode*>(node)) {
        collect_focused_ast_nodes(ret->expr, focused);
    } else if (auto* bindings = dynamic_cast<LetBindingsNode*>(node)) {
        for (auto* binding : bindings->bindings) collect_focused_ast_nodes(binding, focused);
    } else if (auto* binding = dynamic_cast<LetBindingNode*>(node)) {
        collect_focused_ast_nodes(binding->init, focused);
    }
}

static void print_focused_annotated_ast(ProgramNode* program) {
    if (!program) {
        return;
    }

    std::vector<ASTNode*> focused;
    collect_focused_ast_nodes(program, focused);

    print_section("Focused Annotated AST Nodes");
    if (focused.empty()) {
        std::cout << "<no focused nodes found>" << std::endl;
        return;
    }

    for (size_t i = 0; i < focused.size(); ++i) {
        std::cout << "\n--- Focused Node " << (i + 1) << " ---\n";
        focused[i]->print(std::cout, 2);
    }
    std::cout << std::endl;
}

static void print_program_source(const std::string& input) {
    print_section("Input Program");
    std::cout << input << std::endl;
}

static void print_token_stream(
    const std::vector<Token>& tokens,
    const std::vector<std::string>& id_to_name) {
    print_section("Token Stream");
    for (const auto& tk : tokens) {
        std::cout << id_to_name[tk.symbol_id]
                  << " -> \"" << tk.value << "\" ("
                  << tk.line << ":" << tk.column
                  << "-" << tk.end_line << ":" << tk.end_column << ")\n";
    }
    std::cout << std::flush;
}

static void print_usage(const char* argv0) {
    std::cout
        << "Usage: " << argv0 << " [options] [program]\n"
        << "Options:\n"
        << "  --trace-pipeline    Ensure pipeline traces are enabled\n"
        << "  --trace-inference   Ensure inference traces are enabled\n"
        << "  --trace-overloads   Ensure overload traces are enabled\n"
        << "  --dump-ast          Ensure annotated AST dump is enabled\n"
        << "  --dump-symbols      Dump the semantic symbol table\n"
        << "  --dump-deps         Dump the semantic dependency graph\n"
        << "  --dump-tokens       Ensure token stream dump is enabled\n"
        << "  --force-frontend-rebuild\n"
        << "                       Rebuild cached lexer DFA and parser tables\n"
        << "  --frontend-cache-dir <dir>\n"
        << "                       Cache directory for lexer/parser artifacts\n"
        << "  --help              Show this help message\n";
}

static bool parse_args(int argc, char** argv, RunnerOptions& options) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--trace-pipeline") {
            options.trace_pipeline = true;
        } else if (arg == "--trace-inference") {
            options.trace_inference = true;
        } else if (arg == "--trace-overloads") {
            options.trace_overloads = true;
        } else if (arg == "--dump-ast") {
            options.dump_ast = true;
        } else if (arg == "--dump-symbols") {
            options.dump_symbols = true;
        } else if (arg == "--dump-deps") {
            options.dump_deps = true;
        } else if (arg == "--dump-tokens") {
            options.dump_tokens = true;
        } else if (arg == "--force-frontend-rebuild") {
            options.force_frontend_rebuild = true;
        } else if (arg == "--frontend-cache-dir") {
            if (i + 1 >= argc) {
                std::cerr << "Missing value for --frontend-cache-dir\n";
                return false;
            }
            options.frontend_cache_dir = argv[++i];
        } else if (arg == "--help") {
            print_usage(argv[0]);
            std::exit(0);
        } else if (!arg.empty() && arg[0] == '-') {
            std::cerr << "Unknown option: " << arg << "\n";
            print_usage(argv[0]);
            return false;
        } else {
            options.program_path = arg;
        }
    }
    return true;
}

int main(int argc, char** argv) {
    try {
        RunnerOptions options;
        if (!parse_args(argc, argv, options)) {
            return 1;
        }

        print_section("Compiler Pipeline");
        print_stage("Program: " + options.program_path);

        auto trace_pipeline = [&](const std::string& message) {
            if (options.trace_pipeline) {
                std::cout << "[pipeline] " << message << std::endl;
            }
        };

        FrontendCacheOptions cache_options;
        cache_options.cache_dir = options.frontend_cache_dir;
        cache_options.force_rebuild = options.force_frontend_rebuild;
        cache_options.trace = options.trace_pipeline;

        print_stage("Building parser.");
        trace_pipeline("Loading grammar and parser tables.");
        CachedParserBundle parser_bundle = load_cached_parser(
            resolve_path({"../parser/Parser_Generator/grammar.y", "parser/Parser_Generator/grammar.y"}),
            cache_options);
        Grammar& grammar = *parser_bundle.grammar;
        auto& name_to_id = parser_bundle.name_to_id;
        LALRParser& parser = *parser_bundle.parser;
        parser.set_builder(std::make_unique<ASTBuilder>(&grammar));
        print_success("Parser ready.");

        print_stage("Loading input source.");
        std::ifstream file(options.program_path);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open " << options.program_path << "\n";
            return 1;
        }
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string input = buffer.str();
        file.close();
        print_success("Input source loaded.");
        print_program_source(input);

        print_stage("Building lexer.");
        trace_pipeline("Loading lexer DFA.");
        Lexer lexer = load_cached_lexer(
            default_token_specs(),
            resolve_path({"../lexer/tokens.hpp", "lexer/tokens.hpp"}),
            cache_options);
        print_success("Lexer ready.");

        print_stage("Tokenizing input.");
        trace_pipeline("Lexing input program.");
        std::vector<Token> raw_tokens = lexer.tokenize(input);
        std::vector<Token> tokens;
        for (auto& t : raw_tokens) {
            std::string token_name = token_type_to_string(static_cast<TokenType>(t.symbol_id));
            auto it = name_to_id.find(token_name);
            if (it == name_to_id.end()) {
                std::cerr << "Unknown token: " << t.value << "line: "<< t.line <<" column: "<< t.column<<"\n";
                return 1;
            }
            tokens.emplace_back(it->second, t.value, t.line, t.column, t.end_line, t.end_column);
        }
        tokens.emplace_back(name_to_id["$"], "$", 0, 0);
        print_success("Lexing successful (" + std::to_string(raw_tokens.size()) + " token(s)).");

        std::vector<std::string> id_to_name(grammar.symtab.size());
        for (size_t i = 0; i < grammar.symtab.size(); ++i) {
            id_to_name[i] = grammar.symtab[i].name;
        }
        if (options.dump_tokens) {
            print_token_stream(tokens, id_to_name);
        }

        print_stage("Parsing token stream.");
        trace_pipeline("Parsing token stream.");
        ParseResult result = parser.parse(tokens);
        if (!result.is_success()) {
            std::cerr << "Parse failed: " << result.error_message << "\n";
            return 1;
        }
        ProgramNode* program = dynamic_cast<ProgramNode*>(result.ast.get());
        if (!program) {
            std::cerr << "Root is not a ProgramNode\n";
            return 1;
        }
        print_success("AST constructed successfully.");

        print_stage("Running semantic analysis.");
        trace_pipeline("Running semantic analysis.");
        SemanticSymbolTable symTable;
        SemanticAnalyzer analyzer(symTable);
        SemanticDebugOptions debug_options;
        debug_options.trace_pipeline = options.trace_pipeline;
        debug_options.trace_inference = options.trace_inference;
        debug_options.trace_overloads = options.trace_overloads;
        debug_options.dump_symbols = options.dump_symbols;
        debug_options.dump_dependency_graph = options.dump_deps;

        SemanticAnalysisResult semantic_result = analyzer.analyze(program, debug_options);
        if (!semantic_result.traces.empty() || !semantic_result.success()) {
            print_section("Semantic Trace");
            analyzer.reportErrors(std::cout);
        }
        if (!semantic_result.success()) {
            if (options.dump_ast) {
                print_focused_annotated_ast(program);
            }
            return 1;
        }
        print_success("Semantic analysis successful.");
        if (options.dump_ast) {
            print_focused_annotated_ast(program);
        }

        /*
        // LLVM IR Generation
        LLVMIRGenerator llvmGen;
        if (!llvmGen.generate(program)) {
            std::cerr << "LLVM IR generation failed.\n";
            return 1;
        }
        llvmGen.writeToFile("output.ll");
        std::cout << "LLVM IR written to output.ll\n";

        // Compile & execute using the executor
        LLVMExecutor executor;
        int exitCode = executor.compileAndRun(program, "output.ll", "program");
        if (exitCode >= 0)
            std::cout << "Program exited with code " << exitCode << "\n";

        return 0;
        */
    } catch (const std::exception& e) {
        std::cerr << "FATAL: " << e.what() << "\n";
        return 2;
    }
}
//
