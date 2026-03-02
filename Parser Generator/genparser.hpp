// ============================================================================
// genparser.hpp
//
// Frontend para parsear archivos .y (formato Bison)
// y construir una especificación de gramática que luego se convertirá
// en un objeto Grammar
// ============================================================================

#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <set>
#include <fstream>
#include <sstream>
#include <regex>
#include <iostream>

// ============================================================================
// Estructura intermedia: una producción parseada del archivo .y
// ============================================================================

struct ParsedProduction {
    std::string lhs;                      // Left-hand side (non-terminal)
    std::vector<std::string> rhs;         // Right-hand side (symbols)
    std::string action_code;              // Código de acción semántica
};

// ============================================================================
// Especificación de gramática parseada del archivo .y
// ============================================================================

struct GrammarSpec {
    std::vector<std::string> terminals;        // Terminales (tokens)
    std::set<std::string> nonterminals;        // No-terminales
    std::string start_symbol;                  // Símbolo de inicio (%start)
    std::vector<ParsedProduction> productions; // Producciones de la gramática
    std::string user_code;                     // Código C++ del usuario (%{ ... %})
};

// ============================================================================
// Función para parsear línea de producción con alternativas
// ============================================================================

inline void parse_production_line(
    const std::string& line,
    const std::string& lhs,
    GrammarSpec& spec)
{
    // Línea formato: "rhs1 rhs2 ... { acción } | rhs1 rhs2 ... { acción }"
    
    std::string current_line = line;
    size_t pos = 0;
    
    while (pos < current_line.length()) {
        // Buscar el '|' de alternativa
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
        
        // Extraer la acción semántica { ... }
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
        
        // Trim whitespace del RHS
        rhs_part.erase(0, rhs_part.find_first_not_of(" \t"));
        rhs_part.erase(rhs_part.find_last_not_of(" \t") + 1);
        
        // Parsear símbolos del RHS
        std::vector<std::string> rhs_symbols;
        std::istringstream iss(rhs_part);
        std::string sym;
        
        while (iss >> sym) {
            // Ignorar símbolos vacíos y comentarios
            if (!sym.empty() && sym[0] != '#') {
                rhs_symbols.push_back(sym);
                
                // Agregar a nonterminals si no es terminal (mayúscula)
                if (sym[0] >= 'a' && sym[0] <= 'z') {
                    spec.nonterminals.insert(sym);
                }
            }
        }
        
        // Agregar la producción
        spec.productions.push_back({lhs, rhs_symbols, action_code});
    }
}

// ============================================================================
// Función principal para parsear archivo .y
// ============================================================================

inline GrammarSpec parse_grammar_file(const std::string& filename) {
    GrammarSpec spec;
    std::ifstream file(filename);
    
    if (!file) {
        std::cerr << "Error: No se pudo abrir el archivo " << filename << std::endl;
        throw std::runtime_error("Cannot open file: " + filename);
    }
    
    std::string line;
    bool in_productions = false;  // Dentro de la sección %% ... %%
    bool in_user_code = false;    // Dentro de %{ ... %}
    
    std::regex token_regex(R"(%token\s+([A-Za-z0-9_ ]+))");
    std::regex start_regex(R"(%start\s+([A-Za-z0-9_]+))");
    
    std::string current_lhs;  // Para continuar producciones con | en múltiples líneas
    
    while (std::getline(file, line)) {
        // ====================================================================
        // Manejo de secciones %{ ... %}
        // ====================================================================
        
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
        
        // ====================================================================
        // Ignorar líneas vacías y comentarios
        // ====================================================================
        
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        // ====================================================================
        // Sección %% delimita sección de declaraciones y producciones
        // ====================================================================
        
        if (line.find("%%") != std::string::npos) {
            in_productions = !in_productions;
            continue;
        }
        
        // ====================================================================
        // SECCIÓN DE DECLARACIONES (antes del primer %%)
        // ====================================================================
        
        if (!in_productions) {
            std::smatch m;
            
            // Procesar %token
            if (std::regex_search(line, m, token_regex)) {
                std::istringstream iss(m[1].str());
                std::string tok;
                while (iss >> tok) {
                    if (!tok.empty()) {
                        spec.terminals.push_back(tok);
                    }
                }
            }
            
            // Procesar %start
            if (std::regex_search(line, m, start_regex)) {
                spec.start_symbol = m[1].str();
            }
        }
        
        // ====================================================================
        // SECCIÓN DE PRODUCCIONES (después del primer %% hasta el segundo %%)
        // ====================================================================
        
        else {
            // Detectar nueva producción (contiene ':')
            if (line.find(':') != std::string::npos) {
                // Nueva LHS
                size_t colon_pos = line.find(':');
                current_lhs = line.substr(0, colon_pos);
                
                // Trim whitespace
                current_lhs.erase(0, current_lhs.find_first_not_of(" \t"));
                current_lhs.erase(current_lhs.find_last_not_of(" \t") + 1);
                
                // Agregar a nonterminals
                spec.nonterminals.insert(current_lhs);
                
                // Procesar el RHS en la misma línea (después del ':')
                std::string rhs_part = line.substr(colon_pos + 1);
                parse_production_line(rhs_part, current_lhs, spec);
            }
            
            // Continuar con alternativas (línea comienza con '|')
            else if (!current_lhs.empty() && line[0] == '|') {
                std::string alt_part = line.substr(1);
                parse_production_line(alt_part, current_lhs, spec);
            }
            
            // Continuar producciones en múltiples líneas (sin '|', pero sin ':')
            // PERO ignorar líneas que son solo ';' o vacías
            else if (!current_lhs.empty() && !line.empty() &&
                     line.find(':') == std::string::npos) {
                std::string trimmed = line;
                trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
                trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);
                
                // No procesar líneas que solo contienen ';'
                if (trimmed != ";") {
                    parse_production_line(line, current_lhs, spec);
                }
            }
        }
    }
    
    // Si no se especificó start symbol, usar el primer no-terminal
    if (spec.start_symbol.empty() && !spec.nonterminals.empty()) {
        spec.start_symbol = *spec.nonterminals.begin();
    }
    
    return spec;
}
