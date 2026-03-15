#pragma once

// Forward declarations de todos los nodos concretos del AST
class ProgramNode;
class DeclarationListNode;
class StatementListNode;
class ParameterListNode;
class ParameterNode;
class ArgumentListNode;
class ElifListNode;
class LetBindingListNode;
class LetBindingNode;
class FunctionDeclNode;
class TypeDeclNode;
class ProtocolDeclNode;
class AttributeDeclNode;
class BlockNode;
class ExprStmtNode;
class BinaryOpNode;
class UnaryOpNode;
class AssignmentNode;
class VariableNode;
class NumberNode;
class StringNode;
class BoolNode;
class FunctionCallNode;
class VariableDeclNode;
class IfNode;
class WhileNode;
class ForNode;
class InitializationNode;

class Visitor {
public:
    virtual ~Visitor() = default;

    // Visita para cada tipo concreto de nodo
    virtual void visit(ProgramNode& node) = 0;
    virtual void visit(DeclarationListNode& node) = 0;
    virtual void visit(StatementListNode& node) = 0;
    virtual void visit(ParameterListNode& node) = 0;
    virtual void visit(ParameterNode& node) = 0;
    virtual void visit(ArgumentListNode& node) = 0;
    virtual void visit(ElifListNode& node) = 0;
    virtual void visit(LetBindingListNode& node) = 0;
    virtual void visit(LetBindingNode& node) = 0;
    virtual void visit(FunctionDeclNode& node) = 0;
    virtual void visit(TypeDeclNode& node) = 0;
    virtual void visit(ProtocolDeclNode& node) = 0;
    virtual void visit(AttributeDeclNode& node) = 0;
    virtual void visit(BlockNode& node) = 0;
    virtual void visit(ExprStmtNode& node) = 0;
    virtual void visit(BinaryOpNode& node) = 0;
    virtual void visit(UnaryOpNode& node) = 0;
    virtual void visit(AssignmentNode& node) = 0;
    virtual void visit(VariableNode& node) = 0;
    virtual void visit(NumberNode& node) = 0;
    virtual void visit(StringNode& node) = 0;
    virtual void visit(BoolNode& node) = 0;
    virtual void visit(FunctionCallNode& node) = 0;
    virtual void visit(VariableDeclNode& node) = 0;
    virtual void visit(IfNode& node) = 0;
    virtual void visit(WhileNode& node) = 0;
    virtual void visit(ForNode& node) = 0;
    virtual void visit(InitializationNode& node) = 0;
};