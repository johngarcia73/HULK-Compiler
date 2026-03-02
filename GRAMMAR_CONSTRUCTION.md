# Construir Grammar desde GrammarSpec

## Resumen

Se han agregado dos funciones públicas a `genparser.hpp` para construir objetos `Grammar` correctamente a partir de archivos `.y`:

### Opción 1: Directa (Recomendada para uso simple)

```cpp
#include "genparser.hpp"
#include "grammar.hpp"

// Leer .y file y construir Grammar en una sola línea
Grammar grammar = build_grammar_from_file("example_grammar.y");

// Usar la grammar
const auto& prods = grammar.get_productions();
std::cout << "Productions: " << prods.size() << "\n";
for (const auto& prod : prods) {
    std::cout << "  " << grammar.symtab[prod.lhs].name << " → ";
    for (const auto& sym_id : prod.rhs) {
        std::cout << grammar.symtab[sym_id].name << " ";
    }
    std::cout << "  [" << prod.action_code << "]\n";
}
```

### Opción 2: En dos pasos (Recomendada para control total)

```cpp
#include "genparser.hpp"
#include "grammar.hpp"

// Paso 1: Parsear archivo .y → GrammarSpec
GrammarSpec spec = parse_grammar_file("example_grammar.y");

// Aquí puedes inspeccionar/modificar spec si lo necesitas
std::cout << "Start symbol: " << spec.start_symbol << "\n";
std::cout << "Terminals: " << spec.terminals.size() << "\n";
std::cout << "Productions: " << spec.productions.size() << "\n";

// Paso 2: Construir Grammar desde GrammarSpec
Grammar grammar = build_grammar_from_spec(spec);

// Usar la grammar...
```

## Interfaces Públicas

### `GrammarSpec parse_grammar_file(const std::string& filename)`

**Parámetros:**

- `filename`: Ruta al archivo `.y` a parsear

**Retorna:**

- `GrammarSpec`: Estructura con los datos parseados

**Excepciones:**

- `std::runtime_error` si el archivo no existe o hay errores de parsing

**Contenido de GrammarSpec:**

```cpp
struct GrammarSpec {
    std::vector<std::string> terminals;              // De %token
    std::set<std::string> nonterminals;              // Descubiertos automáticamente
    std::string start_symbol;                        // De %start
    std::vector<ParsedProduction> productions;       // Las reglas
    std::string user_code;                           // De %{ ... %}
};

struct ParsedProduction {
    std::string lhs;                    // Non-terminal (left-hand side)
    std::vector<std::string> rhs;       // Símbolos (strings)
    std::string action_code;            // Código semántico { ... }
};
```

### `Grammar build_grammar_from_spec(const GrammarSpec& spec)`

**Parámetros:**

- `spec`: Especificación de gramática parseada

**Retorna:**

- `Grammar`: Objeto Grammar listo para usar

**Qué hace:**

1. Crea una tabla de símbolos vacía
2. Agrega todos los terminales a la tabla
3. Agrega todos los no-terminales a la tabla
4. Agrega el símbolo EOF ($)
5. Agrega todas las producciones con sus `action_code`
6. Establece el símbolo de inicio
7. Construye los índices internos con `build_indices()`

**Excepciones:**

- `std::runtime_error` si falta símbolo de inicio o hay símbolos inválidos

### `Grammar build_grammar_from_file(const std::string& filename)`

**Parámetros:**

- `filename`: Ruta al archivo `.y` a parsear

**Retorna:**

- `Grammar`: Objeto Grammar listo para usar

**Nota:**

- Esta es una función de conveniencia que internamente hace:
  ```cpp
  GrammarSpec spec = parse_grammar_file(filename);
  return build_grammar_from_spec(spec);
  ```

## Acceso a los Datos de la Grammar

Una vez construida la `Grammar`, accedes a los datos así:

```cpp
// Información general
std::cout << "Start symbol: " << grammar.symtab[grammar.start_symbol].name << "\n";
std::cout << "Total symbols: " << grammar.symbol_count() << "\n";

// Iteración sobre producciones
const auto& prods = grammar.get_productions();
for (size_t i = 0; i < prods.size(); ++i) {
    const auto& prod = prods[i];

    // LHS
    std::cout << "LHS: " << grammar.symtab[prod.lhs].name << "\n";

    // RHS
    std::cout << "RHS: ";
    for (const auto& sym_id : prod.rhs) {
        std::cout << grammar.symtab[sym_id].name << " ";
    }
    std::cout << "\n";

    // Action code (lo nuevo!)
    if (!prod.action_code.empty()) {
        std::cout << "Action: " << prod.action_code << "\n";
    }
}

// Producciones de un no-terminal específico
SymbolId expr_id = ...; // ID de 'expr'
const auto& expr_prods = grammar.productions_of(expr_id);
for (auto prod_id : expr_prods) {
    std::cout << "Production " << prod_id << " derives from expr\n";
}
```

## Formato Soportado de Archivos .y

```yacc
%{
// Optional C++ code
#include <iostream>
%}

%token TERMINAL1 TERMINAL2 TERMINAL3
%start non_terminal

%%

non_terminal : alternative1 { $$ = $1; }
             | alternative2 { $$ = $1 + $2; }
             ;

other_rule : symbols symbols { action }
           | single_symbol { action }
           ;

%%

// Optional C++ code
```

**Elementos soportados:**

- ✓ `%{ ... %}` - Código C++ del usuario
- ✓ `%token TERM1 TERM2 TERM3` - Declaración de terminales
- ✓ `%start symbol` - Símbolo inicial
- ✓ `lhs : rhs { action }` - Producciones
- ✓ `| alternative { action }` - Alternativas
- ✓ Multi-línea - Producciones en múltiples líneas
- ✓ `{ ... }` - Acciones semánticas en cualquier posición
- ✓ Comentarios C++ - `//` (se ignoran)

**Notación:**

- Terminales: MAYÚSCULAS (ej: NUMBER, PLUS)
- No-terminales: minúsculas (ej: expr, term)
- Acciones: Entre llaves `{ ... }`

## Ejemplo Completo

```cpp
#include "genparser.hpp"
#include "grammar.hpp"
#include <iostream>

int main() {
    try {
        // Opción 1: Directa
        Grammar grammar = build_grammar_from_file("arithmetic.y");

        // Opción 2: Con inspección intermedia
        // GrammarSpec spec = parse_grammar_file("arithmetic.y");
        // std::cout << "Parsed " << spec.productions.size() << " productions\n";
        // Grammar grammar = build_grammar_from_spec(spec);

        // Usar la grammar
        std::cout << "Start symbol: " << grammar.symtab[grammar.start_symbol].name << "\n";
        std::cout << "Total symbols: " << grammar.symbol_count() << "\n";
        std::cout << "Total productions: " << grammar.get_productions().size() << "\n";

        // Iterar sobre producciones
        for (const auto& prod : grammar.get_productions()) {
            std::cout << grammar.symtab[prod.lhs].name << " → ";
            for (const auto& sym_id : prod.rhs) {
                std::cout << grammar.symtab[sym_id].name << " ";
            }
            if (!prod.action_code.empty()) {
                std::cout << " [" << prod.action_code << "]";
            }
            std::cout << "\n";
        }

        return 0;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
```

## Compilación

```bash
# Compilar con genparser
g++ -std=c++17 -g main.cpp grammar.cpp -o main

# Ejecutar
./main
```

## Notas Importantes

1. **Los `action_code` están preservados**: Cada producción guarda el código semántico original del `.y` file
2. **Compatible con LALR**: La Grammar resultante es compatible con LALR builder
3. **Tabla de símbolos**: Se genera automáticamente con IDs únicos para cada símbolo
4. **Símbolo EOF**: Se agrega automáticamente como símbolo especial `$`
5. **Errores claros**: Si hay problemas, se lanzan excepciones con mensajes descriptivos

---

**Resumen**: Tienes dos funciones públicas simples para construir Grammar desde .y files. Elige la que necesites según tu caso de uso.
