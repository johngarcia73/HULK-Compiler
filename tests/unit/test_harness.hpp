#ifndef TEST_HARNESS_HPP
#define TEST_HARNESS_HPP

#include <iostream>
#include <sstream>
#include <string>

struct TestRunner {
    int total = 0;
    int failed = 0;
    bool verbose = false;

    template<typename A, typename B>
    void expect_eq(const A& a, const B& b, const char* expr, const char* file, int line) {
        ++total;
        if (!(a == b)) {
            ++failed;
            std::cerr << "FAIL: " << expr << " at " << file << ":" << line << " -> got '" << a << "' expected '" << b << "'\n";
        }
    }

    void expect_true(bool cond, const char* expr, const char* file, int line) {
        ++total;
        if (!cond) {
            ++failed;
            std::cerr << "FAIL: " << expr << " at " << file << ":" << line << "\n";
        }
    }

    void summary() {
        std::cout << "Ran " << total << " assertions. Failures: " << failed << "\n";
    }

    bool ok() const { return failed == 0; }
};

#define EXPECT_EQ(runner, a, b) (runner).expect_eq(a, b, #a " == " #b, __FILE__, __LINE__)
#define EXPECT_TRUE(runner, e) (runner).expect_true((e), #e, __FILE__, __LINE__)

#endif
