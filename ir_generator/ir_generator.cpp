#include "ir_generator.hpp"
#include "../parser/AST_Builder/ast_node.hpp"
#include "lowering.hpp"
#include "type_utils.hpp"
#include <typeinfo>
std::string IrGenerator::generate(ASTNodePtr node) {
    TypeUtils::setTarget(this->targetInfo);
    node->accept(*this);  
    return dataBuilder.toString() +
                            "\n\n"+ 
            codeBuilder.toString();
}

// ############ PROGRAM ################################
Type* IrGenerator::visit(ProgramNode& node) { 
    //Visit statemtns
    dataBuilder.addLine("data $Object = align {} {{b 0}}",targetInfo.PointerSize);
    
    for (auto declaration: node.decls)
    {
        declaration->accept(*this);
    }
    codeBuilder.addLine("export function w $main() {{")
    .addLine("@start")
    .indent()
    .addLine("#Initialize GC")
    .addLine("call $_hulk_gc_init()")
    .addLine("call $_initialize_vtables()")
    .addLine("#Register class methods on VTABLES");

    // Emit all vtable registrations here, inside $main, where it is valid IR.
    for (auto declaration : node.decls)
    {
        auto typeDecl = dynamic_cast<TypeDeclNode*>(declaration);
        if (!typeDecl) continue;

        // Register inheritance chain.
        codeBuilder.addLine("call $_register_inheritance(l ${},l ${})",
                            typeDecl->name,
                            typeDecl->parentType);

        // Register each method on the vtable.
        for (auto member : typeDecl->members)
        {
            auto method = dynamic_cast<FunctionDeclNode*>(member);
            if (!method || !method->isMethod) continue;
            // Ensure the method-name symbol exists in the data section.
            if (!nameManager.exists(method->name)) {
                nameManager.add(method->name);
                dataBuilder.addLine("data ${} = align {} {{b 0}}", method->name, targetInfo.PointerSize);
            }
            // Compute the lowered function name (same rule used in FunctionDeclNode visit).
            std::string loweredName = method->ownerTypeName + "_" + method->name;
            codeBuilder.addLine("call $_register_method(l ${},l ${},l $__{})",
                                method->ownerTypeName,
                                method->name,
                                loweredName
                            );
        }
    }

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

    nameForCurrentExpression = nameManager.generateName("block_result");
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

   std::string registerName = nameManager.generateName("_"+node.name);

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

   nameForCurrentExpression = nameManager.generateName("let_in_result");
   
   codeBuilder.addLine("%{} = {} copy %{}", 
                    nameForCurrentExpression,
                    typeForCurrentExpression,
                    bodyResult);
   scopeTable.exitScope();
   
   return nullptr;
}
Type* IrGenerator::visit(IfNode& node) {
    std::string ifResult = nameManager.generateName("if_result");

    node.condition->accept(*this);
    
    std::string thenLabel = nameManager.generateName("then");
    std::string elseBranchLabel = nameManager.generateName("else");
    std::string mergeLabel = nameManager.generateName("merge");


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
Type* IrGenerator::visit(FunctionCallNode &node) 
{
    if(node.name=="print")
    {
        codeBuilder.addLine("#Handle print function call");
        
        node.args[0]->accept(*this);
        int typeFlag = TypeUtils::getRuntimeFlag(node.args[0]->type);

        std::string argType = typeForCurrentExpression;
        std::string argName = nameForCurrentExpression;

        std::string printArgStr = nameManager.generateName("print_arg_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            printArgStr,
                            targetInfo.PointerType,
                            argType,
                            argType,
                            argName,
                            typeFlag
                            );
        std::string printResult = nameManager.generateName("print_result");
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
    
    //Note the underscore here to avoid collisions betwen ir code
    // and hulk code names
    std::string callTarget = "$_" + node.name;
    if (node.receiver) {
        node = *lowering::methodCallToFunctionCallLowering(&node);
        node.receiver->accept(*this);
        std::string classId = nameManager.generateName("class_id");
        codeBuilder.addLine("%{} ={} load{} %{}",
                                classId,
                                targetInfo.PointerType,
                                targetInfo.PointerType,
                                nameForCurrentExpression
                            );
        std::string function_ptr=nameManager.generateName(node.name+"_ptr"); 
        codeBuilder.addLine("%{} = l call $_get_virtual_method(l %{},l ${})",
                                    function_ptr,
                                    classId,
                                    node.name
                            );

        callTarget = "%"+function_ptr; 
        
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
    
    nameForCurrentExpression = nameManager.generateName("call_result");
    typeForCurrentExpression = TypeUtils::toQbeType(node.type);
    
    codeBuilder.add("%{} = {} call {}",nameForCurrentExpression,
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
    nameForCurrentExpression = nameManager.generateName("num_literal");
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
        nameForCurrentExpression = nameManager.generateName("less_than");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w clt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">") 
    {
        nameForCurrentExpression = nameManager.generateName("greater_than");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cgt{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == "<=") 
    {
        nameForCurrentExpression = nameManager.generateName("less_equal");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cle{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    } 
    else if (node.op == ">=") 
    {
        nameForCurrentExpression = nameManager.generateName("greater_equal");
        typeForCurrentExpression = "w";
        codeBuilder.addLine("%{} = w cge{} %{}, %{}",
                                         nameForCurrentExpression,
                                         leftType,
                                         leftName,
                                         rightName);
    }
    else if (node.op == "==") 
    {
        nameForCurrentExpression = nameManager.generateName("equals");
        typeForCurrentExpression = "w";
        if(node.right->type && node.left->type && typeid(*node.left->type)!=typeid(*node.right->type))
        {
            codeBuilder.addLine("%{} = w copy 0", nameForCurrentExpression);
        }
        else if(dynamic_cast<StringType*>(node.left->type))
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
        std::string leftStrName = nameManager.generateName("left_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            leftStrName,
                            targetInfo.PointerType,
                            leftType,
                            leftType,
                            leftName,
                            typeFlag
                            );
                            
        typeFlag = TypeUtils::getRuntimeFlag(node.right->type);                     
        std::string rightStrName = nameManager.generateName("right_str");
        codeBuilder.addLine("%{} = {} call $_{}_to_string({} %{},w {})",
                            rightStrName,
                            targetInfo.PointerType,
                            rightType,
                            rightType,
                            rightName,
                            typeFlag
                            );
        nameForCurrentExpression = nameManager.generateName("concat");
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
        nameForCurrentExpression = nameManager.generateName("addition");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} add %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "-") 
    {
        nameForCurrentExpression = nameManager.generateName("subtraction");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} sub %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    }
    else if (node.op == "*") 
    { 
        nameForCurrentExpression = nameManager.generateName("multiplication");
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
        codeBuilder.addLine("%{} = {} mul %{}, %{}", nameForCurrentExpression, typeForCurrentExpression, leftName, rightName);
    } 
    else if (node.op == "/") 
    {
        nameForCurrentExpression = nameManager.generateName("division");
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
    
    if(!stringPool.contains(node.value))
    {   
        nameForCurrentExpression = stringPool.getOrCreateId(node.value);
        typeForCurrentExpression = TypeUtils::toQbeType(node.type);
                                //Size (int 32)  //the string  //seperator 
        dataBuilder.addLine("data ${} = align {} {{ w {}, b \"{}\", b 0}}",
                            nameForCurrentExpression,
                            targetInfo.StackAlign,
                            node.value.size(),
                            node.value);
    }
    nameForCurrentExpression = stringPool.getOrCreateId(node.value);
    typeForCurrentExpression=targetInfo.PointerType;
    codeBuilder.addLine("%{} = {} copy ${}", 
                            nameForCurrentExpression,
                            typeForCurrentExpression, 
                            nameForCurrentExpression);    
    
    return nullptr;
 }
Type* IrGenerator::visit(BoolNode& node) { 
    
    nameForCurrentExpression = nameManager.generateName("bool_literal");
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
        nameForCurrentExpression = nameManager.generateName("negation");
        codeBuilder.addLine("%{} = {} sub 0, %{}",
                            nameForCurrentExpression,
                            typeForCurrentExpression,
                            operandResult             
                ); 
    }
    else if(node.op=="!"){
        nameForCurrentExpression = nameManager.generateName("not");
        codeBuilder.addLine("%{} = {} xor 1, %{}",
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

        
        int offset = typeRegister.getOffset(typeName,memNode->member);
        std::string wordSize = typeRegister.getWordSize(typeName,memNode->member);

        std::string fieldPtr = nameManager.generateName(baseRegister + "_" + memNode->member);
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
    std::string conditionLabel = nameManager.generateName("loop_cond");
    std::string bodyLabel = nameManager.generateName("loop_body");
    std::string endLabel = nameManager.generateName("loop_end");
    
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
   
    //Allocate object in memory, with name objectRegister
    std::string objectRegister = nameManager.generateName(node.typeName+"_obj");
    codeBuilder.addLine("# Initialize and object of type {}", node.typeName);
    codeBuilder.addLine("%{} = {} call $_hulk_alloc(l {})",
                        objectRegister,
                        targetInfo.PointerType,
                        typeRegister.totalSize(node.typeName)
                    );
    //Set the type name on the first offset of the object
    codeBuilder.addLine("# Set type name on the first offset for the object");
    codeBuilder.addLine("store{} ${}, %{}",
                        targetInfo.PointerType,
                        node.typeName,
                        objectRegister
                        );  
    //Initialize offsets recursivly until we reach to the Object type
    auto constructorArgs =node.args;
    auto currentType = node.typeName;
    while(currentType!="Object")
    {
        //Evaluate constructor arguments on a same scope 
        scopeTable.enterScope();
        codeBuilder.addLine("#Evaluate constructor args expressions of type{}",currentType);

        std::vector<std::string> evaluatedArgsNames; 
        for(int i=0; i<constructorArgs.size(); i++)
        {
            constructorArgs[i]->accept(*this); //Value of argument
            evaluatedArgsNames.push_back(nameForCurrentExpression);
        }

        for (int i=0;i<evaluatedArgsNames.size();i++)
        {
            scopeTable.defineVariable(
                        typeRegister.getDeclaration(currentType)->ctorParams[i],
                        evaluatedArgsNames[i]
            );
        }
        //Initialize attribues with that values
        for(auto member: typeRegister.getDeclaration(currentType)->members){
            //if member is an attribute decleration we need to initialize it
            auto attr = dynamic_cast<AttributeDeclNode*>(member);
            if(!attr) continue;

            int offset = typeRegister.getOffset(currentType,attr->name);
            std::string wordSize = typeRegister.getWordSize(currentType,attr->name);
            codeBuilder.addLine("# Initialize field {}", attr->name);
            std::string fieldRegister = nameManager.generateName(currentType+"_obj_"+attr->name);
        
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

        //get parent info and repeat the process
        constructorArgs = typeRegister.getDeclaration(currentType)->parentArgs; 
        currentType = typeRegister.getDeclaration(currentType)->parentType;
           
    }
    currentType = node.typeName;
    while(currentType!="Object")
    {
        scopeTable.exitScope();
        currentType = typeRegister.getDeclaration(currentType)->parentType;
    }
    nameForCurrentExpression = objectRegister;
    typeForCurrentExpression = targetInfo.PointerType;
    return nullptr;
}
Type* IrGenerator::visit(MemberAccessNode& node) {
    std::string typeName = "Object";
    if (node.base->type) {
        typeName = node.base->type->toString();
    }
    
    
    node.base->accept(*this);
    std::string fieldPointer = nameManager.generateName(
                                    nameForCurrentExpression+"_"+node.member);
    codeBuilder.addLine("%{} = {} add %{}, {}",
                            fieldPointer,
                            typeForCurrentExpression,
                            nameForCurrentExpression,
                            typeRegister.getOffset(typeName,node.member)
                        );
    std::string fieldValue = nameManager.generateName("field_value");
    codeBuilder.addLine("%{} = {} load{} %{}",
                            fieldValue,
                            typeRegister.getWordSize(typeName,node.member),
                            typeRegister.getWordSize(typeName,node.member),
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
        // Only emit the lowered function definition here.
        // The _register_method call is emitted inside $main (visit(ProgramNode))
        // so it lands in valid IR context, not at the top level.
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
        std::string paramName = nameManager.generateName("_"+node.params[i]);
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
    // The type-name symbol must exist before $main runs so that the
    // _register_inheritance / _register_method calls in $main can reference it.
    dataBuilder.addLine("data ${} = align {} {{b 0}}",node.name,targetInfo.PointerSize);
    codeBuilder.addLine("#Functions of type {}",node.name);
    // NOTE: _register_inheritance and _register_method calls have been moved to
    // visit(ProgramNode) so they are emitted inside $main (valid IR context).
    for (auto member: node.members)
    {
        //Attributes info is stored on the typeRegister, this
        //accept will call a FunctionDeclarationNode which will be lowered.
        member->accept(*this);
    }
    return nullptr;
}
//###########################################################################

//#####TODOS####################################################
Type *IrGenerator::visit(LambdaNode &node)
{
    return nullptr;
}
Type *IrGenerator::visit(VectorLiteralNode &node)
{
    return nullptr;
}
Type *IrGenerator::visit(VectorComprehensionNode &node)
{
    return nullptr;
}
Type *IrGenerator::visit(IndexAccessNode &node)
{
    return nullptr;
}


//###############################################################

//################# AUX ######################################################
//TODO agree if posible to delete this types if not visitor use them.
Type* IrGenerator::visit(TypeNode& node) { return nullptr; }
Type* IrGenerator::visit(ParamListNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingNode& node) { return nullptr; }
Type* IrGenerator::visit(LetBindingsNode& node) {return nullptr;}
//###############################################################################