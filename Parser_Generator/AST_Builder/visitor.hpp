#pragma once
#include "../../Semantic_Check/type.hpp"

// Forward declarations de los nodos que realmente usas
class ProgramNode;
class BlockNode;
class FunctionDeclNode;
class LetNode;
class IfNode;
class FunctionCallNode;
class VariableNode;
class NumberNode;
class BinaryOpNode;
class ExprStmtNode;
class ParamListNode;
class LetBindingNode;
class LetBindingsNode;
class UnaryOpNode;

class Visitor {
public:
    virtual ~Visitor() = default;

    virtual Type* visit(ProgramNode& node) = 0;
    virtual Type* visit(BlockNode& node) = 0;
    virtual Type* visit(FunctionDeclNode& node) = 0;
    virtual Type* visit(LetNode& node) = 0;
    virtual Type* visit(IfNode& node) = 0;
    virtual Type* visit(FunctionCallNode& node) = 0;
    virtual Type* visit(VariableNode& node) = 0;
    virtual Type* visit(NumberNode& node) = 0;
    virtual Type* visit(BinaryOpNode& node) = 0;
    virtual Type* visit(ExprStmtNode& node) = 0;
    virtual Type* visit(ParamListNode& node) = 0;
    virtual Type* visit(LetBindingNode& node) = 0;
    virtual Type* visit(LetBindingsNode& node) = 0;
    virtual Type* visit(UnaryOpNode& node) = 0;
};