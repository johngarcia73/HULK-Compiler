#pragma once
#include "lowering.hpp"

namespace lowering {
    FunctionDeclNode* methodToFunctionLowering(const std::string& typeName, FunctionDeclNode* method) {
        if (!method) return nullptr;

        std::string newName = "_" + typeName + "_" + method->name;
        
        std::vector<std::string> newParams = method->params;
        newParams.push_back("self");

        std::vector<Type*> newParamTypes = method->paramTypes;
        newParamTypes.push_back(nullptr); // Using nullptr for the type as requested

        FunctionDeclNode* loweredFunc = nullptr;

        if (method->isInline) {
            loweredFunc = new FunctionDeclNode(newName, newParams, newParamTypes, method->returnType, method->exprBody, true);
        } else {
            loweredFunc = new FunctionDeclNode(newName, newParams, newParamTypes, method->returnType, method->body);
        }

        loweredFunc->paramTypeNames = method->paramTypeNames;
        loweredFunc->paramTypeNames.push_back(typeName); 

        return loweredFunc;
    }

    FunctionCallNode* methodCallToFunctionCallLowering(FunctionCallNode* methodCall) {
        if (!methodCall || !methodCall->receiver) return nullptr;

        std::string typeName = "Object";
        if (methodCall->receiver->type) {
            typeName = methodCall->receiver->type->toString();
        }
        std::vector<ASTNodePtr> newArgs = methodCall->args;
        // Method calling convention from methodToFunctionLowering appends self at the end
        newArgs.push_back(methodCall->receiver);

        auto lowered = new FunctionCallNode(methodCall->name, newArgs);
        lowered->type = methodCall->type;
        lowered->receiver = methodCall->receiver;
        return lowered;
    }
}
