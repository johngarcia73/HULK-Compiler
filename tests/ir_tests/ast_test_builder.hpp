#pragma once

#include "../../parser/AST_Builder/ast_node.hpp"
#include <string>
#include <vector>

namespace ASTTestBuilder {

    // Forward declarations of specific builders
    class BlockBuilder;
    class FunctionBuilder;
    class FunctionCallBuilder;
    class TypeDeclarationBuilder;
    class ProgramBuilder;

    // ============================================================================
    // Basic Node Factories (Terminal nodes & simple expressions)
    // ============================================================================
    inline NumberNode* makeNumber(std::string value) {
        return new NumberNode(value);
    }

    inline BoolNode* makeBoolean(bool value) {
        return new BoolNode(value);
    }

    inline StringNode* makeString(const std::string& value) {
        return new StringNode(value);
    }

    inline VariableNode* makeVariable(const std::string& name) {
        return new VariableNode(name);
    }

    inline BinaryOpNode* makeBinaryOperation(const std::string& operatorSymbol, ASTNodePtr leftOperand, ASTNodePtr rightOperand) {
        return new BinaryOpNode(operatorSymbol, leftOperand, rightOperand);
    }

    inline UnaryOpNode* makeUnaryOperation(const std::string& operatorSymbol, ASTNodePtr operand) {
        return new UnaryOpNode(operatorSymbol, operand);
    }

    inline AssignmentNode* makeAssignment(const std::string& targetName, ASTNodePtr value) {
        return new AssignmentNode(targetName, value);
    }

    inline LetNode* makeLetExpression(const std::string& variableName, ASTNodePtr initialization, ASTNodePtr body) {
        return new LetNode(variableName, initialization, body);
    }

    inline IfNode* makeIfExpression(ASTNodePtr condition, ASTNodePtr thenBranch, ASTNodePtr elseBranch) {
        return new IfNode(condition, thenBranch, elseBranch);
    }

    inline WhileNode* makeWhileLoop(ASTNodePtr condition, ASTNodePtr body) {
        return new WhileNode(condition, body);
    }

    inline ForNode* makeForLoop(const std::string& iteratorName, ASTNodePtr iterable, ASTNodePtr body) {
        return new ForNode(iteratorName, iterable, body);
    }

    inline ExprStmtNode* makeExpressionStatement(ASTNodePtr expression) {
        return new ExprStmtNode(expression);
    }

    inline ReturnNode* makeReturnStatement(ASTNodePtr expression) {
        return new ReturnNode(expression);
    }

    // ============================================================================
    // Fluent Builders for complex structures
    // ============================================================================

    class BlockBuilder {
    private:
        std::vector<ASTNodePtr> statements;
    public:
        BlockBuilder& addStatement(ASTNodePtr statement) {
            statements.push_back(statement);
            return *this;
        }

        BlockBuilder& addExpression(ASTNodePtr expression) {
            statements.push_back(makeExpressionStatement(expression));
            return *this;
        }

        BlockNode* build() const {
            return new BlockNode(statements);
        }
    };

    class FunctionCallBuilder {
    private:
        std::string functionName;
        std::vector<ASTNodePtr> arguments;
    public:
        FunctionCallBuilder(const std::string& name) : functionName(name) {}

        FunctionCallBuilder& addArgument(ASTNodePtr argument) {
            arguments.push_back(argument);
            return *this;
        }

        FunctionCallNode* build() const {
            return new FunctionCallNode(functionName, arguments);
        }
    };

    class FunctionBuilder {
    private:
        std::string functionName;
        std::vector<std::string> parameterNames;
        std::vector<Type*> parameterTypes;
        Type* returnType = nullptr;
        ASTNodePtr functionBody = nullptr;
        bool isInlineMode = false;
        
    public:
        FunctionBuilder(const std::string& name) : functionName(name) {}

        FunctionBuilder& addParameter(const std::string& paramName, Type* paramType = nullptr) {
            parameterNames.push_back(paramName);
            parameterTypes.push_back(paramType);
            return *this;
        }

        FunctionBuilder& setReturnType(Type* type) {
            returnType = type;
            return *this;
        }

        FunctionBuilder& setBlockBody(BlockNode* block) {
            functionBody = block;
            isInlineMode = false;
            return *this;
        }

        FunctionBuilder& setInlineBody(ASTNodePtr expression) {
            functionBody = expression;
            isInlineMode = true;
            return *this;
        }

        FunctionDeclNode* build() const {
            if (isInlineMode) {
                return new FunctionDeclNode(functionName, parameterNames, parameterTypes, returnType, functionBody, true);
            }
            return new FunctionDeclNode(functionName, parameterNames, parameterTypes, returnType, functionBody);
        }
    };

    class ProgramBuilder {
    private:
        std::vector<ASTNodePtr> declarations;
        std::vector<ASTNodePtr> statements;
    public:
        ProgramBuilder& addDeclaration(ASTNodePtr declaration) {
            declarations.push_back(declaration);
            return *this;
        }

        ProgramBuilder& addStatement(ASTNodePtr statement) {
            statements.push_back(statement);
            return *this;
        }

        ProgramNode* build() const {
            return new ProgramNode(declarations, statements);
        }
    };

    inline FunctionCallBuilder buildFunctionCall(const std::string& functionName) {
        return FunctionCallBuilder(functionName);
    }

    inline BlockBuilder buildBlock() {
        return BlockBuilder();
    }

    inline FunctionBuilder buildFunction(const std::string& name) {
        return FunctionBuilder(name);
    }

    inline ProgramBuilder buildProgram() {
        return ProgramBuilder();
    }

} // namespace ASTTestBuilder
