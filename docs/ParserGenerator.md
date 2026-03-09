# LALR Parser Generator – Frontend Overview

This module provides a complete frontend that reads Bison-style `.y` grammar files and constructs an internal `Grammar` object, preserving embedded semantic actions for later code generation.

---

## Features

- **Full `.y` file parsing**
  - Handles `%{ … %}`
  - Handles `%token`
  - Handles `%start`
  - Parses grammar rules with `|` alternatives
  - Preserves semantic actions `{ … }`

- **Automatic symbol discovery**
  - Terminals extracted from `%token`
  - Non-terminals inferred from rule left-hand sides (LHS)

- **Preserves action code**
  - Each production stores its original `{ … }` block as a string.

- **Flexible construction**
  - Two-step build (`GrammarSpec` → `Grammar`)
  - One-step build (direct from file)

---

## Core Components

### `genparser.hpp`

Defines:

```cpp
struct ParsedProduction {
    std::string lhs;
    std::vector<std::string> rhs;
    std::string action_code;
};s

struct GrammarSpec {
    std::vector<std::string> terminals;
    std::set<std::string> nonterminals;
    std::string start_symbol;
    std::vector<ParsedProduction> productions;
    std::string user_code;
};

GrammarSpec parse_grammar_file(const std::string& filename);
Grammar build_grammar_from_spec(const GrammarSpec& spec);
Grammar build_grammar_from_file(const std::string& filename);   // convenience
```
