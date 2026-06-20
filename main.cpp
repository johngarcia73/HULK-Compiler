#include "compiler/frontend_pipeline.hpp"
#include "ir_generator/ir_generator.hpp"
#include "lowering/lowering.hpp"
#include "CLI11.hpp"          // <-- add this
#include <cstdlib>
#include <iostream>
#include <string>
#include <fstream>

// Helper to run a shell command and optionally print it
int run_command(const std::string& cmd, bool verbose) {
    if (verbose) std::cout << "[hulk] " << cmd << '\n';
    return system(cmd.c_str());
}

int main(int argc, char** argv) {
    CLI::App app{"Hulk compiler - compiles .hulk files to native executables"};

    // ----- Define command line options -----
    std::string input_file;
    std::string output_exe = "output";
    bool verbose = false;
    bool debug = false;

    app.add_option("input", input_file, "Source file (.hulk)")
        ->required()
        ->check(CLI::ExistingFile);   // validates file exists

    app.add_option("-o,--output", output_exe, "Output executable name (default: output)");

    app.add_flag("-v,--verbose", verbose, "Print commands as they are executed");
    app.add_flag("--debug", debug, "Output the AST of the compiler frontend");

    // CLI11 automatically adds --help and --version; we only customise version string
    app.set_version_flag("--version", "Hulk compiler 0.1");

    // ----- Parse arguments -----
    try {
        app.parse(argc, argv);
    } catch (const CLI::ParseError& e) {
        return app.exit(e);
    }

    // ----- Frontend pipeline -----
    FrontendPipelineResult frontend = run_frontend_pipeline(input_file.c_str());
    if (!frontend.success()) {
        print_compiler_errors(frontend.errors);
        return exit_code_for(frontend);
    }

    if (debug) {
        
        std::string ast_file = output_exe+".ast";

        std::ofstream out(ast_file);
        if (!out) {
            std::cerr << "Error: cannot write to " << ast_file << '\n';
            return 4;
        }
        frontend.ast->print(out);
      
    }


    ProgramNode* ast = frontend.ast;
    //Lowering stub
    LoweringVisitor lowering;
    ast->accept(lowering);
    // ----- IR generation -----
    IrGenerator ir_generator;
    std::string qbe_ir = ir_generator.generate(ast);

    // Derive intermediate file names from output executable basename
    std::string base = output_exe;
    std::string ssa_file = base + ".ssa";
    std::string asm_file = base + ".s";
    std::string obj_file = base + ".o";

    // Write QBE IR to .ssa file
    {
        std::ofstream out(ssa_file);
        if (!out) {
            std::cerr << ": Fatal: cannotnot write to " << ssa_file << '\n';
            return 4;
        }
        out << qbe_ir;
    }

    // 1. QBE: .ssa → .s (assembly)
    std::string qbe_cmd = "deps/qbe/qbe -o " + asm_file + " " + ssa_file;
    if (run_command(qbe_cmd, verbose) != 0) {
        std::cerr << "Fatal: QBE compilation failed.\n";
        return 4;
    }

    // 2. Assembler: .s → .o
    std::string as_cmd = "cc -c " + asm_file + " -o " + obj_file;
    if (run_command(as_cmd, verbose) != 0) {
        std::cerr << "Fatal: Assembly failed.\n";
        return 4;
    }

    // 3. Link with runtime and Boehm GC → final executable
    std::string link_cmd = "cc " + obj_file + " runtime.o -Ldeps/bdwgc -lgc -lpthread -lm -o " + output_exe;
    if (run_command(link_cmd, verbose) != 0) {
        std::cerr << "Fatal: Linking failed.\n";
        return 4;
    }

    if (verbose) {
        std::cout << "Successfully built " << output_exe << '\n';
    }

    return 0;
}
