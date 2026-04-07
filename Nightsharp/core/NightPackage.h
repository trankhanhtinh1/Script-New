// ============================================================================
// NightPackage.h — .night Package Format Definitions
//
// Defines the binary format for .night plugin packages:
//   [PackageHeader | Manifest | EncryptedPayload]
//
// The payload is an XOR-encrypted PE (.exe) built from the plugin source.
// NightSharp runtime reads this format to load external plugins.
//
// POD only — safe for manual map (no CRT dependency).
// ============================================================================

#pragma once

#include <cstdint>
#include <cstring>

namespace NightPackage {

    // ============================================================================
    // Constants
    // ============================================================================
    constexpr uint32_t MAGIC   = 0x4E495445;  // "NITE" (NightSharp)
    constexpr uint32_t VERSION = 1;

    // ============================================================================
    // Hash algorithms
    // ============================================================================
    enum class HashAlgo : uint32_t {
        None   = 0,
        FNV1a  = 1,
    };

    // ============================================================================
    // Package Header (fixed 128 bytes)
    // ============================================================================
    struct PackageHeader {
        uint32_t Magic;           // Must be MAGIC
        uint32_t Version;         // Package format version
        uint32_t Flags;           // Reserved flags
        uint32_t ManifestOff;     // Offset to manifest from start of file
        uint32_t ManifestLen;     // Size of manifest in bytes
        uint32_t PayloadOff;      // Offset to encrypted payload
        uint32_t PayloadLen;      // Size of encrypted payload
        uint32_t OriginalLen;     // Original (unencrypted) payload size
        uint32_t HashAlgo;        // Hash algorithm used (see HashAlgo enum)
        uint8_t  Hash[32];        // Integrity hash of encrypted payload
        uint8_t  XorSalt[8];     // Salt for XOR encryption
        uint8_t  Reserved[52];   // Reserved for future use (padding to 128)
    };
    static_assert(sizeof(PackageHeader) == 128, "PackageHeader must be 128 bytes");

    // ============================================================================
    // Manifest (matches NightSharp_PluginInfo layout, fixed 640 bytes)
    // ============================================================================
    struct Manifest {
        uint32_t ApiVersion;
        char     PluginId[64];
        char     Name[128];
        char     Author[64];
        char     Description[256];
        char     Version[32];
        uint32_t Type;                      // NightSharp_PluginType
        uint32_t SupportedChampionCount;
        char     SupportedChampions[8][32];
        uint64_t BuildTimestamp;            // Unix timestamp of build
        uint8_t  Reserved[16];
    };

    // ============================================================================
    // XOR Encryption / Decryption (symmetric)
    // ============================================================================
    inline void XorEncryptDecrypt(uint8_t* data, uint32_t size,
                                  const char* pluginId, const uint8_t salt[8])
    {
        // Build key: interleave pluginId bytes with salt
        uint8_t key[72] = {};
        int keyLen = 0;
        for (int i = 0; i < 64 && pluginId[i]; i++) {
            key[keyLen++] = (uint8_t)pluginId[i];
        }
        for (int i = 0; i < 8; i++) {
            key[keyLen++] = salt[i];
        }
        if (keyLen == 0) return;

        for (uint32_t i = 0; i < size; i++) {
            data[i] ^= key[i % keyLen];
        }
    }

    // ============================================================================
    // FNV-1a 64-bit Hash
    // ============================================================================
    inline uint64_t FNV1aHash(const uint8_t* data, uint32_t size) {
        uint64_t hash = 0xcbf29ce484222325ULL;
        for (uint32_t i = 0; i < size; i++) {
            hash ^= data[i];
            hash *= 0x100000001b3ULL;
        }
        return hash;
    }

} // namespace NightPackage
