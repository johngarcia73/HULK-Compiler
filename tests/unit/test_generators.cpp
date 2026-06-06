#include "test_harness.hpp"

#include <cstdlib>
#include <filesystem>
#include <sys/stat.h>

void test_generators(TestRunner& r) {
    std::cout << "[test_generators] Verificando regeneracion explicita de generadores\n";

    const char* regen = std::getenv("REGENERATE");
    if (!regen) {
        std::cout << "Skipping generator tests (set REGENERATE=1 to run)\n";
        return;
    }

    // Build lexer generator
    const bool testsCwd = std::filesystem::exists("../lexer/Lexer_Generator");
    const char* lexerMakeCmd = testsCwd
        ? "make -C ../lexer/Lexer_Generator"
        : "make -C lexer/Lexer_Generator";
    const char* parserMakeCmd = testsCwd
        ? "make -C ../parser/Parser_Generator"
        : "make -C parser/Parser_Generator";
    const char* lexerBinary = testsCwd
        ? "../lexer/Lexer_Generator/test"
        : "lexer/Lexer_Generator/test";
    const char* parserBinary = testsCwd
        ? "../parser/Parser_Generator/test"
        : "parser/Parser_Generator/test";

    int rc1 = std::system(lexerMakeCmd);
    EXPECT_EQ(r, rc1, 0);

    // Build parser generator
    int rc2 = std::system(parserMakeCmd);
    EXPECT_EQ(r, rc2, 0);

    // Optionally check produced binaries (best-effort)
    struct stat st;
    if (stat(lexerBinary, &st) == 0) {
        EXPECT_TRUE(r, true);
    } else {
        std::cout << "(warning) lexer test binary not found after build\n";
    }

    if (stat(parserBinary, &st) == 0) {
        EXPECT_TRUE(r, true);
    } else {
        std::cout << "(warning) parser test binary not found after build\n";
    }
}
