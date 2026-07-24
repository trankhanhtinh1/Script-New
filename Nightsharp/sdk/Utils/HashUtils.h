#pragma once

#include <cstdint>
#include <cstddef>

namespace SDK {
namespace Utils {

// FNV-1a 32-bit Hash Constants
constexpr uint32_t kFnv1aOffsetBasis32 = 0x811c9dc5u;
constexpr uint32_t kFnv1aPrime32       = 0x01000193u;

// Case-Insensitive FNV-1a Hash for C-strings (O(N) single pass, 0 allocation)
constexpr uint32_t HashName(const char* str) {
    if (!str) return 0;
    uint32_t hash = kFnv1aOffsetBasis32;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= kFnv1aPrime32;
    }
    return hash;
}

// Case-Insensitive FNV-1a Hash with explicit string length
constexpr uint32_t HashNameLen(const char* str, size_t len) {
    if (!str) return 0;
    uint32_t hash = kFnv1aOffsetBasis32;
    for (size_t i = 0; i < len; ++i) {
        char c = str[i];
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + 32);
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(c));
        hash *= kFnv1aPrime32;
    }
    return hash;
}

// Case-Sensitive FNV-1a Hash for C-strings
constexpr uint32_t HashNameSensitive(const char* str) {
    if (!str) return 0;
    uint32_t hash = kFnv1aOffsetBasis32;
    for (size_t i = 0; str[i] != '\0'; ++i) {
        hash ^= static_cast<uint32_t>(static_cast<unsigned char>(str[i]));
        hash *= kFnv1aPrime32;
    }
    return hash;
}

// Fast 0-allocation case-insensitive string equality using FNV-1a hashes
inline bool StrEqualInsensitive(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return HashName(a) == HashName(b);
}

} // namespace Utils
} // namespace SDK

namespace Literals {
    constexpr uint32_t operator""_hashLower(const char* str, size_t len) {
        return SDK::Utils::HashNameLen(str, len);
    }
}
