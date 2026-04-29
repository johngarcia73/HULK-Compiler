# Unit tests

This folder provides a small unit test harness for the compiler, split by phases.

Files:

- `main.cpp` — test runner. Usage: `./unit_tests [test_name] [-v]`. `test_name` can be `lexer`, `parser`, `generators`, `semantic`, or `all` (default).
- `test_lexer.cpp` — exercises the `Lexer` with small inputs and prints token streams.
- `test_semantic.cpp` — builds a tiny AST, runs semantic analysis, and prints the annotated AST so inferred types and resolved overloads can be inspected visually.
- `test_generators.cpp` — checks for the presence of a built generator binary `tests/gen/lexer_gen` and runs it. To build that binary, run `make gen-lexer` in the `tests/` directory.

Logging and verbosity:

- Pass `-v` or `--verbose` to `unit_tests` to enable more verbose logging.

Build & Run:
From the `tests/` directory:

```
make unit        # builds unit_tests
make gen-lexer   # builds the lexer generator binary (optional, used by generators test)
./unit_tests -v  # run all tests verbosely
./unit_tests lexer   # run only lexer test
```

Notes:

- The generators test is optional: it expects `gen/lexer_gen` to exist. The test will instruct how to build it if missing.
- The `parser` unit test is a placeholder for now (kept in the runner). The end-to-end parser testing is covered by `pipeline_test.cpp`, but we can add focused parser unit tests later.
