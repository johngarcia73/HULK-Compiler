# Lambda Expressions in the HULK Compiler: A Rigorous Implementation Study

## 1. Introduction: The Problem Domain in HULK's Ecosystem

HULK is a statically typed language that marries object‑oriented constructs (types, inheritance, protocols) with functional sensibilities (higher‑order functions, type inference, and expression‑oriented syntax). Within this hybrid paradigm, lambda expressions serve a critical role: they are the primary vehicle for **behaviour abstraction**—the ability to treat computations as data without the syntactic overhead of named function declarations.

In HULK, the challenge is not merely to support anonymous functions, but to integrate them **seamlessly** with:

- A nominal type system that distinguishes `Number`, `String`, `Bool`, `Object`, and user‑defined types.
- Overload resolution that must consider function types as first‑class citizens.
- Constraint‑based type inference that propagates type information both forward (from arguments) and backward (from expected types).
- Mutation and scoping rules that must correctly capture variables from enclosing scopes.

The compiler must answer several fundamental questions: _How does the semantic visitor infer types for parameters that lack annotations, especially when the lambda is passed to a generic higher‑order function? How are free variables resolved without violating the scoping discipline of an imperative language?_ This report answers these questions by walking through the front‑end pipeline, justifying every design decision with reference to the actual codebase.

---

### 1.1 Eliminating the "Nominal Ceremony"

Without lambdas, higher‑order functions force programmers to pollute the namespace with single‑use function definitions. In HULK, writing:
function apply_twice(f: (Number) -> Number, x: Number): Number { f(f(x)); }
would require a separate definition for every transformation. With lambdas, the transformation is expressed **in situ**, reducing indirection and improving locality. This directly aligns with the principle of **cohesion**: the behaviour is placed exactly where it is relevant.

### 1.2 Enabling Custom Control Structures

HULK permits the definition of functions that abstract over control flow—for example, a `with_resource` pattern that acquires a resource, executes a user‑supplied block, and releases it. Lambdas are the only syntactically lightweight way to pass the "body" of such a custom construct without resorting to macros or code generation. This turns HULK from a fixed‑set‑of‑statements language into an **extensible** one, where programmers can design their own control‑flow primitives.

### 1.3 First‑Class Iteration and Collection Processing

HULK provides built‑in vector types and iteration constructs. The combination of `for` loops and iterators is powerful, but lambdas elevate collection processing to a declarative style. Functions like `map`, `filter`, and `fold` become natural, encouraging a **data‑pipeline** programming model. This is not just syntactic sugar; it shifts the programmer's mental model from _imperative steps_ to _transformational chains_, often reducing logic errors and improving parallelisability.

### 1.4 Delayed Evaluation and Lazy Initialisation

In HULK, lambdas of arity zero (often called _thunks_) allow deferring expensive computations until their result is strictly needed. This is crucial for building lazy data structures, implementing memoisation, or handling optional initialisation in a clean way.

---

## 2 Implementation Architecture

The remainder of this report dissects the implementation path of lambdas through the HULK compiler's core front‑end phases: **AST construction**, **semantic analysis**, and **scoping**. Each subsection refers explicitly to the actual code structures that support these phases.

### 2.1 AST Representation: The `LambdaNode` Structure

The parser's AST builder creates a `LambdaNode` for every lambda expression, regardless of the syntactic form used. The node is defined in `ast_node.hpp` (and implemented in `ast_node.cpp`) as:

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

---

## 4. The Lowering Pass: From `LambdaNode` to a Delegate Object

The semantic phase delivers a type-complete AST where every `LambdaNode` carries its inferred `FunctionType` and a resolved capture environment implied by the symbol table. The front-end, however, does **not** yet translate this into a concrete runtime representation — that is the job of the **lowering pass**, implemented in `lowering/lowering.cpp`.

The lowering visitor is a second AST walk that **rewrites** the tree in-place. It does this through a `parentReference` pointer-to-pointer mechanism: every visit method saves the pointer slot that the parent used to reach the current node, so a visit can atomically splice a completely different subtree into the parent without a separate fixup step.

### 4.1 The Delegate-Object Strategy

HULK lowers every `LambdaNode` into a **heap-allocated delegate object**. The transformation is performed by `LoweringVisitor::visit(LambdaNode&)` and proceeds in three conceptual phases.

#### Phase 1 — Generate a unique `TypeDeclNode`

```
Delegate_1 (type)
├── ctor params: every variable currently in scope (the captured environment)
├── attribute: one field per captured variable (by-value copy)
└── invoke method: the lambda body, with a wrapper that re-binds captures
```

A fresh name `Delegate_<N>` is minted via a static counter (e.g., `Delegate_1`, `Delegate_2`, …). This guarantees uniqueness across the compilation unit.

```cpp
static int delegateId = 0;
std::string delegateName = "Delegate_" + std::to_string(++delegateId);

auto invokeMethod = new FunctionDeclNode(
    "invoke",
    node.params,
    node.paramTypes,
    node.declaredReturnType,
    node.body,
    true
);
invokeMethod->isMethod = true;
invokeMethod->ownerTypeName = delegateName;
```

#### Phase 2 — Wrap the body to restore captured variables

Before handing the original lambda body to the `invoke` method, the lowering visitor wraps it in a chain of `LetNode`s that read each captured variable back out of `self`:

```cpp
auto variablesInScope = scopeTable.getAllVariables();
// Iterate in reverse so the first variable becomes the outermost let
for (int i = variablesInScope.size()-1; i >= 0; i--) {
    auto var  = variablesInScope[i].first;
    auto type = variablesInScope[i].second;

    auto selfVar      = new VariableNode("self");
    selfVar->type     = new NominalType(delegateName);
    auto memberAccess = new MemberAccessNode(selfVar, var);
    memberAccess->type = type;

    wrappedBody = new LetNode(var, memberAccess, wrappedBody);
    wrappedBody->type = node.body->type;
}
invokeMethod->exprBody = wrappedBody;
```

Iterating in reverse order ensures the outermost `let` corresponds to the first captured variable, so the nesting matches the original lexical order. The result is a synthetic expression of the form:

```
let x = self.x in
  let y = self.y in
    <original lambda body>
```

This technique means the body code does not change at all — it still references `x` and `y` as plain variables. The delegate wrapper simply ensures those names are bound before the body executes.

#### Phase 3 — Replace the `LambdaNode` with a `NewNode`

The lowering pass then constructs the full `TypeDeclNode`, populating its constructor parameters and attribute fields from the captured environment:

```cpp
for (auto [var, type] : variablesInScope) {
    ctorParams.push_back(var);
    ctorParamTypes.push_back(type);
    auto attr = new AttributeDeclNode(var, new VariableNode(var));
    members.push_back(attr);
    newArgs.push_back(new VariableNode(var));
}

auto typeDecl = new TypeDeclNode(delegateName, ctorParams, ...);
typeDecl->parentType = "Object";
generatedTypes.push_back(typeDecl);  // injected at front of program decls

ASTNodePtr newNode = new NewNode(delegateName, newArgs);
newNode->type = new NominalType(delegateName);
*parentRefCopy = newNode;  // replaces the LambdaNode in the AST
```

The original `LambdaNode` in the AST is atomically replaced with a plain object-construction expression. From the IR generator's perspective, the lambda has completely vanished.

The `TypeDeclNode` for the delegate is injected at the **front** of `node.decls` in the program node (inside `visit(ProgramNode)`), so it is compiled before any code that references it.

### 4.2 Delegate Call Sites

After lowering, a call like `f(x)` where `f` is a lambda variable needs to dispatch through `invoke` instead of a named function. The semantic visitor records an `overloadNote` of `"Selected functor value: ..."` on any `FunctionCallNode` that resolves to a lambda variable. The lowering pass detects this note:

```cpp
bool isDelegateCall = false;
for (const auto& note : node.overloadNotes) {
    if (note.find("Selected functor value:") != std::string::npos) {
        isDelegateCall = true;
        break;
    }
}

if (isDelegateCall && !node.receiver) {
    auto delegateInstance = new VariableNode(node.name);
    delegateInstance->type = node.resolvedFunctionType;
    auto invokeCall = new FunctionCallNode("invoke", node.args, delegateInstance);
    invokeCall->type = node.type;
    *parentRefCopy = invokeCall;
}
```

The call `f(42)` is rewritten to `f.invoke(42)`. Since `invoke` is a regular method on the delegate type, it goes through the standard virtual-dispatch path (`_get_virtual_method`) at the IR level — exactly the same mechanism used for every other method call in HULK.

### 4.3 Method-to-Function Lowering (applied to `invoke`)

The `invoke` method created during lambda lowering is itself subject to the general method-lowering pass in `ir_generator/lowering.cpp`. This pass transforms every OOP-style method `TypeName::methodName(params)` into a flat C-style function `_TypeName_methodName(params, self)` by appending `self` as an explicit trailing parameter:

```cpp
FunctionDeclNode* methodToFunctionLowering(
    const std::string& typeName, FunctionDeclNode* method)
{
    std::string newName = "_" + typeName + "_" + method->name;

    std::vector<std::string> newParams = method->params;
    newParams.push_back("self");              // self is the last argument

    std::vector<Type*> newParamTypes = method->paramTypes;
    newParamTypes.push_back(nullptr);         // pointer-sized; typed at IR level

    // ...
}
```

This is the universal calling convention for HULK methods, keeping the QBE IR free of any notion of `this` or implicit receivers. The resulting function for a delegate named `Delegate_1` with an identity body is thus `__Delegate_1_invoke(n, self)`.

---

## 5. The Runtime: How Delegates Execute

The HULK runtime (`runtime/runtime.c`) is a thin C layer on top of the **Boehm–Demers–Weiser garbage collector** and a custom open-addressed hash-map. It knows nothing about lambdas specifically; it only sees the delegate objects that the lowering pass produces.

### 5.1 Allocation via the GC

```c
void* _hulk_alloc(size_t size) {
    return GC_malloc(size);  // Boehm GC
}
```

When the IR for `new Delegate_1(x, y)` executes, the IR generator emits:

```qbe
%obj = l call $_hulk_alloc(l <size_of_Delegate_1>)
storel $Delegate_1, %obj          # store type-id pointer at offset 0
```

The GC returns a managed pointer; no explicit `free` is ever needed. All delegate objects are automatically reclaimed when no more references to them exist.

### 5.2 Virtual Dispatch via the Vtable Hash-Map

Method identity is handled at runtime through **two global hash maps** initialised in `$main`:

| Map | Key | Value |
|---|---|---|
| `vtable_map` | class-id (global symbol address as `long`) | per-class method map |
| per-class method map | method-id (global symbol address as `long`) | raw function pointer |
| `inheritance_map` | child class-id | parent class-id |

```c
void _register_method(long class_id, long method_id, void* method_ptr) {
    struct hashmap_s* method_map = hashmap_get(&vtable_map, &class_id, sizeof(long));
    if (method_map == NULL) {
        method_map = malloc(sizeof(struct hashmap_s));
        hashmap_create(16, method_map);
        hashmap_put(&vtable_map, &class_id_copy, sizeof(long), method_map);
    }
    hashmap_put(method_map, &method_id_copy, sizeof(long), method_ptr);
}
```

At program start, `$main` registers every generated delegate type:

```qbe
call $_register_inheritance(l $Delegate_1, l $Object)
call $_register_method(l $Delegate_1, l $invoke, l $__Delegate_1_invoke)
```

When `f.invoke(42)` executes, the IR:

1. Loads the type-id pointer from offset 0 of `f` (the `class_id`).
2. Calls `_get_virtual_method(class_id, $invoke)` which walks the inheritance chain via hash-map lookups until it finds the function pointer.
3. Performs an **indirect call** through that pointer.

```c
void* _get_virtual_method(long class_id, long method_id) {
    long current_id = class_id;
    while (1) {
        struct hashmap_s* method_map = hashmap_get(&vtable_map, &current_id, sizeof(long));
        if (method_map) {
            void* fn = hashmap_get(method_map, &method_id, sizeof(long));
            if (fn) return fn;
        }
        long* parent_ptr = hashmap_get(&inheritance_map, &current_id, sizeof(long));
        if (!parent_ptr) return NULL;
        current_id = *parent_ptr;
    }
}
```

### 5.3 Captured Variables in Memory

Each captured variable becomes a field of the delegate struct at a byte offset tracked by the `TypeRegister`. Reading a captured variable (via the `let x = self.x` wrapper) compiles to:

```qbe
%field_ptr = l add %self, <offset_of_x>
%x         = d loadd %field_ptr    # 'd' for Number (double), 'w' for Bool/int
```

Writing back is symmetric. There is no indirection through a "cell" object (unlike Python) — the value is copied directly into the struct at construction time and read back by pointer arithmetic. This is fast but imposes strict **by-value capture** semantics: mutations to the original variable after the delegate is created are invisible inside the lambda, and vice versa.

---

## 6. Justification of Design Decisions

Every implementation choice outlined above stems from deliberate trade-offs. This section justifies those decisions against alternative approaches.

### 6.1 Why Constraint-Based Inference, not Hindley-Milner?

HULK opts for a bidirectional constraint-based inference (as seen in `refineUnknownExpression`) rather than a full unification-based system like Hindley-Milner (HM).

**Reason:** HULK is an imperative, object-oriented language. Full HM would complicate the interaction with mutation, subtyping, and overloading. Constraint-based inference is easier to integrate with nominal subtyping and produces predictable, local errors (e.g., _"parameter `x` must be `Number`"_) rather than cryptic unification failures.

**Trade-off:** Some lambda parameters require explicit annotations, especially if the body does not constrain them (e.g., `function (x) -> x`). This is acceptable because HULK's design philosophy favors explicitness where ambiguity exists, preventing surprising runtime behaviour.

### 6.2 Why Two Syntaxes (`lambda_expr` and `function_expr`)?

While many languages (e.g., Python and JavaScript) use a single syntax for both expression and block bodies, HULK separates them.

**Reason:** The grammar is **LALR(1)**. Merging them would require a grammar production with a lookahead conflict between `expr` and `block`. While solvable with `%prec` or token-based disambiguation, the separation improves readability of the grammar and maps cleanly to the two distinct AST representations.

**Programmer Benefit:** The `FUNCTION` keyword visually distinguishes block-oriented anonymous functions from lightweight expression lambdas, making code self-documenting.

### 6.3 Why Not Infer Return Types from All `return` Statements?

The visitor takes the body's type as the return type. For block bodies, the type is the type of the last expression. If multiple `return` statements exist with different types, the visitor uses `lowestCommonAncestor` to compute a join type.

**Reason:** This is consistent with HULK's expression-oriented nature. It encourages writing block bodies that have a coherent last value, which aligns with functional idioms.

**Trade-off:** Imperative programmers might find this restrictive. However, explicit return type annotations can always be added to override the inference.

### 6.4 Why Errors for Uninferred Parameters?

Some languages (e.g., Scala with partial unification) allow parameters to remain polymorphic until the call site. HULK does not.

**Reason:** HULK does not support rank-2 polymorphism or global type inference. Allowing `Unknown` to propagate to the call site would require a more complex solver. By forcing an annotation when the body is ambiguous, the compiler keeps the inference algorithm linear and predictable.

**Outcome:** The error message explicitly points to the parameter and suggests adding an annotation—a clear, actionable diagnostic.

### 6.5 Why Class-Based Delegates, not Raw Function Pointers?

The lowering strategy converts lambdas into full type declarations with an `invoke` method rather than a struct holding a raw function pointer + void* context (the classic C closure pattern).

**Reason:** This reuses the entire OOP infrastructure — allocation, virtual dispatch, inheritance registration, field layout — that HULK already possesses for user-defined types. There is no special-case code in the IR generator for lambdas: `IrGenerator::visit(LambdaNode&)` is a no-op stub, because by the time the IR generator runs, every `LambdaNode` has already been replaced by a `NewNode`. The implementation complexity is thus paid once (in the lowering pass) rather than scattered across every back-end stage.

**Trade-off:** Each unique lambda in the source creates a brand-new type in the type register and a brand-new set of vtable entries. For programs with many distinct lambdas this inflates the size of the vtable maps and the `$main` initialisation block. A more sophisticated compiler might merge structurally identical delegates into a single type.

---

## 7. The Price of Abstraction: Overheads and Limitations

Lambda expressions are not free. HULK pays several costs, which are important to acknowledge.

### 7.1 Semantic Complexity

The `visit(LambdaNode)` method, while clear, adds significant complexity to the type inference pass. The visitor must maintain nested scopes correctly, handle the interaction between lambda parameters and outer variables (shadowing), and manage the propagation of constraints across the lambda boundary. This increases the compiler's maintenance burden.

### 7.2 Runtime Overhead

- **Heap Allocation:** Every lambda instantiation allocates a delegate object via `_hulk_alloc` / `GC_malloc`. This is more expensive than a stack frame, and GC pressure grows with closure frequency.

- **Double Indirection for Dispatch:** Invoking a lambda first calls `_get_virtual_method` (a hash-map lookup through the vtable chain), then performs an indirect function call through the returned pointer. Named function calls have neither cost.

- **Vtable Bloat:** Each distinct lambda produces one `TypeDeclNode` and entries in both `vtable_map` and `inheritance_map`. A program with many inline callbacks will have a proportionally large initialisation sequence in `$main`.

- **Capture-Copy Cost:** Every variable in scope at the lambda definition site is copied into the delegate struct at object-construction time. For large captured environments this is non-trivial.

- **Optimisation Barriers:** Because `invoke` is resolved via `_get_virtual_method` at runtime, the IR generator cannot inline the call or specialise it. A C++ compiler, by contrast, knows the concrete closure type at compile time and can inline `operator()` entirely.

### 7.3 Diagnostic Challenges

When an error occurs inside a lambda, the programmer sees a stack trace involving anonymous functions and synthesized type names like `Delegate_3`. Providing meaningful location information (`span`) mitigates this, but understanding the closure's state still requires mental effort.

### 7.4 Inference Limitations

As noted, lambdas with insufficient constraints fail compilation. This is a deliberate trade-off to keep the system simple, but it can be frustrating for developers accustomed to fully inferred functional languages.

### 7.5 The Over-Capture Problem

Because `getAllVariables()` in `common/scope_table.hpp` returns **all** variables visible at the lambda definition site — including ones the lambda body never references — the generated delegate struct is larger than necessary. Consider:

```hulk
let big_array = ... in
let small_x   = 5   in
let f = (n) -> n + small_x in  // only uses small_x
f(10)
```

The lowering pass will capture both `big_array` and `small_x` into the `Delegate_N` struct, even though `big_array` is never read inside `f`. In languages like C++ this is caught at compile time (you must list captures explicitly), but HULK has no mechanism to warn about or eliminate unnecessary captures.

**Consequence:** Objects live longer than necessary (the delegate holds a reference that prevents GC collection of `big_array`), struct sizes are inflated, and the construction call at the `NewNode` evaluates and copies every in-scope variable. For large or deeply nested scopes, this can be significant.

**Potential fix:** A free-variable analysis pass (similar to `freeVars()` in lambda calculus implementations) could compute the minimal capture set before the lowering visitor runs, so only genuinely referenced variables become delegate fields.

---

## 8. The Full Pipeline at a Glance

The following diagram shows the complete lifecycle of a lambda expression through the compiler:

```
Source text
    |
    v
+-----------+
|  Parser   |  Creates LambdaNode with params, body, optional annotations
+-----------+
    |
    v
+------------------------+
| TypeInferenceVisitor   |  Resolves param/return types via constraint propagation
| (Semantic Phase)       |  Sets node.functionType
|                        |  Free variables resolved via symTable scoping
+------------------------+
    |
    v
+------------------------+
| LoweringVisitor        |  Rewrites LambdaNode --> NewNode(Delegate_N)
| (lowering/lowering.cpp)|  Generates TypeDeclNode with invoke method + captured fields
|                        |  Wraps body: let x = self.x in ... let y = self.y in <body>
|                        |  Rewrites call sites: f(x) --> f.invoke(x)
+------------------------+
    |
    v
+------------------------+
| IrGenerator            |  visit(LambdaNode) is a no-op -- already lowered
| (Code Gen Phase)       |  NewNode --> _hulk_alloc + field initialization
|                        |  f.invoke(x) --> _get_virtual_method + indirect call
+------------------------+
    |
    v
+------------------------+
| QBE IR                 |  Flat C-like IR; no lambda concept remains
+------------------------+
    |
    v
+------------------------+
| runtime.c              |  GC allocation, vtable lookup, string helpers
+------------------------+
```

The key insight is the **strict phase separation**: the semantic phase handles _what_ a lambda means; the lowering phase handles _how_ it is represented; the IR generator handles _how_ that representation maps to machine operations; and the runtime handles _how_ those operations execute safely.

---

## 9. Comparison with Other Languages

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

---

## 10. Comparative Analysis

### Performance

| Aspect             | HULK                           | C#                          | Python                   | C++                      |
| ------------------ | ------------------------------ | --------------------------- | ------------------------ | ------------------------ |
| Closure allocation | Heap (GC-managed)              | Heap (GC-managed)           | Heap (cell objects)      | Stack or heap (explicit) |
| Capture overhead   | Field copy into delegate       | Field copy into closure     | Indirection through cell | Direct member access     |
| Zero-cost capture  | No                             | No (except static)          | No                       | Yes (stateless lambdas)  |
| Dispatch overhead  | vtable hash-map + indirect call| JIT inline cache            | Dynamic dispatch          | Inlined at compile time  |
| Inlining potential | None (virtual dispatch)        | JIT-dependent               | None                     | Excellent                |

HULK's double-indirection (vtable hash-map lookup + indirect call) places it below C# in raw dispatch performance, despite both using heap allocation. C# benefits from JIT inline caching that learns the concrete type; HULK's runtime vtable uses a generic hash-map with no such learning.

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

| Aspect          | HULK                       | C#                  | Python                      | C++                                 |
| --------------- | -------------------------- | ------------------- | --------------------------- | ----------------------------------- |
| Default capture | By-value (implicit)        | By-value (implicit) | By-reference (late binding) | Explicit (by-value or by-reference) |
| Capture control | None                       | Static modifier     | Workarounds                 | Full control                        |
| Over-capture    | Yes (all in-scope vars)    | No (used vars only) | N/A (by-ref)                | No (explicit list)                  |
| Memory safety   | Safe (GC)                  | Safe (GC)           | Safe (but can leak)         | Requires care (dangling references) |

C++ provides the most control but also the most responsibility—capturing by reference can lead to dangling references if the lambda outlives the captured scope.

HULK's by-value capture is safe but inflexible, and the over-capture problem (§7.5) sets it apart even from C#, which only captures variables that the lambda body actually uses.

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

---

## 11. Philosophical Implications

The divergent implementations reveal deeper philosophical commitments.

C++ treats lambdas as a compile-time abstraction—a syntactic sugar for manually written functors. The philosophy is _"zero-cost"_: the programmer should not pay for abstraction they do not use. This explains the explicit capture syntax, the generation of unique types, and the absence of runtime overhead for stateless lambdas.

C# treats lambdas as a runtime abstraction—a way to create delegate instances that can be passed around, stored, and even analyzed as expression trees. The philosophy is _"practical pragmatism"_: provide powerful abstractions, optimize the common cases, and allow escape hatches (static lambdas) for performance-critical code.

Python treats lambdas as a lightweight syntactic convenience—a way to avoid defining trivial functions. The philosophy is _"simplicity first"_: lambdas are not a core abstraction but a tool for specific use cases. This explains their syntactic limitations and the surprising late-binding behavior, which is a consequence of Python's uniform treatment of scopes.

HULK treats lambdas as a first-class citizen in a hybrid language—a bridge between functional and object-oriented paradigms. The philosophy is _"local reasoning"_: make the compiler's behavior predictable, force explicitness when ambiguity arises, and provide clear diagnostics. The lowering strategy reinforces this: by reusing the type system's own machinery (types, constructors, method dispatch), HULK avoids special-casing lambdas in the IR generator and the runtime, at the cost of heavier per-lambda overhead and the over-capture problem. This trade-off is consistent with HULK's position as a teaching/research compiler where clarity of implementation outweighs peak performance.
