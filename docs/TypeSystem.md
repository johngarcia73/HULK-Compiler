# Type System, Type Checking, Inference, and Protocols

This implementation adds a semantic type system to the HULK compiler frontend, including nominal typing, structural protocols, type checking, and local type inference.

The compiler currently supports the builtin types `Object`, `Number`, `String`, and `Bool`, as well as internal numeric literal classification into variants such as `int`, `long`, `float`, and `double`, including automatic numeric promotion in arithmetic expressions.

At the language level, the frontend supports:

- typed variables through `let`
- typed functions and methods with parameter and return annotations
- typed attributes
- typed constructor parameters
- object construction with `new`
- member access and method calls
- dynamic type tests and casts through `is` and `as`

The type system also supports user-defined nominal types declared with `type`, including:

- single inheritance via `inherits`
- explicit parent constructor arguments
- `self` support inside methods
- inherited method dispatch through `base()`
- semantic validation of constructors, attributes, and methods
- private attribute access rules

Explicit protocols are supported through `protocol`, including:

- method signature declarations
- protocol extension with `extends`
- protocol annotations in variables, parameters, attributes, and return types

Protocol conformance is structural: a nominal type satisfies a protocol if it provides all required methods, without explicitly declaring implementation. Structural conformance is also supported between protocols through method signature compatibility.

The type checker validates:

- compatibility between expressions and declared annotations
- function, method, and constructor calls
- return types
- attribute and variable initialization
- subtype relationships across nominal types and protocols

Type inference follows a local strategy and can infer types for unannotated symbols from semantic context, including:

- arithmetic and boolean operations
- function and method calls
- conditional expressions
- variable and attribute initializers
- constructor arguments

When a concrete type cannot be inferred, the compiler reports an explicit inference failure.

Numeric typing includes automatic promotion and expression result typing. For conditional expressions and branching constructs, the system computes join types using lowest common ancestor and type conformance rules.

Finally, the compiler produces an annotated AST containing:

- inferred types
- declared types
- function and protocol signatures
- call and method resolution information

This annotated AST serves as the main debugging and validation surface for semantic analysis and type system behavior.
