#pragma once
#include <Windows.h>
#include <cstdio>
#include <cmath>
#include "Offsets.h"
#include "../Vector.h"

// ============================================================================
// AI MANAGER DEBUG TOOL
// ============================================================================
// Mục đích: Tìm các offset cho AiManager để phát hiện:
// - IsMoving (đang di chuyển)
// - IsDashing (đang lướt)  
// - TargetPosition (vị trí đích)
// - CurrentPosition (vị trí hiện tại từ server)
// - PathSegments (các điểm đường đi)
//
// Cách sử dụng:
// 1. Vào game, đứng yên -> gọi DumpAiManagerOffsets()
// 2. Di chuyển -> gọi lại DumpAiManagerOffsets()
// 3. Lướt (sử dụng skill lướt) -> gọi lại DumpAiManagerOffsets()
// 4. So sánh các file log để tìm offset thay đổi
// ============================================================================

namespace AiManagerDebug
{
    // Các offset đã biết cần verify/update:
    // From leagueoflegends-master (OBFUSCATED - có XOR encryption)
    // #define oObjAiManager 0x36F0
    // #define oObjAiManagerManager 0x10
    // #define oObjAiManagerManagerTargetPosition 0x14
    // #define oObjAiManagerManagerIsMoving 0x2BC
    // #define oObjAiManagerManagerCurrentSegment 0x2C0
    // #define oObjAiManagerManagerPathStart 0x2D0
    // #define oObjAiManagerManagerPathEnd 0x2DC
    // #define oObjAiManagerManagerSegments 0x2E8
    // #define oObjAiManagerManagerSegmentsCount 0x2F0
    // #define oObjAiManagerManagerDashSpeed 0x300
    // #define oObjAiManagerManagerIsDashing 0x324
    // #define oObjAiManagerManagerPosition 0x414

    // From Internal_OrbWalker (DIRECT - không encryption)
    // inline constexpr uint64_t oObjAiManager = 0x3108;
    // inline constexpr uint64_t oAiManagerStartPath = 0x1E0;
    // inline constexpr uint64_t oAiManagerEndPath = 0x23C;
    // inline constexpr uint64_t oAiManagerHasPath = 0x268;

    // ============================================================================
    // Hàm decrypt LeagueObfuscation (copy từ leagueoflegends-master)
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
            for (auto i = sizeof(T) - data.xorCount8; i < sizeof(T); ++i)
                *(reinterpret_cast<unsigned char*>(&tXoredValue) + i) ^= ~tXorValuePtr[i];
        }
        return tXoredValue;
    }

    // ============================================================================
    // Dump toàn bộ AiManager structure để tìm offset
    // ============================================================================
    inline void DumpAiManagerOffsets()
    {
        FILE* f = fopen("aimanager_debug.txt", "a");
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

        // Đọc vị trí hiện tại của player để so sánh
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
        fprintf(f, "AI MANAGER DEBUG DUMP - Time: %.2f\n", gameTime);
        fprintf(f, "============================================\n");
        fprintf(f, "ModuleBase: 0x%llX\n", moduleBase);
        fprintf(f, "LocalPlayer: 0x%llX\n", localPlayer);
        fprintf(f, "PlayerPosition: (%.1f, %.1f, %.1f)\n\n", playerPos.x, playerPos.y, playerPos.z);

        // ============================================================================
        // PHƯƠNG PHÁP 1: Đọc AiManager TRỰC TIẾP (Internal_OrbWalker style)
        // Offset: 0x3108
        // ============================================================================
        fprintf(f, "=== PHƯƠNG PHÁP 1: TRỰC TIẾP (0x3108) ===\n");
        uint64_t aiManager1 = 0;
        __try {
            aiManager1 = *(uint64_t*)(localPlayer + 0x3108);
            fprintf(f, "AiManager @ [Player + 0x3108] = 0x%llX\n", aiManager1);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "[CRASH] Cannot read [Player + 0x3108]\n");
        }

        if (aiManager1) {
            // Scan known offsets
            fprintf(f, "\n--- Scanning known Internal_OrbWalker offsets ---\n");
            
            // oAiManagerStartPath = 0x1E0
            __try {
                Vector3 startPath = *(Vector3*)(aiManager1 + 0x1E0);
                fprintf(f, "[0x1E0] StartPath: (%.1f, %.1f, %.1f) ", startPath.x, startPath.y, startPath.z);
                float dist = startPath.Distance(playerPos);
                if (dist < 50) fprintf(f, "<-- MATCHES PLAYER POS!");
                fprintf(f, "\n");
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oAiManagerEndPath = 0x23C
            __try {
                Vector3 endPath = *(Vector3*)(aiManager1 + 0x23C);
                fprintf(f, "[0x23C] EndPath: (%.1f, %.1f, %.1f)\n", endPath.x, endPath.y, endPath.z);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oAiManagerHasPath = 0x268
            __try {
                int hasPath = *(int*)(aiManager1 + 0x268);
                fprintf(f, "[0x268] HasPath: %d\n", hasPath);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // Scan toàn bộ structure để tìm Vec3 positions
            fprintf(f, "\n--- Scanning for Vec3 matching player position ---\n");
            for (int off = 0; off < 0x500; off += 4) {
                __try {
                    Vector3 vec = *(Vector3*)(aiManager1 + off);
                    float dist = vec.Distance(playerPos);
                    if (dist < 100 && vec.x > 100 && vec.x < 15000) {
                        fprintf(f, "[0x%03X] Vec3: (%.1f, %.1f, %.1f) dist=%.1f", off, vec.x, vec.y, vec.z, dist);
                        if (dist < 10) fprintf(f, " <-- POSITION!");
                        if (off == 0x1E0) fprintf(f, " [StartPath]");
                        fprintf(f, "\n");
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }

            // Scan for bool/byte IsMoving, IsDashing
            fprintf(f, "\n--- Scanning for IsMoving/IsDashing (bytes 0/1) ---\n");
            fprintf(f, "(Run this while STANDING STILL, then while MOVING, compare!)\n");
            for (int off = 0x250; off < 0x350; off += 1) {
                __try {
                    uint8_t val = *(uint8_t*)(aiManager1 + off);
                    if (val <= 1) {
                        fprintf(f, "[0x%03X] = %d\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }

            // Scan for floats (DashSpeed, etc.)
            fprintf(f, "\n--- Scanning for DashSpeed (floats 300-2000) ---\n");
            for (int off = 0x2F0; off < 0x350; off += 4) {
                __try {
                    float val = *(float*)(aiManager1 + off);
                    if (val >= 300 && val <= 2000) {
                        fprintf(f, "[0x%03X] = %.1f (possible MoveSpeed/DashSpeed)\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }

            // Scan for SegmentsCount (int 0-20)
            fprintf(f, "\n--- Scanning for SegmentsCount (int 0-20) ---\n");
            for (int off = 0x2D0; off < 0x320; off += 4) {
                __try {
                    int val = *(int*)(aiManager1 + off);
                    if (val >= 0 && val <= 20) {
                        fprintf(f, "[0x%03X] = %d\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
        }

        // ============================================================================
        // PHƯƠNG PHÁP 2: Đọc AiManager với OBFUSCATION (leagueoflegends-master style)
        // Offset: 0x36F0 + Decrypt + 0x10
        // ============================================================================
        fprintf(f, "\n=== PHƯƠNG PHÁP 2: OBFUSCATED (0x36F0 + Decrypt) ===\n");
        uint64_t aiManager2 = 0;
        __try {
            LeagueObfuscation<uint64_t> aiManagerObf = *(LeagueObfuscation<uint64_t>*)(localPlayer + 0x36F0);
            uint64_t decrypted = Decrypt(aiManagerObf);
            fprintf(f, "Obfuscation @ [Player + 0x36F0]\n");
            fprintf(f, "  isInit: %d, xorCount64: %d, xorCount8: %d, valueIndex: %d\n", 
                    aiManagerObf.isInit, aiManagerObf.xorCount64, aiManagerObf.xorCount8, aiManagerObf.valueIndex);
            fprintf(f, "  Decrypted: 0x%llX\n", decrypted);
            
            if (decrypted) {
                aiManager2 = *(uint64_t*)(decrypted + 0x10);
                fprintf(f, "  AiManager (Decrypted + 0x10): 0x%llX\n", aiManager2);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "[CRASH] Cannot read obfuscated AiManager\n");
        }

        if (aiManager2) {
            fprintf(f, "\n--- Scanning known leagueoflegends-master offsets ---\n");

            // oObjAiManagerManagerTargetPosition = 0x14
            __try {
                Vector3 targetPos = *(Vector3*)(aiManager2 + 0x14);
                fprintf(f, "[0x014] TargetPosition: (%.1f, %.1f, %.1f)\n", targetPos.x, targetPos.y, targetPos.z);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerIsMoving = 0x2BC
            __try {
                bool isMoving = *(bool*)(aiManager2 + 0x2BC);
                fprintf(f, "[0x2BC] IsMoving: %d\n", isMoving);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerCurrentSegment = 0x2C0
            __try {
                int curSeg = *(int*)(aiManager2 + 0x2C0);
                fprintf(f, "[0x2C0] CurrentSegment: %d\n", curSeg);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerPathStart = 0x2D0
            __try {
                Vector3 pathStart = *(Vector3*)(aiManager2 + 0x2D0);
                fprintf(f, "[0x2D0] PathStart: (%.1f, %.1f, %.1f)\n", pathStart.x, pathStart.y, pathStart.z);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerPathEnd = 0x2DC
            __try {
                Vector3 pathEnd = *(Vector3*)(aiManager2 + 0x2DC);
                fprintf(f, "[0x2DC] PathEnd: (%.1f, %.1f, %.1f)\n", pathEnd.x, pathEnd.y, pathEnd.z);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerSegmentsCount = 0x2F0
            __try {
                int segCount = *(int*)(aiManager2 + 0x2F0);
                fprintf(f, "[0x2F0] SegmentsCount: %d\n", segCount);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerDashSpeed = 0x300
            __try {
                float dashSpeed = *(float*)(aiManager2 + 0x300);
                fprintf(f, "[0x300] DashSpeed: %.1f\n", dashSpeed);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerIsDashing = 0x324
            __try {
                bool isDashing = *(bool*)(aiManager2 + 0x324);
                fprintf(f, "[0x324] IsDashing: %d\n", isDashing);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}

            // oObjAiManagerManagerPosition = 0x414
            __try {
                Vector3 pos = *(Vector3*)(aiManager2 + 0x414);
                fprintf(f, "[0x414] ServerPosition: (%.1f, %.1f, %.1f)\n", pos.x, pos.y, pos.z);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }

        // ============================================================================
        // PHƯƠNG PHÁP 3: BRUTE FORCE SCAN - Tìm AiManager pointer khắp object
        // ============================================================================
        fprintf(f, "\n=== PHƯƠNG PHÁP 3: BRUTE FORCE SCAN ===\n");
        fprintf(f, "Scanning player structure for pointers leading to Vec3 matching player position...\n\n");
        
        for (int off = 0x3000; off < 0x4000; off += 8) {
            __try {
                uint64_t ptr = *(uint64_t*)(localPlayer + off);
                if (ptr > 0x10000000000 && ptr < 0x800000000000) {
                    // Check if this pointer contains player position
                    for (int suboff = 0; suboff < 0x500; suboff += 0xC) {
                        __try {
                            Vector3 vec = *(Vector3*)(ptr + suboff);
                            float dist = vec.Distance(playerPos);
                            if (dist < 50 && vec.x > 100 && vec.x < 15000) {
                                fprintf(f, "[Player + 0x%04X] -> 0x%llX -> [+0x%03X] = (%.1f, %.1f, %.1f) dist=%.1f\n",
                                        off, ptr, suboff, vec.x, vec.y, vec.z, dist);
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
        }

        fprintf(f, "\n============================================\n");
        fprintf(f, "END OF DUMP\n");
        fprintf(f, "============================================\n\n");

        fclose(f);
    }

    // ============================================================================
    // Movement State Detection - Sử dụng sau khi tìm được offset đúng
    // ============================================================================
    inline bool IsPlayerMoving(uint64_t localPlayer)
    {
        if (!localPlayer) return false;

        __try {
            // Phương pháp 1: Dùng offset trực tiếp (nếu hoạt động)
            uint64_t aiManager = *(uint64_t*)(localPlayer + 0x3108);
            if (aiManager) {
                Vector3 startPath = *(Vector3*)(aiManager + 0x1E0);
                Vector3 endPath = *(Vector3*)(aiManager + 0x23C);
                return startPath.Distance(endPath) > 5.0f;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}

        return false;
    }

    inline bool IsPlayerDashing(uint64_t localPlayer)
    {
        if (!localPlayer) return false;

        __try {
            // Phương pháp: Check DashSpeed > 0 hoặc IsDashing flag
            uint64_t aiManager = *(uint64_t*)(localPlayer + 0x3108);
            if (aiManager) {
                // Cần tìm đúng offset cho IsDashing
                // Tạm thời check DashSpeed
                float dashSpeed = *(float*)(aiManager + 0x300);
                return dashSpeed > 0;
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}

        return false;
    }

    // ============================================================================
    // Debug helper - Call in main loop to continuously log
    // ============================================================================
    inline void ContinuousMovementLog()
    {
        static float lastLogTime = 0;
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        // Log mỗi 0.5 giây
        if (gameTime - lastLogTime < 0.5f) return;
        lastLogTime = gameTime;
        
        uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        if (!localPlayer) return;

        FILE* f = fopen("movement_log.txt", "a");
        if (!f) return;

        Vector3 playerPos = *(Vector3*)(localPlayer + Offset::oObjPosition);
        float moveSpeed = *(float*)(localPlayer + Offset::SpeedPlayer);

        __try {
            uint64_t aiManager = *(uint64_t*)(localPlayer + 0x3108);
            if (aiManager) {
                Vector3 startPath = *(Vector3*)(aiManager + 0x1E0);
                Vector3 endPath = *(Vector3*)(aiManager + 0x23C);
                int hasPath = *(int*)(aiManager + 0x268);
                
                float pathDist = startPath.Distance(endPath);
                bool isMoving = pathDist > 5.0f;
                
                fprintf(f, "[%.2f] Pos(%.0f,%.0f) Start(%.0f,%.0f) End(%.0f,%.0f) Dist=%.0f HasPath=%d Moving=%d Speed=%.0f\n",
                        gameTime, playerPos.x, playerPos.z, 
                        startPath.x, startPath.z, endPath.x, endPath.z,
                        pathDist, hasPath, isMoving, moveSpeed);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}

        fclose(f);
    }
}
