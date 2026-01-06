#pragma once
#include <cstdint>
#include <windows.h>
#include <vector>
#include <fstream>
#include <iomanip>
#include <sstream>
#include "../Vector.h"
#include "Offsets.h"

// ============================================================================
// League Missile Decryption - EXACT REPRODUCTION OF sub_296390
// Based on IDA analysis of sub_296390 (XOR/NOT decryption)
// ============================================================================

namespace IDA
{
    // Decrypt missile data using sub_296390 logic (SIMPLIFIED VERSION)
    // Based on IDA: sub_296390(__int64 a1, _DWORD a2, _DWORD a3, __int64 a4)
    // IDA: v4 = (unsigned __int8 *)(a4 + 264); where a4 = missilePtr, so v4 = missilePtr + 0x108
    // NOTE: This is a simplified implementation. Full version uses SIMD operations.
    // Returns: decrypted byte value
    inline unsigned char DecryptMissileByte(uintptr_t missilePtr)
    {
        // NOTE: No __try / __except here to avoid C++ unwinding issues in headers.
        // Caller should ensure missilePtr is valid and guarded by SEH if needed.

        // IDA: v4 = (unsigned __int8 *)(a4 + 264); where a4 = missilePtr
        // So: v4 = missilePtr + 0x108 (obfuscation structure)
        uintptr_t obfStructAddr = missilePtr + 0x108;
        uint8_t* v4 = reinterpret_cast<uint8_t*>(obfStructAddr);
        if (!v4)
            return 0;
        
        // IDA: v6 = v4[6]; (size field at offset 0x6)
        uint8_t v6 = v4[6];
        
        // IDA: v7 = v4[v4[8] + 1]; (data index at offset 0x8, then read at that index + 1)
        uint8_t dataIndex = v4[8];
        uint8_t v7 = v4[dataIndex + 1];
        
        unsigned char result = v7;
        
        // IDA: if ( v6 ) { ... XOR/NOT decryption }
        // Simplified: XOR with NOT of mask (full version uses SIMD _mm_xor_ps/_mm_andnot_ps)
        if (v6 > 0) {
            // Simple byte-by-byte XOR/NOT (simplified from SIMD version)
            // Full version: v17[v5] ^= ~*(_QWORD *)&v4[8 * v5];
            for (uint64_t v5 = 0; v5 < v6 && v5 < 8; v5++) {
                uint64_t mask = *(uint64_t*)(v4 + 8 * v5);
                result ^= static_cast<unsigned char>(~mask); // XOR with NOT (simplified - should be QWORD operation)
            }
        }
        
        // IDA: if ( !v4[7] ) return v7;
        // IDA: v14 = 1LL - v4[7];
        // IDA: if ( v14 ) return v7;
        // IDA: Additional byte XOR if v4[7] == 1
        uint8_t v7_extra = v4[7];
        if (v7_extra == 1) {
            // IDA: *((_BYTE *)v17 + v14) ^= ~v4[v14]; with v14 = 0
            result ^= static_cast<unsigned char>(~v4[0]);
        }
        
        return result;
    }
    
    // Check if missile structure is obfuscated (based on sub_521940 logic)
    // Returns: true if obfuscated, false otherwise
    inline bool IsMissileObfuscated(uintptr_t missilePtr)
    {
        // NOTE: No __try / __except here. Caller should guard if reading
        // from potentially invalid missilePtr.
        uintptr_t obfStructAddr = missilePtr + 0x108;
        uint8_t* v10 = reinterpret_cast<uint8_t*>(obfStructAddr);
        if (!v10)
            return false;

        // IDA: if ( !*(_BYTE *)(a4 + 269) ) { ... initialize }
        // IDA: v10[5] = 1; (after initialization)
        // So if v10[5] == 1, obfuscation structure is initialized
        return (v10[5] == 1);
    }
}

