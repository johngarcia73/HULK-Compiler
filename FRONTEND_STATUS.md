# Frontend del Parser Generator: .y → Grammar

## Estado: ✓ COMPLETO Y VERIFICADO

### Resumen

Se ha implementado un **frontend completo** que lee archivos Bison-style `.y` y construye objetos `Grammar` que preservan las acciones semánticas embebidas en las reglas de producción.

### Cambios Implementados

#### 1. Modificación de `grammar.hpp`

- **Campo agregado a `Production`**: `std::string action_code;`
- **Método actualizado**: `add_production()` ahora acepta un parámetro opcional `action_code` con valor por defecto `""`
- **Compatibilidad**: 100% backward compatible. Todas las 6 llamadas existentes a `add_production()` siguen funcionando sin cambios.

#### 2. Nuevo archivo: `genparser.hpp`

Parser completo para archivos `.y` que implementa:

```cpp
struct ParsedProduction {
    std::string lhs;                    // Left-hand side
    std::vector<std::string> rhs;       // Right-hand side symbols
    std::string action_code;            // Semantic action { ... }
};

struct GrammarSpec {
    std::vector<std::string> terminals;
    std::set<std::string> nonterminals;
    std::string start_symbol;
    std::vector<ParsedProduction> productions;
    std::string user_code;
};

GrammarSpec parse_grammar_file(const std::string& filename);
```

**Características del parser:**

- Lee secciones `%{ ... %}` para código C++ del usuario
- Procesa declaraciones `%token TERM1 TERM2 ...`
- Procesa declaración `%start symbol`
- Parsea reglas de producción con alternativas separadas por `|`
- Extrae acciones semánticas `{ ... }`
- Maneja producciones multi-línea
- Descubre automáticamente no-terminales

**Formato soportado de archivo .y:**

```yacc
%{
// User C++ code
#include <iostream>
%}

%token NUMBER PLUS TIMES LPAREN RPAREN
%start expr

%%

expr : expr PLUS term { $$ = $1 + $3; }
     | term { $$ = $1; }
     ;

term : term TIMES factor { $$ = $1 * $3; }
     | factor { $$ = $1; }
     ;

factor : NUMBER { $$ = $1; }
       | LPAREN expr RPAREN { $$ = $2; }
       ;

%%

// User C++ code (implementation)
```

#### 3. Archivo de ejemplo: `example_grammar.y`

Gramática de ejemplo (aritmética simple) para pruebas y demostración.

#### 4. Test de validación: `test_frontend_y_to_grammar.cpp`

Test completo que verifica:

1. **Parsing de .y file**: Carga y parsea correctamente el archivo
2. **Extracción de terminales**: Verifica que `%token` se procesan
3. **Extracción de no-terminales**: Verifica que se descubren automáticamente
4. **Extracción de producciones**: Verifica que todas las reglas se parsean
5. **Extracción de acciones**: Verifica que `{ ... }` se extraen correctamente
6. **Construcción de Grammar**: Verifica que se puede construir un objeto Grammar desde GrammarSpec
7. **Acceso a action_code**: Verifica que las acciones están disponibles en Production

### Pipeline Completo (.y → Grammar)

```
example_grammar.y
        ↓
[parse_grammar_file()]
        ↓
   GrammarSpec
   (terminals, nonterminals, productions, start_symbol)
        ↓
[Construir Grammar objeto]
   - Agregar símbolos terminales a symtab
   - Agregar símbolos no-terminales a symtab
   - Agregar símbolo End ($)
   - Agregar producciones pasando action_code
        ↓
   Grammar
   (Accesible via grammar.get_productions()[i].action_code)
```

### Resultados de la Prueba

```
╔════════════════════════════════════════════════════════════════╗
║        TEST: Frontend .y → GrammarSpec → Grammar              ║
╚════════════════════════════════════════════════════════════════╝

[TEST 1] Parsing .y file
  ✓ Parsed successfully
    Terminals: 5
    Non-terminals: 3
    Productions: 6
    Start symbol: 'expr'

[TEST 2] Checking terminals
  ✓ All expected terminals found

[TEST 3] Checking non-terminals
  ✓ All expected non-terminals found

[TEST 4] Checking productions
  ✓ Correct number of productions (6)

[TEST 5] Checking semantic actions
  ✓ Found 6 semantic actions

[TEST 6] Building Grammar object
  ✓ Grammar constructed
    Symbols: 9
    Productions: 6

[TEST 7] Verifying action_code stored in Productions
  ✓ All productions accessible with action_code field
    - action_code='{ $$ = $1 + $3; }' ✓
    - action_code='{ $$ = $1; }' ✓
    ... (6 total)

╔════════════════════════════════════════════════════════════════╗
║                     ALL TESTS PASSED ✓                         ║
╚════════════════════════════════════════════════════════════════╝
```

### Compatibilidad

✓ **Todos los archivos existentes**:

- `grammar.hpp/.cpp` - Actualizado, backward compatible
- `ast_node.hpp/.cpp` - Sin cambios, completamente compatible
- `ast_builder.hpp/.cpp` - Sin cambios, completamente compatible
- `lalr_builder.hpp` - Sin cambios, completamente compatible
- `lalr_algorithms.cpp` - Sin cambios, completamente compatible
- `parser_runtime.cpp` - Sin cambios, completamente compatible
- `integration_example.cpp` - Sin cambios, funciona igual
- Visitor pattern - Sin cambios, completamente compatible

### Próximos Pasos

La siguiente fase sería:

1. Integrar `action_code` con el `CodeGenerator` (generar C++ desde las acciones)
2. Crear el generador de parser (LALR con código semántico)
3. Tests de end-to-end completos

### Archivo Compilación

```bash
# Compilar el test
g++ -std=c++17 -g test_frontend_y_to_grammar.cpp grammar.cpp -o test_frontend_y_to_grammar

# Ejecutar
./test_frontend_y_to_grammar
```

---

**Conclusión**: El frontend está **completo, verificado y funcionando correctamente**. El pipeline `.y → Grammar` es robusto y preserva toda la información de las acciones semánticas necesaria para las siguientes fases de compilación.
