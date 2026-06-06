#include "frontend_cache.hpp"

#include "../parser/Parser_Generator/genparser.hpp"

#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {

constexpr const char* LexerCacheMagic = "HULK_LEXER_CACHE";
constexpr const char* ParserCacheMagic = "HULK_PARSER_CACHE";
constexpr int CacheVersion = 1;

struct FileSignature {
    uintmax_t size = 0;
    int64_t mtime = 0;
};

int64_t mtime_stamp(const std::filesystem::path& path) {
    const auto write_time = std::filesystem::last_write_time(path);
    return write_time.time_since_epoch().count();
}

FileSignature file_signature(const std::filesystem::path& path) {
    return {std::filesystem::file_size(path), mtime_stamp(path)};
}

uint64_t fnv1a_append(uint64_t hash, const std::string& text) {
    constexpr uint64_t prime = 1099511628211ull;
    for (unsigned char ch : text) {
        hash ^= ch;
        hash *= prime;
    }
    return hash;
}

uint64_t token_specs_fingerprint(const std::vector<TokenSpec>& specs) {
    uint64_t hash = 1469598103934665603ull;
    for (const auto& spec : specs) {
        hash = fnv1a_append(hash, std::to_string(static_cast<int>(spec.type)));
        hash = fnv1a_append(hash, spec.regex_infix);
        hash = fnv1a_append(hash, spec.skip ? "skip" : "keep");
    }
    return hash;
}

void ensure_cache_dir(const FrontendCacheOptions& options) {
    std::filesystem::create_directories(options.cache_dir);
}

std::filesystem::path lexer_cache_path(const FrontendCacheOptions& options) {
    return options.cache_dir / "lexer_dfa.cache";
}

std::filesystem::path parser_cache_path(const FrontendCacheOptions& options) {
    return options.cache_dir / "parser_tables.cache";
}

void trace_cache(const FrontendCacheOptions& options, const std::string& message) {
    if (options.trace) {
        std::cout << "[cache] " << message << std::endl;
    }
}

bool read_header(
    std::istream& in,
    const char* expected_magic,
    const FileSignature& expected_signature,
    uint64_t expected_fingerprint) {
    std::string magic;
    int version = 0;
    uintmax_t size = 0;
    int64_t mtime = 0;
    uint64_t fingerprint = 0;

    in >> magic >> version >> size >> mtime >> fingerprint;
    return in.good() &&
        magic == expected_magic &&
        version == CacheVersion &&
        size == expected_signature.size &&
        mtime == expected_signature.mtime &&
        fingerprint == expected_fingerprint;
}

void write_header(
    std::ostream& out,
    const char* magic,
    const FileSignature& signature,
    uint64_t fingerprint) {
    out << magic << ' '
        << CacheVersion << ' '
        << signature.size << ' '
        << signature.mtime << ' '
        << fingerprint << '\n';
}

bool read_dfa_cache(
    const std::filesystem::path& path,
    const FileSignature& token_signature,
    uint64_t fingerprint,
    DFA& dfa,
    std::vector<bool>& skip_table) {
    std::ifstream in(path);
    if (!in.is_open() || !read_header(in, LexerCacheMagic, token_signature, fingerprint)) {
        return false;
    }

    size_t accept_count = 0;
    size_t transition_state_count = 0;
    size_t accept_token_count = 0;
    size_t skip_count = 0;

    in >> dfa.start;
    in >> accept_count;
    dfa.accept.clear();
    for (size_t i = 0; i < accept_count; ++i) {
        int state = 0;
        in >> state;
        dfa.accept.insert(state);
    }

    in >> transition_state_count;
    dfa.transitions.clear();
    for (size_t i = 0; i < transition_state_count; ++i) {
        int state = 0;
        size_t edge_count = 0;
        in >> state >> edge_count;
        auto& edges = dfa.transitions[state];
        for (size_t j = 0; j < edge_count; ++j) {
            int ch = 0;
            int next = 0;
            in >> ch >> next;
            edges[static_cast<char>(ch)] = next;
        }
    }

    in >> accept_token_count;
    dfa.accept_token.clear();
    for (size_t i = 0; i < accept_token_count; ++i) {
        int state = 0;
        int token = 0;
        in >> state >> token;
        dfa.accept_token[state] = token;
    }

    in >> skip_count;
    skip_table.assign(skip_count, false);
    for (size_t i = 0; i < skip_count; ++i) {
        int value = 0;
        in >> value;
        skip_table[i] = value != 0;
    }

    return in.good() || in.eof();
}

void write_dfa_cache(
    const std::filesystem::path& path,
    const FileSignature& token_signature,
    uint64_t fingerprint,
    const DFA& dfa,
    const std::vector<bool>& skip_table) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot write lexer cache: " + path.string());
    }

    write_header(out, LexerCacheMagic, token_signature, fingerprint);
    out << dfa.start << '\n';

    out << dfa.accept.size() << '\n';
    for (int state : dfa.accept) {
        out << state << '\n';
    }

    out << dfa.transitions.size() << '\n';
    for (const auto& [state, edges] : dfa.transitions) {
        out << state << ' ' << edges.size() << '\n';
        for (const auto& [ch, next] : edges) {
            out << static_cast<int>(static_cast<unsigned char>(ch)) << ' ' << next << '\n';
        }
    }

    out << dfa.accept_token.size() << '\n';
    for (const auto& [state, token] : dfa.accept_token) {
        out << state << ' ' << token << '\n';
    }

    out << skip_table.size() << '\n';
    for (bool value : skip_table) {
        out << (value ? 1 : 0) << '\n';
    }
}

void write_action(std::ostream& out, const Action& action) {
    out << static_cast<int>(action.kind) << ' ' << action.value;
}

Action read_action(std::istream& in) {
    int kind = 0;
    int value = 0;
    in >> kind >> value;
    return {static_cast<ActionKind>(kind), value};
}

bool read_parser_cache(
    const std::filesystem::path& path,
    const FileSignature& grammar_signature,
    uint64_t fingerprint,
    size_t expected_symbol_count,
    size_t expected_production_count,
    uint32_t expected_dollar_symbol,
    ParseTables& tables,
    std::vector<Conflict>& conflicts) {
    std::ifstream in(path);
    if (!in.is_open() || !read_header(in, ParserCacheMagic, grammar_signature, fingerprint)) {
        return false;
    }

    size_t symbol_count = 0;
    size_t production_count = 0;
    uint32_t dollar_symbol = 0;
    in >> symbol_count >> production_count >> dollar_symbol;
    if (symbol_count != expected_symbol_count ||
        production_count != expected_production_count ||
        dollar_symbol != expected_dollar_symbol) {
        return false;
    }

    size_t action_state_count = 0;
    in >> action_state_count;
    tables.ACTION.assign(action_state_count, {});
    for (size_t i = 0; i < action_state_count; ++i) {
        size_t entry_count = 0;
        in >> entry_count;
        for (size_t j = 0; j < entry_count; ++j) {
            uint32_t symbol = 0;
            in >> symbol;
            tables.ACTION[i][symbol] = read_action(in);
        }
    }

    size_t goto_state_count = 0;
    in >> goto_state_count;
    tables.GOTO.assign(goto_state_count, {});
    for (size_t i = 0; i < goto_state_count; ++i) {
        size_t entry_count = 0;
        in >> entry_count;
        for (size_t j = 0; j < entry_count; ++j) {
            uint32_t symbol = 0;
            uint32_t state = 0;
            in >> symbol >> state;
            tables.GOTO[i][symbol] = state;
        }
    }

    size_t conflict_count = 0;
    in >> conflict_count;
    conflicts.clear();
    for (size_t i = 0; i < conflict_count; ++i) {
        Conflict conflict;
        in >> conflict.state >> conflict.symbol >> std::quoted(conflict.detail);
        conflicts.push_back(conflict);
    }

    return in.good() || in.eof();
}

void write_parser_cache(
    const std::filesystem::path& path,
    const FileSignature& grammar_signature,
    uint64_t fingerprint,
    const Grammar& grammar,
    uint32_t dollar_symbol,
    const ParseTables& tables,
    const std::vector<Conflict>& conflicts) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) {
        throw std::runtime_error("Cannot write parser cache: " + path.string());
    }

    write_header(out, ParserCacheMagic, grammar_signature, fingerprint);
    out << grammar.symbol_count() << ' '
        << grammar.get_productions().size() << ' '
        << dollar_symbol << '\n';

    out << tables.ACTION.size() << '\n';
    for (const auto& state_map : tables.ACTION) {
        out << state_map.size() << '\n';
        for (const auto& [symbol, action] : state_map) {
            out << symbol << ' ';
            write_action(out, action);
            out << '\n';
        }
    }

    out << tables.GOTO.size() << '\n';
    for (const auto& state_map : tables.GOTO) {
        out << state_map.size() << '\n';
        for (const auto& [symbol, state] : state_map) {
            out << symbol << ' ' << state << '\n';
        }
    }

    out << conflicts.size() << '\n';
    for (const auto& conflict : conflicts) {
        out << conflict.state << ' '
            << conflict.symbol << ' '
            << std::quoted(conflict.detail) << '\n';
    }
}

}  // namespace

Lexer load_cached_lexer(
    const std::vector<TokenSpec>& specs,
    const std::filesystem::path& token_source_path,
    const FrontendCacheOptions& options) {
    ensure_cache_dir(options);

    const FileSignature token_signature = file_signature(token_source_path);
    const uint64_t fingerprint = token_specs_fingerprint(specs);
    const std::filesystem::path cache_path = lexer_cache_path(options);

    if (!options.force_rebuild) {
        DFA cached_dfa;
        std::vector<bool> cached_skip_table;
        if (read_dfa_cache(cache_path, token_signature, fingerprint, cached_dfa, cached_skip_table)) {
            trace_cache(options, "Loaded lexer DFA from " + cache_path.string());
            return Lexer(std::move(cached_dfa), std::move(cached_skip_table));
        }
    }

    trace_cache(options, "Building lexer DFA and refreshing " + cache_path.string());
    Lexer lexer(specs);
    write_dfa_cache(cache_path, token_signature, fingerprint, lexer.get_dfa(), lexer.get_skip_table());
    return lexer;
}

CachedParserBundle load_cached_parser(
    const std::filesystem::path& grammar_path,
    const FrontendCacheOptions& options) {
    ensure_cache_dir(options);

    CachedParserBundle bundle;
    bundle.grammar = std::make_unique<Grammar>(build_grammar_from_file(grammar_path.string()));
    Grammar& grammar = *bundle.grammar;
    for (size_t i = 0; i < grammar.symtab.size(); ++i) {
        bundle.name_to_id[grammar.symtab[i].name] = static_cast<uint32_t>(i);
    }

    const uint32_t dollar_symbol = bundle.name_to_id.at("$");
    const FileSignature grammar_signature = file_signature(grammar_path);
    const uint64_t fingerprint = fnv1a_append(
        1469598103934665603ull,
        std::to_string(grammar.symbol_count()) + ":" +
            std::to_string(grammar.get_productions().size()));
    const std::filesystem::path cache_path = parser_cache_path(options);

    if (!options.force_rebuild) {
        ParseTables tables;
        std::vector<Conflict> conflicts;
        if (read_parser_cache(
                cache_path,
                grammar_signature,
                fingerprint,
                grammar.symbol_count(),
                grammar.get_productions().size(),
                dollar_symbol,
                tables,
                conflicts)) {
            trace_cache(options, "Loaded parser tables from " + cache_path.string());
            bundle.parser = std::make_unique<LALRParser>(
                LALRParser::from_tables(grammar, dollar_symbol, tables, conflicts));
            bundle.loaded_from_cache = true;
            return bundle;
        }
    }

    trace_cache(options, "Building parser tables and refreshing " + cache_path.string());
    auto parser = LALRParser::from_grammar(grammar, dollar_symbol);
    write_parser_cache(
        cache_path,
        grammar_signature,
        fingerprint,
        grammar,
        dollar_symbol,
        parser.parse_tables(),
        parser.conflicts());
    bundle.parser = std::make_unique<LALRParser>(std::move(parser));
    bundle.loaded_from_cache = false;
    return bundle;
}
