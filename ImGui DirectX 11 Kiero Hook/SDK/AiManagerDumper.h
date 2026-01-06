#pragma once
#include <Windows.h>
#include <cstdio>
#include <cmath>
#include "Offsets.h"
#include "../Vector.h"

// ============================================================================
// AI MANAGER OFFSET DUMPER
// ============================================================================
// Sử dụng phương pháp giải mã hóa từ leagueoflegends-master
// để tìm các offset AiManager chính xác
//
// Offset từ leagueoflegends-master (đã verified):
// oObjAiManager = 0x36F0 (with obfuscation)
// oObjAiManagerManager = 0x10
// oObjAiManagerManagerTargetPosition = 0x14
// oObjAiManagerManagerIsMoving = 0x2BC
// oObjAiManagerManagerCurrentSegment = 0x2C0
// oObjAiManagerManagerPathStart = 0x2D0
// oObjAiManagerManagerPathEnd = 0x2DC
// oObjAiManagerManagerSegments = 0x2E8
// oObjAiManagerManagerSegmentsCount = 0x2F0
// oObjAiManagerManagerDashSpeed = 0x300
// oObjAiManagerManagerIsDashing = 0x324
// oObjAiManagerManagerPosition = 0x414
// ============================================================================

namespace AiManagerDumper
{
    // ============================================================================
    // LeagueObfuscation Structure (from leagueoflegends-master)
    // ============================================================================
    template<typename T = int>
    struct LeagueObfuscation
    {
        bool isInit;
        unsigned char xorCount64;
        unsigned char xorCount8;
        T xorKey;
        unsigned char valueIndex;
        T valueTable[4];
    };

    template<typename T = int>
    inline T Decrypt(const LeagueObfuscation<T>& data)
    {
        if (!data.isInit) return 0;
        if (data.xorCount8 != 0 && (data.xorCount8 > sizeof(T) || data.xorCount8 < 0)) return 0;
        if (data.xorCount64 != 0 && (data.xorCount64 > sizeof(T) || data.xorCount64 < 0)) return 0;
        if (data.valueIndex > 4) return 0;

        int xorCount64 = data.xorCount64 >= 1 ? 1 : 0;
        auto tXoredValue = data.valueTable[data.valueIndex];
        auto tXorKeyValue = data.xorKey;
        
        {
            auto tXorValuePtr = reinterpret_cast<uint64_t*>(&tXorKeyValue);
            for (auto i = 0; i < xorCount64; i++)
                *(reinterpret_cast<uint64_t*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
        }
        {
            auto tXorValuePtr = reinterpret_cast<unsigned char*>(&tXorKeyValue);
            for (size_t i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
                *(reinterpret_cast<unsigned char*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
        }
        return tXoredValue;
    }

    // ============================================================================
    // Tìm AiManager bằng phương pháp Obfuscation (leagueoflegends-master)
    // ============================================================================
    inline uint64_t GetAiManagerObfuscated(uint64_t objAddress)
    {
        if (!objAddress) return 0;
        
        // Các offset cần thử (phổ biến từ nhiều patch)
        // Offset 0x36F0 từ leagueoflegends-master
        const uint64_t aiManagerOffsets[] = { 0x36F0, 0x3700, 0x3710, 0x3720, 0x3680, 0x3690 };
        
        for (uint64_t offset : aiManagerOffsets)
        {
            __try {
                LeagueObfuscation<uint64_t> aiManagerObf = *(LeagueObfuscation<uint64_t>*)(objAddress + offset);
                
                if (!aiManagerObf.isInit) continue;
                
                uint64_t decrypted = Decrypt(aiManagerObf);
                if (!decrypted) continue;
                
                // Kiểm tra pointer có valid không (không 0, không quá lớn)
                if (decrypted < 0x10000000000 || decrypted > 0x800000000000) continue;
                
                // Đọc manager pointer (+0x10)
                uint64_t manager = *(uint64_t*)(decrypted + 0x10);
                if (manager < 0x10000000000 || manager > 0x800000000000) continue;
                
                return manager;
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                continue;
            }
        }
        
        return 0;
    }

    // ============================================================================
    // Tìm AiManager bằng phương pháp trực tiếp (Internal_OrbWalker hiện tại)
    // ============================================================================
    inline uint64_t GetAiManagerDirect(uint64_t objAddress)
    {
        if (!objAddress) return 0;
        
        __try {
            return *(uint64_t*)(objAddress + 0x3108);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            return 0;
        }
    }

    // ============================================================================
    // DUMP FUNCTION - So sánh cả 2 phương pháp và tìm offset chính xác
    // ============================================================================
    inline void DumpAndCompare()
    {
        FILE* f = fopen("aimanager_dumper.txt", "a");
        if (!f) return;

        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t localPlayer = 0;

        __try {
            localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "[CRASH] Cannot read LocalPlayer\n");
            fclose(f);
            return;
        }

        if (!localPlayer) {
            fprintf(f, "[ERROR] LocalPlayer is NULL\n");
            fclose(f);
            return;
        }

        // Đọc vị trí hiện tại của player
        Vector3 playerPos;
        __try {
            playerPos = *(Vector3*)(localPlayer + Offset::oObjPosition);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            playerPos = Vector3(0, 0, 0);
        }

        float gameTime = 0;
        __try {
            gameTime = *(float*)(moduleBase + Offset::oGametime);
        } __except(EXCEPTION_EXECUTE_HANDLER) {}

        fprintf(f, "\n============================================\n");
        fprintf(f, "AI MANAGER DUMPER - Time: %.2f\n", gameTime);
        fprintf(f, "============================================\n");
        fprintf(f, "LocalPlayer: 0x%llX\n", localPlayer);
        fprintf(f, "PlayerPosition: (%.1f, %.1f, %.1f)\n\n", playerPos.x, playerPos.y, playerPos.z);

        // ============================================================================
        // PHƯƠNG PHÁP 1: Trực tiếp (hiện tại Internal_OrbWalker)
        // ============================================================================
        fprintf(f, "=== PHƯƠNG PHÁP 1: TRỰC TIẾP (0x3108) ===\n");
        uint64_t aiManagerDirect = GetAiManagerDirect(localPlayer);
        fprintf(f, "AiManager (Direct): 0x%llX\n", aiManagerDirect);
        
        if (aiManagerDirect) {
            // Check các offset hiện tại
            __try {
                Vector3 startPath = *(Vector3*)(aiManagerDirect + 0x1E0);
                Vector3 endPath = *(Vector3*)(aiManagerDirect + 0x23C);
                fprintf(f, "  [0x1E0] StartPath: (%.1f, %.1f, %.1f)\n", startPath.x, startPath.y, startPath.z);
                fprintf(f, "  [0x23C] EndPath:   (%.1f, %.1f, %.1f)\n", endPath.x, endPath.y, endPath.z);
                fprintf(f, "  Distance: %.1f (IsMoving: %s)\n", startPath.Distance(endPath), 
                        startPath.Distance(endPath) > 5.0f ? "YES" : "NO");
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [CRASH reading offsets]\n");
            }
        }

        // ============================================================================
        // PHƯƠNG PHÁP 2: Obfuscation (leagueoflegends-master)
        // ============================================================================
        fprintf(f, "\n=== PHƯƠNG PHÁP 2: OBFUSCATION (0x36F0 + Decrypt) ===\n");
        
        // Thử từng offset
        const uint64_t aiManagerOffsets[] = { 0x36F0, 0x3700, 0x3710, 0x3720, 0x3680, 0x3690 };
        
        for (uint64_t offset : aiManagerOffsets)
        {
            __try {
                LeagueObfuscation<uint64_t> aiManagerObf = *(LeagueObfuscation<uint64_t>*)(localPlayer + offset);
                
                fprintf(f, "\n[+0x%04llX] ", offset);
                fprintf(f, "isInit=%d xorCount64=%d xorCount8=%d valueIndex=%d\n", 
                        aiManagerObf.isInit, aiManagerObf.xorCount64, aiManagerObf.xorCount8, aiManagerObf.valueIndex);
                
                if (!aiManagerObf.isInit) {
                    fprintf(f, "         -> NOT INIT\n");
                    continue;
                }
                
                uint64_t decrypted = Decrypt(aiManagerObf);
                fprintf(f, "         Decrypted: 0x%llX\n", decrypted);
                
                if (!decrypted) continue;
                if (decrypted < 0x10000000000 || decrypted > 0x800000000000) {
                    fprintf(f, "         -> INVALID POINTER\n");
                    continue;
                }
                
                uint64_t manager = *(uint64_t*)(decrypted + 0x10);
                fprintf(f, "         Manager (+0x10): 0x%llX\n", manager);
                
                if (manager < 0x10000000000 || manager > 0x800000000000) continue;
                
                // Test các offset từ leagueoflegends-master
                fprintf(f, "\n         --- Testing leagueoflegends-master offsets ---\n");
                
                // oObjAiManagerManagerTargetPosition = 0x14
                __try {
                    Vector3 targetPos = *(Vector3*)(manager + 0x14);
                    float dist = targetPos.Distance(playerPos);
                    fprintf(f, "         [0x014] TargetPos: (%.1f, %.1f, %.1f) dist=%.1f", 
                            targetPos.x, targetPos.y, targetPos.z, dist);
                    if (dist < 100) fprintf(f, " <-- VALID!");
                    fprintf(f, "\n");
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerIsMoving = 0x2BC
                __try {
                    bool isMoving = *(bool*)(manager + 0x2BC);
                    fprintf(f, "         [0x2BC] IsMoving: %d\n", isMoving);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerCurrentSegment = 0x2C0
                __try {
                    int curSeg = *(int*)(manager + 0x2C0);
                    fprintf(f, "         [0x2C0] CurrentSegment: %d\n", curSeg);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerPathStart = 0x2D0
                __try {
                    Vector3 pathStart = *(Vector3*)(manager + 0x2D0);
                    float dist = pathStart.Distance(playerPos);
                    fprintf(f, "         [0x2D0] PathStart: (%.1f, %.1f, %.1f) dist=%.1f", 
                            pathStart.x, pathStart.y, pathStart.z, dist);
                    if (dist < 100) fprintf(f, " <-- VALID!");
                    fprintf(f, "\n");
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerPathEnd = 0x2DC
                __try {
                    Vector3 pathEnd = *(Vector3*)(manager + 0x2DC);
                    fprintf(f, "         [0x2DC] PathEnd: (%.1f, %.1f, %.1f)\n", 
                            pathEnd.x, pathEnd.y, pathEnd.z);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerSegmentsCount = 0x2F0
                __try {
                    int segCount = *(int*)(manager + 0x2F0);
                    fprintf(f, "         [0x2F0] SegmentsCount: %d\n", segCount);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerDashSpeed = 0x300
                __try {
                    float dashSpeed = *(float*)(manager + 0x300);
                    fprintf(f, "         [0x300] DashSpeed: %.1f\n", dashSpeed);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerIsDashing = 0x324
                __try {
                    bool isDashing = *(bool*)(manager + 0x324);
                    fprintf(f, "         [0x324] IsDashing: %d\n", isDashing);
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
                // oObjAiManagerManagerPosition = 0x414
                __try {
                    Vector3 pos = *(Vector3*)(manager + 0x414);
                    float dist = pos.Distance(playerPos);
                    fprintf(f, "         [0x414] ServerPosition: (%.1f, %.1f, %.1f) dist=%.1f", 
                            pos.x, pos.y, pos.z, dist);
                    if (dist < 100) fprintf(f, " <-- VALID!");
                    fprintf(f, "\n");
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "[CRASH]\n");
            }
        }

        // ============================================================================
        // SCAN TỰ ĐỘNG: Tìm AiManager trong Player object
        // ============================================================================
        fprintf(f, "\n=== AUTO SCAN: Tìm obfuscation structure trong Player ===\n");
        
        for (uint64_t offset = 0x3000; offset < 0x4000; offset += 0x8)
        {
            __try {
                LeagueObfuscation<uint64_t> obf = *(LeagueObfuscation<uint64_t>*)(localPlayer + offset);
                
                if (!obf.isInit) continue;
                if (obf.xorCount64 > 8 || obf.xorCount8 > 8) continue;
                if (obf.valueIndex > 4) continue;
                
                uint64_t decrypted = Decrypt(obf);
                if (!decrypted) continue;
                if (decrypted < 0x10000000000 || decrypted > 0x800000000000) continue;
                
                // Check nếu decrypted+0x10 chứa pointer valid đến Vec3 gần player position
                uint64_t subPtr = 0;
                __try {
                    subPtr = *(uint64_t*)(decrypted + 0x10);
                } __except(EXCEPTION_EXECUTE_HANDLER) { continue; }
                
                if (subPtr < 0x10000000000 || subPtr > 0x800000000000) continue;
                
                // Scan for player position in this structure
                for (int subOffset = 0; subOffset < 0x500; subOffset += 0xC)
                {
                    __try {
                        Vector3 vec = *(Vector3*)(subPtr + subOffset);
                        float dist = vec.Distance(playerPos);
                        if (dist < 50 && vec.x > 100 && vec.x < 20000) {
                            fprintf(f, "[+0x%04llX] Obf VALID -> Decrypt=0x%llX -> +0x10=0x%llX -> [+0x%03X] = (%.1f, %.1f) dist=%.1f\n",
                                    offset, decrypted, subPtr, subOffset, vec.x, vec.z, dist);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }

        fprintf(f, "\n============================================\n");
        fprintf(f, "END OF DUMP\n");
        fprintf(f, "============================================\n\n");

        fclose(f);
    }

    // ============================================================================
    // Continuous Log - Gọi trong main loop để theo dõi liên tục
    // ============================================================================
    inline void ContinuousLog()
    {
        static float lastLogTime = 0;
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        float gameTime = 0;
        __try {
            gameTime = *(float*)(moduleBase + Offset::oGametime);
        } __except(EXCEPTION_EXECUTE_HANDLER) { return; }
        
        if (gameTime - lastLogTime < 0.5f) return;
        lastLogTime = gameTime;
        
        uint64_t localPlayer = 0;
        __try {
            localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        } __except(EXCEPTION_EXECUTE_HANDLER) { return; }
        if (!localPlayer) return;

        Vector3 playerPos;
        __try {
            playerPos = *(Vector3*)(localPlayer + Offset::oObjPosition);
        } __except(EXCEPTION_EXECUTE_HANDLER) { return; }

        FILE* f = fopen("aimanager_continuous.txt", "a");
        if (!f) return;

        // Thử cả 2 phương pháp
        uint64_t aiDirect = GetAiManagerDirect(localPlayer);
        uint64_t aiObf = GetAiManagerObfuscated(localPlayer);

        fprintf(f, "[%.2f] PlayerPos(%.0f, %.0f) ", gameTime, playerPos.x, playerPos.z);

        // Direct method
        if (aiDirect) {
            __try {
                Vector3 startPath = *(Vector3*)(aiDirect + 0x1E0);
                Vector3 endPath = *(Vector3*)(aiDirect + 0x23C);
                float dist = startPath.Distance(endPath);
                fprintf(f, "| DIRECT: Start(%.0f,%.0f) End(%.0f,%.0f) Dist=%.0f ", 
                        startPath.x, startPath.z, endPath.x, endPath.z, dist);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "| DIRECT: CRASH ");
            }
        }

        // Obfuscated method
        if (aiObf) {
            __try {
                Vector3 pathStart = *(Vector3*)(aiObf + 0x2D0);
                Vector3 pathEnd = *(Vector3*)(aiObf + 0x2DC);
                bool isMoving = *(bool*)(aiObf + 0x2BC);
                bool isDashing = *(bool*)(aiObf + 0x324);
                fprintf(f, "| OBF: Start(%.0f,%.0f) End(%.0f,%.0f) Moving=%d Dashing=%d", 
                        pathStart.x, pathStart.z, pathEnd.x, pathEnd.z, isMoving, isDashing);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "| OBF: CRASH ");
            }
        }

        fprintf(f, "\n");
        fclose(f);
    }
}
