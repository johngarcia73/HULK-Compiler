#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <iostream>
#include <sstream>
#include <functional>
#include "type.hpp"

using namespace std;

enum class SemanticSymbolKind {
    Variable,
    Parameter,
    Function,
    Self,
    // Future: Class, Field, Method, ...
};

struct SymbolInfo {
    std::string name;
    std::vector<FunctionType*> overloads;  // just functions
    Type* type;                            // variables/params and not overladed funcs
    SemanticSymbolKind kind;
    int scopeLevel;
    bool isConst = false;

    // Constructor for vars and params (not functions)
    SymbolInfo(const std::string& n, Type* t, SemanticSymbolKind k, int level)
        : name(n), type(t), kind(k), scopeLevel(level) {}

    // Constructor for functions (not overloaded)
    SymbolInfo(const std::string& n, FunctionType* ft, SemanticSymbolKind k, int level)
        : name(n), overloads{ft}, type(ft), kind(k), scopeLevel(level) {}

    // Constructor for overloaded functions
    SymbolInfo(const std::string& n, const std::vector<FunctionType*>& ov, SemanticSymbolKind k, int level)
        : name(n), overloads(ov), type(ov.empty() ? nullptr : ov[0]), kind(k), scopeLevel(level) {}
};

enum class OverloadResolutionStatus {
    Resolved,
    SymbolNotFound,
    NotAFunction,
    ArityMismatch,
    TypeMismatch,
    Ambiguous
};

struct OverloadCandidateReport {
    FunctionType* candidate = nullptr;
    bool matched = false;
    bool arityMatched = false;
    std::vector<std::string> reasons;
};

struct OverloadResolutionResult {
    OverloadResolutionStatus status = OverloadResolutionStatus::SymbolNotFound;
    FunctionType* selected = nullptr;
    std::vector<OverloadCandidateReport> candidates;

    std::string statusString() const {
        switch (status) {
            case OverloadResolutionStatus::Resolved:
                return "resolved";
            case OverloadResolutionStatus::SymbolNotFound:
                return "symbol-not-found";
            case OverloadResolutionStatus::NotAFunction:
                return "not-a-function";
            case OverloadResolutionStatus::ArityMismatch:
                return "arity-mismatch";
            case OverloadResolutionStatus::TypeMismatch:
                return "type-mismatch";
            case OverloadResolutionStatus::Ambiguous:
                return "ambiguous";
        }
        return "symbol-not-found";
    }

    std::string messageFor(const std::string& name) const {
        switch (status) {
            case OverloadResolutionStatus::Resolved:
                return "Resolved overload for '" + name + "'";
            case OverloadResolutionStatus::SymbolNotFound:
                return "Function '" + name + "' not found.";
            case OverloadResolutionStatus::NotAFunction:
                return "'" + name + "' is not callable.";
            case OverloadResolutionStatus::ArityMismatch:
                return "No overload of '" + name + "' accepts this number of arguments.";
            case OverloadResolutionStatus::TypeMismatch:
                return "No overload of '" + name + "' matches the argument types.";
            case OverloadResolutionStatus::Ambiguous:
                return "Call to '" + name + "' is ambiguous.";
        }
        return "Function '" + name + "' not found.";
    }

    std::vector<std::string> notesFor(const std::string& name) const {
        std::vector<std::string> notes;
        if (status == OverloadResolutionStatus::Resolved && selected) {
            notes.push_back("Selected overload: " + selected->toString());
        }
        for (const auto& candidate : candidates) {
            if (!candidate.candidate) {
                continue;
            }
            if (candidate.matched) {
                notes.push_back("Candidate " + candidate.candidate->toString() + " matched.");
                continue;
            }
            if (candidate.reasons.empty()) {
                notes.push_back("Candidate " + candidate.candidate->toString() + " did not match.");
                continue;
            }
            for (const auto& reason : candidate.reasons) {
                notes.push_back("Candidate " + candidate.candidate->toString() + ": " + reason);
            }
        }
        if (notes.empty() && status != OverloadResolutionStatus::Resolved) {
            notes.push_back("No viable candidates found for '" + name + "'.");
        }
        return notes;
    }
};

class SemanticSymbolTable {
    std::vector<std::unordered_map<std::string, SymbolInfo>> scopes;
    int currentLevel = 0;

    static bool defaultConforms(Type* actual, Type* expected) {
        return typeConforms(actual, expected);
    }

    static bool areTypesCompatible(
        Type* paramType,
        Type* argType,
        const std::function<bool(Type*, Type*)>& conforms) {
        if (!paramType || !argType) {
            return false;
        }
        return conforms(argType, paramType);
    }

    static int compatibilityCost(
        Type* paramType,
        Type* argType,
        const std::function<bool(Type*, Type*)>& conforms) {
        if (!paramType || !argType) {
            return 1000;
        }
        if (paramType->equals(argType)) {
            return 0;
        }
        if (paramType->equals(UnknownType::instance())) {
            return 4;
        }
        if (paramType->equals(AnyType::instance())) {
            return 3;
        }
        if (conforms(argType, paramType)) {
            return 1;
        }
        if (isNumberType(paramType) && isNumberType(argType)) {
            auto* expectedNumber = asNumberType(paramType);
            auto* actualNumber = asNumberType(argType);
            if (expectedNumber && actualNumber && expectedNumber->isGeneric()) {
                return actualNumber->isGeneric() ? 0 : 1;
            }
        }
        if (argType->equals(AnyType::instance()) || argType->equals(UnknownType::instance())) {
            return 1;
        }
        return 1000;
    }

public:
    void clear() {
        scopes.clear();
        currentLevel = 0;
    }

    void enterScope() {
        scopes.emplace_back();
        ++currentLevel;
    }
    void exitScope() {
        scopes.pop_back();
        --currentLevel;
    }
    int getCurrentLevel() const { return currentLevel; }

    bool insert(const std::string& name, const SymbolInfo& info) {
        if (scopes.empty()) scopes.emplace_back();
        auto& cur = scopes.back();
        auto result = cur.emplace(name, info);
        return result.second; 
    }

    bool insertFunction(const std::string& name, FunctionType* funcType) {
        if (scopes.empty()) scopes.emplace_back();
        auto& cur = scopes.back();
        auto it = cur.find(name);
        if (it != cur.end()) {
            SymbolInfo& existing = it->second;
            if (existing.kind != SemanticSymbolKind::Function) return false;
         
            for (auto* ft : existing.overloads) {
                if (ft->equals(funcType)) return false;
            }
            existing.overloads.push_back(funcType);
            return true;
        } else {
            cur.emplace(name, SymbolInfo(name, funcType, SemanticSymbolKind::Function, currentLevel));
            return true;
        }
    }

    SymbolInfo* lookup(const std::string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) return &found->second;
        }
        return nullptr;
    }

    SymbolInfo* lookupCurrent(const std::string& name) {
        auto& cur = scopes.back();
        auto it = cur.find(name);
        return it != cur.end() ? &it->second : nullptr;
    }


    bool update(const std::string& name, const SymbolInfo& newInfo) {
        // Buscar desde el ámbito más interno hacia el global
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                // Opcional: verificar que el kind sea el mismo
                if (found->second.kind != newInfo.kind) return false;
                found->second = newInfo;
                return true;
            }
        }
        return false;
    }
    
    // For debgging
    void dump(std::ostream& out) const {
        out << "Symbol table (scopes: " << scopes.size() << ")\n";
        for (size_t i = 0; i < scopes.size(); ++i) {
            out << "  Scope " << i << ":\n";
            for (const auto& [name, info] : scopes[i]) {
                out << "    " << name << " : ";
                if (info.kind == SemanticSymbolKind::Function && info.overloads.size() > 1) {
                    for (auto* ft : info.overloads) {
                        out << ft->toString() << " ";
                    }
                } else if (info.type) {
                    out << info.type->toString();
                } else {
                    out << "unknown";
                }
                out << " (kind=" << static_cast<int>(info.kind) << ")\n";
            }
        }
    }

    OverloadResolutionResult resolveFunction(
        const std::string& name,
        const std::vector<Type*>& argTypes,
        std::function<bool(Type*, Type*)> conforms = defaultConforms) {
        OverloadResolutionResult result;
        SymbolInfo* info = lookup(name);
        if (!info) {
            result.status = OverloadResolutionStatus::SymbolNotFound;
            return result;
        }
        if (info->kind != SemanticSymbolKind::Function) {
            result.status = OverloadResolutionStatus::NotAFunction;
            return result;
        }

        std::vector<std::pair<FunctionType*, int>> matchedCandidates;
        bool sawArityMatch = false;

        for (auto* ft : info->overloads) {
            OverloadCandidateReport candidate;
            candidate.candidate = ft;

            const auto& params = ft->getParamTypes();
            if (params.size() != argTypes.size()) {
                std::ostringstream os;
                os << "expected " << params.size() << " argument(s) but got " << argTypes.size();
                candidate.reasons.push_back(os.str());
                result.candidates.push_back(std::move(candidate));
                continue;
            }

            candidate.arityMatched = true;
            sawArityMatch = true;
            bool match = true;
            int score = 0;

            for (size_t i = 0; i < params.size(); ++i) {
                if (areTypesCompatible(params[i], argTypes[i], conforms)) {
                    score += compatibilityCost(params[i], argTypes[i], conforms);
                    continue;
                }
                match = false;
                std::ostringstream os;
                os << "argument " << (i + 1) << " expected " << params[i]->toString()
                   << " but got " << (argTypes[i] ? argTypes[i]->toString() : std::string("<null>"));
                candidate.reasons.push_back(os.str());
            }

            candidate.matched = match;
            if (match) {
                matchedCandidates.push_back({ft, score});
            }
            result.candidates.push_back(std::move(candidate));
        }

        if (matchedCandidates.size() == 1) {
            result.status = OverloadResolutionStatus::Resolved;
            result.selected = matchedCandidates.front().first;
            return result;
        }
        if (matchedCandidates.size() > 1) {
            int bestScore = matchedCandidates.front().second;
            for (const auto& candidate : matchedCandidates) {
                bestScore = std::min(bestScore, candidate.second);
            }

            std::vector<FunctionType*> bestCandidates;
            for (const auto& candidate : matchedCandidates) {
                if (candidate.second == bestScore) {
                    bestCandidates.push_back(candidate.first);
                }
            }

            if (bestCandidates.size() == 1) {
                result.status = OverloadResolutionStatus::Resolved;
                result.selected = bestCandidates.front();
                return result;
            }

            result.status = OverloadResolutionStatus::Ambiguous;
            for (auto& candidate : result.candidates) {
                if (candidate.matched && candidate.candidate) {
                    candidate.reasons.push_back("candidate remained viable");
                }
            }
            return result;
        }

        result.status = sawArityMatch
            ? OverloadResolutionStatus::TypeMismatch
            : OverloadResolutionStatus::ArityMismatch;
        return result;
    }

    FunctionType* lookupFunction(
        const std::string& name,
        const std::vector<Type*>& argTypes,
        std::function<bool(Type*, Type*)> conforms = defaultConforms) {
        return resolveFunction(name, argTypes, std::move(conforms)).selected;
    }
};
