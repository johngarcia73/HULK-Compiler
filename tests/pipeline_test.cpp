#include "../common/token.hpp"
#include "../parser/utils/Grammar/grammar.hpp"
#include "../parser/Parser_Generator/lalr_builder.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "../parser/AST_Builder/ast_builder.hpp"
#include "../parser/Parser_Generator/genparser.hpp"
#include "../lexer/lexer.hpp"
#include "../semantic/analyzer.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

std::string grammar_dir = "../parser/Parser_Generator/grammar.y";

struct RunnerOptions {
    std::string program_path = "program.hulk";
    bool trace_pipeline = true;
    bool trace_inference = true;
    bool trace_overloads = true;
    bool dump_ast = true;
    bool dump_symbols = false;
    bool dump_deps = false;
    bool dump_tokens = true;
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

        print_stage("Loading grammar.");
        trace_pipeline("Loading grammar and building parser.");
        Grammar grammar = build_grammar_from_file(grammar_dir);
        print_success("Grammar loaded.");

        std::unordered_map<std::string, uint32_t> name_to_id;
        for (size_t i = 0; i < grammar.symtab.size(); ++i)
            name_to_id[grammar.symtab[i].name] = i;

        print_stage("Building parser.");
        uint32_t eof_id = name_to_id["$"];
        LALRParser parser = LALRParser::from_grammar(grammar, eof_id);
        parser.set_builder(std::make_unique<ASTBuilder>());
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
        trace_pipeline("Building lexer.");
        Lexer lexer(default_token_specs());
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
                print_annotated_ast(program);
            }
            return 1;
        }
        print_success("Semantic analysis successful.");
        print_annotated_ast(program);

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