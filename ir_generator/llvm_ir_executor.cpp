#include "llvm_ir_executor.hpp"
#include "llvm_ir_generator.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>

bool LLVMExecutor::generateIRToFile(ProgramNode* ast, const std::string& llFilename) {
    LLVMIRGenerator generator;
    if (!generator.generate(ast)) {
        std::cerr << "LLVM IR generation failed.\n";
        return false;
    }
    if (!generator.writeToFile(llFilename)) {
        std::cerr << "Failed to write IR to " << llFilename << "\n";
        return false;
    }
    std::cout << "LLVM IR written to " << llFilename << "\n";
    return true;
}

std::string LLVMExecutor::getIRString(ProgramNode* ast) {
    LLVMIRGenerator generator;
    if (!generator.generate(ast)) {
        std::cerr << "LLVM IR generation failed.\n";
        return "";
    }
    return generator.getIRString();
}

int LLVMExecutor::compileAndRun(ProgramNode* ast, const std::string& llFilename,
                                const std::string& exeFilename) {
    // Generate IR and write to file
    if (!generateIRToFile(ast, llFilename)) {
        return -1;
    }

    // Compile .ll to executable using clang
    std::string compileCmd = "clang " + llFilename + " -o " + exeFilename + " 2> /dev/null";
    int compileRet = std::system(compileCmd.c_str());
    if (compileRet != 0) {
        std::cerr << "Compilation failed. Ensure clang is installed and in PATH.\n";
        return -1;
    }

    // Run the executable
    std::string runCmd = "./" + exeFilename;
    int runRet = std::system(runCmd.c_str());
    if (runRet != 0) {
        std::cerr << "Execution failed with exit code " << runRet << "\n";
    }
    return runRet;
}