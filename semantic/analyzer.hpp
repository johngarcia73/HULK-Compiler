#pragma once

#include <iostream>
#include <string>
#include <vector>
#include "diagnostics.hpp"
#include "symbol_table.hpp"

struct ProgramNode;

class SemanticAnalyzer {
    SemanticSymbolTable& symTable;
    SemanticAnalysisResult lastResult_;
    std::vector<std::string> errors_;

    void registerBuiltinFunctions(SemanticContext& context);
    void refreshLegacyErrors();

public:
    explicit SemanticAnalyzer(SemanticSymbolTable& table) : symTable(table) {}

    SemanticAnalysisResult analyze(
        ProgramNode* root,
        SemanticDebugOptions options = {});

    void reportErrors(std::ostream& out = std::cerr) const;
    bool hasErrors() const { return !lastResult_.success(); }
    const std::vector<std::string>& getErrors() const { return errors_; }
    const SemanticAnalysisResult& getResult() const { return lastResult_; }

    const SemanticSymbolTable& getSymbolTable() const { return symTable; }
};
