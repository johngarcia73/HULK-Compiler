#pragma once
#include <string>
#include "../semantic/type.hpp"

namespace TypeUtils {
    // Internal state to hold the compilation target. 
    // Defaults to the native host machine.

    enum class CPUArchitecture { X86_64, ARM64, RV64, GENERIC_32 };

    struct TargetInfo {
        CPUArchitecture Architecture;
        int PointerSize;    // Size in bytes (e.g., 8 for 64-bit)
        int StackAlign; // Required stack alignment
        std::string PointerType;   // QBE type: 'l' for 64-bit, 'w' for 32-bit

        static TargetInfo getNative() {
            // Simple compile-time detection for your host
            #if defined(__x86_64__) || defined(_M_X64)
                return {CPUArchitecture::X86_64, 8, 16, "l"};
            #elif defined(__aarch64__) || defined(_M_ARM64)
                return {CPUArchitecture::ARM64, 8, 16, "l"};
            #else
                // Fallback or 32-bit (Note: QBE has limited 32-bit support)
                return {CPUArchitecture::GENERIC_32, 4, 8, "w"};
            #endif
        }
    };
    
    inline TargetInfo currentTarget = TargetInfo::getNative();

    inline void setTarget(const TargetInfo& info) {
        currentTarget = info;
    }

    // Maps a HULK semantic type to a QBE base type (w, l, s, d)
    inline std::string toQbeType(Type* type) {
        if (dynamic_cast<BoolType*>(type)) return "w";
        if (dynamic_cast<NumberType*>(type)) {
            return (currentTarget.PointerSize == 4) ? "s" : "d";
        }
        return currentTarget.PointerType;
    }

    // Returns the size in bytes for a given HULK type
    inline size_t getByteSize(Type* type) {
        if (dynamic_cast<BoolType*>(type)) return 4; 
        return (size_t)currentTarget.PointerSize;
    }

    // Maps a HULK type to the integer flag expected by the C runtime
    inline int getRuntimeFlag(Type* type) {
        if (dynamic_cast<NumberType*>(type)) return 1;
        if (dynamic_cast<BoolType*>(type))   return 2;
        if (dynamic_cast<StringType*>(type)) return 3;
        return 0; // PTR/Object
    }
}