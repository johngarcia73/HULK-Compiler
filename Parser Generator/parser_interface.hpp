#pragma once

#include <vector>
#include <unordered_map>
#include <string>
#include <memory>
#include <stdexcept>
#include "grammar.hpp"
#include "lalr_builder.hpp"
#include "ast_builder.hpp"



// Parse Result
struct ParseResult {
    enum class Status {
        Success,    // Parsing succeeded
        SyntaxError, // Unexpected token
        UnexpectedEOF, // End of input
        InternalError  // Internal parser error
    };
    
    Status status;
    std::string error_message;
    int error_line;
    int error_column;
    std::string error_token;
    
    // Parse tree (simplified - stores reductions)
    std::vector<uint32_t> reduction_sequence;  
    
    std::unique_ptr<ASTNode> ast;

    bool is_success() const { return status == Status::Success; }
};

// LALR Parser
class LALRParser {
public:
    // Factory: Create parser from grammar
    static LALRParser from_grammar(
        const Grammar& grammar,
        uint32_t dollar_symbol
    );
    
    // Copy/Move semantics
    LALRParser(const LALRParser& other);
    LALRParser(LALRParser&& other) noexcept;
    LALRParser& operator=(const LALRParser& other);
    LALRParser& operator=(LALRParser&& other) noexcept;
    ~LALRParser() = default;
    
    // Parse a sequence of tokens
    ParseResult parse(const std::vector<Token>& tokens);
    
    void set_builder(std::unique_ptr<ASTBuilder> builder);

    // Query parser properties
    size_t state_count() const;
    size_t transition_count() const;
    size_t conflict_count() const;
    bool has_conflicts() const;
    
    // Conflict information
    const std::vector<Conflict>& conflicts() const;
    
    // Grammar access
    const Grammar& grammar() const;

    
private:
    // Private constructor (use from_grammar factory)
    LALRParser(const Grammar& grammar, uint32_t dollar_symbol);
    
    // Parser state
    const Grammar* _grammar;
    uint32_t _dollar_symbol;
    ParseTables _tables;
    std::vector<Conflict> _conflicts;
    std::unique_ptr<ASTBuilder> _builder;

    // Parsing helpers
    ParseResult run_parser(const std::vector<Token>& tokens);
    
    std::string format_error(
        const ParseResult::Status status,
        const std::string& token_name,
        int line,
        int column
    );


};

// ============================================================================
// Exceptions
// ============================================================================

class ParserException : public std::runtime_error {
public:
    explicit ParserException(const std::string& msg)
        : std::runtime_error(msg) {}
};

class GrammarException : public std::runtime_error {
public:
    explicit GrammarException(const std::string& msg)
        : std::runtime_error(msg) {}
};

