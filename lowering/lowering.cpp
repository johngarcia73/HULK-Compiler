#include "lowering.hpp"

namespace lowering {

Type* LoweringVisitor::visit(ProgramNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(BlockNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(FunctionDeclNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(LetNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(IfNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(FunctionCallNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(VariableNode& node) {
    return nullptr; 
}

Type* LoweringVisitor::visit(NumberNode& node) { 
    return nullptr; 
}

Type* LoweringVisitor::visit(BinaryOpNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(ExprStmtNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(AssignmentNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(MemberAccessNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(NewNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(WhileNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(ForNode& node) {
    // LOWERING SUGGESTION 1: For-Loops to While-Loops
    // A For loop (e.g., `for (x in iterable) body`) can be lowered into a while loop:
    // `let it = iterable.iterator() in while (it.next()) { let x = it.current() in body }`
    // This allows the IR generator to only worry about implementing `while` loops.
    // To do this, we would replace the ForNode in the AST with the corresponding LetNode + WhileNode structure.
    return nullptr;
}

Type* LoweringVisitor::visit(AttributeDeclNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(TypeDeclNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(ProtocolDeclNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(ParamListNode& node) { return nullptr; }
Type* LoweringVisitor::visit(LetBindingNode& node) { return nullptr; }
Type* LoweringVisitor::visit(LetBindingsNode& node) { return nullptr; }
Type* LoweringVisitor::visit(UnaryOpNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(StringNode& node) { return nullptr; }
Type* LoweringVisitor::visit(BoolNode& node) { return nullptr; }
Type* LoweringVisitor::visit(TypeNode& node) { return nullptr; }

Type* LoweringVisitor::visit(ReturnNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(LambdaNode& node) {
    // LOWERING SUGGESTION 2: Lambdas/Closures to Classes
    // Lambda expressions (closures) can be lowered into an explicit class declaration:
    // 1. Create a new class that implements an `invoke` method containing the lambda body.
    // 2. The captured variables from the surrounding environment become attributes of this class.
    // 3. The lambda instantiation `(x => x + y)` becomes `new Lambda_1(y)`.
    return nullptr;
}

Type* LoweringVisitor::visit(VectorLiteralNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(VectorComprehensionNode& node) {
    // LOWERING SUGGESTION 3: Vector Comprehension to Loops
    // Vector comprehensions like `[f(x) || x in iterable]` can be lowered into:
    // `let vec = new Vector() in { for(x in iterable) vec.append(f(x)); vec }`
    // This removes the need for a separate IR generation path for comprehensions.
    return nullptr;
}

Type* LoweringVisitor::visit(IndexAccessNode& node) {
    // LOWERING SUGGESTION 4: Index Access to Method Call
    // Array/Vector access like `arr[i]` can be lowered to a method call `arr.get(i)` 
    // This aligns the AST closer to standard object-oriented method dispatch.
    return nullptr;
}

} // namespace lowering
