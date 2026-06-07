#include "frontend_pipeline.hpp"
#include "frontend_cache.hpp"

#include "../common/token.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/AST_Builder/ast_builder.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../semantic/analyzer.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <unordered_map>

namespace {

std::string error_type_name(CompilerErrorKind kind) {
    switch (kind) {
        case CompilerErrorKind::Lexical:
            return "LEXICAL";
        case CompilerErrorKind::Syntactic:
            return "SYNTACTIC";
        case CompilerErrorKind::Semantic:
            return "SEMANTIC";
    }
    return "SEMANTIC";
}

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("cannot open input file '" + path + "'");
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

CompilerError lexical_error_from_exception(const std::exception& ex) {
    std::regex pattern(R"(Lexical error at line ([0-9]+), column ([0-9]+): (.*))");
    std::cmatch match;
    if (std::regex_match(ex.what(), match, pattern)) {
        return {
            CompilerErrorKind::Lexical,
            std::stoi(match[1].str()),
            std::stoi(match[2].str()),
            match[3].str()
        };
    }
    return {CompilerErrorKind::Lexical, 0, 0, ex.what()};
}

CompilerError syntactic_error_from_parse_result(const ParseResult& result) {
    std::string message;
    if (result.status == ParseResult::Status::UnexpectedEOF) {
        message = "unexpected end of input";
    } else if (!result.error_token.empty()) {
        message = "unexpected token '" + result.error_token + "'";
    } else {
        message = result.error_message.empty() ? "syntax error" : result.error_message;
    }

    return {
        CompilerErrorKind::Syntactic,
        result.error_line > 0 ? result.error_line : 0,
        result.error_column > 0 ? result.error_column : 0,
        message
    };
}

CompilerError semantic_error_from_diagnostic(const SemanticDiagnostic& diagnostic) {
    int line = 0;
    int column = 0;
    if (diagnostic.span.start.isValid()) {
        line = diagnostic.span.start.line;
        column = diagnostic.span.start.column;
    }
    return {CompilerErrorKind::Semantic, line, column, diagnostic.message};
}

std::vector<Token> remap_tokens_to_grammar_symbols(
    const std::vector<Token>& raw_tokens,
    const std::unordered_map<std::string, uint32_t>& name_to_id,
    std::vector<CompilerError>& errors) {
    std::vector<Token> tokens;
    tokens.reserve(raw_tokens.size() + 1);

    for (const auto& token : raw_tokens) {
        const std::string token_name =
            token_type_to_string(static_cast<TokenType>(token.symbol_id));
        auto symbol = name_to_id.find(token_name);
        if (symbol == name_to_id.end()) {
            errors.push_back({
                CompilerErrorKind::Lexical,
                token.line,
                token.column,
                "token '" + token.value + "' is not valid in this grammar"
            });
            return {};
        }
        tokens.emplace_back(
            symbol->second,
            token.value,
            token.line,
            token.column,
            token.end_line,
            token.end_column);
    }

    tokens.emplace_back(name_to_id.at("$"), "$", 0, 0);
    return tokens;
}

ParseResult parse_without_debug(LALRParser& parser, const std::vector<Token>& tokens) {
    std::ostringstream suppressed_parser_debug;
    auto* previous_cerr = std::cerr.rdbuf(suppressed_parser_debug.rdbuf());
    ParseResult result = parser.parse(tokens);
    std::cerr.rdbuf(previous_cerr);
    return result;
}

FrontendCacheOptions production_cache_options() {
    FrontendCacheOptions options;
    options.cache_dir = ".hulk_cache";
    options.force_rebuild = std::getenv("HULK_FORCE_FRONTEND_REBUILD") != nullptr;
    options.trace = false;
    return options;
}

}  // namespace

FrontendPipelineResult run_frontend_pipeline(const std::string& source_path) {
    FrontendPipelineResult result;
    std::remove("output");

    try {
        const std::string input = read_file(source_path);
        FrontendCacheOptions cache_options = production_cache_options();
        CachedParserBundle parser_bundle = load_cached_parser(
            "parser/Parser_Generator/grammar.y",
            cache_options);
        Grammar& grammar = *parser_bundle.grammar;
        auto& name_to_id = parser_bundle.name_to_id;
        LALRParser& parser = *parser_bundle.parser;

        Lexer lexer = load_cached_lexer(
            default_token_specs(),
            "lexer/tokens.hpp",
            cache_options);
        std::vector<Token> raw_tokens;
        try {
            raw_tokens = lexer.tokenize(input);
        } catch (const std::exception& ex) {
            result.errors.push_back(lexical_error_from_exception(ex));
            return result;
        }

        std::vector<Token> tokens =
            remap_tokens_to_grammar_symbols(raw_tokens, name_to_id, result.errors);
        if (!result.errors.empty()) {
            return result;
        }

        parser.set_builder(std::make_unique<ASTBuilder>(&grammar));
        ParseResult parse_result = parse_without_debug(parser, tokens);
        if (!parse_result.is_success()) {
            result.errors.push_back(syntactic_error_from_parse_result(parse_result));
            return result;
        }

        ProgramNode* ast = dynamic_cast<ProgramNode*>(parse_result.ast.get());
        if (!ast) {
            result.errors.push_back({
                CompilerErrorKind::Syntactic,
                0,
                0,
                "parser did not produce a program AST"
            });
            return result;
        }

        SemanticSymbolTable symbols;
        SemanticAnalyzer analyzer(symbols);
        SemanticAnalysisResult semantic_result = analyzer.analyze(ast, SemanticDebugOptions{});
        if (!semantic_result.success()) {
            for (const auto& diagnostic : semantic_result.diagnostics) {
                if (diagnostic.severity == SemanticDiagnosticSeverity::Error) {
                    result.errors.push_back(semantic_error_from_diagnostic(diagnostic));
                }
            }
            return result;
        }

        result.ast = ast;
        result.ast_owner = std::move(parse_result.ast);
        return result;
    } catch (const std::exception& ex) {
        result.errors.push_back({CompilerErrorKind::Syntactic, 0, 0, ex.what()});
        return result;
    }
}

int exit_code_for(const FrontendPipelineResult& result) {
    if (result.success()) {
        return 0;
    }
    if (result.errors.empty()) {
        return static_cast<int>(CompilerErrorKind::Syntactic);
    }
    return static_cast<int>(result.errors.front().kind);
}

void print_compiler_errors(const std::vector<CompilerError>& errors) {
    for (const auto& error : errors) {
        std::cerr << "(" << error.line << "," << error.column << ") "
                  << error_type_name(error.kind) << ": " << error.message << "\n";
    }
}
