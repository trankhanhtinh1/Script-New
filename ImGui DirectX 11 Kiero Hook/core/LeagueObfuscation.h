#pragma once
#include <cstdint>

// ============================================================================
// League Obfuscation Decryption
// Layout: xorKey → valueTable[4] → isInit, xorCount64, xorCount8, valueIndex
// ============================================================================

template<typename T = int>
struct LeagueObfuscation
{
    T xorKey;                    // +0x00
    T valueTable[4];             // +sizeof(T)
    bool isInit;                 // +5*sizeof(T)
    unsigned char xorCount64;    // +5*sizeof(T)+1
    unsigned char xorCount8;     // +5*sizeof(T)+2
    unsigned char valueIndex;    // +5*sizeof(T)+3
};

// Sizes:
// LeagueObfuscation<short>  = 14 bytes
// LeagueObfuscation<int>    = 24 bytes
// LeagueObfuscation<float>  = 24 bytes

template<typename T = int>
inline T Decrypt(const LeagueObfuscation<T>& data)
{
    if (!data.isInit) return T{};
    if (data.xorCount8 != 0 && data.xorCount8 > sizeof(T)) return T{};
    if (data.xorCount64 != 0 && data.xorCount64 > sizeof(T)) return T{};
    if (data.valueIndex > 3) return T{};

    auto tXoredValue = data.valueTable[data.valueIndex];
    auto tXorKeyValue = data.xorKey;

    // Phase 1: XOR with 64-bit chunks
    int xorCount64 = data.xorCount64 >= 1 ? 1 : 0;
    {
        auto tXorValuePtr = reinterpret_cast<const uint64_t*>(&tXorKeyValue);
        for (int i = 0; i < xorCount64; i++)
            *(reinterpret_cast<uint64_t*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
    }

    // Phase 2: XOR remaining individual bytes
    {
        auto tXorValuePtr = reinterpret_cast<const unsigned char*>(&tXorKeyValue);
        for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
            *(reinterpret_cast<unsigned char*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
    }

    return tXoredValue;
}

template<typename T = int>
inline void Encrypt(LeagueObfuscation<T>& data, T value)
{
    if (!data.isInit) return;

    auto tXorKeyValue = data.xorKey;
    auto tXoredValue = value;

    // Reverse Phase 2
    {
        auto tXorValuePtr = reinterpret_cast<const unsigned char*>(&tXorKeyValue);
        for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
            *(reinterpret_cast<unsigned char*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
    }

    // Reverse Phase 1
    int xorCount64 = data.xorCount64 >= 1 ? 1 : 0;
    {
        auto tXorValuePtr = reinterpret_cast<const uint64_t*>(&tXorKeyValue);
        for (int i = 0; i < xorCount64; i++)
            *(reinterpret_cast<uint64_t*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
    }

    // Rotate index and store
    data.valueIndex = (data.valueIndex + 1) & 3;
    data.valueTable[data.valueIndex] = tXoredValue;
}
