#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>
#include "../parser/AST_Builder/ast_node.hpp"
#include "diagnostics.hpp"
#include "type.hpp"

struct RegisteredAttribute {
    std::string name;
    AttributeDeclNode* declaration = nullptr;
    Type* type = nullptr;
    std::string declaredTypeName;
    std::string ownerTypeName;
};

struct RegisteredMethod {
    std::string name;
    FunctionDeclNode* declaration = nullptr;
    FunctionType* type = nullptr;
    std::string ownerTypeName;
    bool ownerIsProtocol = false;
};

struct RegisteredType {
    std::string name;
    TypeDeclNode* declaration = nullptr;
    Type* type = nullptr;
    Type* parent = ObjectType::instance();
    std::string parentName = "Object";
    std::vector<std::string> ctorParamNames;
    std::vector<Type*> ctorParamTypes;
    std::vector<std::string> ctorParamTypeNames;
    std::vector<ASTNodePtr> parentArgs;
    bool hasExplicitParentArgs = false;
    std::unordered_map<std::string, RegisteredAttribute> attributes;
    std::unordered_map<std::string, RegisteredMethod> methods;
};

struct RegisteredProtocol {
    std::string name;
    ProtocolDeclNode* declaration = nullptr;
    Type* type = nullptr;
    Type* parent = nullptr;
    std::string parentName;
    std::unordered_map<std::string, RegisteredMethod> methods;
};

class NominalTypeRegistry {
    std::unordered_map<std::string, RegisteredType> userTypes_;
    std::vector<std::string> declarationOrder_;
    std::unordered_map<std::string, RegisteredProtocol> protocols_;
    std::vector<std::string> protocolDeclarationOrder_;

    RegisteredType* findUserType(const std::string& name) {
        auto it = userTypes_.find(name);
        return it == userTypes_.end() ? nullptr : &it->second;
    }

    const RegisteredType* findUserType(const std::string& name) const {
        auto it = userTypes_.find(name);
        return it == userTypes_.end() ? nullptr : &it->second;
    }

    RegisteredProtocol* findUserProtocol(const std::string& name) {
        auto it = protocols_.find(name);
        return it == protocols_.end() ? nullptr : &it->second;
    }

    const RegisteredProtocol* findUserProtocol(const std::string& name) const {
        auto it = protocols_.find(name);
        return it == protocols_.end() ? nullptr : &it->second;
    }

    bool detectTypeCycleFrom(
        const std::string& name,
        std::unordered_map<std::string, int>& states,
        SemanticContext& context) {
        auto it = userTypes_.find(name);
        if (it == userTypes_.end()) {
            return false;
        }

        int& state = states[name];
        if (state == 1) {
            context.error(
                SemanticPhase::Declarations,
                "Inheritance cycle detected involving type '" + name + "'.",
                it->second.declaration ? it->second.declaration->span : SourceSpan{});
            it->second.parent = ObjectType::instance();
            it->second.parentName = "Object";
            return true;
        }
        if (state == 2) {
            return false;
        }

        state = 1;
        if (!it->second.parentName.empty() &&
            it->second.parentName != "Object" &&
            userTypes_.count(it->second.parentName)) {
            detectTypeCycleFrom(it->second.parentName, states, context);
        }
        state = 2;
        return false;
    }

    bool detectProtocolCycleFrom(
        const std::string& name,
        std::unordered_map<std::string, int>& states,
        SemanticContext& context) {
        auto it = protocols_.find(name);
        if (it == protocols_.end()) {
            return false;
        }

        int& state = states[name];
        if (state == 1) {
            context.error(
                SemanticPhase::Declarations,
                "Protocol extension cycle detected involving protocol '" + name + "'.",
                it->second.declaration ? it->second.declaration->span : SourceSpan{});
            it->second.parent = nullptr;
            it->second.parentName.clear();
            return true;
        }
        if (state == 2) {
            return false;
        }

        state = 1;
        if (!it->second.parentName.empty() && protocols_.count(it->second.parentName)) {
            detectProtocolCycleFrom(it->second.parentName, states, context);
        }
        state = 2;
        return false;
    }

    void collectProtocolMethods(
        const RegisteredProtocol* protocol,
        std::unordered_map<std::string, const RegisteredMethod*>& methods) const {
        if (!protocol) {
            return;
        }
        if (!protocol->parentName.empty()) {
            collectProtocolMethods(findUserProtocol(protocol->parentName), methods);
        }
        for (const auto& [name, method] : protocol->methods) {
            methods[name] = &method;
        }
    }

    const RegisteredMethod* lookupMemberMethod(Type* actualType, const std::string& name) const {
        if (const auto* protocol = findProtocol(actualType)) {
            auto methods = allProtocolMethods(protocol->type);
            auto it = methods.find(name);
            return it == methods.end() ? nullptr : it->second;
        }
        return findMethod(actualType, name);
    }

    bool methodSatisfiesRequirement(
        const FunctionType* implementation,
        const FunctionType* requirement,
        std::unordered_set<std::string>& seen) const {
        if (!implementation || !requirement) {
            return false;
        }

        const auto& implementationParams = implementation->getParamTypes();
        const auto& requirementParams = requirement->getParamTypes();
        if (implementationParams.size() != requirementParams.size()) {
            return false;
        }

        for (size_t i = 0; i < implementationParams.size(); ++i) {
            if (!conformsImpl(requirementParams[i], implementationParams[i], seen)) {
                return false;
            }
        }

        return conformsImpl(implementation->getReturnType(), requirement->getReturnType(), seen);
    }

    bool actualConformsToProtocol(
        const Type* actual,
        const ProtocolType* expected,
        std::unordered_set<std::string>& seen) const {
        auto expectedMethods = allProtocolMethods(expected);
        for (const auto& [name, requiredMethod] : expectedMethods) {
            const RegisteredMethod* implementationMethod =
                lookupMemberMethod(const_cast<Type*>(actual), name);
            if (!implementationMethod || !implementationMethod->type || !requiredMethod || !requiredMethod->type) {
                return false;
            }
            if (!methodSatisfiesRequirement(implementationMethod->type, requiredMethod->type, seen)) {
                return false;
            }
        }
        return !actual->equals(VoidType::instance());
    }

    bool conformsImpl(
        const Type* actual,
        const Type* expected,
        std::unordered_set<std::string>& seen) const {
        if (!actual || !expected) {
            return false;
        }

        const std::string key = actual->toString() + "<=" + expected->toString();
        if (seen.count(key)) {
            return true;
        }
        seen.insert(key);

        if (actual->equals(UnknownType::instance()) || expected->equals(UnknownType::instance())) {
            return true;
        }
        if (actual->equals(AnyType::instance()) || expected->equals(AnyType::instance())) {
            return true;
        }
        if (actual->equals(expected)) {
            return true;
        }
        if (expected->equals(ObjectType::instance()) && !actual->equals(VoidType::instance())) {
            return isObjectLikeType(actual);
        }
        if (isNumberType(expected) && isNumberType(actual)) {
            return areNumberTypesCompatible(expected, actual);
        }
        if (dynamic_cast<const StringType*>(expected) || dynamic_cast<const BoolType*>(expected)) {
            return actual->equals(expected);
        }

        if (const auto* expectedProtocol = asProtocolType(expected)) {
            return actualConformsToProtocol(actual, expectedProtocol, seen);
        }

        if (asProtocolType(actual)) {
            return false;
        }

        auto* actualNominal = asNominalType(actual);
        if (actualNominal) {
            std::unordered_set<std::string> walked;
            const Type* cursor = actualNominal;
            while (cursor && !walked.count(cursor->toString())) {
                walked.insert(cursor->toString());
                if (cursor->equals(expected)) {
                    return true;
                }
                auto* nominalCursor = asNominalType(cursor);
                cursor = nominalCursor ? nominalCursor->parent() : nullptr;
            }
        }
        return false;
    }

public:
    void clear() {
        userTypes_.clear();
        declarationOrder_.clear();
        protocols_.clear();
        protocolDeclarationOrder_.clear();
    }

    bool isBuiltinTypeName(const std::string& name) const {
        return name == "Object" || name == "Number" || name == "String" || name == "Bool" || name == "Boolean";
    }

    Type* resolveTypeName(const std::string& name) const {
        if (name.empty()) {
            return nullptr;
        }
        if (name == "Object") {
            return ObjectType::instance();
        }
        if (name == "Number") {
            return NumberType::instance();
        }
        if (name == "String") {
            return StringType::instance();
        }
        if (name == "Bool" || name == "Boolean") {
            return BoolType::instance();
        }
        if (auto typeIt = userTypes_.find(name); typeIt != userTypes_.end()) {
            return typeIt->second.type;
        }
        auto protocolIt = protocols_.find(name);
        return protocolIt == protocols_.end() ? nullptr : protocolIt->second.type;
    }

    bool registerType(TypeDeclNode& declaration, SemanticContext& context) {
        if (isBuiltinTypeName(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Type '" + declaration.name + "' is reserved.",
                declaration.span);
            return false;
        }
        if (userTypes_.count(declaration.name) || protocols_.count(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Type '" + declaration.name + "' is already declared.",
                declaration.span);
            return false;
        }

        RegisteredType record;
        record.name = declaration.name;
        record.declaration = &declaration;
        record.type = new NominalType(declaration.name);
        record.parentName = declaration.parentType.empty() ? "Object" : declaration.parentType;
        record.ctorParamNames = declaration.ctorParams;
        record.ctorParamTypes = declaration.ctorParamTypes;
        record.ctorParamTypeNames = declaration.ctorParamTypeNames;
        record.parentArgs = declaration.parentArgs;
        record.hasExplicitParentArgs = declaration.hasExplicitParentArgs;
        userTypes_.emplace(declaration.name, std::move(record));
        declarationOrder_.push_back(declaration.name);
        return true;
    }

    bool registerProtocol(ProtocolDeclNode& declaration, SemanticContext& context) {
        if (isBuiltinTypeName(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Protocol '" + declaration.name + "' is reserved.",
                declaration.span);
            return false;
        }
        if (protocols_.count(declaration.name) || userTypes_.count(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Protocol '" + declaration.name + "' is already declared.",
                declaration.span);
            return false;
        }

        RegisteredProtocol record;
        record.name = declaration.name;
        record.declaration = &declaration;
        record.type = new ProtocolType(declaration.name);
        record.parentName = declaration.extendedProtocol;
        protocols_.emplace(declaration.name, std::move(record));
        protocolDeclarationOrder_.push_back(declaration.name);
        return true;
    }

    void resolveParentsAndConstructors(SemanticContext& context) {
        for (auto& [name, record] : userTypes_) {
            if (record.parentName.empty()) {
                record.parentName = "Object";
            }

            if (record.parentName == "Object") {
                record.parent = ObjectType::instance();
                if (auto* nominal = asNominalType(record.type)) {
                    nominal->setParent(ObjectType::instance());
                }
                continue;
            }

            if (record.parentName == "Number" || record.parentName == "String" ||
                record.parentName == "Bool" || record.parentName == "Boolean") {
                context.error(
                    SemanticPhase::Declarations,
                    "Type '" + record.name + "' cannot inherit from builtin type '" + record.parentName + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parent = ObjectType::instance();
                record.parentName = "Object";
                if (auto* nominal = asNominalType(record.type)) {
                    nominal->setParent(ObjectType::instance());
                }
                continue;
            }

            if (protocols_.count(record.parentName)) {
                context.error(
                    SemanticPhase::Declarations,
                    "Type '" + record.name + "' cannot inherit from protocol '" + record.parentName + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parent = ObjectType::instance();
                record.parentName = "Object";
                if (auto* nominal = asNominalType(record.type)) {
                    nominal->setParent(ObjectType::instance());
                }
                continue;
            }

            Type* parentType = resolveTypeName(record.parentName);
            if (!parentType) {
                context.error(
                    SemanticPhase::Declarations,
                    "Unknown parent type '" + record.parentName + "' for type '" + record.name + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parent = ObjectType::instance();
                record.parentName = "Object";
                if (auto* nominal = asNominalType(record.type)) {
                    nominal->setParent(ObjectType::instance());
                }
                continue;
            }

            record.parent = parentType;
            if (auto* nominal = asNominalType(record.type)) {
                nominal->setParent(parentType);
            }
        }

        std::unordered_map<std::string, int> typeStates;
        for (const auto& [name, _] : userTypes_) {
            detectTypeCycleFrom(name, typeStates, context);
        }

        for (auto& [name, record] : userTypes_) {
            const RegisteredType* parentRecord = findUserType(record.parentName);
            if (!record.hasExplicitParentArgs && record.ctorParamNames.empty() && parentRecord) {
                record.ctorParamNames = parentRecord->ctorParamNames;
                record.ctorParamTypes = parentRecord->ctorParamTypes;
                record.ctorParamTypeNames = parentRecord->ctorParamTypeNames;
                continue;
            }

            if (!record.hasExplicitParentArgs && parentRecord) {
                if (record.ctorParamNames.size() != parentRecord->ctorParamNames.size()) {
                    context.error(
                        SemanticPhase::Declarations,
                        "Type '" + record.name + "' changes constructor parameters but does not provide explicit parent arguments.",
                        record.declaration ? record.declaration->span : SourceSpan{});
                    continue;
                }
                for (size_t i = 0; i < record.ctorParamTypeNames.size() && i < parentRecord->ctorParamTypeNames.size(); ++i) {
                    const std::string& lhs = record.ctorParamTypeNames[i];
                    const std::string& rhs = parentRecord->ctorParamTypeNames[i];
                    if (!lhs.empty() && !rhs.empty() && lhs != rhs) {
                        context.error(
                            SemanticPhase::Declarations,
                            "Type '" + record.name + "' changes constructor parameter types but does not provide explicit parent arguments.",
                            record.declaration ? record.declaration->span : SourceSpan{});
                        break;
                    }
                }
            }
        }

        for (auto& [name, record] : protocols_) {
            if (record.parentName.empty()) {
                continue;
            }
            if (isBuiltinTypeName(record.parentName)) {
                context.error(
                    SemanticPhase::Declarations,
                    "Protocol '" + record.name + "' cannot extend builtin type '" + record.parentName + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parentName.clear();
                record.parent = nullptr;
                continue;
            }
            if (userTypes_.count(record.parentName)) {
                context.error(
                    SemanticPhase::Declarations,
                    "Protocol '" + record.name + "' cannot extend nominal type '" + record.parentName + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parentName.clear();
                record.parent = nullptr;
                continue;
            }
            Type* parentType = resolveTypeName(record.parentName);
            if (!parentType || !isProtocolType(parentType)) {
                context.error(
                    SemanticPhase::Declarations,
                    "Unknown protocol '" + record.parentName + "' extended by protocol '" + record.name + "'.",
                    record.declaration ? record.declaration->span : SourceSpan{});
                record.parentName.clear();
                record.parent = nullptr;
                continue;
            }
            record.parent = parentType;
        }

        std::unordered_map<std::string, int> protocolStates;
        for (const auto& [name, _] : protocols_) {
            detectProtocolCycleFrom(name, protocolStates, context);
        }

        for (auto& [name, record] : protocols_) {
            if (!record.parent) {
                continue;
            }
            for (const auto& [methodName, method] : record.methods) {
                const RegisteredMethod* inherited = findMethod(record.parent, methodName);
                if (!inherited || !inherited->type || !method.type) {
                    continue;
                }
                std::unordered_set<std::string> seen;
                if (!methodSatisfiesRequirement(method.type, inherited->type, seen)) {
                    context.error(
                        SemanticPhase::Declarations,
                        "Method '" + methodName + "' in protocol '" + name +
                            "' is not compatible with the inherited protocol method.",
                        method.declaration ? method.declaration->span : SourceSpan{},
                        {
                            "Inherited signature: " + inherited->type->toString(),
                            "Declared signature: " + method.type->toString()
                        });
                }
            }
        }
    }

    bool registerAttribute(
        const std::string& ownerTypeName,
        AttributeDeclNode& declaration,
        Type* declaredType,
        const std::string& declaredTypeName,
        SemanticContext& context) {
        RegisteredType* owner = findUserType(ownerTypeName);
        if (!owner) {
            context.error(
                SemanticPhase::Declarations,
                "Internal error: unknown owner type '" + ownerTypeName + "' for attribute '" + declaration.name + "'.",
                declaration.span);
            return false;
        }
        if (owner->attributes.count(declaration.name) || owner->methods.count(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Member '" + declaration.name + "' is already declared in type '" + ownerTypeName + "'.",
                declaration.span);
            return false;
        }

        owner->attributes.emplace(
            declaration.name,
            RegisteredAttribute{
                declaration.name,
                &declaration,
                declaredType,
                declaredTypeName,
                ownerTypeName});
        return true;
    }

    bool registerMethod(
        const std::string& ownerTypeName,
        FunctionDeclNode& declaration,
        FunctionType* functionType,
        SemanticContext& context) {
        RegisteredType* owner = findUserType(ownerTypeName);
        if (!owner) {
            context.error(
                SemanticPhase::Declarations,
                "Internal error: unknown owner type '" + ownerTypeName + "' for method '" + declaration.name + "'.",
                declaration.span);
            return false;
        }
        if (owner->methods.count(declaration.name) || owner->attributes.count(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Member '" + declaration.name + "' is already declared in type '" + ownerTypeName + "'.",
                declaration.span);
            return false;
        }

        const RegisteredMethod* inherited = nullptr;
        if (const RegisteredType* parent = findUserType(owner->parentName)) {
            inherited = findMethod(parent->type, declaration.name);
        }
        if (inherited && inherited->type && functionType && !inherited->type->equals(functionType)) {
            context.error(
                SemanticPhase::Declarations,
                "Method '" + declaration.name + "' in type '" + ownerTypeName + "' must keep the exact inherited signature.",
                declaration.span,
                {
                    "Inherited signature: " + inherited->type->toString(),
                    "Declared signature: " + functionType->toString()
                });
        }

        owner->methods.emplace(
            declaration.name,
            RegisteredMethod{
                declaration.name,
                &declaration,
                functionType,
                ownerTypeName,
                false});
        return true;
    }

    bool registerProtocolMethod(
        const std::string& ownerProtocolName,
        FunctionDeclNode& declaration,
        FunctionType* functionType,
        SemanticContext& context) {
        RegisteredProtocol* owner = findUserProtocol(ownerProtocolName);
        if (!owner) {
            context.error(
                SemanticPhase::Declarations,
                "Internal error: unknown owner protocol '" + ownerProtocolName + "' for method '" + declaration.name + "'.",
                declaration.span);
            return false;
        }
        if (owner->methods.count(declaration.name)) {
            context.error(
                SemanticPhase::Declarations,
                "Method '" + declaration.name + "' is already declared in protocol '" + ownerProtocolName + "'.",
                declaration.span);
            return false;
        }

        if (!owner->parentName.empty()) {
            if (const RegisteredMethod* inherited = findMethod(owner->parent, declaration.name)) {
                std::unordered_set<std::string> seen;
                if (!methodSatisfiesRequirement(functionType, inherited->type, seen)) {
                    context.error(
                        SemanticPhase::Declarations,
                        "Method '" + declaration.name + "' in protocol '" + ownerProtocolName + "' is not compatible with the inherited protocol method.",
                        declaration.span,
                        {
                            "Inherited signature: " + inherited->type->toString(),
                            "Declared signature: " + functionType->toString()
                        });
                }
            }
        }

        owner->methods.emplace(
            declaration.name,
            RegisteredMethod{
                declaration.name,
                &declaration,
                functionType,
                ownerProtocolName,
                true});
        return true;
    }

    RegisteredType* findType(Type* type) {
        auto* nominal = asNominalType(type);
        return nominal ? findUserType(nominal->name()) : nullptr;
    }

    const RegisteredType* findType(Type* type) const {
        auto* nominal = asNominalType(type);
        return nominal ? findUserType(nominal->name()) : nullptr;
    }

    RegisteredProtocol* findProtocol(Type* type) {
        auto* protocol = asProtocolType(type);
        return protocol ? findUserProtocol(protocol->name()) : nullptr;
    }

    const RegisteredProtocol* findProtocol(Type* type) const {
        auto* protocol = asProtocolType(type);
        return protocol ? findUserProtocol(protocol->name()) : nullptr;
    }

    const RegisteredAttribute* findDeclaredAttribute(Type* type, const std::string& name) const {
        const RegisteredType* record = findType(type);
        if (!record) {
            return nullptr;
        }
        auto it = record->attributes.find(name);
        return it == record->attributes.end() ? nullptr : &it->second;
    }

    std::unordered_map<std::string, const RegisteredMethod*> allProtocolMethods(const Type* type) const {
        std::unordered_map<std::string, const RegisteredMethod*> methods;
        collectProtocolMethods(findProtocol(const_cast<Type*>(type)), methods);
        return methods;
    }

    const RegisteredMethod* findMethod(Type* type, const std::string& name) const {
        if (const RegisteredProtocol* protocol = findProtocol(type)) {
            auto methods = allProtocolMethods(protocol->type);
            auto it = methods.find(name);
            return it == methods.end() ? nullptr : it->second;
        }

        for (const RegisteredType* record = findType(type); record; record = findUserType(record->parentName)) {
            auto it = record->methods.find(name);
            if (it != record->methods.end()) {
                return &it->second;
            }
            if (record->parentName == "Object") {
                break;
            }
        }
        return nullptr;
    }

    const RegisteredMethod* findBaseMethod(
        const std::string& ownerTypeName,
        const std::string& methodName) const {
        const RegisteredType* owner = findUserType(ownerTypeName);
        if (!owner || owner->parentName == "Object") {
            return nullptr;
        }
        const RegisteredType* parent = findUserType(owner->parentName);
        if (!parent) {
            return nullptr;
        }
        return findMethod(parent->type, methodName);
    }

    const std::vector<Type*>* constructorParamTypes(Type* type) const {
        const RegisteredType* record = findType(type);
        if (!record) {
            return nullptr;
        }
        return &record->ctorParamTypes;
    }

    const std::vector<std::string>* constructorParamNames(Type* type) const {
        const RegisteredType* record = findType(type);
        if (!record) {
            return nullptr;
        }
        return &record->ctorParamNames;
    }

    const std::vector<std::string>* constructorParamTypeNames(Type* type) const {
        const RegisteredType* record = findType(type);
        if (!record) {
            return nullptr;
        }
        return &record->ctorParamTypeNames;
    }

    std::vector<RegisteredType*> declaredTypesInOrder() {
        std::vector<RegisteredType*> ordered;
        ordered.reserve(declarationOrder_.size());
        for (const auto& name : declarationOrder_) {
            if (auto* record = findUserType(name)) {
                ordered.push_back(record);
            }
        }
        return ordered;
    }

    std::vector<RegisteredProtocol*> declaredProtocolsInOrder() {
        std::vector<RegisteredProtocol*> ordered;
        ordered.reserve(protocolDeclarationOrder_.size());
        for (const auto& name : protocolDeclarationOrder_) {
            if (auto* record = findUserProtocol(name)) {
                ordered.push_back(record);
            }
        }
        return ordered;
    }

    bool conforms(const Type* actual, const Type* expected) const {
        std::unordered_set<std::string> seen;
        return conformsImpl(actual, expected, seen);
    }

    Type* lowestCommonAncestor(Type* left, Type* right) const {
        if (!left) {
            return right;
        }
        if (!right) {
            return left;
        }
        if (left->equals(UnknownType::instance())) {
            return right;
        }
        if (right->equals(UnknownType::instance())) {
            return left;
        }
        if (left->equals(right)) {
            return left;
        }
        if (isNumberType(left) && isNumberType(right)) {
            return commonNumberType(left, right);
        }
        if (conforms(left, right)) {
            return right;
        }
        if (conforms(right, left)) {
            return left;
        }

        auto* leftNominal = asNominalType(left);
        auto* rightNominal = asNominalType(right);
        if (leftNominal && rightNominal) {
            std::unordered_set<std::string> ancestors;
            for (Type* cursor = leftNominal; cursor; ) {
                ancestors.insert(cursor->toString());
                auto* nominalCursor = asNominalType(cursor);
                cursor = nominalCursor ? nominalCursor->parent() : nullptr;
            }
            for (Type* cursor = rightNominal; cursor; ) {
                if (ancestors.count(cursor->toString())) {
                    return cursor;
                }
                auto* nominalCursor = asNominalType(cursor);
                cursor = nominalCursor ? nominalCursor->parent() : nullptr;
            }
            return ObjectType::instance();
        }

        if (isObjectLikeType(left) && isObjectLikeType(right)) {
            return ObjectType::instance();
        }
        return UnknownType::instance();
    }
};
