#pragma once

#include <string>
#include "../parser/AST_Builder/ast_node.hpp"

namespace lowering {
    FunctionDeclNode* methodToFunctionLowering(const std::string& typeName, FunctionDeclNode* method);
    FunctionCallNode* methodCallToFunctionCallLowering(FunctionCallNode* methodCall);
}
