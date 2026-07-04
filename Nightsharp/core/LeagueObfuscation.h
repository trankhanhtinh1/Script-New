#pragma once

#include <cstddef>
#include <cstdint>

// Riot xor_value<T> layout used by R3nzSkin/encryption.hpp:
// xorKey, valueTable[4], isInit, xorCount32, xorCount8, valueIndex.
template <typename T = int>
struct LeagueObfuscation {
    T xorKey;
    T valueTable[4];
    bool isInit;
    unsigned char xorCount32;
    unsigned char xorCount8;
    unsigned char valueIndex;
};

template <typename T = int>
inline T Decrypt(const LeagueObfuscation<T>& data) {
    if (!data.isInit) return T{};
    if (data.xorCount8 > sizeof(T)) return T{};
    constexpr size_t kMaxChunks = sizeof(T) / sizeof(uint32_t);
    if (data.xorCount32 > kMaxChunks) return T{};
    if (data.valueIndex > 3) return T{};

    auto xoredValue = data.valueTable[data.valueIndex];
    auto xorKeyValue = data.xorKey;

    auto* xored32 = reinterpret_cast<uint32_t*>(&xoredValue);
    const auto* key32 = reinterpret_cast<const uint32_t*>(&xorKeyValue);
    for (size_t i = 0; i < data.xorCount32; ++i) {
        xored32[i] ^= ~key32[i];
    }

    auto* xored8 = reinterpret_cast<uint8_t*>(&xoredValue);
    const auto* key8 = reinterpret_cast<const uint8_t*>(&xorKeyValue);
    for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i) {
        xored8[i] ^= ~key8[i];
    }

    return xoredValue;
}

template <typename T = int>
inline void Encrypt(LeagueObfuscation<T>& data, T value) {
    if (!data.isInit) return;

    auto xorKeyValue = data.xorKey;
    auto xoredValue = value;

    auto* xored32 = reinterpret_cast<uint32_t*>(&xoredValue);
    const auto* key32 = reinterpret_cast<const uint32_t*>(&xorKeyValue);
    for (size_t i = 0; i < data.xorCount32; ++i) {
        xored32[i] ^= ~key32[i];
    }

    auto* xored8 = reinterpret_cast<uint8_t*>(&xoredValue);
    const auto* key8 = reinterpret_cast<const uint8_t*>(&xorKeyValue);
    for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i) {
        xored8[i] ^= ~key8[i];
    }

    data.valueIndex = (data.valueIndex + 1) & 3;
    data.valueTable[data.valueIndex] = xoredValue;
}
