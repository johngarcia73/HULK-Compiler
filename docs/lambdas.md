# Lambda Expressions in the HULK Compiler: A Rigorous Implementation Study

## 1. Introduction: The Problem Domain in HULK’s Ecosystem

HULK is a statically typed language that marries object‑oriented constructs (types, inheritance, protocols) with functional sensibilities (higher‑order functions, type inference, and expression‑oriented syntax). Within this hybrid paradigm, lambda expressions serve a critical role: they are the primary vehicle for **behaviour abstraction**—the ability to treat computations as data without the syntactic overhead of named function declarations.

In HULK, the challenge is not merely to support anonymous functions, but to integrate them **seamlessly** with:

- A nominal type system that distinguishes `Number`, `String`, `Bool`, `Object`, and user‑defined types.
- Overload resolution that must consider function types as first‑class citizens.
- Constraint‑based type inference that propagates type information both forward (from arguments) and backward (from expected types).
- Mutation and scoping rules that must correctly capture variables from enclosing scopes.

The compiler must answer several fundamental questions: _How does the semantic visitor infer types for parameters that lack annotations, especially when the lambda is passed to a generic higher‑order function? How are free variables resolved without violating the scoping discipline of an imperative language?_ This report answers these questions by walking through the front‑end pipeline, justifying every design decision with reference to the actual codebase.

---

### 1.1 Eliminating the “Nominal Ceremony”

Without lambdas, higher‑order functions force programmers to pollute the namespace with single‑use function definitions. In HULK, writing:
function apply_twice(f: (Number) -> Number, x: Number): Number { f(f(x)); }
would require a separate definition for every transformation. With lambdas, the transformation is expressed **in situ**, reducing indirection and improving locality. This directly aligns with the principle of **cohesion**: the behaviour is placed exactly where it is relevant.

### 1.2 Enabling Custom Control Structures

HULK permits the definition of functions that abstract over control flow—for example, a `with_resource` pattern that acquires a resource, executes a user‑supplied block, and releases it. Lambdas are the only syntactically lightweight way to pass the “body” of such a custom construct without resorting to macros or code generation. This turns HULK from a fixed‑set‑of‑statements language into an **extensible** one, where programmers can design their own control‑flow primitives.

### 1.3 First‑Class Iteration and Collection Processing

HULK provides built‑in vector types and iteration constructs. The combination of `for` loops and iterators is powerful, but lambdas elevate collection processing to a declarative style. Functions like `map`, `filter`, and `fold` become natural, encouraging a **data‑pipeline** programming model. This is not just syntactic sugar; it shifts the programmer’s mental model from _imperative steps_ to _transformational chains_, often reducing logic errors and improving parallelisability.

### 1.4 Delayed Evaluation and Lazy Initialisation

In HULK, lambdas of arity zero (often called _thunks_) allow deferring expensive computations until their result is strictly needed. This is crucial for building lazy data structures, implementing memoisation, or handling optional initialisation in a clean way.

---

## 2 Implementation Architecture

The remainder of this report dissects the implementation path of lambdas through the HULK compiler’s core front‑end phases: **AST construction**, **semantic analysis**, and **scoping**. Each subsection refers explicitly to the actual code structures that support these phases.

### 2.1 AST Representation: The `LambdaNode` Structure

The parser’s AST builder creates a `LambdaNode` for every lambda expression, regardless of the syntactic form used. The node is defined in `ast_node.hpp` (and implemented in `ast_node.cpp`) as:

```cpp
class LambdaNode : public ASTNode {
public:
    std::vector<std::string> params;
    std::vector<Type*> paramTypes;
    std::vector<std::string> paramTypeNames;
    Type* declaredReturnType;
    std::string declaredReturnTypeName;
    bool hasExplicitReturnType;
    ASTNodePtr body;
    FunctionType* functionType;  // Inferred or declared signature

    LambdaNode(...);
    void print(std::ostream& o, int indent_n) const override;
    Type* accept(Visitor& v) override;
};
```

## 3 The Computational Edge: What Lambdas Enable in HULK

Before dissecting the implementation, it is essential to articulate _why_ lambdas are not a mere syntactic nicety in HULK, but a foundational enabler of several programming patterns.

### Rationale for Each Field

- **`params`**: The identifiers of the lambda's formal parameters.

- **`paramTypes` / `paramTypeNames`**: These store the resolved types during semantic analysis. Initially they may contain `UnknownType` or `nullptr`; they are filled by the visitor. Storing both the resolved `Type*` and the original string name is essential for diagnostics and for preserving explicit annotations.

- **`declaredReturnType` / `hasExplicitReturnType`**: If the programmer specifies `: Number` before the arrow, this field holds the user-provided type. This is crucial for type checking the body.

- **`body`**: The AST of the lambda's code, which can be either an expression node (for expression-bodied lambdas) or a `BlockNode` (for block-bodied lambdas).

- **`functionType`**: After semantic analysis, this field holds a `FunctionType` object—a subtype of `Type` that stores the full signature `(ParamTypes) -> ReturnType`. This is what permits lambdas to be assigned to variables, passed as arguments, and returned.

The separation of declared and inferred types (e.g., `declaredReturnType` vs. `functionType->getReturnType()`) is a recurring pattern in HULK, allowing the visitor to emit precise errors when the body's inferred type does not conform to the declaration.

### 3.1 The Semantic Pipeline: Visiting `LambdaNode` in `TypeInferenceVisitor`

The core of lambda implementation lives in `TypeInferenceVisitor::visit(LambdaNode& node)`. This method orchestrates scoping, type inference, and constraint solving. We now dissect it step by step.

#### Step 1: Resolving Explicit Annotations

The visitor first resolves the declared parameter types and the declared return type using `resolveDeclaredType`. This function queries the `NominalTypeRegistry` to map type names (e.g., `"Number"`, `"String"`, user-defined types) to their `Type*` representations. If a type name is unknown, an error is emitted and `UnknownType` is substituted to avoid cascading failures.

#### Step 2: Entering the Lambda's Scope

```cpp
symTable.enterScope();

for (size_t i = 0; i < node.params.size(); ++i) {
    symTable.insert(node.params[i], SymbolInfo(...));
}
```

A new symbol table scope is pushed. This is essential for lexical scoping: variables defined inside the lambda (e.g., in a block body) must not leak to the enclosing scope.

Each parameter is inserted into the symbol table with its current type—which might be `UnknownType` if the programmer did not supply an annotation.

This scope is entered before the body is analysed, so that references to the parameters inside the body resolve correctly.

#### Step 3: Analysing the Body

```cpp
Type* bodyType = node.body
    ? coerceUnknown(node.body->accept(*this))
    : VoidType::instance();
```

The body is visited, triggering the full inference machinery. During this traversal, the parameters (which are now symbols in the table) may be refined by constraints.

For example, if the body contains `x + 1`, the binary operator visitor calls:

```cpp
refineUnknownExpression(node.left, NumberType::instance());
```

which updates the symbol for `x` from `Unknown` to `Number`.

#### Step 4: Capturing Final Parameter Types

```cpp
for (size_t i = 0; i < node.params.size(); ++i) {
    SymbolInfo* symbol = symTable.lookup(node.params[i]);

    if (symbol && symbol->type) {
        node.paramTypes[i] = coerceUnknown(symbol->type);
    }

    if (node.paramTypes[i]->equals(UnknownType::instance())) {
        error(...);
    }
}
```

After the body is fully analysed, the visitor extracts the final type for each parameter from the symbol table.

If any parameter remains `Unknown` and was not explicitly annotated, the visitor emits a hard error:

> _"Could not infer the type of lambda parameter."_

This is a conscious design decision. Unlike languages with global type inference (e.g., Haskell), HULK's inference is local and constraint-based, not full Hindley-Milner. For lambdas, this means the compiler refuses to guess at the call site; it forces the programmer to add annotations if the body does not constrain the type enough.

#### Step 5: Determining the Return Type

```cpp
Type* returnType =
    node.declaredReturnType
        ? coerceUnknown(node.declaredReturnType)
        : bodyType;

if (node.declaredReturnType) {
    checkTypeConformance(
        bodyType,
        node.declaredReturnType,
        node,
        ...
    );
}

node.functionType = new FunctionType(node.paramTypes, returnType);
```

If the return type was explicitly declared, the visitor verifies that the body's type conforms to it using `checkTypeConformance`.

Otherwise, the body's type becomes the return type. If the body is `Void` (e.g., an empty block), the lambda returns `Void`.

A `FunctionType` is constructed and stored in `node.functionType`, and also set as `node.type` so that the lambda can be treated as a type in the broader AST.

#### Step 6: Exiting the Scope

```cpp
symTable.exitScope();
```

This finalises the lexical isolation of the lambda, preventing its parameters from polluting the outer scope.

### 3.2 Scoping and Free Variable Resolution

A lambda's body may reference variables defined in the enclosing scope. In HULK, this is supported implicitly through the symbol table's scoping rules. When the visitor analyses the body, `symTable.lookup(name)` traverses the nested scopes (the lambda's own scope, then the outer function's scope, etc.). If a variable is found in an outer scope, the reference is considered valid, and its type is propagated.

The front-end does not need to know about closure allocation or code generation details; it only needs to provide a clean semantic model where free variables are resolved correctly. The `visit(LambdaNode)` method achieves this by simply relying on the standard lookup mechanism. The back-end later translates this into the appropriate closure representation (by-value capture, heap allocation, etc.) without affecting the semantic analysis.

### 3.3 Lambda Types and Overload Resolution

Because a lambda's type is `FunctionType`, it participates fully in HULK's overload resolution system. In `visit(FunctionCallNode)`, when the callee is not a named function but a variable of function type, the following code path applies:

```cpp
SymbolInfo* callableSymbol = symTable.lookup(node.name);

if (callableSymbol &&
    callableSymbol->kind != SemanticSymbolKind::Function &&
    callableSymbol->type) {

    if (auto* functionType =
            dynamic_cast<FunctionType*>(callableSymbol->type)) {

        // validate arguments against functionType's parameters
        // ...
    }
}
```

The symbol table returns the variable's `FunctionType`.

The visitor checks that the arguments conform to the lambda's parameter types.

The return type of the function call becomes the lambda's return type.

Thus, lambdas are treated on equal footing with named functions—a testament to HULK's consistent first-class function support.

## 4. Justification of Design Decisions

Every implementation choice outlined above stems from deliberate trade-offs. This section justifies those decisions against alternative approaches.

### 4.1 Why Constraint-Based Inference, not Hindley-Milner?

HULK opts for a bidirectional constraint-based inference (as seen in `refineUnknownExpression`) rather than a full unification-based system like Hindley-Milner (HM).

**Reason:** HULK is an imperative, object-oriented language. Full HM would complicate the interaction with mutation, subtyping, and overloading. Constraint-based inference is easier to integrate with nominal subtyping and produces predictable, local errors (e.g., _"parameter `x` must be `Number`"_) rather than cryptic unification failures.

**Trade-off:** Some lambda parameters require explicit annotations, especially if the body does not constrain them (e.g., `function (x) -> x`). This is acceptable because HULK's design philosophy favors explicitness where ambiguity exists, preventing surprising runtime behaviour.

### 4.2 Why Two Syntaxes (`lambda_expr` and `function_expr`)?

While many languages (e.g., Python and JavaScript) use a single syntax for both expression and block bodies, HULK separates them.

**Reason:** The grammar is **LALR(1)**. Merging them would require a grammar production with a lookahead conflict between `expr` and `block`. While solvable with `%prec` or token-based disambiguation, the separation improves readability of the grammar and maps cleanly to the two distinct AST representations.

**Programmer Benefit:** The `FUNCTION` keyword visually distinguishes block-oriented anonymous functions from lightweight expression lambdas, making code self-documenting.

### 4.3 Why Not Infer Return Types from All `return` Statements?

The visitor takes the body's type as the return type. For block bodies, the type is the type of the last expression. If multiple `return` statements exist with different types, the visitor uses `lowestCommonAncestor` to compute a join type.

**Reason:** This is consistent with HULK's expression-oriented nature. It encourages writing block bodies that have a coherent last value, which aligns with functional idioms.

**Trade-off:** Imperative programmers might find this restrictive. However, explicit return type annotations can always be added to override the inference.

### 4.4 Why Errors for Uninferred Parameters?

Some languages (e.g., Scala with partial unification) allow parameters to remain polymorphic until the call site. HULK does not.

**Reason:** HULK does not support rank-2 polymorphism or global type inference. Allowing `Unknown` to propagate to the call site would require a more complex solver. By forcing an annotation when the body is ambiguous, the compiler keeps the inference algorithm linear and predictable.

**Outcome:** The error message explicitly points to the parameter and suggests adding an annotation—a clear, actionable diagnostic.

## 5. The Price of Abstraction: Overheads and Limitations

Lambda expressions are not free. HULK pays several costs, which are important to acknowledge.

### 5.1 Semantic Complexity

The `visit(LambdaNode)` method, while clear, adds significant complexity to the type inference pass. The visitor must maintain nested scopes correctly, handle the interaction between lambda parameters and outer variables (shadowing), and manage the propagation of constraints across the lambda boundary. This increases the compiler's maintenance burden.

### 5.2 Runtime Overhead

- **Heap Allocation:** Every lambda instantiation requires allocating a closure object on the heap (or using some form of garbage-collected record). This is more expensive than calling a plain function.

- **Indirection:** Invoking a lambda involves dereferencing the closure to obtain the function pointer and the captured environment.

- **Optimisation Barriers:** Inlining lambdas across functions is difficult unless the compiler performs aggressive escape analysis and inlining heuristics.

### 5.3 Diagnostic Challenges

When an error occurs inside a lambda, the programmer sees a stack trace involving anonymous functions. Providing meaningful location information (`span`) mitigates this, but understanding the closure's state still requires mental effort.

### 5.4 Inference Limitations

As noted, lambdas with insufficient constraints fail compilation. This is a deliberate trade-off to keep the system simple, but it can be frustrating for developers accustomed to fully inferred functional languages.

## Comparison with other languages

## C#: Delegate-Based Pragmatism

C# approaches lambdas through the lens of delegates—type-safe function pointers that serve as the universal currency of functional programming in the .NET ecosystem. When the C# compiler encounters a lambda, it generates a delegate instance, which may be backed by either a compiler-generated method or an expression tree.

### The Closure Object

C# lambdas that capture variables are implemented through a compiler-generated closure class. This sealed class contains fields for each captured variable, and the lambda body becomes a method on this class. When the lambda is created, an instance of this closure class is allocated on the heap, the captured values are stored in its fields, and a delegate pointing to the lambda method is returned.

The performance implications are significant. Each closure allocation imposes a heap allocation cost, and as one .NET optimization discussion notes, _"Don't use a closure and compile to the static delegate"_ to avoid this overhead. Modern .NET runtimes have sophisticated optimizations—the JIT compiler can sometimes eliminate closure allocations through escape analysis, and the runtime caches delegates for lambdas that capture no state. But when state is captured, the allocation is unavoidable.

### Expression Trees and LINQ

A distinctive feature of C# is the ability to represent lambdas as expression trees rather than compiled delegates. When a lambda is assigned to an `Expression<TDelegate>` variable, the compiler emits code that builds a tree representation of the lambda's structure. This enables runtime analysis, transformation, and compilation—the foundation of LINQ providers that translate C# queries into SQL or other target languages. This capability is unique among the languages examined and represents a profound extension of the lambda concept beyond mere function abstraction.

### Static Lambdas and Optimization

Beginning with C# 9, the language supports static lambdas, which cannot capture local variables or instance state. This allows the compiler to cache the delegate instance globally, eliminating per-invocation allocation overhead. The feature acknowledges a hard truth: in performance-sensitive code, the cost of closure allocation matters.

## Python: Dynamic Flexibility at a Cost

Python's lambdas are the most constrained of the four languages—they can only contain a single expression, cannot contain statements, and are syntactically limited. Yet beneath this simplicity lies a complex implementation with profound consequences for performance and correctness.

### Late Binding and the Loop Lambda Problem

The most notorious feature of Python's lambda implementation is late binding of captured variables. When a lambda captures a variable, it captures a reference to the variable itself, not its value at the time of definition. This leads to the classic _"loop lambda"_ problem:

```python
funcs = [lambda: i for i in range(3)]
print(funcs[0]())  # Outputs 2, not 0!
```

All three lambdas share the same cell object referencing `i`, and by the time they are called, the loop has advanced `i` to `2`. The solution—creating a new scope with a default argument—is a workaround that reveals the underlying mechanism: Python's closures capture cell variables, not values.

### Cell Variables and Memory Retention

Python implements closures through cell objects—heap-allocated containers that hold references to captured variables. Each lambda closure gets a reference to the same cell object, and as variables change, the cell's contents are updated. This design has two consequences:

First, it introduces indirection for every access to a captured variable.

Second, it can lead to unintended memory retention: if a lambda outlives the scope in which it was defined, the cell keeps the captured variables alive, potentially preventing garbage collection.

### Performance Characteristics

Python lambdas are implemented using the same bytecode machinery as regular functions, but with additional indirection for cell variable access. Empirical evidence suggests that lambdas can be slightly slower than equivalent `def` functions, though the difference is often negligible in practice. The real cost is not in execution speed but in the cognitive overhead of understanding late binding semantics.

## C++: Zero-Cost Abstraction

C++ embodies a philosophy of _"you don't pay for what you don't use"_, and its lambda implementation reflects this principle with remarkable fidelity. When the C++ compiler encounters a lambda, it generates a unique, compiler-generated class with an overloaded `operator()`.

### The Compiler-Generated Functor

Each lambda becomes a distinct type—a closure object whose size depends on the capture list. Captured variables become member variables of this class, initialized from the outer scope. The lambda body becomes the `operator()` method. This is not a runtime abstraction; it is a compile-time transformation that produces code as efficient as hand-written function objects.

Crucially, a lambda that captures no variables is stateless—its closure object has zero size, and it can be implicitly converted to a plain function pointer. This allows non-capturing lambdas to be used in C APIs and other contexts that require raw function pointers, with zero overhead.

### Explicit Capture Control

Unlike HULK's implicit by-value capture, C++ requires the programmer to specify the capture mode explicitly:

- `[=]` for by-value.
- `[&]` for by-reference.
- An explicit list of variables.

This explicitness has two benefits.

First, it makes the performance characteristics of the lambda visible: capturing by reference avoids copying but introduces aliasing risks; capturing by value copies data but isolates the lambda from changes in the outer scope.

Second, it prevents accidental captures—the programmer must consciously decide what state the lambda will access.

### The Cost of `std::function`

While lambdas themselves impose no runtime overhead, C++ provides `std::function` as a type-erased wrapper for any callable object. This convenience comes at a cost: `std::function` involves heap allocation, virtual dispatch, and type erasure. The C++ standards committee has even considered `std::reference_closure` as a performance optimization for specific use cases. The lesson is clear: the lambda itself is zero-cost, but the abstractions built atop it may not be.

## Comparative Analysis

### Performance

| Aspect             | HULK                        | C#                          | Python                   | C++                      |
| ------------------ | --------------------------- | --------------------------- | ------------------------ | ------------------------ |
| Closure allocation | Heap (by-value)             | Heap (by-value)             | Heap (cell objects)      | Stack or heap (explicit) |
| Capture overhead   | Indirection through closure | Indirection through closure | Indirection through cell | Direct member access     |
| Zero-cost capture  | No                          | No (except static)          | No                       | Yes (stateless lambdas)  |
| Inlining potential | Limited                     | JIT-dependent               | None                     | Excellent                |

C++ achieves the highest performance because lambdas are resolved entirely at compile time; the closure object is a concrete type, and the compiler can inline the `operator()` call.

C# achieves good performance through JIT optimization, but heap allocation for closures remains a cost.

HULK, as a compiled language, could potentially inline small lambdas, but the heap-allocated closure representation introduces indirection.

Python pays the highest cost due to dynamic dispatch and cell indirection.

### Type Safety and Inference

| Aspect               | HULK                    | C#                 | Python  | C++                    |
| -------------------- | ----------------------- | ------------------ | ------- | ---------------------- |
| Static typing        | Yes                     | Yes                | No      | Yes                    |
| Parameter inference  | Constraint-based        | Delegate signature | Runtime | Template deduction     |
| Inference errors     | Compile-time            | Compile-time       | Runtime | Compile-time           |
| Explicit annotations | Required when ambiguous | Optional           | N/A     | Required for templates |

HULK's constraint-based inference occupies a middle ground: it is more expressive than C++'s template-based inference in some contexts but less powerful than C#'s delegate-based inference.

Python's dynamic typing means no static inference is performed at all—errors manifest at runtime.

### Capture Semantics

| Aspect          | HULK                | C#                  | Python                      | C++                                 |
| --------------- | ------------------- | ------------------- | --------------------------- | ----------------------------------- |
| Default capture | By-value (implicit) | By-value (implicit) | By-reference (late binding) | Explicit (by-value or by-reference) |
| Capture control | None                | Static modifier     | Workarounds                 | Full control                        |
| Memory safety   | Safe                | Safe                | Safe (but can leak)         | Requires care (dangling references) |

C++ provides the most control but also the most responsibility—capturing by reference can lead to dangling references if the lambda outlives the captured scope.

HULK's by-value capture is safe but inflexible.

C# provides a reasonable default with static lambdas as an optimization.

Python's late binding is the most surprising and error-prone.

### Expressiveness

| Aspect                  | HULK                         | C#  | Python | C++ |
| ----------------------- | ---------------------------- | --- | ------ | --- |
| Expression body         | Yes                          | Yes | Yes    | Yes |
| Statement body          | Yes (via `function` keyword) | Yes | No     | Yes |
| Multi-statement lambdas | Yes                          | Yes | No     | Yes |
| Expression trees        | No                           | Yes | No     | No  |

C# uniquely supports expression trees, enabling runtime metaprogramming.

HULK and C++ support full statement bodies, while Python restricts lambdas to single expressions.

## Philosophical Implications

The divergent implementations reveal deeper philosophical commitments.

C++ treats lambdas as a compile-time abstraction—a syntactic sugar for manually written functors. The philosophy is _"zero-cost"_: the programmer should not pay for abstraction they do not use. This explains the explicit capture syntax, the generation of unique types, and the absence of runtime overhead for stateless lambdas.

C# treats lambdas as a runtime abstraction—a way to create delegate instances that can be passed around, stored, and even analyzed as expression trees. The philosophy is _"practical pragmatism"_: provide powerful abstractions, optimize the common cases, and allow escape hatches (static lambdas) for performance-critical code.

Python treats lambdas as a lightweight syntactic convenience—a way to avoid defining trivial functions. The philosophy is _"simplicity first"_: lambdas are not a core abstraction but a tool for specific use cases. This explains their syntactic limitations and the surprising late-binding behavior, which is a consequence of Python's uniform treatment of scopes.

HULK treats lambdas as a first-class citizen in a hybrid language—a bridge between functional and object-oriented paradigms. The philosophy is _"local reasoning"_: make the compiler's behavior predictable, force explicitness when ambiguity arises, and provide clear diagnostics. This explains the constraint-based inference that refuses to guess at the call site.
