#include "lowering.hpp"


Type* LoweringVisitor::visit(ProgramNode &node) {
    for(auto& decl :node.decls){
        parentReference =&decl;
        decl->accept(*this);
    } 
    for(auto& stmt: node.stmts){
        parentReference = &stmt;
        stmt->accept(*this);
    }
    for(auto& typeDecl : generatedTypes){
        node.decls.insert(node.decls.begin(), typeDecl); 
    }
    generatedTypes.clear();
    return nullptr;
}

Type* LoweringVisitor::visit(BlockNode& node) {
    scopeTable.enterScope();
    for (auto& stmt : node.stmts) {
        parentReference = &stmt;
        stmt->accept(*this);
    }
    scopeTable.exitScope();
    return nullptr;
}

Type* LoweringVisitor::visit(FunctionDeclNode& node) {
    scopeTable.enterScope();
    for (size_t i = 0; i < node.params.size(); i++)
    {
        scopeTable.saveVariable(node.params[i],node.paramTypes[i]);
    }
    
    auto parentRefCopy = parentReference;
    if (node.body) {
        parentReference = &node.body;
        node.body->accept(*this);
    }
    if(node.exprBody){
        parentReference = &node.exprBody;
        node.exprBody->accept(*this);
    }
    scopeTable.exitScope();
    if(!node.isInline){
        auto funcDeclNode = new FunctionDeclNode(node.name,node.params, node.paramTypes,
                     node.returnType, node.body, true);
        funcDeclNode->isMethod = node.isMethod;
        funcDeclNode->ownerTypeName = node.ownerTypeName;
        funcDeclNode->type = node.type;
        *parentRefCopy = funcDeclNode;
    }
    
    return nullptr;
}

Type* LoweringVisitor::visit(LetNode& node) {
    
    if (node.init) {
        parentReference = &node.init;
        node.init->accept(*this);
    }
    scopeTable.enterScope();
    scopeTable.saveVariable(node.name,node.init->type);
    if (node.body) {
        parentReference = &node.body;
        node.body->accept(*this);
    }
    scopeTable.exitScope();
    return nullptr;
}

Type* LoweringVisitor::visit(IfNode& node) {
    
    if (node.condition) {
        parentReference = &node.condition;
        node.condition->accept(*this);
    }
    scopeTable.enterScope();
    if (node.then_branch) {
        parentReference = &node.then_branch;
        node.then_branch->accept(*this);
    }
    scopeTable.exitScope();

    scopeTable.enterScope();
    if (node.else_branch) {
        parentReference = &node.else_branch;
        node.else_branch->accept(*this);
    }
    scopeTable.exitScope();
    return nullptr;
}

Type* LoweringVisitor::visit(FunctionCallNode& node) {
    auto parentRefCopy = parentReference;
    if (node.receiver) {
        parentReference = &node.receiver;
        node.receiver->accept(*this);
    }
    for (auto& arg : node.args) {
        parentReference = &arg;
        arg->accept(*this);
    }
    
    bool isDelegateCall = false;
    for (const auto& note : node.overloadNotes) {
        if (note.find("Selected functor value:") != std::string::npos) {
            isDelegateCall = true;
            break;
        }
    }
    
    if (isDelegateCall && !node.receiver) {
        auto delegateInstance = new VariableNode(node.name);
        delegateInstance->type = node.resolvedFunctionType;
        auto invokeCall = new FunctionCallNode("invoke", node.args, delegateInstance);
        invokeCall->type = node.type;
        invokeCall->span = node.span;
        *parentRefCopy = invokeCall;
    }
    
    return nullptr;
}

Type* LoweringVisitor::visit(VariableNode& node) {
    return nullptr; 
}

Type* LoweringVisitor::visit(NumberNode& node) { 
    return nullptr; 
}

Type* LoweringVisitor::visit(BinaryOpNode& node) {
    auto parentRefCopy = parentReference;
    
    if (node.left) {
        parentReference = &node.left;
        node.left->accept(*this);
    }
    if (node.right) {
        parentReference = &node.right;
        node.right->accept(*this);
    }
    
    if(node.op=="^"){
        std::vector<ASTNodePtr> args;
        args.push_back(node.left);
        args.push_back(node.right);
        auto loweredNode = new FunctionCallNode("pow",args,nullptr);
        loweredNode->type = NumberType::instance();
        *parentRefCopy = loweredNode;
        return nullptr;
    }
    if (node.op=="%"){
        std::vector<ASTNodePtr> args;
        args.push_back(node.left);
        args.push_back(node.right);
        auto loweredNode = new FunctionCallNode("mod",args,nullptr);
        loweredNode->type = NumberType::instance();
        *parentRefCopy = loweredNode;
        return nullptr;
    }
    if(node.op=="!="){
        auto equalsNode = new BinaryOpNode("==",node.left,node.right);
        equalsNode->type = BoolType::instance();
        auto notNode = new UnaryOpNode("!",equalsNode);
        notNode->type = BoolType::instance();
        *parentRefCopy = notNode;
        return nullptr;
    }
    if(node.op=="@@"){
        auto blankLiteral = new StringNode(" ");
        blankLiteral->type = StringType::instance();
        auto leftWithBlank = new BinaryOpNode("@",node.left,blankLiteral);
        leftWithBlank->type = StringType::instance();
        auto leftWithBlankRigth= new BinaryOpNode("@",leftWithBlank, node.right);
        leftWithBlankRigth->type = StringType::instance();
        *parentRefCopy = leftWithBlankRigth;
        return nullptr;        
    }
    return nullptr;
}

Type* LoweringVisitor::visit(ExprStmtNode& node) {
    if (node.expr) {
        parentReference = &node.expr;
        node.expr->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(AssignmentNode& node) {
    if (node.target) {
        parentReference = &node.target;
        node.target->accept(*this);
    }
    if (node.value) {
        parentReference = &node.value;
        node.value->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(MemberAccessNode& node) {
    if (node.base) {
        parentReference = &node.base;
        node.base->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(NewNode& node) {
    for (auto& arg : node.args) {
        parentReference = &arg;
        arg->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(WhileNode& node) {
    if (node.condition) {
        parentReference = &node.condition;
        node.condition->accept(*this);
    }
    scopeTable.enterScope();
    if (node.body) {
        parentReference = &node.body;
        node.body->accept(*this);
    }
    scopeTable.exitScope();
    return nullptr;
}

Type* LoweringVisitor::visit(ForNode& node) {
    // LOWERING SUGGESTION 1: For-Loops to While-Loops
    // A For loop (e.g., `for (x in iterable) body`) can be lowered into a while loop:
    // `let it = iterable.iterator() in while (it.next()) { let x = it.current() in body }`
    // This allows the IR generator to only worry about implementing `while` loops.
    // To do this, we would replace the ForNode in the AST with the corresponding LetNode + WhileNode structure.
    //Lowers for loop into 
    /*


    while (iterable.next())
        let x = iterable.current() in
        body
        */
        //auto whileNode = 
    auto parentRefCopy = parentReference;
    if (node.iterable) {
        parentReference = &node.iterable;
        node.iterable->accept(*this);
    }
    scopeTable.enterScope();
    if (node.body) {
        parentReference = &node.body;
        node.body->accept(*this);
    }
    scopeTable.exitScope();
    std::vector<ASTNodePtr> emptyVec;
    auto iterableVarCond = new VariableNode("iterable");
    iterableVarCond->type = node.iterable->type;
    auto whileCond = new FunctionCallNode("next",emptyVec,iterableVarCond);
    whileCond->type = BoolType::instance();
    auto iterableVarCurrent = new VariableNode("iterable");
    iterableVarCurrent->type = node.iterable->type;
    auto currentCall =new FunctionCallNode("current",emptyVec,iterableVarCurrent);
    currentCall->type = node.type;
    ASTNodePtr whileBody = new LetNode(node.iterator,currentCall,node.body);
    whileBody->type = node.type;
    std::vector<ASTNodePtr> singleExpVector;
    singleExpVector.push_back(whileBody);
    whileBody = new BlockNode(singleExpVector);
    whileBody->type = node.type;
    ASTNodePtr whileNode =new WhileNode(whileCond,whileBody);
    whileNode->type = node.type; 
    ASTNodePtr letIterableNode = new LetNode("iterable", node.iterable, whileNode);
    letIterableNode->type = node.type;
    *parentRefCopy= letIterableNode;
    return nullptr;
}

Type* LoweringVisitor::visit(AttributeDeclNode& node) {
    if (node.init) {
        parentReference = &node.init;
        node.init->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(TypeDeclNode& node) {
    for (auto& arg : node.parentArgs) {
        parentReference = &arg;
        arg->accept(*this);
    }
    for (auto& member : node.members) {
        parentReference = &member;
        member->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(ProtocolDeclNode& node) {
    for (auto& method : node.methods) {
        parentReference = &method;
        method->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(ParamListNode& node) { return nullptr; }

Type* LoweringVisitor::visit(LetBindingNode& node) {
    if (node.init) {
        parentReference = &node.init;
        node.init->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(LetBindingsNode& node) {
    return nullptr;
}

Type* LoweringVisitor::visit(UnaryOpNode& node) {
    if (node.operand) {
        parentReference = &node.operand;
        node.operand->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(StringNode& node) { return nullptr; }
Type* LoweringVisitor::visit(BoolNode& node) { return nullptr; }
Type* LoweringVisitor::visit(TypeNode& node) { return nullptr; }

Type* LoweringVisitor::visit(ReturnNode& node) {
    if (node.expr) {
        parentReference = &node.expr;
        node.expr->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(LambdaNode& node) {
    auto parentRefCopy = parentReference;
    
    if (node.body) {
        parentReference = &node.body;
        node.body->accept(*this);
    }
      
    static int delegateId = 0;
    std::string delegateName = "Delegate_" + std::to_string(++delegateId);
    
    auto invokeMethod = new FunctionDeclNode(
        "invoke",
        node.params,
        node.paramTypes,
        node.declaredReturnType,
        node.body,
        true
    );

    invokeMethod->isMethod = true;
    invokeMethod->ownerTypeName = delegateName;
    invokeMethod->paramTypeNames = node.paramTypeNames;
    invokeMethod->declaredReturnTypeName = node.declaredReturnTypeName;
    invokeMethod->hasExplicitReturnType = node.hasExplicitReturnType;
    
    ASTNodePtr wrappedBody = node.body;  // start with the original lambda body
    auto variablesInScope = scopeTable.getAllVariables();
    // Iterate in reverse so the first variable becomes the outermost let
    for (int i=variablesInScope.size()-1;i>=0; i--) {
        auto vt = variablesInScope[i];
        auto var = vt.first;
        auto type = vt.second;
        // Build self.variableName
        auto selfVar = new VariableNode("self");
        selfVar->type = new NominalType(delegateName);
        auto memberAccess = new MemberAccessNode(selfVar, var);
        memberAccess->type=type,
        // Create LetNode: let var = self.var in wrappedBody
        
        wrappedBody = new LetNode(var, memberAccess, wrappedBody, nullptr, "", false);
        wrappedBody->type = node.body->type;
    }
    invokeMethod->exprBody = wrappedBody;   // replace the original body with the wrapped one
    std::vector<std::string> ctorParams;
    std::vector<Type*> ctorParamTypes;
    std::vector<std::string> ctorParamTypeNames;
    std::string parentType = "";
    std::vector<ASTNodePtr> parentArgs;
    std::vector<ASTNodePtr> members;
    members.push_back(invokeMethod);
    variablesInScope = scopeTable.getAllVariables();
    std::vector<ASTNodePtr> newArgs;
    
    for (size_t i = 0; i < variablesInScope.size(); i++)
    {
        auto var = variablesInScope[i].first;
        auto type = variablesInScope[i].second;
        ctorParams.push_back(var);
        ctorParamTypes.push_back(type);
        ctorParamTypeNames.push_back(type->toString());
        ASTNodePtr variableNode = new VariableNode(var);
        variableNode->type = type;
        auto attr = new AttributeDeclNode(var,variableNode);
        attr->type=type;
        members.push_back(attr);
        variableNode = new VariableNode(var);
        variableNode->type = type;
        newArgs.push_back(variableNode);
    }
    
    
    auto typeDecl = new TypeDeclNode(
        delegateName,
        ctorParams,
        ctorParamTypes,
        ctorParamTypeNames,
        parentType,
        parentArgs,
        members,
        false
    );
    typeDecl->parentType = "Object";
    generatedTypes.push_back(typeDecl);
    
    
    ASTNodePtr newNode = new NewNode(delegateName, newArgs);
    newNode->type = new NominalType(delegateName);
    *parentRefCopy = newNode;
    
    return nullptr;
}

Type* LoweringVisitor::visit(VectorLiteralNode& node) {
    for (auto& element : node.elements) {
        parentReference = &element;
        element->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(VectorComprehensionNode& node) {
    // LOWERING SUGGESTION 3: Vector Comprehension to Loops
    // Vector comprehensions like `[f(x) || x in iterable]` can be lowered into:
    // `let vec = new Vector() in { for(x in iterable) vec.append(f(x)); vec }`
    // This removes the need for a separate IR generation path for comprehensions.
    if (node.expression) {
        parentReference = &node.expression;
        node.expression->accept(*this);
    }
    if (node.iterable) {
        parentReference = &node.iterable;
        node.iterable->accept(*this);
    }
    return nullptr;
}

Type* LoweringVisitor::visit(IndexAccessNode& node) {
    // LOWERING SUGGESTION 4: Index Access to Method Call
    // Array/Vector access like `arr[i]` can be lowered to a method call `arr.get(i)` 
    // This aligns the AST closer to standard object-oriented method dispatch.
    if (node.base) {
        parentReference = &node.base;
        node.base->accept(*this);
    }
    if (node.index) {
        parentReference = &node.index;
        node.index->accept(*this);
    }
    return nullptr;
}


 // namespace lowering
