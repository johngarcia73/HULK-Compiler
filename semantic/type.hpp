#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_set>

enum class NumberKind {
    Int,
    Long,
    Float,
    Double
};

inline std::string numberKindToString(NumberKind kind) {
    switch (kind) {
        case NumberKind::Int:
            return "int";
        case NumberKind::Long:
            return "long";
        case NumberKind::Float:
            return "float";
        case NumberKind::Double:
            return "double";
    }
    return "int";
}

inline int numberKindRank(NumberKind kind) {
    switch (kind) {
        case NumberKind::Int:
            return 0;
        case NumberKind::Long:
            return 1;
        case NumberKind::Float:
            return 2;
        case NumberKind::Double:
            return 3;
    }
    return 0;
}

class Type {
public:
    virtual ~Type() = default;
    virtual std::string toString() const = 0;
    virtual bool equals(const Type* other) const = 0;
};

class ObjectType : public Type {
public:
    static ObjectType* instance() {
        static ObjectType inst;
        return &inst;
    }

    std::string toString() const override { return "Object"; }
    bool equals(const Type* other) const override {
        return dynamic_cast<const ObjectType*>(other) != nullptr;
    }

private:
    ObjectType() = default;
};

class NominalType : public Type {
    std::string name_;
    Type* parent_ = ObjectType::instance();

public:
    explicit NominalType(std::string name, Type* parent = ObjectType::instance())
        : name_(std::move(name)), parent_(parent ? parent : ObjectType::instance()) {}

    std::string toString() const override { return name_; }
    bool equals(const Type* other) const override {
        auto* nominal = dynamic_cast<const NominalType*>(other);
        return nominal && nominal->name_ == name_;
    }

    const std::string& name() const { return name_; }
    Type* parent() const { return parent_; }
    void setParent(Type* parent) {
        parent_ = parent ? parent : ObjectType::instance();
    }
};

class ProtocolType : public Type {
    std::string name_;

public:
    explicit ProtocolType(std::string name)
        : name_(std::move(name)) {}

    std::string toString() const override { return name_; }
    bool equals(const Type* other) const override {
        auto* protocol = dynamic_cast<const ProtocolType*>(other);
        return protocol && protocol->name_ == name_;
    }

    const std::string& name() const { return name_; }
};

class NumberType : public Type {
    bool generic_;
    NumberKind kind_;

    NumberType(bool isGeneric, NumberKind kind)
        : generic_(isGeneric), kind_(kind) {}
public:
    static NumberType* instance() {
        static NumberType inst(true, NumberKind::Int);
        return &inst;
    }

    static NumberType* instance(NumberKind kind) {
        static NumberType intInst(false, NumberKind::Int);
        static NumberType longInst(false, NumberKind::Long);
        static NumberType floatInst(false, NumberKind::Float);
        static NumberType doubleInst(false, NumberKind::Double);

        switch (kind) {
            case NumberKind::Int:
                return &intInst;
            case NumberKind::Long:
                return &longInst;
            case NumberKind::Float:
                return &floatInst;
            case NumberKind::Double:
                return &doubleInst;
        }
        return &intInst;
    }

    std::string toString() const override {
        if (generic_) {
            return "Number";
        }
        return "Number<" + numberKindToString(kind_) + ">";
    }

    bool equals(const Type* other) const override {
        auto* otherNumber = dynamic_cast<const NumberType*>(other);
        if (!otherNumber) {
            return false;
        }
        if (generic_ != otherNumber->generic_) {
            return false;
        }
        return generic_ || kind_ == otherNumber->kind_;
    }

    bool isGeneric() const { return generic_; }
    NumberKind kind() const { return kind_; }
    bool isIntegral() const {
        return !generic_ && (kind_ == NumberKind::Int || kind_ == NumberKind::Long);
    }
};

inline const NumberType* asNumberType(const Type* type) {
    return dynamic_cast<const NumberType*>(type);
}

inline NumberType* asNumberType(Type* type) {
    return dynamic_cast<NumberType*>(type);
}

inline bool isNumberType(const Type* type) {
    return asNumberType(type) != nullptr;
}

inline const NominalType* asNominalType(const Type* type) {
    return dynamic_cast<const NominalType*>(type);
}

inline NominalType* asNominalType(Type* type) {
    return dynamic_cast<NominalType*>(type);
}

inline const ProtocolType* asProtocolType(const Type* type) {
    return dynamic_cast<const ProtocolType*>(type);
}

inline ProtocolType* asProtocolType(Type* type) {
    return dynamic_cast<ProtocolType*>(type);
}

inline bool isProtocolType(const Type* type) {
    return asProtocolType(type) != nullptr;
}

inline Type* commonNumberType(Type* left, Type* right) {
    auto* leftNumber = asNumberType(left);
    auto* rightNumber = asNumberType(right);
    if (!leftNumber || !rightNumber) {
        return nullptr;
    }
    if (leftNumber->isGeneric() || rightNumber->isGeneric()) {
        return NumberType::instance();
    }

    NumberKind promotedKind = numberKindRank(leftNumber->kind()) >= numberKindRank(rightNumber->kind())
        ? leftNumber->kind()
        : rightNumber->kind();
    return NumberType::instance(promotedKind);
}

inline bool areNumberTypesCompatible(const Type* expected, const Type* actual) {
    auto* expectedNumber = asNumberType(expected);
    auto* actualNumber = asNumberType(actual);
    if (!expectedNumber || !actualNumber) {
        return false;
    }
    if (expectedNumber->isGeneric()) {
        return true;
    }
    if (actualNumber->isGeneric()) {
        return false;
    }
    return expectedNumber->kind() == actualNumber->kind();
}

class BoolType : public Type {
public:
    static BoolType* instance() {
        static BoolType inst;
        return &inst;
    }
    std::string toString() const override { return "Bool"; }
    bool equals(const Type* other) const override {
        return dynamic_cast<const BoolType*>(other) != nullptr;
    }
private:
    BoolType() = default;
};


class FunctionType : public Type {
    std::vector<Type*> paramTypes;
    Type* returnType;
public:
    FunctionType(const std::vector<Type*>& params, Type* ret)
        : paramTypes(params), returnType(ret) {}
    std::string toString() const override {
        std::string s = "(";
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (i > 0) s += ", ";
            s += paramTypes[i]->toString();
        }
        s += ") -> " + returnType->toString();
        return s;
    }

    
    bool equals(const Type* other) const override {
        auto* otherFunc = dynamic_cast<const FunctionType*>(other);
        if (!otherFunc) return false;
        if (paramTypes.size() != otherFunc->paramTypes.size()) return false;
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (!paramTypes[i]->equals(otherFunc->paramTypes[i])) return false;
        }
        return returnType->equals(otherFunc->returnType);
    }
    const std::vector<Type*>& getParamTypes() const { return paramTypes; }
    std::vector<Type*>& mutableParamTypes() { return paramTypes; }
    Type* getReturnType() const { return returnType; }
    void setReturnType(Type* t) { returnType = t; }
    void setParamType(size_t index, Type* type) {
        if (index < paramTypes.size()) {
            paramTypes[index] = type;
        }
    }

};

class StringType : public Type {
public:
    static StringType* instance() {
        static StringType inst;
        return &inst;
    }
    std::string toString() const override { return "String"; }
    bool equals(const Type* other) const override {
        return dynamic_cast<const StringType*>(other) != nullptr;
    }
private:
    StringType() = default;
};

class VoidType : public Type {
public:
    static VoidType* instance() { static VoidType inst; return &inst; }
    std::string toString() const override { return "Void"; }
    bool equals(const Type* other) const override { return dynamic_cast<const VoidType*>(other) != nullptr; }
private:
    VoidType() = default;
};

class UnknownType : public Type {
public:
    static UnknownType* instance() { static UnknownType inst; return &inst; }
    std::string toString() const override { return "Unknown"; }
    bool equals(const Type* other) const override { return dynamic_cast<const UnknownType*>(other) != nullptr; }
private:
    UnknownType() = default;
};

class AnyType : public Type {
public:
    static AnyType* instance() { static AnyType inst; return &inst; }
    std::string toString() const override { return "Any"; }
    bool equals(const Type* other) const override { return dynamic_cast<const AnyType*>(other) != nullptr; }
private:
    AnyType() = default;
};

inline bool isObjectLikeType(const Type* type) {
    return dynamic_cast<const ObjectType*>(type) != nullptr ||
           dynamic_cast<const NominalType*>(type) != nullptr ||
           dynamic_cast<const ProtocolType*>(type) != nullptr ||
           isNumberType(type) ||
           dynamic_cast<const StringType*>(type) != nullptr ||
           dynamic_cast<const BoolType*>(type) != nullptr;
}

inline bool typeConforms(const Type* actual, const Type* expected) {
    if (!actual || !expected) {
        return false;
    }
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
    if (isProtocolType(expected) || isProtocolType(actual)) {
        return actual->equals(expected);
    }
    if (dynamic_cast<const StringType*>(expected) || dynamic_cast<const BoolType*>(expected)) {
        return actual->equals(expected);
    }

    auto* actualNominal = asNominalType(actual);
    if (actualNominal) {
        std::unordered_set<std::string> seen;
        const Type* cursor = actualNominal;
        while (cursor && !seen.count(cursor->toString())) {
            seen.insert(cursor->toString());
            if (cursor->equals(expected)) {
                return true;
            }
            auto* nominalCursor = asNominalType(cursor);
            cursor = nominalCursor ? nominalCursor->parent() : nullptr;
        }
    }
    return false;
}

inline Type* lowestCommonAncestor(Type* left, Type* right) {
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
    if (isProtocolType(left) || isProtocolType(right)) {
        if (left->equals(right)) {
            return left;
        }
        if (isObjectLikeType(left) && isObjectLikeType(right)) {
            return ObjectType::instance();
        }
        return UnknownType::instance();
    }
    if (typeConforms(left, right)) {
        return right;
    }
    if (typeConforms(right, left)) {
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
