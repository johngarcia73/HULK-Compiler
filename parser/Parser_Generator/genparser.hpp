#pragma once

#include "../utils/Grammar/grammar.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>
#include <cctype>

// -----------------------------------------------------------------------------
// Parsed production
// -----------------------------------------------------------------------------

struct ParsedProduction {
    std::string lhs;
    std::vector<std::string> rhs;
};

struct GrammarSpec {
    std::vector<std::string> terminals;
    std::vector<std::string> nonterminals;
    std::string start_symbol;
    std::vector<ParsedProduction> productions;
    std::string user_code;
};

// -----------------------------------------------------------------------------
// Utility
// -----------------------------------------------------------------------------

inline void trim(std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    size_t b = s.find_last_not_of(" \t\r\n");

    if (a == std::string::npos)
    {
        s.clear();
        return;
    }

    s = s.substr(a, b - a + 1);
}

// Remove C-style comments /* ... */
inline std::string remove_c_comments(const std::string &line)
{
    std::string result;
    bool in_comment = false;
    for (size_t i = 0; i < line.size(); ++i) {
        if (!in_comment && i + 1 < line.size() && line[i] == '/' && line[i+1] == '*') {
            in_comment = true;
            ++i; // skip '*'
            continue;
        }
        if (in_comment && i + 1 < line.size() && line[i] == '*' && line[i+1] == '/') {
            in_comment = false;
            ++i; // skip '/'
            continue;
        }
        if (!in_comment) {
            result += line[i];
        }
    }
    return result;
}

// -----------------------------------------------------------------------------
// Split RHS alternatives ignoring {actions}
// -----------------------------------------------------------------------------

inline void parse_production_line(
    const std::string &line,
    const std::string &lhs,
    GrammarSpec &spec)
{
    // Check if the line is empty after trimming (epsilon production)
    std::string trimmed = line;
    trim(trimmed);
    if (trimmed.empty()) {
        spec.productions.push_back({lhs, {}});
        return;
    }

    size_t pos = 0;
    while (pos < line.size())
    {
        int brace_level = 0;
        size_t split = std::string::npos;

        for (size_t i = pos; i < line.size(); ++i)
        {
            if (line[i] == '{')
                brace_level++;

            else if (line[i] == '}')
                brace_level--;

            else if (line[i] == '|' && brace_level == 0)
            {
                split = i;
                break;
            }
        }

        std::string alt;

        if (split != std::string::npos)
        {
            alt = line.substr(pos, split - pos);
            pos = split + 1;
        }
        else
        {
            alt = line.substr(pos);
            pos = line.size();
        }

        // Remove C comments from the alternative
        alt = remove_c_comments(alt);
        trim(alt);

        // Remove semantic action (inside {})
        size_t brace = alt.find('{');
        if (brace != std::string::npos)
        {
            alt = alt.substr(0, brace);
        }
        trim(alt);

        // Epsilon production?
        if (alt.empty())
        {
            spec.productions.push_back({lhs, {}});
            continue;
        }

        // Tokenize RHS
        std::vector<std::string> rhs;
        std::stringstream ss(alt);
        std::string sym;
        while (ss >> sym)
        {
            rhs.push_back(sym);
        }

        spec.productions.push_back({lhs, rhs});

        // Discover possible nonterminals
        for (auto &s : rhs)
        {
            bool is_terminal = false;
            for (auto &t : spec.terminals)
                if (t == s)
                    is_terminal = true;

            if (!is_terminal)
            {
                bool found = false;
                for (auto &nt : spec.nonterminals)
                    if (nt == s)
                        found = true;
                if (!found)
                    spec.nonterminals.push_back(s);
            }
        }
    }
}

// -----------------------------------------------------------------------------
// Parse .y file
// -----------------------------------------------------------------------------

inline GrammarSpec parse_grammar_file(const std::string &filename)
{
    GrammarSpec spec;

    std::ifstream file(filename);
    if (!file)
        throw std::runtime_error("Cannot open grammar file");

    std::string line;
    bool in_productions = false;
    bool in_user_code = false;

    std::regex token_regex(R"(%token\s+(.+)$)");
    std::regex start_regex(R"(%start\s+([A-Za-z0-9_']+))");

    std::string current_lhs;

    while (std::getline(file, line))
    {
        // Remove C comments from the whole line first
        line = remove_c_comments(line);
        trim(line);

        if (line.empty())
            continue;

        // User code block
        if (line.find("%{") != std::string::npos)
        {
            in_user_code = true;
            continue;
        }
        if (line.find("%}") != std::string::npos)
        {
            in_user_code = false;
            continue;
        }
        if (in_user_code)
        {
            spec.user_code += line + "\n";
            continue;
        }

        // Separator %%
        if (line == "%%")
        {
            in_productions = !in_productions;
            continue;
        }

        // Declarations section
        if (!in_productions)
        {
            std::smatch m;
            if (std::regex_search(line, m, token_regex))
            {
                std::stringstream ss(m[1].str());
                std::string tok;
                while (ss >> tok)
                    spec.terminals.push_back(tok);
            }
            if (std::regex_search(line, m, start_regex))
            {
                spec.start_symbol = m[1].str();
            }
            continue;
        }

        // Productions section
        if (line.find(':') != std::string::npos)
        {
            size_t pos = line.find(':');
            current_lhs = line.substr(0, pos);
            trim(current_lhs);

            // Ensure LHS is in nonterminals
            bool found = false;
            for (auto &nt : spec.nonterminals)
                if (nt == current_lhs)
                    found = true;
            if (!found)
                spec.nonterminals.push_back(current_lhs);

            std::string rhs = line.substr(pos + 1);
            parse_production_line(rhs, current_lhs, spec);
        }
        else if (!current_lhs.empty() && line[0] == '|')
        {
            parse_production_line(line.substr(1), current_lhs, spec);
        }
        else if (line == ";")
        {
            current_lhs.clear();
        }
    }

    return spec;
}

// -----------------------------------------------------------------------------
// Build Grammar object
// -----------------------------------------------------------------------------

inline Grammar build_grammar_from_spec(const GrammarSpec &spec)
{
    Grammar grammar;

    std::unordered_map<std::string, SymbolId> symbol_ids;

    // Terminals
    for (auto &t : spec.terminals)
    {
        SymbolId id = grammar.symtab.add(SymbolKind::Terminal, t);
        symbol_ids[t] = id;
    }

    // EOF
    SymbolId eof = grammar.symtab.add(SymbolKind::End, "$");
    symbol_ids["$"] = eof;

    // Nonterminals
    for (auto &nt : spec.nonterminals)
    {
        SymbolId id = grammar.symtab.add(SymbolKind::NonTerminal, nt);
        symbol_ids[nt] = id;
    }

    // Determine start symbol
    std::string start = spec.start_symbol;
    if (start.empty() && !spec.productions.empty()) {
        start = spec.productions[0].lhs;
        std::cerr << "Warning: no %start directive, using first production LHS: " << start << "\n";
    }
    if (start.empty()) {
        throw std::runtime_error("No start symbol and no productions");
    }
    if (!symbol_ids.count(start)) {
        SymbolId id = grammar.symtab.add(SymbolKind::NonTerminal, start);
        symbol_ids[start] = id;
    }

    // Add all productions
    for (auto &p : spec.productions)
    {
        if (!symbol_ids.count(p.lhs))
        {
            SymbolId id = grammar.symtab.add(SymbolKind::NonTerminal, p.lhs);
            symbol_ids[p.lhs] = id;
        }
        SymbolId lhs = symbol_ids[p.lhs];
        std::vector<SymbolId> rhs;
        for (auto &sym : p.rhs)
        {
            if (!symbol_ids.count(sym))
            {
                std::cerr << "Warning: implicit symbol " << sym << "\n";
                SymbolId id = grammar.symtab.add(SymbolKind::NonTerminal, sym);
                symbol_ids[sym] = id;
            }
            rhs.push_back(symbol_ids[sym]);
        }
        grammar.add_production(lhs, rhs, "");
    }

    // Start symbol
    SymbolId start_id = symbol_ids[start];

    // Augmented grammar S' → S
    std::string aug = start + "'";
    SymbolId aug_id = grammar.symtab.add(SymbolKind::NonTerminal, aug);
    grammar.add_production(aug_id, {start_id}, "");
    grammar.start_symbol = aug_id;

    grammar.build_indices();

    return grammar;
}

// -----------------------------------------------------------------------------
// Convenience wrapper
// -----------------------------------------------------------------------------

inline Grammar build_grammar_from_file(const std::string &filename)
{
    GrammarSpec spec = parse_grammar_file(filename);
    return build_grammar_from_spec(spec);
}
