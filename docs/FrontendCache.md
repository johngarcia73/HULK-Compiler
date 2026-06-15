# Frontend artifact cache

This project now caches the expensive frontend artifacts used by the test
pipeline:

- the lexer DFA built from `lexer/tokens.hpp`;
- the parser ACTION/GOTO tables built from `parser/Parser_Generator/grammar.y`.

The cache is intentionally limited to generated frontend data. It does not cache
ASTs, semantic state, symbol tables, diagnostics, or backend artifacts.

## Default behavior

`compiler/frontend_pipeline.cpp` and `tests/pipeline_test.cpp` use the cache
automatically.

On startup, the runner tries to load:

- `.hulk_cache/lexer_dfa.cache`
- `.hulk_cache/parser_tables.cache`

If the cache files are present and match the current source inputs, they are
loaded. If they are missing, stale, or unreadable, the runner rebuilds the
artifact and overwrites the cache.

The cache is invalidated when:

- `lexer/tokens.hpp` changes size or modification time;
- `parser/Parser_Generator/grammar.y` changes size or modification time;
- the token specification fingerprint changes;
- the grammar symbol/production counts do not match the cached parser tables;
- the cache format version changes;
- the user requests a forced rebuild.

Corrupt cache files are treated as misses. They do not abort the pipeline.

## Runner flags

Use the normal runner:

```bash
./tests/test tests/program.hulk
```

Force regeneration of both lexer and parser artifacts:

```bash
./tests/test --force-frontend-rebuild tests/program.hulk
```

Use a different cache directory:

```bash
./tests/test --frontend-cache-dir /tmp/hulk-cache tests/program.hulk
```

Both flags can be combined:

```bash
./tests/test --frontend-cache-dir /tmp/hulk-cache --force-frontend-rebuild tests/program.hulk
```

When pipeline traces are enabled, cache events are printed with `[cache]`, for
example:

```text
[cache] Loaded parser tables from .hulk_cache/parser_tables.cache
[cache] Loaded lexer DFA from .hulk_cache/lexer_dfa.cache
```

or:

```text
[cache] Building parser tables and refreshing .hulk_cache/parser_tables.cache
[cache] Building lexer DFA and refreshing .hulk_cache/lexer_dfa.cache
```

## Programmatic API

The reusable API lives in `compiler/frontend_cache.hpp`.

```cpp
FrontendCacheOptions options;
options.cache_dir = ".hulk_cache";
options.force_rebuild = false;
options.trace = true;

CachedParserBundle parser_bundle = load_cached_parser(
    "parser/Parser_Generator/grammar.y",
    options);

Lexer lexer = load_cached_lexer(
    default_token_specs(),
    "lexer/tokens.hpp",
    options);
```

`CachedParserBundle` owns:

- `grammar`: the grammar loaded from `grammar.y`;
- `grammar` is owned by the bundle through a stable heap allocation;
- `name_to_id`: grammar symbol lookup used to remap lexer token ids;
- `parser`: a `LALRParser` built from cache or from the grammar;
- `loaded_from_cache`: whether parser tables came from cache.

Important lifetime rule: the parser stores a pointer to the owned `Grammar`, so
keep the `CachedParserBundle` alive while using `parser_bundle.parser`.

## Implementation notes

`Lexer` now exposes:

- `Lexer(DFA cached_dfa, std::vector<bool> cached_skip_table)`;
- `get_dfa()`;
- `get_skip_table()`.

Those methods are only artifact plumbing. Tokenization behavior is unchanged.

`LALRParser` now exposes:

- `LALRParser::from_tables(...)`;
- `parse_tables()`.

This allows reconstructing a parser from cached ACTION/GOTO tables while still
using the normal `Grammar` object for symbol names, productions, error messages,
and `ASTBuilder`.

## What this does not replace

This cache does not replace the explicit generator tests. If lexer/parser
generator binaries need to be rebuilt or validated independently, use the
generator-specific test path that already exists in `tests/unit/test_generators.cpp`.

The cache only avoids rebuilding generated runtime artifacts on every end-to-end
pipeline run.
