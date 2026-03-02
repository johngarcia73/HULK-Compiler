// ============================================================================
// generate_ast_builder.cpp
//
// Genera automáticamente ast_builder.cpp a partir de integration_grammar.y
// Debe ejecutarse antes de compilar el proyecto
// ============================================================================

#include "code_generator.hpp"
#include <iostream>

int main() {
    std::cout << "\n╔════════════════════════════════════════════════════════╗\n";
    std::cout << "║      Generating AST Builder from Grammar File         ║\n";
    std::cout << "╚════════════════════════════════════════════════════════╝\n\n";
    
    try {
        generate_ast_builder("integration_grammar.y", "ast_builder.cpp");
        std::cout << "\n✓ Successfully generated ast_builder.cpp\n";
        std::cout << "  You can now compile the project normally.\n\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "\n✗ Error: " << e.what() << "\n\n";
        return 1;
    }
}
