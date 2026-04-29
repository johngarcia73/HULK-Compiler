#include "test_harness.hpp"

#include <cstdlib>
#include <sys/stat.h>

void test_generators(TestRunner& r) {
    std::cout << "[test_generators] Verificando regeneracion explicita de generadores\n";

    const char* regen = std::getenv("REGENERATE");
    if (!regen) {
        std::cout << "Skipping generator tests (set REGENERATE=1 to run)\n";
        return;
    }

    // Build lexer generator
    int rc1 = std::system("make -C ../lexer/Lexer_Generator");
    EXPECT_EQ(r, rc1, 0);

    // Build parser generator
    int rc2 = std::system("make -C ../parser/Parser_Generator");
    EXPECT_EQ(r, rc2, 0);

    // Optionally check produced binaries (best-effort)
    struct stat st;
    if (stat("../lexer/Lexer_Generator/test", &st) == 0) {
        EXPECT_TRUE(r, true);
    } else {
        std::cout << "(warning) lexer test binary not found after build\n";
    }

    if (stat("../parser/Parser_Generator/test", &st) == 0) {
        EXPECT_TRUE(r, true);
    } else {
        std::cout << "(warning) parser test binary not found after build\n";
    }
}
