#include "ir_generator.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "lowering.hpp"
#include "type_utils.hpp"

std::string IrGenerator::generate(ASTNode& node) {
    TypeUtils::setTarget(this->targetInfo);
    node.accept(*this);  
    return dataBuilder.toString() +
                            "\n\n"+ 
            codeBuilder.toString();
}

// ############ PROGRAM ################################
Type* IrGenerator::visit(ProgramNode& node) { 
    //Visit statemtns
    for (auto declaration: node.decls)
    {
        declaration->accept(*this);
    }

    codeBuilder
    .addLine("export function w $main() {{")
    .addLine("@start")
    .indent()
    .addLine("#Initialize GC")
    .addLine("call $_hulk_gc_init()");
    for (auto statment : node.stmts)
    {
        statment->accept(*this);
    }
    codeBuilder 
    .addLine("ret 0")
    .unindent()
    .addLine("}}");
    return nullptr; 
}
//#######################################

//################ EXPRESSIONS AND STATEMENTS####################
Type* IrGenerator::visit(BlockNode& node) {
    scopeTable.enterScope();

    for(auto statement:node.stmts)
    {
        statement->accept(*this);   
    }

    std::string lastExpressionName = nameForCurrentExpression;

    nameForCurrentExpression = nameGenerator.generateName("block_result");
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    codeBuilder.addLine("%{} = {} copy %{}",
                        nameForCurrentExpression,
                        typeForCurrentExpression,
                        lastExpressionName
                    );

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
   typeForCurrentExpression = TypeUtils::toQbeType(node.type);

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
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    return nullptr;
}
Type* IrGenerator::visit(FunctionCallNode& node) 
{
    if (node.receiver) {
        auto loweredNode = lowering::methodCallToFunctionCallLowering(&node);
        return loweredNode->accept(*this);
    }
    if(node.name=="print")
    {
         //TODO use real type flags based on the actual type
        codeBuilder.addLine("#Handle print function call");
        
        node.args[0]->accept(*this);
        int typeFlag = TypeUtils::getRuntimeFlag(node.args[0]->type);

        std::string argType = typeForCurrentExpression;
        std::string argName = nameForCurrentExpression;

        std::string printArgStr = nameGenerator.generateName("print_arg_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            printArgStr,
                            targetInfo.PointerType,
                            argType,
                            argType,
                            argName,
                            typeFlag
                            );
        std::string printResult = nameGenerator.generateName("print_result");
        codeBuilder.addLine("%{} = {} call $_print({} %{})",
                            printResult,
                            targetInfo.PointerType,
                            targetInfo.PointerType,
                            printArgStr
                            );
        nameForCurrentExpression = printResult;
        typeForCurrentExpression =targetInfo.PointerType;
        if(node.args[0]->type!=StringType::instance())
        {
            nameForCurrentExpression = argName;
            typeForCurrentExpression = argType;   
        }
        return nullptr;
    }
    
     
    //Evaluate the arguments of the function
    std::vector<std::string> argResultsNames; 
    std::vector<std::string> argResultsTypes;
    for (auto arg : node.args)
    {
        arg->accept(*this);
        argResultsNames.push_back(nameForCurrentExpression);
        argResultsTypes.push_back(typeForCurrentExpression);
    }
    
    std::string callTarget = "_" + node.name;

    nameForCurrentExpression = nameGenerator.generateName("call_result");
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    //Note the underscore here to avoid collisions betwen ir code
    // and hulk code names
    codeBuilder.add("%{} = {} call ${}",nameForCurrentExpression,
                                        typeForCurrentExpression,
                                        callTarget);
    codeBuilder.add("(");

    std::string separator = "";
    
    for(int i=0; i < argResultsNames.size(); i++)
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
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    return nullptr;
}
Type* IrGenerator::visit(NumberNode& node) { 
    nameForCurrentExpression = nameGenerator.generateName("num_literal");
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    codeBuilder.addLine("%{} ={} copy {}_{}",
                        nameForCurrentExpression,
                        typeForCurrentExpression,
                        typeForCurrentExpression,
                        node.value
                    );
    return nullptr;
}
Type* IrGenerator::visit(BinaryOpNode& node) {
    
    node.left->accept(*this);
    std::string leftName=nameForCurrentExpression;
    std::string leftType=typeForCurrentExpression;
    node.right->accept(*this);
    std::string rightName=nameForCurrentExpression;
    std::string rightType=typeForCurrentExpression;

    if (node.op == "<") 
    {   
        nameForCurrentExpression = nameGenerator.generateName("less_than");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w clt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">") 
    {
        nameForCurrentExpression = nameGenerator.generateName("greater_than");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cgt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == "<=") 
    {
        nameForCurrentExpression = nameGenerator.generateName("less_equal");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cle{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">=") 
    {
        nameForCurrentExpression = nameGenerator.generateName("greater_equal");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cge{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    }
    else if (node.op == "==") 
    {
        nameForCurrentExpression = nameGenerator.generateName("equals");
        typeForCurrentExpression = "w";
        if(node.left->type!=node.right->type)
        {
            codeBuilder.addLine("%{} = w copy 0", nameForCurrentExpression);
        }
        else if(node.left->type==StringType::instance())
        {
            codeBuilder.addLine("%{} = w call $_string_compair({} %{}, {} %{})", 
                            nameForCurrentExpression, 
                            leftType, 
                            leftName, 
                            rightType,
                            rightName);
        }
        else 
        {
            codeBuilder.addLine("%{} = w ceq{} %{}, %{}", 
                nameForCurrentExpression,
                leftType,
                leftName,
                rightName);
        }
    } 
    
    else if (node.op == "@") 
    {   //Add code for conert to string
        int typeFlag = TypeUtils::getRuntimeFlag(node.left->type); 
        std::string leftStrName = nameGenerator.generateName("left_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            leftStrName,
                            targetInfo.PointerType,
                            leftType,
                            leftType,
                            leftName,
                            typeFlag
                            );
                            
        typeFlag = TypeUtils::getRuntimeFlag(node.right->type);                     
        std::string rightStrName = nameGenerator.generateName("right_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            rightStrName,
                            targetInfo.PointerType,
                            rightType,
                            rightType,
                            rightName,
                            typeFlag
                            );
        nameForCurrentExpression = nameGenerator.generateName("concat");
        typeForCurrentExpression = targetInfo.PointerType;
        codeBuilder.addLine("%{} = {} call $_string_concat({} %{}, {} %{})", 
                            nameForCurrentExpression, 
                            typeForCurrentExpression,
                            targetInfo.PointerType,
                            leftStrName,
                            targetInfo.PointerType, 
                            rightStrName);
    } 
    else if (node.op == "+") 
    {
        nameForCurrentExpression = nameGenerator.generateName("addition");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} add %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "-") 
    {
        nameForCurrentExpression = nameGenerator.generateName("subtraction");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} sub %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    }
    else if (node.op == "*") 
    { 
        nameForCurrentExpression = nameGenerator.generateName("multiplication");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} mul %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "/") 
    {
        nameForCurrentExpression = nameGenerator.generateName("division");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
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
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
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
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
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
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);    
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
Type* IrGenerator::visit(AssignmentNode& node) {
    node.value->accept(*this);
    std::string valueResult = nameForCurrentExpression;
    std::string valueType = TypeUtils::toQbeType(node.type);

    if (auto varNode = dynamic_cast<VariableNode*>(node.target)) {
        std::string variableRegister = scopeTable.resolveVariable(varNode->name);
        codeBuilder.addLine("%{} = {} copy %{}",
                            variableRegister,
                            valueType,
                            valueResult
                            );
    } 
    else if (auto memNode = dynamic_cast<MemberAccessNode*>(node.target)) {
        memNode->base->accept(*this);
        std::string baseRegister = nameForCurrentExpression;

        std::string typeName = "Object";
        if (memNode->base->type) {
            typeName = memNode->base->type->toString();
        }

        TypeInfo info = typeRegister.getInfo(typeName);
        int offset = info.getOffset(memNode->member);
        std::string wordSize = info.getWordSize(memNode->member);

        std::string fieldPtr = nameGenerator.generateName(baseRegister + "_" + memNode->member);
        codeBuilder.addLine("%{} = {} add %{}, {}",
                            fieldPtr,
                            targetInfo.PointerType,
                            baseRegister,
                            offset
                            );
        
        codeBuilder.addLine("store{} %{}, %{}",
                            wordSize,
                            valueResult,
                            fieldPtr
                            );
    }
    
    nameForCurrentExpression = valueResult;
    typeForCurrentExpression = valueType;
    return nullptr;
}
Type* IrGenerator::visit(WhileNode& node) {
    std::string conditionLabel = nameGenerator.generateName("loop_cond");
    std::string bodyLabel = nameGenerator.generateName("loop_body");
    std::string endLabel = nameGenerator.generateName("loop_end");
    
    //loop_cond
    codeBuilder.addLine("@{}", conditionLabel);
    codeBuilder.indent();
    node.condition->accept(*this);
    codeBuilder.addLine("jnz %{},@{},@{}",
                        nameForCurrentExpression,
                        bodyLabel,
                        endLabel
                    );
    codeBuilder.unindent();

    //loop_body
    codeBuilder.addLine("@{}", bodyLabel);
    codeBuilder.indent();
    scopeTable.enterScope();
    
    node.body->accept(*this);
    
    codeBuilder.addLine("jmp @{}", conditionLabel);
    scopeTable.exitScope();
    codeBuilder.unindent();

    //loop_end
    codeBuilder.addLine("@{}", endLabel);
    codeBuilder.indent();
   
    codeBuilder.addLine("#This label is used just for mark of the end loop");
    codeBuilder.addLine("#this comment is to make that clear");
   
    codeBuilder.unindent();
    //The result of a while loop the result of the last result of it's body.
    return nullptr;
    
}
Type* IrGenerator::visit(ForNode& node) {
    
    //TODO implement for loop,is missing the iterator
    //rigth now.
    return nullptr;
}
Type* IrGenerator::visit(NewNode& node) {
    TypeInfo info = typeRegister.getInfo(node.typeName);
    
    
    std::string objectRegister = nameGenerator.generateName(node.typeName+"_obj");
    codeBuilder.addLine("# Initialize and object of type {}", node.typeName);
    codeBuilder.addLine("%{} = {} call $_hulk_alloc(l {})",
                        objectRegister,
                        targetInfo.PointerType,
                        info.totalSize);
    scopeTable.enterScope();
    codeBuilder.addLine("#Evaluate constructor args expressions of type{}",node.typeName);
    for(int i=0; i<node.args.size(); i++)
    {
       node.args[i]->accept(*this); //Value of argument
       scopeTable.defineVariable(
                    info.declaration->ctorParams[i],
                    nameForCurrentExpression);

    }

    for(auto member: info.declaration->members){
        //if member is an attribute decleration we need to initialize it
        auto attr = dynamic_cast<AttributeDeclNode*>(member);
        if(!attr) continue;

        int offset = info.getOffset(attr->name);
        std::string wordSize = info.getWordSize(attr->name);
        codeBuilder.addLine("# Initialize field {}", attr->name);
        std::string fieldRegister = nameGenerator.generateName(node.typeName+"_obj_"+attr->name);
        
        codeBuilder.addLine("%{} = {} add %{}, {}",
                            fieldRegister,
                            targetInfo.PointerType,
                            objectRegister,
                            offset);
        attr->init->accept(*this);
        codeBuilder.addLine("store{} %{}, %{}",
                            wordSize,
                            nameForCurrentExpression,
                            fieldRegister
                        );
    }
    scopeTable.exitScope();
    nameForCurrentExpression = objectRegister;
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    return nullptr;
}
Type* IrGenerator::visit(MemberAccessNode& node) {
    std::string typeName = "Object";
    if (node.base->type) {
        typeName = node.base->type->toString();
    }
    
    TypeInfo info = typeRegister.getInfo(typeName);

    node.base->accept(*this);
    std::string fieldPointer = nameGenerator.generateName(
                                    nameForCurrentExpression+"_"+node.member);
    codeBuilder.addLine("%{} = {} add %{}, {}",
                            fieldPointer,
                            typeForCurrentExpression,
                            nameForCurrentExpression,
                            info.getOffset(node.member)
                        );
    std::string fieldValue = nameGenerator.generateName("field_value");
    codeBuilder.addLine("%{} = {} load{} %{}",
                            fieldValue,
                            info.getWordSize(node.member),
                            info.getWordSize(node.member),
                            fieldPointer
                        );
    nameForCurrentExpression = fieldValue;
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    return nullptr;
}

//################# DECLARATIONS #######################
Type* IrGenerator::visit(ProtocolDeclNode& node) {
    //TODO implement protocol declaration
    return nullptr; 
}

Type* IrGenerator::visit(FunctionDeclNode& node) 
{ 
    if(node.isMethod){
        auto loweredNode = lowering::methodToFunctionLowering(node.ownerTypeName, &node);
        return loweredNode->accept(*this);
    }
    codeBuilder.add("export function {} $_{}",TypeUtils::toQbeType(node.returnType),node.name);
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
            TypeUtils::toQbeType(node.paramTypes[i]),
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
    if(node.returnType == VoidType::instance())
    {
        codeBuilder.addLine("ret 0");
    }
    else
    {
        codeBuilder.addLine("ret %{}", nameForCurrentExpression);
    }
    codeBuilder.unindent();
    codeBuilder.addLine("}}");
    codeBuilder.addLine();
    
    scopeTable.exitScope();
    return nullptr; 
}
Type* IrGenerator::visit(AttributeDeclNode& node) {
    //Already handled  TypeDeclNode when it registers a type declaration
    //The type is the same as the init
    return nullptr; 

}
Type* IrGenerator::visit(TypeDeclNode &node) {
    typeRegister.registerFromDeclaration(&node);
    codeBuilder.addLine("#Functions of type {}",node.name);
    for  (auto member: node.members)
    {
        //Attributes info is stored on the typeRegister,this 
        //accept will call a FunctionDeclarationNode wich will be lowered
        //will lower the functions
        member->accept(*this);
    }
    return nullptr;
}
//###########################################################################

//################# AUX ######################################################
//TODO agree if posible to delete this types if not visitor use them.
Type* IrGenerator::visit(TypeNode& node) { return nullptr; }
Type* IrGenerator::visit(ParamListNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingsNode& node) {return nullptr;}
//###############################################################################
