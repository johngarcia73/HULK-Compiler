#pragma once

#include "../lexer/lexer.hpp"
#include "../parser/Parser_Generator/parser_interface.hpp"
#include "../parser/utils/Grammar/grammar.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

struct FrontendCacheOptions {
    std::filesystem::path cache_dir = ".hulk_cache";
    bool force_rebuild = false;
    bool trace = false;
};

struct CachedParserBundle {
    std::unique_ptr<Grammar> grammar;
    std::unordered_map<std::string, uint32_t> name_to_id;
    std::unique_ptr<LALRParser> parser;
    bool loaded_from_cache = false;
};

Lexer load_cached_lexer(
    const std::vector<TokenSpec>& specs,
    const std::filesystem::path& token_source_path,
    const FrontendCacheOptions& options);

CachedParserBundle load_cached_parser(
    const std::filesystem::path& grammar_path,
    const FrontendCacheOptions& options);
