#include "test_harness.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

void test_lexer(TestRunner& r);
void test_parser(TestRunner& r);
void test_generators(TestRunner& r);
void test_semantic(TestRunner& r);

namespace {

struct TestCase {
    const char* name;
    void (*fn)(TestRunner&);
};

void log_header(const std::string& name) {
    std::cout << "\n=== Running test: " << name << " ===\n";
}

}  // namespace

int main(int argc, char** argv) {
    TestRunner runner;
    std::string selected = "all";

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-v" || arg == "--verbose") {
            runner.verbose = true;
        } else {
            selected = arg;
        }
    }

    std::vector<TestCase> tests = {
        {"lexer", test_lexer},
        {"parser", test_parser},
        {"generators", test_generators},
        {"semantic", test_semantic},
    };

    auto run = [&](const TestCase& test) {
        log_header(test.name);
        test.fn(runner);
    };

    if (selected == "all") {
        for (const auto& test : tests) {
            run(test);
        }
    } else {
        auto it = std::find_if(
            tests.begin(),
            tests.end(),
            [&](const TestCase& test) { return selected == test.name; });
        if (it == tests.end()) {
            std::cerr << "Unknown test: " << selected << "\n";
            std::cerr << "Available tests: lexer, parser, generators, semantic, all\n";
            return 2;
        }
        run(*it);
    }

    std::cout << "\nSummary: ";
    runner.summary();
    return runner.ok() ? 0 : 1;
}
