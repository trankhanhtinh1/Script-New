#pragma once
#include <cstdint>
#include <Windows.h>

// ============================================================================
// Globals - Cached module base and pointers
// ============================================================================

namespace Globals {
    // Module base address (set once at init)
    inline uintptr_t base = 0;

    // Initialize module base
    inline bool Init() {
        base = (uintptr_t)GetModuleHandleA("League of Legends.exe");
        if (!base) base = (uintptr_t)GetModuleHandleA(nullptr);
        return base != 0;
    }

    // Safe memory read
    template<typename T>
    inline T Read(uintptr_t addr) {
        __try {
            return *(T*)(addr);
        }
        __except (1) {
            return T{};
        }
    }

    // Safe memory write
    template<typename T>
    inline bool Write(uintptr_t addr, T value) {
        __try {
            *(T*)(addr) = value;
            return true;
        }
        __except (1) {
            return false;
        }
    }

    // Read pointer chain
    inline uintptr_t ReadPtr(uintptr_t addr) {
        return Read<uintptr_t>(addr);
    }

    // Check if address is valid
    inline bool IsValidPtr(uintptr_t addr) {
        return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
    }

    // ====================================================================
    // SEH-safe helpers (no C++ objects → compatible with __try)
    // ====================================================================

    // Read a MSVC std::string (SSO) into raw char buffer.
    inline bool ReadStdString(uintptr_t nameAddr, char* out, int maxOut) {
        __try {
            size_t len = *(size_t*)(nameAddr + 0x10);
            size_t capacity = *(size_t*)(nameAddr + 0x18);
            if (len == 0 || len >= (size_t)maxOut) { out[0] = 0; return false; }
            if (capacity < len || capacity > 0x100000) { out[0] = 0; return false; }

            const char* src;
            if (capacity > 15) {
                uintptr_t ptr = *(uintptr_t*)(nameAddr);
                if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) { out[0] = 0; return false; }
                src = (const char*)ptr;
            } else {
                src = (const char*)nameAddr;
            }

            for (size_t i = 0; i < len; i++) out[i] = src[i];
            out[len] = 0;
            return true;
        } __except(1) { out[0] = 0; return false; }
    }

    // Read Riot/LolString into raw char buffer.
    inline bool ReadRiotString(uintptr_t nameAddr, char* out, int maxOut) {
        __try {
            int len = *(int*)(nameAddr + 0x10);
            int maxLen = *(int*)(nameAddr + 0x14);
            if (len <= 0 || len >= maxOut) { out[0] = 0; return false; }
            if (maxLen < 0 || maxLen > 0x10000) { out[0] = 0; return false; }
            const char* src;
            if (maxLen >= 16) {
                uintptr_t ptr = *(uintptr_t*)(nameAddr);
                if (ptr < 0x10000 || ptr > 0x7FFFFFFFFFFF) { out[0] = 0; return false; }
                src = (const char*)ptr;
            } else {
                src = (const char*)nameAddr;
            }

            for (int i = 0; i < len; i++) out[i] = src[i];
            out[len] = 0;
            return true;
        } __except(1) { out[0] = 0; return false; }
    }

    // Read a game string into raw char buffer. Tries std::string first, then Riot/LolString.
    inline bool ReadGameString(uintptr_t nameAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            return false;
        }
        out[0] = 0;
        if (ReadStdString(nameAddr, out, maxOut)) {
            return true;
        }
        return ReadRiotString(nameAddr, out, maxOut);
    }

    // Read a pointer array from a manager struct (SEH safe)
    // Returns number of valid entries read into out[]
    inline int ReadPtrArray(uintptr_t listAddr, int count, uintptr_t* out, int maxOut) {
        __try {
            if (count <= 0 || count > maxOut) return 0;
            if (listAddr < 0x10000 || listAddr > 0x7FFFFFFFFFFF) return 0;
            for (int i = 0; i < count; i++) {
                out[i] = *(uintptr_t*)(listAddr + i * 8);
            }
            return count;
        } __except(1) { return 0; }
    }
}
