#pragma once
#include <string>
#include <vector>
#include <memory>

class Type {
public:
    virtual ~Type() = default;
    virtual std::string toString() const = 0;
    virtual bool equals(const Type* other) const = 0;
};

class IntType : public Type {
public:
    static IntType* instance() {
        static IntType inst;
        return &inst;
    }
    std::string toString() const override { return "int"; }
    bool equals(const Type* other) const override {
        return dynamic_cast<const IntType*>(other) != nullptr;
    }
private:
    IntType() = default;
};

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
        auto* ft = dynamic_cast<const FunctionType*>(other);
        if (!ft) return false;
        if (paramTypes.size() != ft->paramTypes.size()) return false;
        for (size_t i = 0; i < paramTypes.size(); ++i) {
            if (!paramTypes[i]->equals(ft->paramTypes[i])) return false;
        }
        return returnType->equals(ft->returnType);
    }
    const std::vector<Type*>& getParamTypes() const { return paramTypes; }
    Type* getReturnType() const { return returnType; }
};