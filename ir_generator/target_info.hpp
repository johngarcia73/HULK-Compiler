#pragma once
#include <string>

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
