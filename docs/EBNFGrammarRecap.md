# EBNF Grammar

### 1. Rule of Naming: Caps vs. Lowercase

To keep things mathematically clear and easy to read, compiler engineers use a strict casing convention to separate the **Lexer** (tokens) from the **Parser** (grammar rules):

* **Terminals (Lexer Symbols):** Written in **ALL_CAPS**. These are the raw tokens the lexer spits out. They are the "atoms" of the language.
* *Examples:* `NUMBER`, `STRING`, `PLUS`, `MINUS`, `IDENTIFIER`, `EOF`.


* **Non-Terminals (Parser Rules):** Written in **lowercase**. These are the structures the parser builds by combining tokens and other rules. They are the "molecules."
* *Examples:* `expr`, `term`, `statement`, `program`, `assignment`.

### 2. The Notation 

* `|` **(Alternation):** Means "OR". Choose one of the options.
* `*` **(Kleene Star):** Means "zero or more times".
* `+` **(Plus):** Means "one or more times".
* `?` **(Optional):** Means "zero or one time".
* `()` **(Grouping):** Groups symbols together to apply operators like `*` or `?` to the whole group.
* `"..."` or `'...'`: Literal characters that must appear exactly as written.
* `,` or ` `:Concatenation of symbols;


