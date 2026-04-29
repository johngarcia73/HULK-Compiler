# Semantic Pipeline

This document describes the current semantic-analysis pipeline, the shared
diagnostic model, and the debug flags exposed by `tests/test`.

## Pipeline Stages

1. Builtins are registered in a fresh semantic symbol table.
2. Declarations are collected and function dependencies are recorded.
3. Functions are analyzed in topological order when possible.
4. Global statements are analyzed after functions.
5. Diagnostics, traces, and optional dumps are emitted from a single semantic
   report sink.

The semantic pass no longer performs a second full AST validation walk after
type inference. `SemanticAnalyzer` orchestrates the phases and
`TypeInferenceVisitor` performs inference plus expression validation.

## Diagnostics

Semantic output is represented with:

- `SemanticDebugOptions`
- `SemanticDiagnostic`
- `SemanticAnalysisResult`
- `SourceSpan`

Each diagnostic includes:

- severity
- phase
- human-readable message
- source span when available
- optional notes

Example:

```text
[error][overload] 4:1: No overload of 'sqrt' matches the argument types.
  note: Candidate (Number) -> Number: argument 1 expected Number but got String
```

## Annotated AST

`ASTNode::print()` now emits semantic annotations instead of a syntax-only dump.
Each printed node includes:

- `type=...`
- `span=...`
- overload resolution metadata for function calls
- inferred function signatures and resolved return types for declarations

Example:

```text
FunctionCall(print) [type=Void, span=3:15-3:23]
  ResolvedOverload: (Number) -> Void
  OverloadStatus: resolved
```

## Runner Flags

The test runner at `tests/test` supports:

- `--trace-pipeline`
- `--trace-inference`
- `--trace-overloads`
- `--dump-ast`
- `--dump-symbols`
- `--dump-deps`
- `--dump-tokens`

The default run is intentionally verbose and diagnostic-friendly: it prints the
input program, token stream, stage-by-stage pipeline progress, semantic trace
output, and the annotated AST after successful semantic analysis. The trace and
dump flags remain available, but the standard runner already surfaces the most
useful information by default.

## Notes

- Lexer tokens now preserve start and end positions so semantic diagnostics can
  point to real source spans.
- The parser no longer dumps grammar productions or LALR states on every run.
- Overload resolution now distinguishes symbol-not-found, arity mismatch, type
  mismatch, and ambiguity, and surfaces candidate notes in diagnostics and AST
  dumps.
