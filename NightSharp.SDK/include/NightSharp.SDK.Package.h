#pragma once

#include <cstddef>
#include <cstdint>

namespace NightSharp::Package {

inline constexpr std::uint32_t kMagic = 0x3147504Eu; // "NPG1" little-endian
inline constexpr std::uint32_t kFormatVersion = 1;
inline constexpr std::uint32_t kFlagEncrypted = 1u << 0;

inline constexpr std::uint64_t kFnvOffset = 14695981039346656037ull;
inline constexpr std::uint64_t kFnvPrime = 1099511628211ull;

inline constexpr std::uint64_t kStreamKey0 = 0x6E69676874736861ull;
inline constexpr std::uint64_t kStreamKey1 = 0x72702D6E732D7031ull;
inline constexpr std::uint64_t kStreamKey2 = 0x53444B2D706C7567ull;
inline constexpr std::uint64_t kSignatureKey0 = 0x4E5350472D736967ull;
inline constexpr std::uint64_t kSignatureKey1 = 0x6873682D61626931ull;

#pragma pack(push, 1)
struct Header {
    std::uint32_t Magic;
    std::uint32_t HeaderSize;
    std::uint32_t FormatVersion;
    std::uint32_t Flags;
    std::uint32_t AbiRevision;
    std::uint32_t Category;
    std::uint32_t AutoLoad;
    std::uint32_t Reserved0;
    std::uint64_t OriginalSize;
    std::uint64_t PayloadSize;
    std::uint64_t PlainHash;
    std::uint64_t CipherHash;
    std::uint64_t Signature0;
    std::uint64_t Signature1;
    std::uint64_t CreatedUnix;
    char SdkAbiId[64];
    char Name[96];
    char InternalId[96];
    char Author[96];
    char ChampionName[64];
    char PluginVersion[32];
    char Dependencies[512];
    std::uint8_t Nonce[16];
    std::uint8_t Reserved1[32];
};
#pragma pack(pop)

static_assert(sizeof(Header) == 1096, "NightSharp .NS header size changed");

inline std::uint64_t RotateLeft64(std::uint64_t value, unsigned shift) noexcept {
    shift &= 63u;
    return shift ? ((value << shift) | (value >> (64u - shift))) : value;
}

inline std::uint64_t Mix64(std::uint64_t value) noexcept {
    value += 0x9E3779B97F4A7C15ull;
    value = (value ^ (value >> 30u)) * 0xBF58476D1CE4E5B9ull;
    value = (value ^ (value >> 27u)) * 0x94D049BB133111EBull;
    return value ^ (value >> 31u);
}

inline std::uint64_t Fnv1a64(const void* data,
                            std::size_t size,
                            std::uint64_t seed = kFnvOffset) noexcept {
    auto* bytes = static_cast<const std::uint8_t*>(data);
    std::uint64_t hash = seed;
    for (std::size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kFnvPrime;
    }
    return hash;
}

inline std::uint64_t ReadU64Le(const std::uint8_t* data) noexcept {
    std::uint64_t value = 0;
    for (int i = 7; i >= 0; --i) {
        value <<= 8u;
        value |= data[i];
    }
    return value;
}

inline std::uint64_t StreamBlock(const Header& header, std::uint64_t counter) noexcept {
    const std::uint64_t n0 = ReadU64Le(header.Nonce);
    const std::uint64_t n1 = ReadU64Le(header.Nonce + 8);
    const std::uint64_t a = kStreamKey0 ^ header.PlainHash ^ n0 ^ Mix64(counter);
    const std::uint64_t b = kStreamKey1 ^ header.PayloadSize ^ n1 ^ RotateLeft64(counter, 17);
    return Mix64(a + counter * 0xD6E8FEB86659FD93ull) ^ Mix64(b + kStreamKey2 + counter);
}

inline void XorCrypt(const Header& header, std::uint8_t* data, std::size_t size) noexcept {
    if (!data || size == 0) {
        return;
    }

    std::uint64_t block = 0;
    for (std::size_t i = 0; i < size; ++i) {
        if ((i & 7u) == 0) {
            block = StreamBlock(header, static_cast<std::uint64_t>(i >> 3u));
        }
        data[i] ^= static_cast<std::uint8_t>((block >> ((i & 7u) * 8u)) & 0xFFu);
    }
}

inline std::uint64_t HeaderHashForSignature(const Header& header,
                                            std::uint64_t seed) noexcept {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(&header);
    constexpr std::size_t sigOffset = offsetof(Header, Signature0);
    constexpr std::size_t afterSigOffset = offsetof(Header, CreatedUnix);

    std::uint64_t hash = Fnv1a64(bytes, sigOffset, seed);
    hash = Fnv1a64(bytes + afterSigOffset,
                   sizeof(Header) - afterSigOffset,
                   hash);
    return hash;
}

inline void MakeSignature(const Header& header,
                          const std::uint8_t* cipherPayload,
                          std::size_t cipherSize,
                          std::uint64_t& out0,
                          std::uint64_t& out1) noexcept {
    const std::uint64_t headerHash = HeaderHashForSignature(header, kSignatureKey0);
    const std::uint64_t payloadHash0 = Fnv1a64(cipherPayload, cipherSize, kSignatureKey1);
    const std::uint64_t payloadHash1 = Fnv1a64(cipherPayload, cipherSize, headerHash ^ kFnvOffset);
    out0 = Mix64(headerHash ^ payloadHash0 ^ header.CipherHash ^ kSignatureKey0);
    out1 = Mix64(RotateLeft64(headerHash, 29) ^ payloadHash1 ^ header.PayloadSize ^ kSignatureKey1);
}

inline bool VerifySignature(const Header& header,
                            const std::uint8_t* cipherPayload,
                            std::size_t cipherSize) noexcept {
    std::uint64_t expected0 = 0;
    std::uint64_t expected1 = 0;
    MakeSignature(header, cipherPayload, cipherSize, expected0, expected1);
    return header.Signature0 == expected0 && header.Signature1 == expected1;
}

inline void CopyFixed(char* dst, std::size_t dstSize, const char* src) noexcept {
    if (!dst || dstSize == 0) {
        return;
    }

    std::size_t i = 0;
    if (src) {
        for (; i + 1 < dstSize && src[i]; ++i) {
            dst[i] = src[i];
        }
    }
    dst[i] = '\0';
    for (++i; i < dstSize; ++i) {
        dst[i] = '\0';
    }
}

} // namespace NightSharp::Package
