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
    class ProtocolBuilder;

    // ============================================================================
    // Basic Node Factories (Terminal nodes & simple expressions)
    // ============================================================================
    inline NumberNode* makeNumber(std::string value, Type* type = nullptr) {
        auto node = new NumberNode(value);
        node->type = type;
        return node;
    }

    inline BoolNode* makeBoolean(bool value, Type* type = nullptr) {
        auto node = new BoolNode(value);
        node->type = type;
        return node;
    }

    inline StringNode* makeString(const std::string& value, Type* type = nullptr) {
        auto node = new StringNode(value);
        node->type = type;
        return node;
    }

    inline VariableNode* makeVariable(const std::string& name, Type* type = nullptr) {
        auto node = new VariableNode(name);
        node->type = type;
        return node;
    }

    inline BinaryOpNode* makeBinaryOperation(const std::string& operatorSymbol, ASTNodePtr leftOperand, ASTNodePtr rightOperand, Type* type = nullptr) {
        auto node = new BinaryOpNode(operatorSymbol, leftOperand, rightOperand);
        node->type = type;
        return node;
    }

    inline UnaryOpNode* makeUnaryOperation(const std::string& operatorSymbol, ASTNodePtr operand, Type* type = nullptr) {
        auto node = new UnaryOpNode(operatorSymbol, operand);
        node->type = type;
        return node;
    }

    inline AssignmentNode* makeAssignment(const std::string& targetName, ASTNodePtr value, Type* type = nullptr) {
        auto node = new AssignmentNode(targetName, value);
        node->type = type;
        return node;
    }

    inline AssignmentNode* makeAssignment(ASTNodePtr target, ASTNodePtr value, Type* type = nullptr) {
        auto node = new AssignmentNode(target, value);
        node->type = type;
        return node;
    }

    inline LetNode* makeLetExpression(const std::string& variableName, ASTNodePtr initialization, ASTNodePtr body, Type* type = nullptr) {
        auto node = new LetNode(variableName, initialization, body);
        node->type = type;
        return node;
    }

    inline IfNode* makeIfExpression(ASTNodePtr condition, ASTNodePtr thenBranch, ASTNodePtr elseBranch, Type* type = nullptr) {
        auto node = new IfNode(condition, thenBranch, elseBranch);
        node->type = type;
        return node;
    }

    inline WhileNode* makeWhileLoop(ASTNodePtr condition, ASTNodePtr body, Type* type = nullptr) {
        auto node = new WhileNode(condition, body);
        node->type = type;
        return node;
    }

    inline ForNode* makeForLoop(const std::string& iteratorName, ASTNodePtr iterable, ASTNodePtr body, Type* type = nullptr) {
        auto node = new ForNode(iteratorName, iterable, body);
        node->type = type;
        return node;
    }

    inline ExprStmtNode* makeExpressionStatement(ASTNodePtr expression, Type* type = nullptr) {
        auto node = new ExprStmtNode(expression);
        node->type = type;
        return node;
    }

    inline ReturnNode* makeReturnStatement(ASTNodePtr expression, Type* type = nullptr) {
        auto node = new ReturnNode(expression);
        node->type = type;
        return node;
    }

    inline NewNode* makeNewNode(const std::string& typeName, const std::vector<ASTNodePtr>& arguments = {}, Type* type = nullptr) {
        auto node = new NewNode(typeName, arguments);
        node->type = type;
        return node;
    }

    inline MemberAccessNode* makeMemberAccess(ASTNodePtr base, const std::string& member, Type* type = nullptr) {
        auto node = new MemberAccessNode(base, member);
        node->type = type;
        return node;
    }

    // ============================================================================
    // Fluent Builders for complex structures
    // ============================================================================

    class BlockBuilder {
    private:
        std::vector<ASTNodePtr> statements;
        Type* type = nullptr;
    public:
        BlockBuilder& addStatement(ASTNodePtr statement) {
            statements.push_back(statement);
            return *this;
        }

        BlockBuilder& addExpression(ASTNodePtr expression) {
            statements.push_back(makeExpressionStatement(expression));
            return *this;
        }

        BlockBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        BlockNode* build() const {
            auto node = new BlockNode(statements);
            node->type = type;
            return node;
        }
    };

    class FunctionCallBuilder {
    private:
        std::string functionName;
        std::vector<ASTNodePtr> arguments;
        ASTNodePtr receiver = nullptr;
        Type* type = nullptr;
    public:
        FunctionCallBuilder(const std::string& name) : functionName(name) {}

        FunctionCallBuilder& addArgument(ASTNodePtr argument) {
            arguments.push_back(argument);
            return *this;
        }

        FunctionCallBuilder& setReceiver(ASTNodePtr rec) {
            receiver = rec;
            return *this;
        }

        FunctionCallBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        FunctionCallNode* build() const {
            auto node = new FunctionCallNode(functionName, arguments, receiver);
            node->type = type;
            return node;
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
        Type* type = nullptr;
        
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

        FunctionBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        FunctionDeclNode* build() const {
            FunctionDeclNode* node = nullptr;
            if (isInlineMode) {
                node = new FunctionDeclNode(functionName, parameterNames, parameterTypes, returnType, functionBody, true);
            } else {
                node = new FunctionDeclNode(functionName, parameterNames, parameterTypes, returnType, functionBody);
            }
            node->type = type;
            return node;
        }
    };

    class ProgramBuilder {
    private:
        std::vector<ASTNodePtr> declarations;
        std::vector<ASTNodePtr> statements;
        Type* type = nullptr;
    public:
        ProgramBuilder& addDeclaration(ASTNodePtr declaration) {
            declarations.push_back(declaration);
            return *this;
        }

        ProgramBuilder& addStatement(ASTNodePtr statement) {
            statements.push_back(statement);
            return *this;
        }

        ProgramBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        ProgramNode* build() const {
            auto node = new ProgramNode(declarations, statements);
            node->type = type;
            return node;
        }
    };

    class TypeDeclarationBuilder {
    private:
        std::string name;
        std::vector<std::string> ctorParams;
        std::vector<Type*> ctorParamTypes;
        std::vector<std::string> ctorParamTypeNames;
        std::string parentType = "Object";
        std::vector<ASTNodePtr> parentArgs;
        std::vector<ASTNodePtr> members;
        bool explicitParentArgs = false;
        Type* type = nullptr;

    public:
        TypeDeclarationBuilder(const std::string& typeName) : name(typeName) {}

        TypeDeclarationBuilder& addCtorParam(const std::string& paramName, Type* type = nullptr, const std::string& typeName = "") {
            ctorParams.push_back(paramName);
            ctorParamTypes.push_back(type);
            ctorParamTypeNames.push_back(typeName);
            return *this;
        }

        TypeDeclarationBuilder& setParent(const std::string& parentName, const std::vector<ASTNodePtr>& arguments = {}) {
            parentType = parentName;
            parentArgs = arguments;
            explicitParentArgs = true;
            return *this;
        }

        TypeDeclarationBuilder& addAttribute(const std::string& attrName, ASTNodePtr init, Type* type = nullptr) {
            auto attr = new AttributeDeclNode(attrName, init, type);
            attr->type = type;
            members.push_back(attr);
            return *this;
        }

        TypeDeclarationBuilder& addMethod(FunctionDeclNode* methodNode) {
            methodNode->isMethod = true;
            methodNode->ownerTypeName = name;
            members.push_back(methodNode);
            return *this;
        }

        TypeDeclarationBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        TypeDeclNode* build() const {
            auto node = new TypeDeclNode(name, ctorParams, ctorParamTypes, ctorParamTypeNames, parentType, parentArgs, members, explicitParentArgs);
            node->type = type;
            return node;
        }
    };

    class ProtocolBuilder {
    private:
        std::string name;
        std::string extendedProtocol;
        std::vector<ASTNodePtr> methods;
        Type* type = nullptr;
    public:
        ProtocolBuilder(const std::string& n) : name(n) {}

        ProtocolBuilder& setParent(const std::string& parent) {
            extendedProtocol = parent;
            return *this;
        }

        ProtocolBuilder& addMethod(FunctionDeclNode* method) {
            method->isProtocolMethod = true;
            method->ownerProtocolName = name;
            methods.push_back(method);
            return *this;
        }

        ProtocolBuilder& setType(Type* t) {
            type = t;
            return *this;
        }

        ProtocolDeclNode* build() const {
            auto node = new ProtocolDeclNode(name, extendedProtocol, methods);
            node->type = type;
            return node;
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

    inline TypeDeclarationBuilder buildTypeDeclaration(const std::string& name) {
        return TypeDeclarationBuilder(name);
    }

    inline ProtocolBuilder buildProtocolDeclaration(const std::string& name) {
        return ProtocolBuilder(name);
    }
} // namespace ASTTestBuilder
