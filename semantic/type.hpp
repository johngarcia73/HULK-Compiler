#pragma once
#include <string>
#include <vector>
#include <memory>

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
    std::string toString() const override { return "bool"; }
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
    Type* getReturnType() const { return returnType; }
    void setReturnType(Type* t) { returnType = t; }

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
