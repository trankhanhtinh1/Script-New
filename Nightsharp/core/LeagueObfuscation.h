#pragma once
#include <cstdint>
#include <cstddef>

// ============================================================================
// League Obfuscation Decryption
// Layout: xorKey → valueTable[4] → isInit, xorCount32, xorCount8, valueIndex
//
// IMPORTANT: Riot's xor_value<T> uses **uint32_t chunks** for the bulk XOR
// (see R3nzSkin/encryption.hpp). The legacy field name `xorCount64` was
// misleading — it has always counted 4-byte chunks.  Reading via uint64*
// (sizeof T < 8) reads past the stack buffer and corrupts adjacent locals.
// May 2026: switched to correct uint32 chunking; result identical for
// T=uint64 (2x uint32 XOR ≡ 1x uint64 XOR), now safe for T=int32 (skin id).
// ============================================================================

template<typename T = int>
struct LeagueObfuscation
{
    T xorKey;                    // +0x00
    T valueTable[4];             // +sizeof(T)
    bool isInit;                 // +5*sizeof(T)
    unsigned char xorCount32;    // +5*sizeof(T)+1   (count of 4-byte chunks)
    unsigned char xorCount8;     // +5*sizeof(T)+2   (count of trailing bytes)
    unsigned char valueIndex;    // +5*sizeof(T)+3
};

// Sizes:
// LeagueObfuscation<short>  = 14 bytes
// LeagueObfuscation<int>    = 24 bytes
// LeagueObfuscation<float>  = 24 bytes
// LeagueObfuscation<uint64> = 56 bytes

template<typename T = int>
inline T Decrypt(const LeagueObfuscation<T>& data)
{
    if (!data.isInit) return T{};
    if (data.xorCount8 > sizeof(T)) return T{};
    constexpr size_t kMaxChunks = sizeof(T) / sizeof(uint32_t);
    if (data.xorCount32 > kMaxChunks) return T{};
    if (data.valueIndex > 3) return T{};

    auto tXoredValue = data.valueTable[data.valueIndex];
    auto tXorKeyValue = data.xorKey;

    // Phase 1: XOR with 32-bit chunks (R3nzSkin xor_value layout)
    {
        auto* xored32 = reinterpret_cast<uint32_t*>(&tXoredValue);
        const auto* key32 = reinterpret_cast<const uint32_t*>(&tXorKeyValue);
        for (size_t i = 0; i < data.xorCount32; ++i)
            xored32[i] ^= ~key32[i];
    }

    // Phase 2: XOR remaining trailing bytes (for sub-uint32 sizes)
    {
        auto* xored8 = reinterpret_cast<uint8_t*>(&tXoredValue);
        const auto* key8 = reinterpret_cast<const uint8_t*>(&tXorKeyValue);
        for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
            xored8[i] ^= ~key8[i];
    }

    return tXoredValue;
}

template<typename T = int>
inline void Encrypt(LeagueObfuscation<T>& data, T value)
{
    if (!data.isInit) return;

    auto tXorKeyValue = data.xorKey;
    auto tXoredValue = value;

    // Phase 1 (matches Decrypt order): XOR with 32-bit chunks
    {
        auto* xored32 = reinterpret_cast<uint32_t*>(&tXoredValue);
        const auto* key32 = reinterpret_cast<const uint32_t*>(&tXorKeyValue);
        for (size_t i = 0; i < data.xorCount32; ++i)
            xored32[i] ^= ~key32[i];
    }

    // Phase 2: XOR trailing bytes
    {
        auto* xored8 = reinterpret_cast<uint8_t*>(&tXoredValue);
        const auto* key8 = reinterpret_cast<const uint8_t*>(&tXorKeyValue);
        for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
            xored8[i] ^= ~key8[i];
    }

    // Rotate index and store (R3nzSkin: pre-increment, modulo 4)
    data.valueIndex = (data.valueIndex + 1) & 3;
    data.valueTable[data.valueIndex] = tXoredValue;
}
