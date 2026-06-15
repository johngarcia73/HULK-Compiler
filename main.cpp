#include "compiler/frontend_pipeline.hpp"

#include <iostream>

int main(int argc, char** argv) {
    if (argc != 2) {
        print_compiler_errors({{
            CompilerErrorKind::Syntactic,
            0,
            0,
            "usage: ./hulk <file.hulk>"
        }});
        return static_cast<int>(CompilerErrorKind::Syntactic);
    }

    FrontendPipelineResult frontend = run_frontend_pipeline(argv[1]);
    if (!frontend.success()) {
        print_compiler_errors(frontend.errors);
        return exit_code_for(frontend);
    }

    ProgramNode* ast = frontend.ast;

    return 0;
}
