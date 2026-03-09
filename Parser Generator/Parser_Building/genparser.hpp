#pragma once

#include "../utils/Grammar/grammar.hpp"
#include <string>
#include <vector>
#include <unordered_map>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>


// Intermediate representation of a production
struct ParsedProduction {
    std::string lhs;                      // Left-hand side (non-terminal)
    std::vector<std::string> rhs;         // Right-hand side (symbols)
    std::string action_code;              // Csemantic action code (Bison-style, e.g. "{ $$ = $1 + $3; }")
};


struct GrammarSpec {
    std::vector<std::string> terminals;        
    std::vector<std::string> nonterminals;     
    std::string start_symbol;                  // (%start)
    std::vector<ParsedProduction> productions; 
    std::string user_code;                     // C++ code (%{ ... %})
};
// ============================================================================

GrammarSpec parse_grammar_file(const std::string& filename);
Grammar build_grammar_from_spec(const GrammarSpec& spec);
Grammar build_grammar_from_file(const std::string& filename);

// Parsing production lines with alternations (|)
inline void parse_production_line(
    const std::string& line,
    const std::string& lhs,
    GrammarSpec& spec)
{
    // Line format: "rhs1 rhs2 ... { action } | rhs1 rhs2 ... { action }"
    
    std::string current_line = line;
    size_t pos = 0;
    
    while (pos < current_line.length()) {
        // Find alternate part (split by '|')
        size_t pipe_pos = current_line.find('|', pos);
        std::string alt_part;
        
        if (pipe_pos != std::string::npos) {
            alt_part = current_line.substr(pos, pipe_pos - pos);
            pos = pipe_pos + 1;
        } else {
            alt_part = current_line.substr(pos);
            pos = current_line.length();
        }
        
        // Trim whitespace
        alt_part.erase(0, alt_part.find_first_not_of(" \t\n\r"));
        alt_part.erase(alt_part.find_last_not_of(" \t\n\r") + 1);
        
        if (alt_part.empty()) continue;
        
        // Extract semantic action { ... }
        std::string action_code;
        size_t brace_open = alt_part.find('{');
        std::string rhs_part = alt_part;
        
        if (brace_open != std::string::npos) {
            size_t brace_close = alt_part.find('}');
            if (brace_close != std::string::npos) {
                action_code = alt_part.substr(brace_open, brace_close - brace_open + 1);
                rhs_part = alt_part.substr(0, brace_open);
            }
        }
        
        // Trim whitespace of RHS
        rhs_part.erase(0, rhs_part.find_first_not_of(" \t"));
        rhs_part.erase(rhs_part.find_last_not_of(" \t") + 1);
        
        // Parse RHS symbols
        std::vector<std::string> rhs_symbols;
        std::istringstream iss(rhs_part);
        std::string sym;
        
        while (iss >> sym) {
            // Ignore empty symbols and comments
            if (!sym.empty() && sym[0] != '#') {
                rhs_symbols.push_back(sym);
                
                bool is_terminal = false;
                for (const auto& t : spec.terminals) {
                    if (t == sym) {
                        is_terminal = true;
                        break;
                    }
                }
                
                if (!is_terminal) {
                    // Add a nonterminal if not already present
                    bool found = false;
                    for (const auto& nt : spec.nonterminals) {
                        if (nt == sym) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        spec.nonterminals.push_back(sym);
                    }
                }
            }
        }
        
        // Add production to spec
        spec.productions.push_back({lhs, rhs_symbols, action_code});
    }
}

// Parse .y file and build Grammar object
inline GrammarSpec parse_grammar_file(const std::string& filename) {
    GrammarSpec spec;
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    bool in_productions = false;  // Inside %% ... %% section
    bool in_user_code = false;    // Inside %{ ... %}
    
    std::regex token_regex(R"(%token\s+(.+)$)");
    std::regex start_regex(R"(%start\s+([A-Za-z0-9_']+))");
    
    std::string current_lhs;  // To continue productions with | in multiple lines
    
    while (std::getline(file, line)) {
        // Handling %{ ... %} sections
        if (line.find("%{") != std::string::npos) {
            in_user_code = true;
            size_t pos = line.find("%{");
            if (pos + 2 < line.size()) {
                spec.user_code += line.substr(pos + 2) + "\n";
            }
            continue;
        }
        
        if (line.find("%}") != std::string::npos && in_user_code) {
            in_user_code = false;
            size_t pos = line.find("%}");
            if (pos > 0) {
                spec.user_code += line.substr(0, pos) + "\n";
            }
            continue;
        }
        
        if (in_user_code) {
            spec.user_code += line + "\n";
            continue;
        }
        
        // Ignore empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // %% section delimits declarations and productions section
        if (line.find("%%") != std::string::npos) {
            in_productions = !in_productions;
            continue;
        }
        
        // DECLARATIONS SECTION (before the first %%)
        if (!in_productions) {
            std::smatch m;
            
            // Process %token
            if (std::regex_search(line, m, token_regex)) {
                std::istringstream iss(m[1].str());
                std::string tok;
                while (iss >> tok) {
                    if (!tok.empty()) {
                        spec.terminals.push_back(tok);
                    }
                }
            }
            
            // Process %start
            if (std::regex_search(line, m, start_regex)) {
                spec.start_symbol = m[1].str();
            }
        }
        
        // PRODUCTIONS SECTION (after the first %% until the second %%)
        else {
            // Detect new production (contains ':')
            if (line.find(':') != std::string::npos) {
                // New LHS
                size_t colon_pos = line.find(':');
                current_lhs = line.substr(0, colon_pos);
                
                // Trim whitespace
                current_lhs.erase(0, current_lhs.find_first_not_of(" \t"));
                current_lhs.erase(current_lhs.find_last_not_of(" \t") + 1);
                
                // Add to nonterminals if not already present
                bool found = false;
                for (const auto& nt : spec.nonterminals) {
                    if (nt == current_lhs) {
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    spec.nonterminals.push_back(current_lhs);
                }
                
                // Process the RHS on the same line (after the ':')
                std::string rhs_part = line.substr(colon_pos + 1);
                parse_production_line(rhs_part, current_lhs, spec);
            }
            
            // Continue with alternatives (line starts with '|')
            else if (!current_lhs.empty() && line[0] == '|') {
                std::string alt_part = line.substr(1);
                parse_production_line(alt_part, current_lhs, spec);
            }
            
            // Continue productions on multiple lines (without '|', but without ':')
            // BUT ignore lines that are only ';' or empty
            else if (!current_lhs.empty() && !line.empty() &&
                     line.find(':') == std::string::npos) {
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
                
                // Dont process lines that are just ';' or empty
                if (trimmed != ";") {
                    parse_production_line(line, current_lhs, spec);
                }
            }
        }
    }
    
    return spec;
}

// Build Grammar from GrammarSpec
inline Grammar build_grammar_from_spec(const GrammarSpec& spec) {
    Grammar grammar;
    

    std::unordered_map<std::string, SymbolId> symbol_ids;
    
    // Add terminals
    for (const auto& terminal : spec.terminals) {
        SymbolId id = grammar.symtab.add(SymbolKind::Terminal, terminal);
        symbol_ids[terminal] = id;
    }
    
    // Add end-of-input symbol $
    SymbolId dollar_id = grammar.symtab.add(SymbolKind::End, "$");
    symbol_ids["$"] = dollar_id;
    
    // Add non-terminals
    for (const auto& nonterminal : spec.nonterminals) {
        SymbolId id = grammar.symtab.add(SymbolKind::NonTerminal, nonterminal);
        symbol_ids[nonterminal] = id;
    }
    
    // Add eveery production with their action codes
    for (const auto& parsed_prod : spec.productions) {
        SymbolId lhs_id = symbol_ids[parsed_prod.lhs];
        
        // Convert RHS strings into IDs
        std::vector<SymbolId> rhs_ids;
        for (const auto& sym : parsed_prod.rhs) {
            auto it = symbol_ids.find(sym);
            if (it != symbol_ids.end()) {
                rhs_ids.push_back(it->second);
            } else {
                throw std::runtime_error("Symbol not found in symbol table: " + sym);
            }
        }
        
        // Add production and action code to grammar
        grammar.add_production(lhs_id, rhs_ids, parsed_prod.action_code);
    }
    
    // Set start symbol
    if (spec.start_symbol.empty()) {
        throw std::runtime_error("No start symbol defined in grammar");
    }
    
    auto it = symbol_ids.find(spec.start_symbol);
    if (it == symbol_ids.end()) {
        throw std::runtime_error("Start symbol not found: " + spec.start_symbol);
    }
    
    grammar.start_symbol = it->second;
    
    grammar.build_indices();
    
    return grammar;
}

// Builds a Grammar object directly from a .y file
inline Grammar build_grammar_from_file(const std::string& filename) {
    GrammarSpec spec = parse_grammar_file(filename);
    return build_grammar_from_spec(spec);
}
