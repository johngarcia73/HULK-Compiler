#ifndef SNAPSHOT_HARNESS_HPP
#define SNAPSHOT_HARNESS_HPP

#include <iostream>
#include <sstream>
#include <string>
#include <fstream>
#include <cstdlib>

struct SnapshotRunner {
    int total = 0;
    int failed = 0;

    void expect_snapshot(const std::string& actual_output, const std::string& snapshot_path, const char* file, int line) {
        total++;
        std::ifstream snapshot_file(snapshot_path);
        
        const char* force_update = std::getenv("UPDATE_SNAPSHOTS");
        bool update = (force_update != nullptr && std::string(force_update) == "1");

        if (!snapshot_file.good() || update) {
            // First run or forced update: write the golden file and pass
            std::ofstream out_file(snapshot_path);
            out_file << actual_output;
            if (update) {
                std::cout << "[UPDATED] Snapshot: " << snapshot_path << " at " << file << ":" << line << "\n";
            } else {
                std::cout << "[CREATED] Snapshot: " << snapshot_path << " at " << file << ":" << line << "\n";
            }
        } else {
            // Read golden file and compare
            std::stringstream buffer;
            buffer << snapshot_file.rdbuf();
            std::string expected_output = buffer.str();

            if (actual_output != expected_output) {
                failed++;
                std::string actual_path = snapshot_path;
                size_t pos = actual_path.find(".ssa");
                if (pos != std::string::npos) {
                    actual_path.replace(pos, 4, ".actual.ssa");
                } else {
                    actual_path += ".actual";
                }
                std::ofstream out_actual(actual_path);
                out_actual << actual_output;
                std::cerr << "FAIL: Snapshot mismatch at " << file << ":" << line << "\n"
                          << "      Expected contents of '" << snapshot_path << "'\n"
                          << "      Actual output saved to '" << actual_path << "' (Use string comp or diff tools to compare)\n";
            }
        }
    }

    void summary() {
        std::cout << "Ran " << total << " snapshot assertions. Failures: " << failed << "\n";
    }

    bool ok() const { return failed == 0; }
};

#define EXPECT_SNAPSHOT(runner, output, snapshot_path) (runner).expect_snapshot(output, snapshot_path, __FILE__, __LINE__)

#endif // SNAPSHOT_HARNESS_HPP
