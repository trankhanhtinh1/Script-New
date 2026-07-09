#pragma once

#include <Windows.h>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace Globals {

    inline uintptr_t base = 0;

    inline bool Init() {
        base = reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if (!base) {
            base = reinterpret_cast<uintptr_t>(GetModuleHandleA("League of Legends.exe"));
        }
        return base != 0;
    }

    inline bool IsValidPtr(uintptr_t addr) {
        if constexpr (sizeof(void*) == 8) {
            return addr > 0x10000 && addr < 0x7FFFFFFFFFFF;
        } else {
            return addr > 0x10000 && addr < 0x7FFFFFFF;
        }
    }

    inline bool IsReadablePtr(uintptr_t addr, size_t bytes = 1) {
        if (!IsValidPtr(addr) || bytes == 0) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) {
            return false;
        }

        const uintptr_t base = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t regionEnd = base + mbi.RegionSize;
        if (regionEnd < base || addr >= regionEnd) {
            return false;
        }
        return bytes <= (regionEnd - addr);
    }

    inline bool IsWritablePtr(uintptr_t addr, size_t bytes = 1) {
        if (!IsReadablePtr(addr, bytes)) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi))) {
            return false;
        }

        const DWORD protect = mbi.Protect & 0xFF;
        return protect == PAGE_READWRITE ||
               protect == PAGE_WRITECOPY ||
               protect == PAGE_EXECUTE_READWRITE ||
               protect == PAGE_EXECUTE_WRITECOPY;
    }

    inline bool IsExecutablePtr(uintptr_t addr, size_t bytes = 1) {
        if (!IsValidPtr(addr) || bytes == 0) {
            return false;
        }

        MEMORY_BASIC_INFORMATION mbi = {};
        if (!VirtualQuery(reinterpret_cast<const void*>(addr), &mbi, sizeof(mbi))) {
            return false;
        }
        if (mbi.State != MEM_COMMIT) {
            return false;
        }
        if ((mbi.Protect & PAGE_GUARD) || (mbi.Protect & PAGE_NOACCESS)) {
            return false;
        }

        const DWORD protect = mbi.Protect & 0xFF;
        if (protect != PAGE_EXECUTE &&
            protect != PAGE_EXECUTE_READ &&
            protect != PAGE_EXECUTE_READWRITE &&
            protect != PAGE_EXECUTE_WRITECOPY) {
            return false;
        }

        const uintptr_t baseAddress = reinterpret_cast<uintptr_t>(mbi.BaseAddress);
        const uintptr_t regionEnd = baseAddress + mbi.RegionSize;
        return regionEnd >= baseAddress && addr < regionEnd && bytes <= (regionEnd - addr);
    }

    template <typename T>
    inline T Read(uintptr_t addr) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Globals::Read<T> only supports trivially-copyable types");
        if (!IsValidPtr(addr)) {
            return T{};
        }

        __try {
            return *reinterpret_cast<T*>(addr);
        }
        __except (1) {
            return T{};
        }
    }

    template <typename T>
    inline bool Write(uintptr_t addr, T value) {
        static_assert(std::is_trivially_copyable_v<T>,
                      "Globals::Write<T> only supports trivially-copyable types");
        if (!IsValidPtr(addr)) {
            return false;
        }

        __try {
            *reinterpret_cast<T*>(addr) = value;
            return true;
        }
        __except (1) {
            return false;
        }
    }

    inline uintptr_t ReadPtr(uintptr_t addr) {
        return Read<uintptr_t>(addr);
    }

    inline bool ReadStdString(uintptr_t nameAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        // IsValidPtr is a fast range check (~2ns). The __try/__except below
        // catches any access violation — VirtualQuery is redundant and costly.
        if (!IsValidPtr(nameAddr)) {
            out[0] = 0;
            return false;
        }

        __try {
            const size_t len = *reinterpret_cast<const size_t*>(nameAddr + 0x10);
            const size_t capacity = *reinterpret_cast<const size_t*>(nameAddr + 0x18);
            if (len == 0 || len >= static_cast<size_t>(maxOut) || capacity < len || capacity > 0x100000) {
                out[0] = 0;
                return false;
            }

            uintptr_t src = 0;
            if (capacity > 15) {
                src = *reinterpret_cast<const uintptr_t*>(nameAddr);
            } else {
                src = nameAddr;
            }

            for (size_t i = 0; i < len; ++i) {
                out[i] = *reinterpret_cast<const char*>(src + i);
            }
            out[len] = 0;
            return true;
        }
        __except (1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool ReadRiotString(uintptr_t nameAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        if (!IsValidPtr(nameAddr)) {
            out[0] = 0;
            return false;
        }

        __try {
            const int len = *reinterpret_cast<const int*>(nameAddr + 0x10);
            const int maxLen = *reinterpret_cast<const int*>(nameAddr + 0x14);
            if (len <= 0 || len >= maxOut || maxLen < 0 || maxLen > 0x10000) {
                out[0] = 0;
                return false;
            }

            uintptr_t src = 0;
            if (maxLen >= 16) {
                src = *reinterpret_cast<const uintptr_t*>(nameAddr);
            } else {
                src = nameAddr;
            }

            for (int i = 0; i < len; ++i) {
                out[i] = *reinterpret_cast<const char*>(src + static_cast<uintptr_t>(i));
            }
            out[len] = 0;
            return true;
        }
        __except (1) {
            out[0] = 0;
            return false;
        }
    }

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

    inline bool IsRuntimeStringChar(unsigned char ch) {
        return ch >= 0x20 && ch <= 0x7E;
    }

    inline bool CopyRuntimeChars(uintptr_t src, size_t len, char* out, int maxOut) {
        if (!out || maxOut <= 1 || len == 0 || len >= static_cast<size_t>(maxOut) ||
            !IsValidPtr(src)) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        __try {
            for (size_t i = 0; i < len; ++i) {
                const unsigned char ch =
                    *reinterpret_cast<const unsigned char*>(src + i);
                if (!IsRuntimeStringChar(ch)) {
                    out[0] = 0;
                    return false;
                }
                out[i] = static_cast<char>(ch);
            }
            out[len] = 0;
            return true;
        }
        __except (1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool ReadCompactString(uintptr_t nameAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        if (!IsValidPtr(nameAddr)) {
            out[0] = 0;
            return false;
        }

        __try {
            const uintptr_t ptr = *reinterpret_cast<const uintptr_t*>(nameAddr);
            const int len = *reinterpret_cast<const int*>(nameAddr + 0x8);
            const int capacity = *reinterpret_cast<const int*>(nameAddr + 0xC);
            if (len <= 0 || len >= maxOut || capacity < len || capacity > 0x10000) {
                out[0] = 0;
                return false;
            }
            return CopyRuntimeChars(ptr, static_cast<size_t>(len), out, maxOut);
        }
        __except (1) {
            out[0] = 0;
            return false;
        }
    }

    inline bool ReadCString(uintptr_t strAddr, char* out, int maxOut) {
        if (!out || maxOut <= 1) {
            if (out && maxOut > 0) {
                out[0] = 0;
            }
            return false;
        }

        if (!IsValidPtr(strAddr)) {
            out[0] = 0;
            return false;
        }

        __try {
            int count = 0;
            for (; count < (maxOut - 1); ++count) {
                const unsigned char ch =
                    *(reinterpret_cast<const unsigned char*>(strAddr) + count);
                if (ch == 0) {
                    out[count] = 0;
                    return count > 0;
                }

                if (!IsRuntimeStringChar(ch)) {
                    out[0] = 0;
                    return false;
                }
                out[count] = static_cast<char>(ch);
            }

            out[0] = 0;
            return false;
        }
        __except (1) {
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

        if (ReadCompactString(fieldAddr, out, maxOut)) {
            return true;
        }

        const uintptr_t ptr = Read<uintptr_t>(fieldAddr);
        if (ReadCString(ptr, out, maxOut)) {
            return true;
        }

        return ReadCString(fieldAddr, out, maxOut);
    }

    inline int ReadPtrArray(uintptr_t listAddr, int count, uintptr_t* out, int maxOut) {
        if (!out || count <= 0 || count > maxOut || !IsValidPtr(listAddr)) {
            return 0;
        }

        __try {
            for (int i = 0; i < count; ++i) {
                out[i] = Read<uintptr_t>(listAddr + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
            }
            return count;
        }
        __except (1) {
            return 0;
        }
    }

} // namespace Globals
