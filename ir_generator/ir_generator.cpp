#include "ir_generator.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
static std::string hulkTypeToQbeType(Type* type) {
    std::string typeStr="l";
    if(type){
       typeStr =type->toString();
    }
     
    if (typeStr == "bool") {
        return "b";
    } 
    return "l";
}

std::string IrGenerator::generate(ASTNode& node) {
    node.accept(*this);  
    return dataBuilder.toString() +
                            "\n\n"+ 
            codeBuilder.toString();
}

Type* IrGenerator::visit(ProgramNode& node) { 
    dataBuilder.addLine("data $msg = align 8 {{ b \"Result is %d \\n \", b 0}}");
    

    //Visit statemtns
    for (auto declaration: node.decls)
    {
        declaration->accept(*this);
    }

    codeBuilder
    .addLine("export function w $main() {{")
    .addLine("@start")
    .indent();
    
    for (auto statment : node.stmts)
    {
        statment->accept(*this);
    }

    codeBuilder
    .addLine("call $printf(l $msg,...,l %{})", nameForCurrentExpression)
    .addLine("call $_hulk_cleanup()")
    .addLine("ret 0")
    .unindent()
    .addLine("}}");
    return nullptr; 
}
Type* IrGenerator::visit(BlockNode& node) {
    scopeTable.enterScope();

    for(auto statement:node.stmts)
    {
        statement->accept(*this);   
    }

    std::string lastExpressionName = nameForCurrentExpression;

    nameForCurrentExpression = nameGenerator.generateName("block_result");
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
    codeBuilder.addLine("%{} = {} copy %{}",
                        nameForCurrentExpression,
                        typeForCurrentExpression,
                        lastExpressionName
                    );

    scopeTable.exitScope();

    return nullptr;
 }
Type* IrGenerator::visit(FunctionDeclNode& node) 
{ 
    codeBuilder.add("export function {} $_{}",hulkTypeToQbeType(node.returnType),node.name);
    codeBuilder.add("(");
    scopeTable.enterScope();

    std::string separator = "";
    for (int i = 0; i < node.params.size(); i++)
    {
        //To avoid conflict naming betwen QBE and hulk code.
        std::string paramName = nameGenerator.generateName("_"+node.params[i]);
        scopeTable.defineVariable(node.params[i], paramName);
        codeBuilder.add("{} {} %{}",
            separator ,
            hulkTypeToQbeType(node.paramTypes[i]),
            paramName
        );
        separator = ",";
    }
    codeBuilder.add(")");
    codeBuilder.addLine("{{");
    
    codeBuilder.addLine("@start");
    codeBuilder.indent();

    if (node.isInline && node.exprBody) 
    {
        node.exprBody->accept(*this);
    } else if (node.body) {
        node.body->accept(*this);
    }

    codeBuilder.addLine("ret %{}", nameForCurrentExpression);
    
    codeBuilder.unindent();
    codeBuilder.addLine("}}");
    codeBuilder.addLine();
    
    scopeTable.exitScope();
    return nullptr; 
}
Type* IrGenerator::visit(LetNode& node) { 
   
   scopeTable.enterScope();

   //Map the hulk variable name to a qbe register so it can be accesed later

   std::string registerName = nameGenerator.generateName("_"+node.name);

   scopeTable.defineVariable(node.name,registerName);

   node.init->accept(*this);

   codeBuilder.addLine("%{} = {} copy %{} ",
    registerName,
    typeForCurrentExpression,
    nameForCurrentExpression
    );

   node.body->accept(*this);
   
   std::string bodyResult = nameForCurrentExpression;
   typeForCurrentExpression = hulkTypeToQbeType(node.type);

   nameForCurrentExpression = nameGenerator.generateName("let_in_result");
   
   codeBuilder.addLine("%{} = {} copy %{}", 
                    nameForCurrentExpression,
                    typeForCurrentExpression,
                    bodyResult);
   scopeTable.exitScope();
   
   return nullptr;
}

Type* IrGenerator::visit(IfNode& node) {

    std::string ifResult = nameGenerator.generateName("if_result");

    node.condition->accept(*this);
    
    std::string thenLabel = nameGenerator.generateName("then");
    std::string elseBranchLabel = nameGenerator.generateName("else");
    std::string mergeLabel = nameGenerator.generateName("merge");


    codeBuilder.addLine("jnz %{},@{},@{}",nameForCurrentExpression,thenLabel,elseBranchLabel);
    codeBuilder.addLine();
    codeBuilder.addLine("@{}",thenLabel);
    codeBuilder.indent();
    
    
    //Build  then branch recursivly
    node.then_branch->accept(*this);

    scopeTable.enterScope();
    
  
    codeBuilder.addLine("%{} = {} copy %{}", 
                        ifResult,
                        typeForCurrentExpression,
                        nameForCurrentExpression
                        );

    codeBuilder.addLine("jmp @{}",mergeLabel);
    codeBuilder.unindent();
    codeBuilder.addLine("@{}",elseBranchLabel);
    codeBuilder.indent();

    scopeTable.exitScope();
    //Build else  branch recursivly (if there is one)
    if(node.else_branch)
    {
        scopeTable.enterScope();

        node.else_branch->accept(*this);
        
        codeBuilder.addLine("%{}= {} copy %{}",
                            ifResult,
                            typeForCurrentExpression,
                            nameForCurrentExpression);
        codeBuilder.unindent();
        
        scopeTable.exitScope();
    }
    codeBuilder.addLine("@{}",mergeLabel);
    

    nameForCurrentExpression = ifResult;
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
    return nullptr;
}
Type* IrGenerator::visit(FunctionCallNode& node) 
{
        //Evaluate the arguments of the function
        std::vector<std::string> argResultsNames; 
        std::vector<std::string> argResultsTypes;
        for (auto arg : node.args)
        {
            arg->accept(*this);
            argResultsNames.push_back(nameForCurrentExpression);
            argResultsTypes.push_back(typeForCurrentExpression);
        }
    

        nameForCurrentExpression = nameGenerator.generateName("call_result");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        //Note the underscore here to avoid collisions betwen ir code
        // and hulk code names
        codeBuilder.add("%{} = {} call $_{}",nameForCurrentExpression,
                                            typeForCurrentExpression,
                                            node.name);
        codeBuilder.add("(");

        std::string separator = "";
        
        for(int i=0; i<node.args.size(); i++)
        {
            codeBuilder.add("{} {} %{}",
                separator,
                argResultsTypes[i],
                argResultsNames[i]);
            separator = ",";
        }   
        
        codeBuilder.add(")");
        codeBuilder.addLine();
       
        return nullptr;
}
Type* IrGenerator::visit(VariableNode& node) {
    nameForCurrentExpression = scopeTable.resolveVariable(node.name);
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
    return nullptr;
}
Type* IrGenerator::visit(NumberNode& node) { 
    nameForCurrentExpression = nameGenerator.generateName("num_literal");
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
    codeBuilder.addLine("%{} ={} copy {}",
                        nameForCurrentExpression,
                        typeForCurrentExpression,
                        node.value
                    );
    return nullptr;
}
//TODO concatenation.
Type* IrGenerator::visit(BinaryOpNode& node) {
    
    node.left->accept(*this);
    std::string leftName=nameForCurrentExpression;
    node.right->accept(*this);
    std::string rightName=nameForCurrentExpression;

    if (node.op == "<") 
    {   
        nameForCurrentExpression = nameGenerator.generateName("less_than");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} cslt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         typeForCurrentExpression,
                                         targetInfo.PointerType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">") 
    {
        nameForCurrentExpression = nameGenerator.generateName("greater_than");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} csgt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         typeForCurrentExpression,
                                         targetInfo.PointerType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == "<=") 
    {
        nameForCurrentExpression = nameGenerator.generateName("less_equal");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} csle{} %{}, %{}",
                                         nameForCurrentExpression,
                                         typeForCurrentExpression,
                                         targetInfo.PointerType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">=") 
    {
        nameForCurrentExpression = nameGenerator.generateName("greater_equal");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} csge{} %{}, %{}",
                                         nameForCurrentExpression,
                                         typeForCurrentExpression,
                                         targetInfo.PointerType,
                                         leftName,
                                         rightName);
    }
    else if (node.op == "==") 
    {
        if(node.left->type!=node.right->type)
        {
            codeBuilder.addLine("%{} = {} copy 0", nameForCurrentExpression, typeForCurrentExpression);
        }
        else if(node.left->type==StringType::instance())
        {
            codeBuilder.addLine("%{} = {} call $_string_compair(l %{}, l %{})", 
                            nameForCurrentExpression, 
                            typeForCurrentExpression, 
                            leftName, 
                            rightName);
        }
        else 
        {
            nameForCurrentExpression = nameGenerator.generateName("equal");
            typeForCurrentExpression = hulkTypeToQbeType(node.type);
            codeBuilder.addLine("%{} = {} cse{} %{}, %{}", 
                nameForCurrentExpression,
                typeForCurrentExpression,
                targetInfo.PointerType,
                leftName,
                rightName);
        }
    } 
    
    else if (node.op == "@") 
    {   
        nameForCurrentExpression = nameGenerator.generateName("concat");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} call $_string_concat(l %{}, l %{})", 
                            nameForCurrentExpression, 
                            typeForCurrentExpression, 
                            leftName, 
                            rightName);
    } 
    else if (node.op == "+") 
    {
        nameForCurrentExpression = nameGenerator.generateName("addition");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} add %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "-") 
    {
        nameForCurrentExpression = nameGenerator.generateName("subtraction");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} sub %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    }
    else if (node.op == "*") 
    { 
        nameForCurrentExpression = nameGenerator.generateName("multiplication");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} mul %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "/") 
    {
        nameForCurrentExpression = nameGenerator.generateName("division");
        typeForCurrentExpression = hulkTypeToQbeType(node.type);
        codeBuilder.addLine("%{} = {} div %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    }
    
    return nullptr; 
}
Type* IrGenerator::visit(ExprStmtNode& node) { 
    node.expr->accept(*this);
    //The type and name are correct because recursion, we don't need to update them
    return nullptr;
 }


Type* IrGenerator::visit(StringNode& node) { 
    nameForCurrentExpression = stringPool.getOrCreateId(node.value);
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
                            //Size (int 32)  //the string  //seperator 
    dataBuilder.addLine("data ${} = align {} {{ w {}, b \"{}\", b 0}}",
                        nameForCurrentExpression,
                        targetInfo.StackAlign,
                        node.value.size(),
                        node.value);
   
    
    codeBuilder.addLine("%{} = {} copy ${}", nameForCurrentExpression, typeForCurrentExpression, nameForCurrentExpression);    
    
    return nullptr;
 }


Type* IrGenerator::visit(BoolNode& node) { 
    
    nameForCurrentExpression = nameGenerator.generateName("bool_literal");
    typeForCurrentExpression = hulkTypeToQbeType(node.type);
    codeBuilder.addLine("%{} = {} copy {}",
                        nameForCurrentExpression,
                        typeForCurrentExpression,
                        node.value?1:0);

    return nullptr;

}

Type* IrGenerator::visit(ReturnNode& node) {
    
    node.expr->accept(*this);
    codeBuilder.addLine("ret %{}", nameForCurrentExpression);
    return nullptr;
}
Type* IrGenerator::visit(UnaryOpNode& node) { 
    node.operand->accept(*this);
    std::string operandResult =nameForCurrentExpression;
    typeForCurrentExpression = hulkTypeToQbeType(node.type);    
    if(node.op=="-"){
        nameForCurrentExpression = nameGenerator.generateName("negation");
        codeBuilder.addLine("%{} = {} sub 0, %{}",
                            nameForCurrentExpression,
                            typeForCurrentExpression,
                            operandResult             
                ); 
    }
    return nullptr;
}
Type* IrGenerator::visit(TypeNode& node) { return nullptr; }
Type* IrGenerator::visit(ParamListNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingsNode& node) {return nullptr;}
