#pragma once
#include <cstdint>
#include <type_traits>
#include <Windows.h>

// ============================================================================
// Globals - Cached module base and pointers
// ============================================================================

namespace Globals {
    // Module base address (set once at init)
    inline uintptr_t base = 0;

    // Initialize module base
    inline bool Init() {
        base = (uintptr_t)GetModuleHandleW(nullptr);
        if (!base) {
            base = (uintptr_t)GetModuleHandleA("League of Legends.exe");
        }
        return base != 0;
    }

    // Safe memory read
    template<typename T>
    inline T Read(uintptr_t addr) {
        static_assert(std::is_trivially_copyable_v<T>,
            "Globals::Read<T> only works with trivially-copyable types");
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
        static_assert(std::is_trivially_copyable_v<T>,
            "Globals::Write<T> only works with trivially-copyable types");
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

    // Check if address is valid (works for both x86 and x64)
    inline bool IsValidPtr(uintptr_t addr) {
        if constexpr (sizeof(void*) == 8)
            return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
        else
            return addr > 0x10000 && addr < 0x7FFFFFFF;
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

    inline bool ReadCString(uintptr_t strAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1 || !IsValidPtr(strAddr)) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        __try {
            int count = 0;
            for (; count < (maxOut - 1); ++count) {
                const char ch = *((const char*)strAddr + count);
                if (ch == 0) {
                    out[count] = 0;
                    return count > 0;
                }

                const unsigned char uch = static_cast<unsigned char>(ch);
                if (uch < 0x20 || uch > 0x7E) {
                    out[0] = 0;
                    return false;
                }

                out[count] = ch;
            }

            out[maxOut - 1] = 0;
            return true;
        } __except(1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool ReadRuntimeStringField(uintptr_t fieldAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            return false;
        }
        out[0] = 0;

        if (ReadGameString(fieldAddr, out, maxOut)) {
            return true;
        }

        const uintptr_t ptr = Read<uintptr_t>(fieldAddr);
        if (IsValidPtr(ptr) && ReadCString(ptr, out, maxOut)) {
            return true;
        }

        return ReadCString(fieldAddr, out, maxOut);
    }

    // Read a pointer array from a manager struct (SEH safe)
    // Returns number of valid entries read into out[]
    inline int ReadPtrArray(uintptr_t listAddr, int count, uintptr_t* out, int maxOut) {
        __try {
            if (count <= 0 || count > maxOut) return 0;
            if (!IsValidPtr(listAddr)) return 0;
            for (int i = 0; i < count; i++) {
                out[i] = *(uintptr_t*)(listAddr + i * sizeof(uintptr_t));
            }
            return count;
        } __except(1) { return 0; }
    }
}
