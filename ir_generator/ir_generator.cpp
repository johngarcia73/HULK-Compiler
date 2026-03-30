// ir_generator.cpp
#include "ir_generator.hpp"
#include <sstream>
#include <algorithm>

std::string IRGenerator::getStringLabel(const std::string& str) {
    auto it = stringLabels.find(str);
    if (it != stringLabels.end()) return it->second;
    std::string label = "s" + std::to_string(stringCounter++);
    stringLabels[str] = label;
    return label;
}

void IRGenerator::collectStrings(ASTNode* node) {
    if (auto* n = dynamic_cast<StringNode*>(node)) {
        getStringLabel(n->value);
    } else if (auto* n = dynamic_cast<BinaryOpNode*>(node)) {
        collectStrings(n->left);
        collectStrings(n->right);
    } else if (auto* n = dynamic_cast<UnaryOpNode*>(node)) {
        collectStrings(n->operand);
    } else if (auto* n = dynamic_cast<ExprStmtNode*>(node)) {
        collectStrings(n->expr);
    } else if (auto* n = dynamic_cast<BlockNode*>(node)) {
        for (auto* s : n->stmts) collectStrings(s);
    } else if (auto* n = dynamic_cast<LetNode*>(node)) {
        collectStrings(n->init);
        collectStrings(n->body);
    } else if (auto* n = dynamic_cast<IfNode*>(node)) {
        collectStrings(n->condition);
        collectStrings(n->then_branch);
        collectStrings(n->else_branch);
    } else if (auto* n = dynamic_cast<FunctionCallNode*>(node)) {
        for (auto* a : n->args) collectStrings(a);
    } else if (auto* n = dynamic_cast<ProgramNode*>(node)) {
        for (auto* d : n->decls) collectStrings(d);
        for (auto* s : n->stmts) collectStrings(s);
    } else if (auto* n = dynamic_cast<FunctionDeclNode*>(node)) {
        collectStrings(n->body);
    }
}

std::string IRGenerator::generate(ProgramNode* root) {
    collectStrings(root);

    std::vector<FunctionData> functions;
    for (auto* decl : root->decls) {
        if (auto* f = dynamic_cast<FunctionDeclNode*>(decl)) {
            functions.push_back(generateFunction(f));
        }
    }
    functions.push_back(generateEntry(root->stmts));

    std::ostringstream out;
    out << ".TYPES\n\n.DATA\n";
    for (const auto& [str, label] : stringLabels) {
        out << label << " = \"" << str << "\";\n";
    }
    out << "\n.CODE\n";
    for (const auto& func : functions) {
        out << "function " << func.name << " {\n";
        for (const auto& p : func.params) {
            out << "    PARAM " << p << ";\n";
        }
        for (const auto& l : func.locals) {
            out << "    LOCAL " << l << ";\n";
        }
        for (const auto& instr : func.instructions) {
            out << "    " << instr << "\n";
        }
        out << "}\n\n";
    }
    return out.str();
}

IRGenerator::FunctionData IRGenerator::generateFunction(FunctionDeclNode* node) {
    FunctionData data;
    data.name = node->name;
    data.params = node->params;

    CodeGenContext ctx;
    ctx.pushScope();
    for (size_t i = 0; i < node->params.size(); ++i) {
        ctx.addMapping(node->params[i], node->params[i], true);
    }
    std::string result = generateExpr(node->body, ctx);
    ctx.instructions.push_back("RETURN " + result + ";");
    data.locals.assign(ctx.locals.begin(), ctx.locals.end());
    data.instructions = std::move(ctx.instructions);
    ctx.popScope();
    return data;
}

IRGenerator::FunctionData IRGenerator::generateEntry(const std::vector<ASTNodePtr>& stmts) {
    FunctionData data;
    data.name = "entry";

    CodeGenContext ctx;
    ctx.pushScope();
    for (auto* stmt : stmts) {
        generateExpr(stmt, ctx);
    }
    ctx.instructions.push_back("RETURN 0;");
    data.locals.assign(ctx.locals.begin(), ctx.locals.end());
    data.instructions = std::move(ctx.instructions);
    ctx.popScope();
    return data;
}

std::string IRGenerator::generateExpr(ASTNode* node, CodeGenContext& ctx) {
    if (auto* n = dynamic_cast<NumberNode*>(node)) return generateNumberNode(n, ctx);
    if (auto* n = dynamic_cast<BoolNode*>(node)) return generateBoolNode(n, ctx);
    if (auto* n = dynamic_cast<StringNode*>(node)) return generateStringNode(n, ctx);
    if (auto* n = dynamic_cast<VariableNode*>(node)) return generateVariableNode(n, ctx);
    if (auto* n = dynamic_cast<BinaryOpNode*>(node)) return generateBinaryOpNode(n, ctx);
    if (auto* n = dynamic_cast<UnaryOpNode*>(node)) return generateUnaryOpNode(n, ctx);
    if (auto* n = dynamic_cast<LetNode*>(node)) return generateLetNode(n, ctx);
    if (auto* n = dynamic_cast<IfNode*>(node)) return generateIfNode(n, ctx);
    if (auto* n = dynamic_cast<FunctionCallNode*>(node)) return generateFunctionCallNode(n, ctx);
    if (auto* n = dynamic_cast<BlockNode*>(node)) return generateBlockNode(n, ctx);
    if (auto* n = dynamic_cast<ExprStmtNode*>(node)) return generateExprStmtNode(n, ctx);
    return "0";
}

std::string IRGenerator::generateNumberNode(NumberNode* node, CodeGenContext& ctx) {
    return std::to_string(node->value);
}

std::string IRGenerator::generateBoolNode(BoolNode* node, CodeGenContext& ctx) {
    return node->value ? "1" : "0";
}

std::string IRGenerator::generateStringNode(StringNode* node, CodeGenContext& ctx) {
    std::string label = getStringLabel(node->value);
    std::string temp = ctx.newTemp();
    ctx.locals.insert(temp);
    ctx.instructions.push_back(temp + " = LOAD " + label + ";");
    return temp;
}

std::string IRGenerator::generateVariableNode(VariableNode* node, CodeGenContext& ctx) {
    return ctx.lookup(node->name);
}

std::string IRGenerator::generateBinaryOpNode(BinaryOpNode* node, CodeGenContext& ctx) {
    std::string left = generateExpr(node->left, ctx);
    std::string right = generateExpr(node->right, ctx);
    std::string dest = ctx.newTemp();
    ctx.locals.insert(dest);

    if (node->op == "+" || node->op == "-" || node->op == "*" || node->op == "/") {
        ctx.instructions.push_back(dest + " = " + left + " " + node->op + " " + right + ";");
    } else if (node->op == "<") {
        ctx.instructions.push_back("LT " + dest + ", " + left + ", " + right + ";");
    } else if (node->op == ">") {
        ctx.instructions.push_back("GT " + dest + ", " + left + ", " + right + ";");
    } else if (node->op == "<=") {
        ctx.instructions.push_back("LE " + dest + ", " + left + ", " + right + ";");
    } else if (node->op == ">=") {
        ctx.instructions.push_back("GE " + dest + ", " + left + ", " + right + ";");
    } else if (node->op == "==") {
        ctx.instructions.push_back("EQ " + dest + ", " + left + ", " + right + ";");
    } else if (node->op == "@") {
        ctx.instructions.push_back("PARAM " + left + ";");
        ctx.instructions.push_back("PARAM " + right + ";");
        ctx.instructions.push_back(dest + " = CALL _concat;");
    }
    return dest;
}

std::string IRGenerator::generateUnaryOpNode(UnaryOpNode* node, CodeGenContext& ctx) {
    std::string operand = generateExpr(node->operand, ctx);
    std::string dest = ctx.newTemp();
    ctx.locals.insert(dest);
    if (node->op == "-") {
        ctx.instructions.push_back(dest + " = -" + operand + ";");
    } else if (node->op == "!") {
        ctx.instructions.push_back("NOT " + dest + ", " + operand + ";");
    }
    return dest;
}

std::string IRGenerator::generateLetNode(LetNode* node, CodeGenContext& ctx) {
    std::string init_val = generateExpr(node->init, ctx);
    std::string var_name = ctx.newVar(node->name);
    ctx.pushScope();
    ctx.addMapping(node->name, var_name, false);
    ctx.instructions.push_back(var_name + " = " + init_val + ";");
    ctx.locals.insert(var_name);
    std::string body_result = generateExpr(node->body, ctx);
    ctx.popScope();
    return body_result;
}

std::string IRGenerator::generateIfNode(IfNode* node, CodeGenContext& ctx) {
    std::string cond = generateExpr(node->condition, ctx);
    std::string result = ctx.newTemp();
    ctx.locals.insert(result);
    std::string then_label = ctx.newLabel();
    std::string else_label = ctx.newLabel();
    std::string merge_label = ctx.newLabel();

    ctx.instructions.push_back("IF " + cond + " GOTO " + then_label + ";");
    ctx.instructions.push_back("GOTO " + else_label + ";");
    ctx.instructions.push_back("LABEL " + then_label + ";");
    std::string then_val = generateExpr(node->then_branch, ctx);
    ctx.instructions.push_back(result + " = " + then_val + ";");
    ctx.instructions.push_back("GOTO " + merge_label + ";");
    ctx.instructions.push_back("LABEL " + else_label + ";");
    std::string else_val = generateExpr(node->else_branch, ctx);
    ctx.instructions.push_back(result + " = " + else_val + ";");
    ctx.instructions.push_back("LABEL " + merge_label + ";");
    return result;
}

std::string IRGenerator::generateFunctionCallNode(FunctionCallNode* node, CodeGenContext& ctx) {
    for (auto* arg : node->args) {
        std::string arg_val = generateExpr(arg, ctx);
        ctx.instructions.push_back("PARAM " + arg_val + ";");
    }
    std::string dest = ctx.newTemp();
    ctx.locals.insert(dest);
    ctx.instructions.push_back(dest + " = CALL " + node->name + ";");
    return dest;
}

std::string IRGenerator::generateBlockNode(BlockNode* node, CodeGenContext& ctx) {
    std::string last = "0";
    for (auto* stmt : node->stmts) {
        last = generateExpr(stmt, ctx);
    }
    return last;
}

std::string IRGenerator::generateExprStmtNode(ExprStmtNode* node, CodeGenContext& ctx) {
    return generateExpr(node->expr, ctx);
}