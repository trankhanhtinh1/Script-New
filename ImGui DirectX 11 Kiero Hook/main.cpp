#define _CRT_SECURE_NO_WARNINGS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "includes.h"
#include <iostream>
#include <thread>
#include <Windows.h>
#include <vector>
#include <string>
#include <fstream> // ensure fstream included
#include <algorithm> // for std::min, std::max

// Project includes
#include "Render.h"
#include "Menu.h"
#include "Vector.h"
#include "Spoof_call/spoofcall.h"
#include "SDK/ObjectManager.h"
#include "SDK/Game.h"
#include "SDK/Orbwalker.h"
#include "SDK/BuffManager.h"
#include "SDK/Spell.h"
#include "SDK/TargetSelector.h"
#include "SDK/MissileManager.h"
#include "SDK/AiManagerScan.h" // NEW: IDA-based AiManager decryption and scanning
#include "SDK/AiManagerNavGridScan.h" // NEW: NavGrid-based path offset scanner
#include "SDK/MissileScan.h" // NEW: IDA-based Missile decryption
#include "SDK/MissileOffsetScanner.h" // NEW: Dynamic offset discovery for multi-version support
#include "SDK/SpellDatabase.h" // NEW: Database for missile speed/radius/width verification
#include "Console.h"

// ImGui
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "imgui/imgui_impl_dx11.h"

// Kiero
#include "kiero/kiero.h"

// Debug Logging Wrapper
namespace Debug {
    void Log(const std::string& str) {
        std::ofstream logFile("orbwalker_debug.txt", std::ios_base::app);
        if (logFile.is_open()) {
            logFile << "[ORBWALKER] " << str << std::endl;
            logFile.close();
        }
    }
}
#define DBG_LOG(x) Debug::Log(x)

// ============================================================================
// SAFE OFFSET DEBUG LOGGER - Writes to file with exception handling
// Each offset read is wrapped in __try to prevent crash
// ============================================================================
void DumpOffsetDebugToFile() {
    FILE* file = fopen("offset_debug.txt", "w");
    if (!file) return;
    
    fprintf(file, "========== OFFSET DEBUG DUMP ==========\n");
    fprintf(file, "Time: %s\n\n", __TIMESTAMP__);
    
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    fprintf(file, "ModuleBase: 0x%llX\n\n", moduleBase);
    
    // Read LocalPlayer
    uint64_t localPlayer = 0;
    __try {
        localPlayer = *(uint64_t*)(moduleBase + 0x1D66AE0); // oLocalPlayer
        fprintf(file, "[OK] LocalPlayer (0x1D66AE0): 0x%llX\n", localPlayer);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] LocalPlayer (0x1D66AE0): N/A\n");
    }
    
    if (!localPlayer) {
        fprintf(file, "\nLocalPlayer is NULL - cannot read object offsets\n");
        fclose(file);
        return;
    }
    
    fprintf(file, "\n========== LOCAL PLAYER OFFSETS ==========\n");
    
    // NetId
    __try {
        int val = *(int*)(localPlayer + 0xC4);
        fprintf(file, "[OK] NetId (0xC4): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] NetId (0xC4): N/A\n");
    }
    
    // Team
    __try {
        int val = (int)(*(uint8_t*)(localPlayer + 0x251));
        fprintf(file, "[OK] Team (0x251): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Team (0x251): N/A\n");
    }
    
    // Position
    __try {
        float x = *(float*)(localPlayer + 0x254);
        float y = *(float*)(localPlayer + 0x254 + 4);
        float z = *(float*)(localPlayer + 0x254 + 8);
        fprintf(file, "[OK] Position (0x254): (%.1f, %.1f, %.1f)\n", x, y, z);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Position (0x254): N/A\n");
    }
    
    // Health
    __try {
        float val = *(float*)(localPlayer + 0x10A8);
        fprintf(file, "[OK] Health (0x10A8): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Health (0x10A8): N/A\n");
    }
    
    // MaxHealth
    __try {
        float val = *(float*)(localPlayer + 0x10D0);
        fprintf(file, "[OK] MaxHealth (0x10D0): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MaxHealth (0x10D0): N/A\n");
    }
    
    // Mana
    __try {
        float val = *(float*)(localPlayer + 0x358);
        fprintf(file, "[OK] Mana (0x358): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Mana (0x358): N/A\n");
    }
    
    // MaxMana
    __try {
        float val = *(float*)(localPlayer + 0x380);
        fprintf(file, "[OK] MaxMana (0x380): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MaxMana (0x380): N/A\n");
    }
    
    // AttackRange
    __try {
        float val = *(float*)(localPlayer + 0x181C);
        fprintf(file, "[OK] AttackRange (0x181C): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] AttackRange (0x181C): N/A\n");
    }
    
    // BaseAD
    __try {
        float val = *(float*)(localPlayer + 0x17D4);
        fprintf(file, "[OK] BaseAD (0x17D4): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] BaseAD (0x17D4): N/A\n");
    }
    
    // BonusAD
    __try {
        float val = *(float*)(localPlayer + 0x1730);
        fprintf(file, "[OK] BonusAD (0x1730): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] BonusAD (0x1730): N/A\n");
    }
    
    // MoveSpeed
    __try {
        float val = *(float*)(localPlayer + 0x1814);
        fprintf(file, "[OK] MoveSpeed (0x1814): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MoveSpeed (0x1814): N/A\n");
    }
    
    // Armor
    __try {
        float val = *(float*)(localPlayer + 0x17FC);
        fprintf(file, "[OK] Armor (0x17FC): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Armor (0x17FC): N/A\n");
    }
    
    // MagicResist
    __try {
        float val = *(float*)(localPlayer + 0x1804);
        fprintf(file, "[OK] MagicResist (0x1804): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MagicResist (0x1804): N/A\n");
    }
    
    // Dead
    __try {
        int val = (int)(*(uint8_t*)(localPlayer + 0x250));
        fprintf(file, "[OK] Dead (0x250): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Dead (0x250): N/A\n");
    }
    
    // Visible
    __try {
        int val = (int)(*(uint8_t*)(localPlayer + 0x300));
        fprintf(file, "[OK] Visible (0x300): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Visible (0x300): N/A\n");
    }
    
    // Targetable (old offset)
    __try {
        int val = (int)(*(uint8_t*)(localPlayer + 0x458));
        fprintf(file, "[OK] Targetable_old (0x458): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Targetable_old (0x458): N/A\n");
    }
    
    // Targetable (new offset)
    __try {
        int val = (int)(*(uint8_t*)(localPlayer + 0xEC8));
        fprintf(file, "[OK] Targetable_new (0xEC8): %d\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Targetable_new (0xEC8): N/A\n");
    }
    
    // BoundingRadius offset read
    __try {
        float val = *(float*)(localPlayer + 0x6D8);
        fprintf(file, "[OK] BoundingRadius_offset (0x6D8): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] BoundingRadius_offset (0x6D8): N/A\n");
    }
    
    fprintf(file, "\n========== FUNCTION CALLS ==========\n");
    
    // GetBoundingRadius function
    __try {
        typedef float(__fastcall* fnGetBoundingRadius)(uint64_t);
        fnGetBoundingRadius fn = (fnGetBoundingRadius)(moduleBase + 0x280DA0);
        float val = fn(localPlayer);
        fprintf(file, "[OK] GetBoundingRadius() (0x280DA0): %.2f\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] GetBoundingRadius() (0x280DA0): N/A\n");
    }
    
    // GetAttackDelay function
    __try {
        typedef float(__cdecl* fnGetAttackDelay)(uint64_t);
        fnGetAttackDelay fn = (fnGetAttackDelay)(moduleBase + 0x540DB0);
        float val = fn(localPlayer);
        fprintf(file, "[OK] GetAttackDelay() (0x540DB0): %.3f sec\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] GetAttackDelay() (0x540DB0): N/A\n");
    }
    
    // GetAttackWindup function
    __try {
        typedef float(__cdecl* fnGetAttackWindup)(uint64_t, int);
        fnGetAttackWindup fn = (fnGetAttackWindup)(moduleBase + 0x540CB0);
        float val = fn(localPlayer, 0x40);
        fprintf(file, "[OK] GetAttackWindup() (0x540CB0): %.3f sec\n", val);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] GetAttackWindup() (0x540CB0): N/A\n");
    }
    
    fprintf(file, "\n========== MANAGERS ==========\n");
    
    // HeroManager
    __try {
        uint64_t heroMgr = *(uint64_t*)(moduleBase + 0x1D2F3B0);
        fprintf(file, "[OK] HeroManager (0x1D2F3B0): 0x%llX\n", heroMgr);
        if (heroMgr) {
            int size = *(int*)(heroMgr + 0x10);
            fprintf(file, "     -> Size: %d\n", size);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] HeroManager (0x1D2F3B0): N/A\n");
    }
    
    // MinionManager
    __try {
        uint64_t minionMgr = *(uint64_t*)(moduleBase + 0x1D32AF0);
        fprintf(file, "[OK] MinionManager (0x1D32AF0): 0x%llX\n", minionMgr);
        if (minionMgr) {
            int count = *(int*)(minionMgr + 0x70);
            fprintf(file, "     -> LaneMinionCount: %d\n", count);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MinionManager (0x1D32AF0): N/A\n");
    }
    
    // MissileManager
    __try {
        uint64_t missileMgr = *(uint64_t*)(moduleBase + 0x1D32B08);
        fprintf(file, "[OK] MissileManager (0x1D32B08): 0x%llX\n", missileMgr);
        if (missileMgr) {
            int size = *(int*)(missileMgr + 0x10);
            fprintf(file, "     -> Size: %d\n", size);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] MissileManager (0x1D32B08): N/A\n");
    }
    
    // TurretManager
    __try {
        uint64_t turretMgr = *(uint64_t*)(moduleBase + 0x1D3BEB8);
        fprintf(file, "[OK] TurretManager (0x1D3BEB8): 0x%llX\n", turretMgr);
        if (turretMgr) {
            int size = *(int*)(turretMgr + 0x10);
            fprintf(file, "     -> Size: %d\n", size);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] TurretManager (0x1D3BEB8): N/A\n");
    }
    
    fprintf(file, "\n========== MISSILE RAW DUMP (for offset discovery) ==========\n");
    fprintf(file, "Looking for: SpellInfo pointer, SourceNetId, TargetNetId, Position, Name\n\n");
    
    // Dump raw missile structure to find correct offsets
    __try {
        uint64_t missileMgr = *(uint64_t*)(moduleBase + 0x1D32B08);
        if (missileMgr) {
            uint64_t arrayPtr = *(uint64_t*)(missileMgr + 0x08);
            int size = *(int*)(missileMgr + 0x10);
            fprintf(file, "MissileManager: 0x%llX, ArrayPtr: 0x%llX, Size: %d\n\n", missileMgr, arrayPtr, size);
            
            // Dump first 3 missiles
            for (int m = 0; m < size && m < 3; m++) {
                uint64_t missile = *(uint64_t*)(arrayPtr + m * 0x8);
                if (!missile) continue;
                
                fprintf(file, "=== MISSILE[%d] @ 0x%llX ===\n", m, missile);
                
                // Dump raw bytes looking for pointers and interesting values
                // Scan first 0x400 bytes of missile structure
                fprintf(file, "Scanning for pointers (values > 0x10000000000):\n");
                
                for (int offset = 0; offset < 0x400; offset += 8) {
                    __try {
                        uint64_t val = *(uint64_t*)(missile + offset);
                        // Check if it looks like a valid pointer (in reasonable range)
                        if (val > 0x10000000000 && val < 0x800000000000) {
                            fprintf(file, "  [0x%03X] PTR: 0x%llX\n", offset, val);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        // Skip
                    }
                }
                
                fprintf(file, "\nScanning for NetIDs (values 0x40000000 - 0x50000000):\n");
                for (int offset = 0; offset < 0x400; offset += 4) {
                    __try {
                        int val = *(int*)(missile + offset);
                        // NetIDs are typically in this range
                        if (val > 0x40000000 && val < 0x50000000) {
                            fprintf(file, "  [0x%03X] NetID?: %d (0x%X)\n", offset, val, val);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        // Skip
                    }
                }
                
                fprintf(file, "\nScanning for Floats (position-like, 0-15000):\n");
                for (int offset = 0; offset < 0x400; offset += 4) {
                    __try {
                        float val = *(float*)(missile + offset);
                        // Position values are typically in map range
                        if (val > 100.0f && val < 15000.0f) {
                            fprintf(file, "  [0x%03X] Float: %.1f\n", offset, val);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        // Skip
                    }
                }
                
                fprintf(file, "\n");
            }
        } else {
            fprintf(file, "MissileManager is NULL\n");
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(file, "[CRASH] Missile dump failed\n");
    }
    
    fprintf(file, "\n========== END ==========\n");
    fclose(file);
}

// ============================================================================
// TURRET AGGRO DETECTION - Check turret target directly (missiles don't work)
// ============================================================================
void ContinuousTurretLog() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + 0x1D66AE0);
    int playerNetId = localPlayer ? *(int*)(localPlayer + 0xC4) : 0;
    
    uint64_t turretMgr = *(uint64_t*)(moduleBase + 0x1D3BEB8); // oTurretList
    if (!turretMgr) return;
    
    uint64_t arrayPtr = *(uint64_t*)(turretMgr + 0x08);
    int size = *(int*)(turretMgr + 0x10);
    
    if (size <= 0 || size >= 50) return;
    
    FILE* f = fopen("turret_log.txt", "a");
    if (!f) return;
    
    fprintf(f, "\n=== TURRET LOG (Size: %d) | PlayerNetID: 0x%X ===\n", size, playerNetId);
    
    // Only scan enemy turrets (Team 2 if player is Team 1, or Team 1 if player is Team 2)
    int playerTeam = localPlayer ? (*(int*)(localPlayer + 0x251) & 0xFF) : 0;
    
    for (int t = 0; t < size && t < 24; t++) {
        __try {
            uint64_t turret = *(uint64_t*)(arrayPtr + t * 0x8);
            if (!turret) continue;
            
            int turretNetId = *(int*)(turret + 0xC4);
            int turretTeam = *(int*)(turret + 0x251) & 0xFF;
            
            // Only log enemy turrets
            if (turretTeam == playerTeam) continue;
            
            fprintf(f, "EnemyTurret[%d] @ 0x%llX | NetID: 0x%X | Team: %d\n", t, turret, turretNetId, turretTeam);
            
            // Scan entire structure for player NetID (direct match)
            fprintf(f, "  --- Scanning for PlayerNetID 0x%X ---\n", playerNetId);
            bool found = false;
            for (int off = 0; off < 0x2000; off += 4) {
                __try {
                    int val = *(int*)(turret + off);
                    if (val == playerNetId) {
                        fprintf(f, "  [0x%04X] = 0x%X <-- PLAYER DIRECT!\n", off, val);
                        found = true;
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Scan ALL pointers and check their NetID
            fprintf(f, "  --- Scanning pointers for player object ---\n");
            for (int ptrOff = 0; ptrOff < 0x2000; ptrOff += 8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(turret + ptrOff);
                    if (ptr > 0x10000000000 && ptr < 0x800000000000) {
                        __try {
                            int targetNetId = *(int*)(ptr + 0xC4);
                            if (targetNetId == playerNetId) {
                                fprintf(f, "  PTR[0x%04X] -> +0xC4 = 0x%X <-- PLAYER FOUND!\n", ptrOff, targetNetId);
                                found = true;
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            if (!found) {
                fprintf(f, "  >>> PLAYER NOT FOUND IN THIS TURRET <<<\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    fclose(f);
}

// ============================================================================
// COMBAT STATS DEBUG - Scan for CritChance, BonusArmor, etc. offsets
// ============================================================================
// Usage: Buy items in Practice Tool then enable this to find offsets
// - For CritChance: Buy Infinity Edge (20% crit) -> look for 0.2 float
// - For BonusArmor: Buy Plated Steelcaps (20 armor) -> look for 20.0 float near Armor offset
// - For BonusMR: Buy Mercury's Treads (25 MR) -> look for 25.0 float near MR offset
void CombatStatsDebug() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + 0x1D66AE0);
    if (!localPlayer) return;
    
    FILE* f = fopen("combat_stats_debug.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== COMBAT STATS DEBUG ===\n");
    fprintf(f, "LocalPlayer @ 0x%llX\n\n", localPlayer);
    
    // Known offsets for reference
    float armor = *(float*)(localPlayer + 0x17FC);
    float mr = *(float*)(localPlayer + 0x1804);
    float ap = *(float*)(localPlayer + 0x1808);
    float critDmg = *(float*)(localPlayer + 0x17F8);
    float armorPenFlat = *(float*)(localPlayer + 0x16F0);
    float armorPenPct = *(float*)(localPlayer + 0x16D8);
    float magicPenFlat = *(float*)(localPlayer + 0x16D4);
    float magicPenPct = *(float*)(localPlayer + 0x16DC);
    
    fprintf(f, "=== VERIFIED OFFSETS ===\n");
    fprintf(f, "[0x17FC] Armor = %.2f\n", armor);
    fprintf(f, "[0x1804] MagicResist = %.2f\n", mr);
    fprintf(f, "[0x1808] AbilityPower = %.2f\n", ap);
    fprintf(f, "[0x17F8] CritDamage = %.2f\n", critDmg);
    fprintf(f, "[0x16F0] ArmorPenFlat = %.2f\n", armorPenFlat);
    fprintf(f, "[0x16D8] ArmorPenPercent = %.2f\n", armorPenPct);
    fprintf(f, "[0x16D4] MagicPenFlat = %.2f\n", magicPenFlat);
    fprintf(f, "[0x16DC] MagicPenPercent = %.2f\n", magicPenPct);
    
    fprintf(f, "\n=== SCANNING FOR CRIT CHANCE (0.0 - 1.0 range) ===\n");
    fprintf(f, "Look for: 0.20 = 20%% crit, 0.25 = 25%% crit, etc.\n");
    for (int off = 0x16D0; off < 0x1850; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            if (val >= 0.05f && val <= 1.5f && val != 1.0f) {
                fprintf(f, "[0x%04X] = %.4f", off, val);
                if (val >= 0.15f && val <= 0.30f) fprintf(f, " <-- POSSIBLE CRIT CHANCE (20-25%%)");
                if (val >= 0.35f && val <= 0.45f) fprintf(f, " <-- POSSIBLE CRIT CHANCE (40%%)");
                if (val >= 0.55f && val <= 0.65f) fprintf(f, " <-- POSSIBLE CRIT CHANCE (60%%)");
                fprintf(f, "\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "\n=== SCANNING FOR CRIT DAMAGE (1.5 - 2.5 range) ===\n");
    fprintf(f, "Base = 1.75 (175%%), with Infinity Edge = 2.15 (215%%)\n");
    for (int off = 0x16D0; off < 0x1850; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            if (val >= 1.5f && val <= 2.5f) {
                fprintf(f, "[0x%04X] = %.4f", off, val);
                if (val >= 1.70f && val <= 1.80f) fprintf(f, " <-- POSSIBLE CRIT DAMAGE (base 175%%)");
                if (val >= 2.10f && val <= 2.20f) fprintf(f, " <-- POSSIBLE CRIT DAMAGE (with IE 215%%)");
                if (val >= 0.35f && val <= 0.45f) fprintf(f, " <-- POSSIBLE CRIT DAMAGE BONUS (40%% IE)");
                fprintf(f, "\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "\n=== SCANNING FOR BONUS ARMOR (near 0x17FC) ===\n");
    fprintf(f, "Look for: item armor values (20, 25, 30, 40, 45, 50, etc.)\n");
    for (int off = 0x17E0; off < 0x1820; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            if (val >= 10.0f && val <= 200.0f) {
                fprintf(f, "[0x%04X] = %.2f", off, val);
                if (off == 0x17FC) fprintf(f, " <-- KNOWN: Total Armor");
                if (val == 20.0f || val == 25.0f || val == 30.0f || val == 40.0f || val == 45.0f || val == 50.0f) {
                    fprintf(f, " <-- POSSIBLE BONUS ARMOR (common item value)");
                }
                fprintf(f, "\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "\n=== SCANNING FOR BONUS MR (near 0x1804) ===\n");
    fprintf(f, "Look for: item MR values (25, 30, 35, 40, 50, 60, etc.)\n");
    for (int off = 0x17F0; off < 0x1830; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            if (val >= 10.0f && val <= 200.0f) {
                fprintf(f, "[0x%04X] = %.2f", off, val);
                if (off == 0x1804) fprintf(f, " <-- KNOWN: Total MR");
                if (val == 25.0f || val == 30.0f || val == 35.0f || val == 40.0f || val == 50.0f || val == 60.0f) {
                    fprintf(f, " <-- POSSIBLE BONUS MR (common item value)");
                }
                fprintf(f, "\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "\n=== SCANNING FOR BONUS ARMOR PEN %% (near 0x16D8) ===\n");
    fprintf(f, "Look for: Lord Dominik's = 0.35 (35%%), Serylda's = 0.30 (30%%)\n");
    for (int off = 0x16C0; off < 0x1720; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            if (val >= 0.1f && val <= 0.5f) {
                fprintf(f, "[0x%04X] = %.4f", off, val);
                if (off == 0x16D8) fprintf(f, " <-- KNOWN: ArmorPenPercent");
                if (val >= 0.28f && val <= 0.37f) fprintf(f, " <-- POSSIBLE BONUS ARMOR PEN (30-35%%)");
                fprintf(f, "\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "\n=== FULL DUMP 0x1680 - 0x1880 ===\n");
    for (int off = 0x1680; off < 0x1880; off += 4) {
        __try {
            float val = *(float*)(localPlayer + off);
            int ival = *(int*)(localPlayer + off);
            if ((val > 0.001f && val < 10000.0f) || (ival > 0 && ival < 1000)) {
                fprintf(f, "[0x%04X] float=%.4f int=%d\n", off, val, ival);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fclose(f);
}

// ============================================================================
// SPELL DEBUG - Debug spell info (cooldown, level, mana cost, name)
// C-style function to allow __try/__except (SEH)
// ============================================================================
// Helper to dump single spell slot (C-style for SEH compatibility)
static void DumpSpellSlotSafe(FILE* f, uint64_t spellBook, int slot, float gameTime, const char* slotName) {
    __try {
        uint64_t spellSlot = *(uint64_t*)(spellBook + slot * 8 + Offset::oObjSpellBookSpellSlot);
        if (!spellSlot) {
            fprintf(f, "[%s] SpellSlot = NULL\n", slotName);
            return;
        }
        
        fprintf(f, "[%s] SpellSlot @ 0x%llX\n", slotName, spellSlot);
        
        int level = *(int*)(spellSlot + Offset::oSpellSlotLevel);
        float cooldown = *(float*)(spellSlot + Offset::oSpellSlotCooldown);
        float totalCooldown = *(float*)(spellSlot + Offset::oSpellSlotTotalCooldown);
        float remainingCooldown = cooldown > gameTime ? cooldown - gameTime : 0.0f;
        
        fprintf(f, "  Level: %d\n", level);
        fprintf(f, "  Cooldown Ready: %.2f (Remaining: %.2f)\n", cooldown, remainingCooldown);
        fprintf(f, "  Total Cooldown: %.2f\n", totalCooldown);
        
        uint64_t spellInfo = *(uint64_t*)(spellSlot + Offset::oSpellSlotSpellInfo);
        if (spellInfo) {
            fprintf(f, "  SpellInfo @ 0x%llX\n", spellInfo);
            
            uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
            if (spellData) {
                fprintf(f, "  SpellData @ 0x%llX\n", spellData);
                
                uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                if (namePtr) {
                    char name[64] = {0};
                    for (int i = 0; i < 63; i++) {
                        char c = *(char*)(namePtr + i);
                        if (c == 0 || c < 32 || c > 126) break;
                        name[i] = c;
                    }
                    if (name[0]) fprintf(f, "  Name: %s\n", name);
                }
                
                float manaCost = *(float*)(spellData + Offset::oSpellDataManaCost);
                if (manaCost > 0 && manaCost < 500) {
                    fprintf(f, "  ManaCost: %.0f\n", manaCost);
                }
            }
        }
        fprintf(f, "\n");
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "[%s] CRASH reading spell\n\n", slotName);
    }
}

// ============================================================================
// CAST SPELL OFFSET DEBUG - Find correct offsets for CastSpell functionality
// Scans for: oCastSpellWrapper, CastSpell2CheckFlag, oSpellSlotSpellInfo, oSpellSlotSpellInput
// Results written to castspell_debug.txt
// ============================================================================
void CastSpellOffsetDebug() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
    if (!localPlayer) return;
    
    FILE* f = fopen("castspell_debug.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== CAST SPELL OFFSET DEBUG ===\n");
    fprintf(f, "Module Base: 0x%llX\n", moduleBase);
    fprintf(f, "LocalPlayer: 0x%llX\n\n", localPlayer);
    
    // ============================================================================
    // 1. Pattern Scan for oCastSpellWrapper
    // Pattern: 48 89 48 ? 55 56 57 41 54 41 55 48 (CastSpell2NoAceHook)
    // ============================================================================
    fprintf(f, "=== 1. PATTERN SCAN: oCastSpellWrapper ===\n");
    fprintf(f, "Pattern: 48 89 48 ?? 55 56 57 41 54 41 55 48\n\n");
    
    char* castSpellResult = mem::ScanModInternal(
        (char*)"\x48\x89\x48\x00\x55\x56\x57\x41\x54\x41\x55\x48",
        (char*)"xxx?xxxxxxxx",
        (char*)moduleBase
    );
    
    if (castSpellResult) {
        uint64_t castSpellOffset = (uint64_t)castSpellResult - moduleBase;
        fprintf(f, "FOUND: oCastSpellWrapper = 0x%llX (address: 0x%llX)\n", castSpellOffset, (uint64_t)castSpellResult);
        fprintf(f, "Current offset in code: 0x%llX\n", Offset::Function::oCastSpellWrapper);
        if (castSpellOffset != Offset::Function::oCastSpellWrapper) {
            fprintf(f, ">>> MISMATCH! Update Offsets.h <<<\n");
        } else {
            fprintf(f, ">>> MATCH - offset is correct <<<\n");
        }
    } else {
        fprintf(f, "NOT FOUND - try alternative pattern\n");
        // Try alternative scan - look for function prologue
        char* altResult = mem::ScanModInternal(
            (char*)"\x48\x89\x4C\x24\x08\x55\x56\x57\x41\x54\x41\x55",
            (char*)"xxxxxxxxxxxx",
            (char*)moduleBase
        );
        if (altResult) {
            uint64_t altOffset = (uint64_t)altResult - moduleBase;
            fprintf(f, "ALT FOUND: 0x%llX (try this if crash persists)\n", altOffset);
        }
    }
    fprintf(f, "\n");
    
    // ============================================================================
    // 2. Pattern Scan for CastSpell2CheckFlag
    // Pattern: 88 44 24 ? 48 FF E1 C3 CC
    // ============================================================================
    fprintf(f, "=== 2. PATTERN SCAN: CastSpell2CheckFlag ===\n");
    fprintf(f, "Pattern: 88 44 24 ?? 48 FF E1 C3 CC\n\n");
    
    char* checkFlagResult = mem::ScanModInternal(
        (char*)"\x88\x44\x24\x00\x48\xFF\xE1\xC3\xCC",
        (char*)"xxx?xxxxx",
        (char*)moduleBase
    );
    
    if (checkFlagResult) {
        uint64_t checkFlagOffset = (uint64_t)checkFlagResult - moduleBase;
        fprintf(f, "FOUND: CastSpell2CheckFlag = 0x%llX\n", checkFlagOffset);
    } else {
        fprintf(f, "NOT FOUND\n");
    }
    fprintf(f, "\n");
    
    // ============================================================================
    // 3. Probe SpellSlot for SpellInfo/SpellInput pointers
    // Scan offsets 0x100 - 0x180 for valid pointer patterns
    // ============================================================================
    fprintf(f, "=== 3. SPELLSLOT POINTER PROBE (Q Slot) ===\n");
    fprintf(f, "Scanning 0x100 - 0x180 for valid pointers...\n\n");
    
    uint64_t spellBook = localPlayer + Offset::oObjSpellBook;
    uint64_t spellSlotQ = *(uint64_t*)(spellBook + 0 * 8 + Offset::oObjSpellBookSpellSlot);
    
    fprintf(f, "SpellBook: 0x%llX\n", spellBook);
    fprintf(f, "SpellSlot[Q]: 0x%llX\n\n", spellSlotQ);
    
    if (spellSlotQ && spellSlotQ > 0x10000 && spellSlotQ < 0x7FFFFFFFFFFF) {
        fprintf(f, "--- Pointer Candidates (0x100-0x180) ---\n");
        for (int off = 0x100; off < 0x180; off += 8) {
            __try {
                uint64_t ptr = *(uint64_t*)(spellSlotQ + off);
                if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                    fprintf(f, "[0x%03X] = 0x%llX", off, ptr);
                    
                    // Try to identify SpellInfo vs SpellInput by probing sub-structure
                    __try {
                        // SpellInfo typically has SpellData ptr at +0x18 or +0x60
                        uint64_t sub18 = *(uint64_t*)(ptr + 0x18);
                        uint64_t sub60 = *(uint64_t*)(ptr + 0x60);
                        
                        bool hasSpellData18 = (sub18 > 0x10000 && sub18 < 0x7FFFFFFFFFFF);
                        bool hasSpellData60 = (sub60 > 0x10000 && sub60 < 0x7FFFFFFFFFFF);
                        
                        if (hasSpellData18 || hasSpellData60) {
                            fprintf(f, " <- Likely SPELLINFO (has sub-ptrs)");
                            
                            // Try to read spell name from SpellData
                            uint64_t spellData = hasSpellData18 ? sub18 : sub60;
                            uint64_t namePtr = *(uint64_t*)(spellData + 0x8);
                            if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                char name[32] = {0};
                                for (int i = 0; i < 31; i++) {
                                    char c = *(char*)(namePtr + i);
                                    if (c == 0 || c < 32 || c > 126) break;
                                    name[i] = c;
                                }
                                if (name[0]) fprintf(f, " [Name: %s]", name);
                            }
                        }
                        
                        // SpellInput typically has Vector3 positions at +0x18 (StartPos), +0x24 (EndPos)
                        float startX = *(float*)(ptr + 0x18);
                        float startY = *(float*)(ptr + 0x1C);
                        float startZ = *(float*)(ptr + 0x20);
                        
                        // Check if looks like a valid position (reasonable game coords)
                        if (!hasSpellData18 && !hasSpellData60 &&
                            startX > -5000 && startX < 20000 &&
                            startY > -500 && startY < 2000 &&
                            startZ > -5000 && startZ < 20000) {
                            fprintf(f, " <- Likely SPELLINPUT (has Vec3 @ +0x18: %.0f,%.0f,%.0f)", startX, startY, startZ);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        fprintf(f, " <- CRASH probing sub-structure");
                    }
                    
                    fprintf(f, "\n");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "[0x%03X] = CRASH\n", off);
            }
        }
        
        fprintf(f, "\n--- Current Offsets vs Probe Results ---\n");
        fprintf(f, "oSpellSlotSpellInfo = 0x%llX\n", Offset::oSpellSlotSpellInfo);
        fprintf(f, "oSpellSlotSpellInput = 0x%llX\n", Offset::oSpellSlotSpellInput);
        fprintf(f, "\nCompare with pointer candidates above to verify!\n");
    } else {
        fprintf(f, "SpellSlot[Q] invalid: 0x%llX\n", spellSlotQ);
    }
    
    // ============================================================================
    // 4. HudInstance SpellInfo offset check
    // ============================================================================
    fprintf(f, "\n=== 4. HUD INSTANCE SPELLINFO CHECK ===\n");
    uint64_t hud = *(uint64_t*)(moduleBase + Offset::oHudInstance);
    fprintf(f, "HudInstance: 0x%llX\n", hud);
    
    if (hud && hud > 0x10000 && hud < 0x7FFFFFFFFFFF) {
        fprintf(f, "Scanning HudInstance for SpellInfo pointer...\n\n");
        for (int off = 0x60; off <= 0x80; off += 8) {
            __try {
                uint64_t ptr = *(uint64_t*)(hud + off);
                if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                    fprintf(f, "[0x%02X] = 0x%llX", off, ptr);
                    if (off == 0x68) fprintf(f, " <- oHudInstanceSpellInfo (expected)");
                    fprintf(f, "\n");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "[0x%02X] = CRASH\n", off);
            }
        }
    }
    
    fprintf(f, "\n=== DEBUG COMPLETE ===\n");
    fprintf(f, "Check the pointer candidates and update Offsets.h accordingly.\n");
    
    fclose(f);
}

void SpellDebug() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
    if (!localPlayer) return;
    
    FILE* f = fopen("spell_debug.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== SPELL DEBUG ===\n");
    fprintf(f, "LocalPlayer @ 0x%llX\n\n", localPlayer);
    
    float gameTime = *(float*)(moduleBase + Offset::oGametime);
    fprintf(f, "GameTime: %.2f\n\n", gameTime);
    
    uint64_t spellBook = localPlayer + Offset::oObjSpellBook;
    fprintf(f, "SpellBook @ 0x%llX (offset 0x%llX from player)\n\n", spellBook, (uint64_t)Offset::oObjSpellBook);
    
    const char* slotNames[] = {"Q", "W", "E", "R", "D", "F", "Item1", "Item2", "Item3", "Item4", "Item5", "Item6", "Trinket"};
    
    for (int slot = 0; slot < 13; slot++) {
        DumpSpellSlotSafe(f, spellBook, slot, gameTime, slotNames[slot]);
    }
    
    // Scan for spell-related offsets - Use W slot (has ManaCost, Range, Speed)
    fprintf(f, "\n=== SPELL OFFSET SCAN (SpellSlot W) ===\n");
    fprintf(f, "Using W slot: Jinx W has ManaCost=50-110, Range=1450, Speed=3300, CastTime=0.6\n\n");
    
    __try {
        uint64_t spellSlotQ = *(uint64_t*)(spellBook + 1 * 8 + Offset::oObjSpellBookSpellSlot); // Slot 1 = W
        if (spellSlotQ) {
            fprintf(f, "SpellSlot W @ 0x%llX\n", spellSlotQ);
            
            // Scan for spell level (int 1-5)
            fprintf(f, "\n--- Integers 1-5 (possible Level) ---\n");
            for (int off = 0; off < 0x200; off += 4) {
                int val = *(int*)(spellSlotQ + off);
                if (val >= 1 && val <= 5) {
                    fprintf(f, "[0x%03X] = %d", off, val);
                    if (off == 0x28) fprintf(f, " <-- KNOWN: oSpellSlotLevel");
                    fprintf(f, "\n");
                }
            }
            
            // Scan for cooldown times (float > gameTime or 0)
            fprintf(f, "\n--- Floats (possible Cooldown, Time) ---\n");
            for (int off = 0; off < 0x200; off += 4) {
                float val = *(float*)(spellSlotQ + off);
                if ((val > 0.5f && val < 200.0f) || (val > gameTime - 60 && val < gameTime + 200)) {
                    fprintf(f, "[0x%03X] = %.2f", off, val);
                    if (off == 0x30) fprintf(f, " <-- KNOWN: oSpellSlotCooldown (ready time)");
                    if (off == 0x34) fprintf(f, " <-- KNOWN: oSpellSlotStartTime");
                    if (off == 0x74) fprintf(f, " <-- KNOWN: oSpellSlotTotalCooldown");
                    fprintf(f, "\n");
                }
            }
            
            // Scan for pointers (SpellInfo, SpellInput)
            fprintf(f, "\n--- Pointers (possible SpellInfo, SpellData) ---\n");
            for (int off = 0x100; off < 0x180; off += 8) {
                uint64_t ptr = *(uint64_t*)(spellSlotQ + off);
                if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                    fprintf(f, "[0x%03X] = 0x%llX", off, ptr);
                    if (off == 0x120) fprintf(f, " <-- KNOWN: oSpellSlotSpellInput");
                    if (off == 0x128) fprintf(f, " <-- KNOWN: oSpellSlotSpellInfo");
                    fprintf(f, "\n");
                }
            }
            
            // ======= SCAN SPELLINFO STRUCTURE (with crash protection) =======
            uint64_t spellInfo = *(uint64_t*)(spellSlotQ + 0x128);
            if (spellInfo && spellInfo > 0x10000 && spellInfo < 0x7FFFFFFFFFFF) {
                fprintf(f, "\n=== SPELLINFO STRUCTURE SCAN @ 0x%llX ===\n", spellInfo);
                fprintf(f, "(SpellInfo offsets already verified via SpellData->Name)\n\n");
                
                // Get SpellData directly using verified offset
                uint64_t spellData = *(uint64_t*)(spellInfo + 0x18);
                
                if (spellData && spellData > 0x10000 && spellData < 0x7FFFFFFFFFFF) {
                    fprintf(f, "=== SPELLDATA STRUCTURE SCAN @ 0x%llX ===\n", spellData);
                    
                    // Print spell name first
                    uint64_t namePtr = *(uint64_t*)(spellData + 0x8);
                    if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                        char spellName[64] = {0};
                        for (int i = 0; i < 63; i++) {
                            char c = *(char*)(namePtr + i);
                            if (c == 0 || c < 32 || c > 126) break;
                            spellName[i] = c;
                        }
                        fprintf(f, "Spell Name: %s\n\n", spellName);
                    }
                    
                    fprintf(f, "--- Looking for Jinx W values ---\n");
                    fprintf(f, "ManaCost=50-110, Range=1450, Speed=3300, CastTime=0.6\n\n");
                    
                    // FULL SCAN 0x0 - 0x1000 for all interesting floats
                    fprintf(f, "=== FULL FLOAT SCAN (0x0 - 0x1000) ===\n\n");
                    
                    // Scan for ManaCost (50-110)
                    fprintf(f, "--- Values 50-120 (ManaCost) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        float val = *(float*)(spellData + off);
                        if (val >= 50.0f && val <= 120.0f) {
                            fprintf(f, "[0x%03X] = %.1f\n", off, val);
                        }
                    }
                    
                    // Scan for Range as INT (1400-1500 for JinxW)
                    fprintf(f, "\n--- INT Values 1400-1600 (Range=1450) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        int val = *(int*)(spellData + off);
                        if (val >= 1400 && val <= 1600) {
                            fprintf(f, "[0x%03X] = %d (int)\n", off, val);
                        }
                    }
                    
                    // Scan for Range as FLOAT (wider range 500-2000)
                    fprintf(f, "\n--- FLOAT Values 500-2000 (Range) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        float val = *(float*)(spellData + off);
                        if (val >= 500.0f && val <= 2000.0f) {
                            fprintf(f, "[0x%03X] = %.1f\n", off, val);
                        }
                    }
                    
                    // Scan for Speed as INT (3000-3500 for JinxW)
                    fprintf(f, "\n--- INT Values 3000-3500 (Speed=3300) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        int val = *(int*)(spellData + off);
                        if (val >= 3000 && val <= 3500) {
                            fprintf(f, "[0x%03X] = %d (int)\n", off, val);
                        }
                    }
                    
                    // Scan for Speed as FLOAT (wider range 2000-5000)
                    fprintf(f, "\n--- FLOAT Values 2000-5000 (MissileSpeed) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        float val = *(float*)(spellData + off);
                        if (val >= 2000.0f && val <= 5000.0f) {
                            fprintf(f, "[0x%03X] = %.1f\n", off, val);
                        }
                    }
                    
                    // Scan for CastTime (0.3-1.0 wider range)
                    fprintf(f, "\n--- FLOAT Values 0.3-1.0 (CastTime=0.6) ---\n");
                    for (int off = 0; off < 0x1000; off += 4) {
                        float val = *(float*)(spellData + off);
                        if (val >= 0.3f && val <= 1.0f) {
                            fprintf(f, "[0x%03X] = %.3f\n", off, val);
                        }
                    }
                    
                    // Also check via SpellDataResource pointer at 0x60
                    fprintf(f, "\n=== CHECK SpellDataResource @ 0x60 ===\n");
                    uint64_t spellDataResource = *(uint64_t*)(spellData + 0x60);
                    if (spellDataResource > 0x10000 && spellDataResource < 0x7FFFFFFFFFFF) {
                        fprintf(f, "SpellDataResource @ 0x%llX\n", spellDataResource);
                        
                        fprintf(f, "\n--- ManaCost in SpellDataResource (50-120) ---\n");
                        for (int off = 0; off < 0x200; off += 4) {
                            float val = *(float*)(spellDataResource + off);
                            if (val >= 50.0f && val <= 120.0f) {
                                fprintf(f, "[0x%03X] = %.1f\n", off, val);
                            }
                        }
                    } else {
                        fprintf(f, "SpellDataResource invalid: 0x%llX\n", spellDataResource);
                    }
                } else {
                    fprintf(f, "SpellData pointer invalid: 0x%llX\n", spellData);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "CRASH during offset scan\n");
    }
    
    fclose(f);
}

// ============================================================================
// BUFF MANAGER DEBUG - Safe offset discovery for CC detection
// Scans multiple offset candidates with __try/__except to prevent crash
// Results written to buff_debug.txt
// ============================================================================

// Helper: Read string safely from memory (C-style for SEH)
static bool TryReadBuffNameSafe(uint64_t buffNamePtr, char* outBuffer, int bufferSize) {
    if (!buffNamePtr) return false;
    
    __try {
        // leagueoflegends-master pattern: BuffNamePtr -> +0x8 is actual string
        uint64_t nameStrPtr = *(uint64_t*)(buffNamePtr + 0x8);
        if (!nameStrPtr || nameStrPtr < 0x10000 || nameStrPtr > 0x7FFFFFFFFFFF) {
            // Try reading inline string
            for (int i = 0; i < bufferSize - 1; i++) {
                char c = *(char*)(buffNamePtr + i);
                if (c == 0 || c < 32 || c > 126) { outBuffer[i] = 0; break; }
                outBuffer[i] = c;
            }
            return outBuffer[0] != 0;
        }
        
        // Read from pointer
        for (int i = 0; i < bufferSize - 1; i++) {
            char c = *(char*)(nameStrPtr + i);
            if (c == 0 || c < 32 || c > 126) { outBuffer[i] = 0; break; }
            outBuffer[i] = c;
        }
        return outBuffer[0] != 0;
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

// Main BuffManager debug function
void BuffManagerDebug() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
    if (!localPlayer) return;
    
    FILE* f = fopen("buff_debug.txt", "w");
    if (!f) return;
    
    float gameTime = *(float*)(moduleBase + Offset::oGametime);
    
    fprintf(f, "========================================\n");
    fprintf(f, "BUFF MANAGER DEBUG - Offset Discovery\n");
    fprintf(f, "========================================\n\n");
    fprintf(f, "LocalPlayer @ 0x%llX\n", localPlayer);
    fprintf(f, "GameTime: %.2f\n\n", gameTime);
    
    // ========================================================================
    // SCAN 1: Try current offset (0x2E68) with different structure interpretations
    // ========================================================================
    fprintf(f, "=== SCAN 1: Current Offset 0x2E68 ===\n\n");
    
    __try {
        // Method A: Embedded structure (LEA pattern) - current implementation
        uint64_t buffMgrEmbedded = localPlayer + Offset::oObjBuffManager;
        fprintf(f, "[Method A] Embedded @ 0x%llX\n", buffMgrEmbedded);
        
        uint64_t arrayStartA = *(uint64_t*)(buffMgrEmbedded + 0x18);
        uint64_t arrayEndA = *(uint64_t*)(buffMgrEmbedded + 0x20);
        fprintf(f, "  ArrayStart (+0x18): 0x%llX\n", arrayStartA);
        fprintf(f, "  ArrayEnd (+0x20): 0x%llX\n", arrayEndA);
        
        if (arrayStartA && arrayEndA > arrayStartA) {
            size_t countA = (arrayEndA - arrayStartA) / sizeof(uint64_t);
            fprintf(f, "  Calculated count: %llu\n", countA);
            if (countA > 256) countA = 256;
            
            for (size_t i = 0; i < countA && i < 5; i++) {
                __try {
                    uint64_t buffPtr = *(uint64_t*)(arrayStartA + i * sizeof(uint64_t));
                    if (buffPtr && buffPtr > 0x10000 && buffPtr < 0x7FFFFFFFFFFF) {
                        fprintf(f, "    [%llu] BuffPtr: 0x%llX\n", i, buffPtr);
                        
                        // Try reading buff name at oBuffScriptName pattern
                        uint64_t buffScript = *(uint64_t*)(buffPtr + 0x10);
                        if (buffScript && buffScript > 0x10000 && buffScript < 0x7FFFFFFFFFFF) {
                            char buffName[64] = {0};
                            if (TryReadBuffNameSafe(buffScript + 0x8, buffName, 64)) {
                                fprintf(f, "         Name (Script+0x8): %s\n", buffName);
                            }
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    fprintf(f, "    [%llu] CRASH reading buff\n", i);
                }
            }
        }
        
        // Method B: Pointer dereference (MOV pattern) - reference implementation
        fprintf(f, "\n[Method B] Pointer dereference\n");
        uint64_t buffMgrPtr = *(uint64_t*)(localPlayer + Offset::oObjBuffManager);
        fprintf(f, "  BuffManager* = 0x%llX\n", buffMgrPtr);
        
        if (buffMgrPtr && buffMgrPtr > 0x10000 && buffMgrPtr < 0x7FFFFFFFFFFF) {
            // leagueoflegends-master: array starts at BuffManager, ends at BuffManager + 0x10
            uint64_t arrayEndB = *(uint64_t*)(localPlayer + Offset::oObjBuffManager + 0x10);
            fprintf(f, "  EntriesEnd (+0x10 from obj): 0x%llX\n", arrayEndB);
            
            if (arrayEndB > buffMgrPtr) {
                size_t countB = (arrayEndB - (uint64_t)buffMgrPtr) / sizeof(uint64_t);
                fprintf(f, "  Calculated count: %llu\n", countB);
                if (countB > 256) countB = 256;
                
                for (size_t i = 0; i < countB && i < 5; i++) {
                    __try {
                        // BuffEntry pointer at BuffManager[i]
                        uint64_t buffEntry = *(uint64_t*)(buffMgrPtr + i * sizeof(uint64_t));
                        if (!buffEntry || buffEntry < 0x10000) continue;
                        
                        fprintf(f, "    [%llu] BuffEntry: 0x%llX\n", i, buffEntry);
                        
                        // BuffEntry -> Buff* at +0x10 (oBuffEntryBuff)
                        uint64_t buff = *(uint64_t*)(buffEntry + 0x10);
                        if (!buff || buff < 0x10000) continue;
                        
                        fprintf(f, "         Buff: 0x%llX\n", buff);
                        
                        // Buff -> NamePtr at +0x10 (oBuffNamePtr)
                        uint64_t namePtr = *(uint64_t*)(buff + 0x10);
                        char buffName[64] = {0};
                        if (TryReadBuffNameSafe(namePtr, buffName, 64)) {
                            fprintf(f, "         Name: %s\n", buffName);
                        }
                        
                        // Read buff type at +0x8
                        int buffType = *(int*)(buff + 0x8);
                        fprintf(f, "         Type: %d\n", buffType);
                        
                        // Read times
                        float startTime = *(float*)(buff + 0x18);
                        float endTime = *(float*)(buff + 0x1C);
                        fprintf(f, "         Time: %.2f - %.2f (remaining: %.2f)\n", 
                                startTime, endTime, endTime - gameTime);
                        
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        fprintf(f, "    [%llu] CRASH\n", i);
                    }
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "CRASH in SCAN 1\n");
    }
    
    // ========================================================================
    // SCAN 2: Search for BuffManager offset in range 0x2500 - 0x3500
    // Look for pattern: array of pointers where elements look like BuffEntry
    // ========================================================================
    fprintf(f, "\n\n=== SCAN 2: Offset Discovery (0x2500-0x3500) ===\n");
    fprintf(f, "Looking for pattern: ptr -> array of pointers -> BuffEntry -> Buff -> Name\n\n");
    
    int candidateCount = 0;
    
    for (uint64_t testOffset = 0x2500; testOffset <= 0x3500; testOffset += 0x8) {
        __try {
            uint64_t testPtr = *(uint64_t*)(localPlayer + testOffset);
            if (!testPtr || testPtr < 0x10000 || testPtr > 0x7FFFFFFFFFFF) continue;
            
            // Check if it looks like an array of pointers
            uint64_t firstEntry = *(uint64_t*)(testPtr);
            if (!firstEntry || firstEntry < 0x10000 || firstEntry > 0x7FFFFFFFFFFF) continue;
            
            // Try to read as BuffEntry -> Buff pattern
            __try {
                uint64_t buff = *(uint64_t*)(firstEntry + 0x10);
                if (!buff || buff < 0x10000 || buff > 0x7FFFFFFFFFFF) continue;
                
                // Try to read buff name
                uint64_t namePtr = *(uint64_t*)(buff + 0x10);
                char buffName[64] = {0};
                if (TryReadBuffNameSafe(namePtr, buffName, 64)) {
                    fprintf(f, ">>> CANDIDATE OFFSET: 0x%llX <<<\n", testOffset);
                    fprintf(f, "    BuffManager*: 0x%llX\n", testPtr);
                    fprintf(f, "    First BuffEntry: 0x%llX\n", firstEntry);
                    fprintf(f, "    First Buff: 0x%llX\n", buff);
                    fprintf(f, "    First Buff Name: %s\n\n", buffName);
                    candidateCount++;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fprintf(f, "Found %d candidate offsets\n", candidateCount);
    
    // ========================================================================
    // SCAN 3: Alternative - search for BuffManager as embedded array
    // Pattern: embedded struct with array pointers at +0x0 and +0x8 (or +0x18/+0x20)
    // ========================================================================
    fprintf(f, "\n\n=== SCAN 3: Embedded BuffManager Search ===\n");
    fprintf(f, "Looking for embedded struct with [ArrayStart, ArrayEnd] pointers\n\n");
    
    for (uint64_t testOffset = 0x2500; testOffset <= 0x3500; testOffset += 0x8) {
        __try {
            // Check +0x0 and +0x8 as potential array bounds
            uint64_t ptr1 = *(uint64_t*)(localPlayer + testOffset);
            uint64_t ptr2 = *(uint64_t*)(localPlayer + testOffset + 0x8);
            
            if (!ptr1 || !ptr2) continue;
            if (ptr1 < 0x10000 || ptr1 > 0x7FFFFFFFFFFF) continue;
            if (ptr2 < 0x10000 || ptr2 > 0x7FFFFFFFFFFF) continue;
            if (ptr2 <= ptr1) continue;
            
            size_t diff = (ptr2 - ptr1) / sizeof(uint64_t);
            if (diff < 1 || diff > 256) continue; // Reasonable buff count
            
            // Check first entry
            __try {
                uint64_t firstPtr = *(uint64_t*)(ptr1);
                if (!firstPtr || firstPtr < 0x10000) continue;
                
                // Try reading buff name via Script offset
                uint64_t script = *(uint64_t*)(firstPtr + 0x10);
                if (!script || script < 0x10000) continue;
                
                char buffName[64] = {0};
                if (TryReadBuffNameSafe(script + 0x8, buffName, 64)) {
                    fprintf(f, ">>> EMBEDDED CANDIDATE @ 0x%llX <<<\n", testOffset);
                    fprintf(f, "    ArrayStart (+0x00): 0x%llX\n", ptr1);
                    fprintf(f, "    ArrayEnd (+0x08): 0x%llX\n", ptr2);
                    fprintf(f, "    Entry Count: %llu\n", diff);
                    fprintf(f, "    First Entry: 0x%llX\n", firstPtr);
                    fprintf(f, "    Script: 0x%llX\n", script);
                    fprintf(f, "    Buff Name: %s\n\n", buffName);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    // ========================================================================
    // SCAN 4: Try reference offset 0x27C8
    // ========================================================================
    fprintf(f, "\n\n=== SCAN 4: Reference Offset 0x27C8 ===\n\n");
    
    __try {
        uint64_t refBuffMgr = *(uint64_t*)(localPlayer + 0x27C8);
        fprintf(f, "BuffManager @ 0x27C8: 0x%llX\n", refBuffMgr);
        
        if (refBuffMgr && refBuffMgr > 0x10000 && refBuffMgr < 0x7FFFFFFFFFFF) {
            uint64_t refEntriesEnd = *(uint64_t*)(localPlayer + 0x27C8 + 0x10);
            fprintf(f, "EntriesEnd (+0x10): 0x%llX\n", refEntriesEnd);
            
            if (refEntriesEnd > refBuffMgr) {
                size_t count = (refEntriesEnd - refBuffMgr) / sizeof(uint64_t);
                fprintf(f, "Count: %llu\n\n", count);
                
                for (size_t i = 0; i < count && i < 10; i++) {
                    __try {
                        uint64_t entry = *(uint64_t*)(refBuffMgr + i * sizeof(uint64_t));
                        if (!entry) continue;
                        
                        uint64_t buff = *(uint64_t*)(entry + 0x10);
                        if (!buff) continue;
                        
                        uint64_t namePtr = *(uint64_t*)(buff + 0x10);
                        char name[64] = {0};
                        if (TryReadBuffNameSafe(namePtr, name, 64)) {
                            int type = *(int*)(buff + 0x8);
                            float endTime = *(float*)(buff + 0x1C);
                            fprintf(f, "[%llu] %s (type=%d, end=%.2f)\n", i, name, type, endTime);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "CRASH reading reference offset\n");
    }
    
    // ========================================================================
    // SCAN 5: DEEP SCAN BuffInstance structure to find name offset
    // We know ArrayStart/End work at 0x2E68+0x18/0x20, now find name in BuffInstance
    // ========================================================================
    fprintf(f, "\n\n=== SCAN 5: BuffInstance Structure Discovery ===\n");
    fprintf(f, "Scanning ALL buffs to find buffs with StackCount > 1 (Zed passive, etc.)...\n\n");
    
    __try {
        uint64_t buffMgr = localPlayer + Offset::oObjBuffManager;
        uint64_t arrayStart = *(uint64_t*)(buffMgr + 0x18);
        uint64_t arrayEnd = *(uint64_t*)(buffMgr + 0x20);
        
        if (arrayStart && arrayEnd > arrayStart) {
            size_t count = (arrayEnd - arrayStart) / sizeof(uint64_t);
            // Scan tất cả buffs, không giới hạn 5
            size_t maxScan = (count > 100) ? 100 : count; // Limit 100 để tránh spam
            
            // First pass: Find buffs with StackCount > 1 AND valid (1-100, not corrupted)
            fprintf(f, "=== PASS 1: Finding buffs with StackCount > 1 (VALID: 1-100) ===\n");
            int highStackCount = 0;
            for (size_t i = 0; i < maxScan; i++) {
                __try {
                    uint64_t buffPtr = *(uint64_t*)(arrayStart + i * sizeof(uint64_t));
                    if (!buffPtr || buffPtr < 0x10000) continue;
                    
                    int stacks38 = *(int*)(buffPtr + 0x38);
                    // Only count valid stack counts (1-100), ignore corrupted data (millions)
                    if (stacks38 > 1 && stacks38 <= 100) {
                        highStackCount++;
                        // Try to get buff name
                        char buffName[64] = {0};
                        __try {
                            uint64_t scriptPtr = *(uint64_t*)(buffPtr + 0x10);
                            if (scriptPtr && scriptPtr > 0x10000 && scriptPtr < 0x7FFFFFFFFFFF) {
                                uint64_t namePtr = *(uint64_t*)(scriptPtr + 0x8);
                                if (namePtr && namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                    for (int c = 0; c < 63; c++) {
                                        char ch = *(char*)(namePtr + c);
                                        if (ch == 0) break;
                                        if (ch < 32 || ch > 126) { buffName[0] = 0; break; }
                                        buffName[c] = ch;
                                    }
                                }
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                        
                        if (buffName[0]) {
                            fprintf(f, "BUFF[%llu] @ 0x%llX \"%s\" has StackCount = %d\n", i, buffPtr, buffName, stacks38);
                        } else {
                            fprintf(f, "BUFF[%llu] @ 0x%llX (no name) has StackCount = %d\n", i, buffPtr, stacks38);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            fprintf(f, "Found %d VALID buffs with StackCount > 1 (1-100)\n\n", highStackCount);
            
            // PASS 1.5: Find buffs by name (Conqueror, etc.)
            fprintf(f, "=== PASS 1.5: Finding buffs by name (Conqueror, Dark Harvest, etc.) ===\n");
            int namedBuffs = 0;
            for (size_t i = 0; i < maxScan; i++) {
                __try {
                    uint64_t buffPtr = *(uint64_t*)(arrayStart + i * sizeof(uint64_t));
                    if (!buffPtr || buffPtr < 0x10000) continue;
                    
                    // Get buff name
                    char buffName[64] = {0};
                    __try {
                        uint64_t scriptPtr = *(uint64_t*)(buffPtr + 0x10);
                        if (scriptPtr && scriptPtr > 0x10000 && scriptPtr < 0x7FFFFFFFFFFF) {
                            uint64_t namePtr = *(uint64_t*)(scriptPtr + 0x8);
                            if (namePtr && namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                for (int c = 0; c < 63; c++) {
                                    char ch = *(char*)(namePtr + c);
                                    if (ch == 0) break;
                                    if (ch < 32 || ch > 126) { buffName[0] = 0; break; }
                                    buffName[c] = ch;
                                }
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    
                    if (buffName[0]) {
                        int stacks38 = *(int*)(buffPtr + 0x38);
                        int count3C = *(int*)(buffPtr + 0x3C);
                        int count8C = *(int*)(buffPtr + 0x8C);
                        
                        // Look for stack-related buffs
                        if (strstr(buffName, "Conqueror") || strstr(buffName, "conqueror") ||
                            strstr(buffName, "DarkHarvest") || strstr(buffName, "darkharvest") ||
                            strstr(buffName, "Electrocute") || strstr(buffName, "electrocute") ||
                            strstr(buffName, "Grasp") || strstr(buffName, "grasp") ||
                            strstr(buffName, "Fleet") || strstr(buffName, "fleet") ||
                            stacks38 > 1) {
                            namedBuffs++;
                            fprintf(f, "BUFF[%llu] @ 0x%llX \"%s\": StackCount=%d, Count(0x3C)=%d, Count(0x8C)=%d\n", 
                                i, buffPtr, buffName, stacks38, count3C, count8C);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            fprintf(f, "Found %d named buffs (stack-related or high stack)\n\n", namedBuffs);
            
            // Second pass: Detailed scan of all buffs (prioritize high stack ones)
            fprintf(f, "=== PASS 2: Detailed scan of all buffs ===\n");
            for (size_t i = 0; i < maxScan; i++) {
                __try {
                    uint64_t buffPtr = *(uint64_t*)(arrayStart + i * sizeof(uint64_t));
                    if (!buffPtr || buffPtr < 0x10000) continue;
                    
                    int stacks38 = *(int*)(buffPtr + 0x38);
                    // Only consider valid stack counts (1-100) as high stack
                    bool isHighStack = (stacks38 > 1 && stacks38 <= 100);
                    
                    // Always show high stack buffs, limit others to first 10
                    if (!isHighStack && i >= 10) continue;
                    
                    fprintf(f, "--- BUFF[%llu] @ 0x%llX", i, buffPtr);
                    if (isHighStack) fprintf(f, " ⭐ HIGH STACK (%d)", stacks38);
                    fprintf(f, " ---\n");
                    
                    // Scan all pointer offsets in BuffInstance (0x0 - 0x100)
                    fprintf(f, "Scanning pointers that lead to strings:\n");
                    
                    for (int ptrOff = 0; ptrOff <= 0x80; ptrOff += 0x8) {
                        __try {
                            uint64_t ptr1 = *(uint64_t*)(buffPtr + ptrOff);
                            if (!ptr1 || ptr1 < 0x10000 || ptr1 > 0x7FFFFFFFFFFF) continue;
                            
                            // Try direct string at ptr1
                            char name1[32] = {0};
                            bool foundDirect = false;
                            __try {
                                for (int c = 0; c < 31; c++) {
                                    char ch = *(char*)(ptr1 + c);
                                    if (ch == 0) break;
                                    if (ch < 32 || ch > 126) { name1[0] = 0; break; }
                                    name1[c] = ch;
                                }
                                if (name1[0] && strlen(name1) > 3) foundDirect = true;
                            } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            
                            // Try pointer at ptr1 + small offsets (0x0, 0x8, 0x10)
                            for (int strOff = 0; strOff <= 0x18; strOff += 0x8) {
                                __try {
                                    uint64_t ptr2 = *(uint64_t*)(ptr1 + strOff);
                                    if (!ptr2 || ptr2 < 0x10000 || ptr2 > 0x7FFFFFFFFFFF) continue;
                                    
                                    char name2[32] = {0};
                                    for (int c = 0; c < 31; c++) {
                                        char ch = *(char*)(ptr2 + c);
                                        if (ch == 0) break;
                                        if (ch < 32 || ch > 126) { name2[0] = 0; break; }
                                        name2[c] = ch;
                                    }
                                    
                                    if (name2[0] && strlen(name2) > 3) {
                                        fprintf(f, "  [+0x%02X] -> [+0x%02X] = \"%s\"\n", ptrOff, strOff, name2);
                                    }
                                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            }
                            
                            if (foundDirect) {
                                fprintf(f, "  [+0x%02X] DIRECT = \"%s\"\n", ptrOff, name1);
                            }
                            
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }
                    
                    // Also dump raw values at known offsets
                    fprintf(f, "\nRaw values at reference offsets:\n");
                    __try {
                        int type = *(int*)(buffPtr + 0x8);
                        int typeC = *(int*)(buffPtr + 0xC);
                        float start = *(float*)(buffPtr + 0x18);
                        float end = *(float*)(buffPtr + 0x1C);
                        int stacks38 = *(int*)(buffPtr + 0x38);
                        int count3C = *(int*)(buffPtr + 0x3C);  // Current offset
                        int stacks78 = *(int*)(buffPtr + 0x78);
                        int stacks8C = *(int*)(buffPtr + 0x8C);
                        
                        fprintf(f, "  +0x08 (type?): %d\n", type);
                        fprintf(f, "  +0x0C (type?): %d\n", typeC);
                        fprintf(f, "  +0x18 (start?): %.2f\n", start);
                        fprintf(f, "  +0x1C (end?): %.2f (remaining: %.2f)\n", end, end - gameTime);
                        fprintf(f, "  +0x38 (StackCount - VERIFIED): %d\n", stacks38);
                        fprintf(f, "  +0x3C (Count - CURRENT OFFSET): %d", count3C);
                        // Analyze Count value
                        if (count3C == 0) {
                            fprintf(f, " ⚠️  (Always 0 - might be wrong offset or unused field)");
                        } else if (count3C == stacks38) {
                            fprintf(f, " ⚠️  (Same as StackCount - might be duplicate)");
                        } else if (count3C == 1 && stacks38 > 1) {
                            fprintf(f, " ✅ (Count=1, StackCount=%d - Count might be instance count!)", stacks38);
                        } else if (count3C > stacks38) {
                            fprintf(f, " ✅ (Count=%d > StackCount=%d - Count might be apply count!)", count3C, stacks38);
                        } else {
                            fprintf(f, " ✅ (Different from StackCount - might be correct!)");
                        }
                        fprintf(f, "\n");
                        fprintf(f, "  +0x78 (count? - CANDIDATE): %d", stacks78);
                        if (stacks78 != 0 && stacks78 != stacks38) {
                            fprintf(f, " ⚠️  (Non-zero and different - might be Count!)");
                        }
                        fprintf(f, "\n");
                        fprintf(f, "  +0x8C (stacks? - CANDIDATE): %d", stacks8C);
                        if (stacks8C != 0 && stacks8C != stacks38) {
                            fprintf(f, " ⚠️  (Non-zero and different - might be Count!)");
                        }
                        fprintf(f, "\n");
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    
                    fprintf(f, "\n");
                    
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    fprintf(f, "CRASH scanning buff[%llu]\n", i);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "CRASH in SCAN 5\n");
    }
    
    fprintf(f, "\n\n========================================\n");
    fprintf(f, "DEBUG COMPLETE\n");
    fprintf(f, "========================================\n");
    
    fclose(f);
}

// ============================================================================
// BUFF SCREEN DISPLAY - Render active buffs on screen (uses SEH for safety)
// ============================================================================
bool g_showBuffOverlay = false;

// Struct to hold buff info for safe display
struct SafeBuffInfo {
    char name[64];
    int type;
    float startTime;
    float endTime;
    float remaining;
    int stacks;
    bool valid;
};

// Global array for safe buff storage (updated by SEH function, read by ImGui)
SafeBuffInfo g_buffInfos[32];
int g_buffInfoCount = 0;

// Safe function to populate buff info for any unit (C-style for SEH)
void GetSafeBuffs(uint64_t unitAddress, SafeBuffInfo* outArr, int& outCount, int maxCount) {
    outCount = 0;
    if (!unitAddress) return;
    
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    float gameTime = *(float*)(moduleBase + Offset::oGametime);
    
    __try {
        uint64_t buffMgr = unitAddress + Offset::oObjBuffManager;
        uint64_t arrayStart = *(uint64_t*)(buffMgr + 0x18);
        uint64_t arrayEnd = *(uint64_t*)(buffMgr + 0x20);
        
        if (!arrayStart || arrayEnd <= arrayStart) return;
        
        size_t count = (arrayEnd - arrayStart) / sizeof(uint64_t);
        if (count > 256) count = 256;
        
        int validCount = 0;
        
        for (size_t i = 0; i < count && validCount < maxCount; i++) {
            __try {
                uint64_t buffPtr = *(uint64_t*)(arrayStart + i * sizeof(uint64_t));
                if (!buffPtr || buffPtr < 0x10000) continue;
                
                // Read basic values first
                float endTime = *(float*)(buffPtr + 0x1C);
                
                // Skip invalid buffs (ended or permanent internal)
                // Keep permanent buffs (endTime > 25000) for inspection
                if (endTime <= 0 || (endTime < gameTime && endTime < 20000)) continue;
                
                float startTime = *(float*)(buffPtr + 0x18);
                int type = *(int*)(buffPtr + 0x08);
                int stacks = *(int*)(buffPtr + 0x38);
                
                // Try to read name
                char name[64] = {0};
                __try {
                    uint64_t buffScript = *(uint64_t*)(buffPtr + 0x10);
                    if (buffScript && buffScript > 0x10000 && buffScript < 0x7FFFFFFFFFFF) {
                        char* namePtr = *(char**)(buffScript + 0x08);
                        if (namePtr && (uint64_t)namePtr > 0x10000 && (uint64_t)namePtr < 0x7FFFFFFFFFFF) {
                            for (int c = 0; c < 63; c++) {
                                char ch = namePtr[c];
                                if (ch == 0) break;
                                if (ch < 32 || ch > 126) { name[0] = 0; break; }
                                name[c] = ch;
                            }
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    name[0] = 0;
                }
                
                // Only add if we got a name
                if (name[0] != 0) {
                    SafeBuffInfo& info = outArr[validCount];
                    strncpy_s(info.name, name, 63);
                    info.type = type;
                    info.startTime = startTime;
                    info.endTime = endTime;
                    info.remaining = endTime - gameTime;
                    info.stacks = stacks;
                    info.valid = true;
                    validCount++;
                }
                
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                // Skip this buff
            }
        }
        
        outCount = validCount;
        
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        outCount = 0;
    }
}

// Global for spell debug
bool showSpellDebugOverlay = false;
bool continuousSpellDump = false;

// Global for BuffManager & SpellData debug
bool showBuffManagerDebug = false;
bool showSpellDataDebug = false;

// ============================================================================
// CONTINUOUS MISSILE LOGGER - Logs missiles with SpellName and Position for enemy spell capture
// ============================================================================
void ContinuousMissileLog() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + 0x1D66AE0);
    int playerNetId = localPlayer ? *(int*)(localPlayer + 0xC4) : 0;
    
    uint64_t missileMgr = *(uint64_t*)(moduleBase + 0x1D32B08);
    if (!missileMgr) return;
    
    uint64_t arrayPtr = *(uint64_t*)(missileMgr + 0x08);
    int size = *(int*)(missileMgr + 0x10);
    
    if (size <= 0 || size >= 100) return;
    
    FILE* f = fopen("missile_log.txt", "a");
    if (!f) return;
    
    float gameTime = *(float*)(moduleBase + 0x1D3D370); // oGameTime
    fprintf(f, "\n=== GameTime: %.2f | Missiles: %d | PlayerNetID: 0x%X ===\n", gameTime, size, playerNetId);
    
    for (int m = 0; m < size && m < 10; m++) {
        __try {
            uint64_t missile = *(uint64_t*)(arrayPtr + m * 0x8);
            if (!missile) continue;
            
            // Get SpellName via SpellInfo chain (0x1D8 -> 0x18 -> 0x8)
            char spellName[64] = "???";
            __try {
                uint64_t spellInfo = *(uint64_t*)(missile + 0x1D8);
                if (spellInfo > 0x10000) {
                    uint64_t spellData = *(uint64_t*)(spellInfo + 0x18);
                    if (spellData > 0x10000) {
                        uint64_t namePtr = *(uint64_t*)(spellData + 0x8);
                        if (namePtr > 0x10000) {
                            for (int i = 0; i < 63; i++) {
                                char c = *(char*)(namePtr + i);
                                if (c == 0 || c < 32 || c > 126) { spellName[i] = 0; break; }
                                spellName[i] = c;
                            }
                        }
                    }
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            fprintf(f, "Missile[%d] @ 0x%llX | NetID: 0x%X | Name: %s\n", m, missile, *(int*)(missile + 0x20), spellName);
            
            // Find all valid Vec3 positions
            fprintf(f, "  Positions: ");
            int posCount = 0;
            for (int off = 0x100; off < 0x400; off += 4) {
                __try {
                    float x = *(float*)(missile + off);
                    float y = *(float*)(missile + off + 4);
                    float z = *(float*)(missile + off + 8);
                    if (x > 100 && x < 16000 && z > 100 && z < 16000 && y > -100 && y < 500) {
                        fprintf(f, "[0x%03X](%.0f,%.0f,%.0f) ", off, x, y, z);
                        posCount++;
                        if (posCount >= 5) break;
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (posCount == 0) fprintf(f, "NONE FOUND");
            fprintf(f, "\n");
            
            // Scan ALL NetIDs in missile - focus on player
            fprintf(f, "  NetIDs: ");
            bool foundPlayer = false;
            for (int off = 0; off < 0x400; off += 4) {
                __try {
                    int val = *(int*)(missile + off);
                    if (val > 0x40000000 && val < 0x50000000) {
                        fprintf(f, "[0x%03X]=0x%X ", off, val);
                        if (val == playerNetId) { fprintf(f, "<PLAYER!> "); foundPlayer = true; }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            fprintf(f, "\n");
            
            // Also scan all pointers for player NetID
            if (!foundPlayer) {
                fprintf(f, "  --- DEEP SCAN FOR PLAYER ---\n");
                for (int ptrOff = 0; ptrOff < 0x200; ptrOff += 8) {
                    __try {
                        uint64_t ptr = *(uint64_t*)(missile + ptrOff);
                        if (ptr > 0x10000000000 && ptr < 0x800000000000) {
                            for (int innerOff = 0; innerOff < 0x200; innerOff += 4) {
                                __try {
                                    int val = *(int*)(ptr + innerOff);
                                    if (val == playerNetId) {
                                        fprintf(f, "    PTR[0x%03X] + 0x%03X = 0x%X <-- PLAYER FOUND!\n", ptrOff, innerOff, val);
                                        foundPlayer = true;
                                    }
                                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            if (!foundPlayer) fprintf(f, "  >>> PLAYER NOT TARGET OF THIS MISSILE <<<\n");
            
            // Scan pointers for SpellInfo
            fprintf(f, "  --- SCANNING POINTERS FOR SPELLINFO ---\n");
            for (int ptrOff = 0; ptrOff < 0x200; ptrOff += 8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(missile + ptrOff);
                    if (ptr > 0x10000000000 && ptr < 0x800000000000) {
                        bool foundNetId = false;
                        for (int innerOff = 0x80; innerOff < 0x150; innerOff += 4) {
                            __try {
                                int val = *(int*)(ptr + innerOff);
                                if (val > 0x40000000 && val < 0x50000000) {
                                    if (!foundNetId) {
                                        fprintf(f, "  PTR[0x%03X] = 0x%llX\n", ptrOff, ptr);
                                        foundNetId = true;
                                    }
                                    fprintf(f, "      [+0x%03X] NetID: 0x%X", innerOff, val);
                                    if (val == playerNetId) fprintf(f, " <-- PLAYER");
                                    fprintf(f, "\n");
                                }
                            } __except(EXCEPTION_EXECUTE_HANDLER) {}
                        }
                        
                        // Try to find spell name - scan more offsets
                        __try {
                            // Direct name in structure
                            for (int nameOff = 0x20; nameOff < 0x100; nameOff += 8) {
                                __try {
                                    char* name = (char*)(ptr + nameOff);
                                    if (name[0] >= 'A' && name[0] <= 'Z' && name[1] >= 'a' && name[1] <= 'z' && name[2] >= 'a' && name[2] <= 'z') {
                                        fprintf(f, "  PTR[0x%03X] +0x%X: %.40s\n", ptrOff, nameOff, name);
                                    }
                                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            }
                            // SpellData pointer then name
                            int spellDataOffsets[] = {0x0, 0x8, 0x50, 0x60, 0x68, 0x128};
                            int nameOffsets[] = {0x28, 0x80, 0x88};
                            for (int sd = 0; sd < 6; sd++) {
                                uint64_t spellData = *(uint64_t*)(ptr + spellDataOffsets[sd]);
                                if (spellData > 0x10000000000 && spellData < 0x800000000000) {
                                    for (int nm = 0; nm < 3; nm++) {
                                        __try {
                                            char* name = (char*)(spellData + nameOffsets[nm]);
                                            if (name[0] >= 'A' && name[0] <= 'Z' && name[1] >= 'a' && name[1] <= 'z') {
                                                fprintf(f, "  PTR[0x%03X]+0x%X -> +0x%X: %.40s\n", 
                                                    ptrOff, spellDataOffsets[sd], nameOffsets[nm], name);
                                            }
                                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                                    }
                                }
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Dump ALL positions (full scan 0x0 - 0x500)
            fprintf(f, "  --- ALL POSITIONS (0x0-0x500) ---\n");
            for (int off = 0; off < 0x500; off += 4) {
                __try {
                    float x = *(float*)(missile + off);
                    float y = *(float*)(missile + off + 4);
                    float z = *(float*)(missile + off + 8);
                    // Valid map coords: X/Z 100-15000, Y -100 to 500
                    if (x > 100 && x < 15000 && z > 100 && z < 15000 && y > -100 && y < 500) {
                        fprintf(f, "    [0x%03X] (%.1f, %.1f, %.1f)\n", off, x, y, z);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Also scan pointers for position (SpellInfo->StartPos, EndPos)
            fprintf(f, "  --- POSITIONS IN POINTERS ---\n");
            for (int ptrOff = 0; ptrOff < 0x200; ptrOff += 8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(missile + ptrOff);
                    if (ptr > 0x10000000000 && ptr < 0x800000000000) {
                        for (int posOff = 0; posOff < 0x200; posOff += 4) {
                            __try {
                                float x = *(float*)(ptr + posOff);
                                float y = *(float*)(ptr + posOff + 4);
                                float z = *(float*)(ptr + posOff + 8);
                                if (x > 100 && x < 15000 && z > 100 && z < 15000 && y > -100 && y < 500) {
                                    fprintf(f, "    PTR[0x%03X]+0x%03X (%.1f, %.1f, %.1f)\n", ptrOff, posOff, x, y, z);
                                }
                            } __except(EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    fclose(f);
}

// ============================================================================
// MISSILE OFFSET SCANNER - Find MissileData offsets (continuous mode)
// ============================================================================
bool continuousMissileScan = false;

void MissileOffsetScan() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
    int playerNetId = localPlayer ? *(int*)(localPlayer + 0xC4) : 0;
    
    uint64_t missileMgr = *(uint64_t*)(moduleBase + Offset::oMissileManager);
    if (!missileMgr) return;
    
    FILE* f = fopen("missile_offset_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== MISSILE MANAGER STRUCTURE SCAN ===\n");
    fprintf(f, "MissileManager @ 0x%llX\n", missileMgr);
    fprintf(f, "PlayerNetID: 0x%X\n\n", playerNetId);
    
    // Scan MissileManager structure to find correct list offset
    fprintf(f, "--- SCANNING MANAGER STRUCTURE (0x0 - 0x100) ---\n");
    for (int off = 0; off < 0x100; off += 8) {
        __try {
            uint64_t ptr = *(uint64_t*)(missileMgr + off);
            int val32 = *(int*)(missileMgr + off);
            fprintf(f, "[0x%02X] ptr=0x%llX  int32=%d\n", off, ptr, val32);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "[0x%02X] UNREADABLE\n", off);
        }
    }
    fprintf(f, "\n");
    
    // Try different list structures
    fprintf(f, "--- TRYING DIFFERENT LIST STRUCTURES ---\n\n");
    
    // Structure 1: Direct array at +0x08, size at +0x10 (MinionList style)
    __try {
        uint64_t arr1 = *(uint64_t*)(missileMgr + 0x08);
        int size1 = *(int*)(missileMgr + 0x10);
        fprintf(f, "Structure 1 (MinionList): arr=0x%llX, size=%d\n", arr1, size1);
        if (arr1 > 0x10000 && size1 > 0 && size1 < 200) {
            for (int i = 0; i < 3 && i < size1; i++) {
                uint64_t obj = *(uint64_t*)(arr1 + i * 8);
                fprintf(f, "  [%d] = 0x%llX\n", i, obj);
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "Structure 1: FAILED\n"); }
    fprintf(f, "\n");
    
    // Structure 2: Direct pointer (missileMgr itself is the list)
    __try {
        fprintf(f, "Structure 2 (Direct): First 5 pointers from missileMgr\n");
        for (int i = 0; i < 5; i++) {
            uint64_t obj = *(uint64_t*)(missileMgr + i * 8);
            fprintf(f, "  [%d] = 0x%llX\n", i, obj);
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "Structure 2: FAILED\n"); }
    fprintf(f, "\n");
    
    // Structure 3: Linked list (first->next pattern)
    __try {
        uint64_t first = *(uint64_t*)(missileMgr);
        fprintf(f, "Structure 3 (LinkedList): first=0x%llX\n", first);
        if (first > 0x10000) {
            uint64_t current = first;
            for (int i = 0; i < 5 && current > 0x10000; i++) {
                fprintf(f, "  Node[%d] = 0x%llX\n", i, current);
                uint64_t next = *(uint64_t*)(current); // next at +0x0
                if (next == current) break;
                current = next;
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "Structure 3: FAILED\n"); }
    fprintf(f, "\n");
    
    // Now use the standard structure and scan objects
    uint64_t arrayPtr = *(uint64_t*)(missileMgr + 0x08);
    int size = *(int*)(missileMgr + 0x10);
    
    if (size <= 0 || size >= 200) {
        fprintf(f, "Invalid size: %d, trying alternative...\n", size);
        // Try size at different offset
        size = *(int*)(missileMgr + 0x18);
        fprintf(f, "Size at 0x18: %d\n", size);
        if (size <= 0 || size >= 200) {
            fclose(f);
            return;
        }
    }
    
    fprintf(f, "\n=== SCANNING ALL %d OBJECTS ===\n\n", size);
    
    // Scan ALL missiles, not just first 3
    for (int m = 0; m < size && m < 20; m++) {
        __try {
            uint64_t missile = *(uint64_t*)(arrayPtr + m * 0x8);
            if (!missile) continue;
            
            fprintf(f, "=== MISSILE[%d] @ 0x%llX ===\n\n", m, missile);
            
            // Quick check: Is this a spell missile or env object?
            bool isEnvObject = false;
            __try {
                char* nameCheck = (char*)(missile + 0x0C0);
                if (nameCheck[0] == 'P' && nameCheck[1] == 'l' && nameCheck[2] == 'a' && nameCheck[3] == 'y') {
                    fprintf(f, ">>> SKIP: Environmental object (Play_env_*) <<<\n\n");
                    isEnvObject = true;
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            if (isEnvObject) continue;
            
            // FULL SCAN for PlayerNetID (0x0 - 0x800)
            fprintf(f, "--- FULL SCAN FOR PLAYER NETID 0x%X ---\n", playerNetId);
            bool isPlayerSpell = false;
            for (int off = 0; off < 0x800; off += 4) {
                __try {
                    int val = *(int*)(missile + off);
                    if (val == playerNetId) {
                        fprintf(f, "[0x%03X] = 0x%X  <<< PLAYER NETID FOUND! >>>\n", off, val);
                        isPlayerSpell = true;
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            if (isPlayerSpell) {
                fprintf(f, "\n*** THIS IS YOUR SPELL! ***\n");
            } else {
                fprintf(f, "Player NetID NOT found in this missile (0x0 - 0x800)\n");
            }
            fprintf(f, "\n");
            
            // RAW HEX DUMP first 0x100 bytes
            fprintf(f, "--- RAW HEX DUMP (0x0 - 0x100) ---\n");
            for (int row = 0; row < 0x100; row += 0x10) {
                fprintf(f, "[0x%03X] ", row);
                for (int col = 0; col < 0x10; col += 4) {
                    __try {
                        uint32_t val = *(uint32_t*)(missile + row + col);
                        fprintf(f, "%08X ", val);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        fprintf(f, "???????? ");
                    }
                }
                fprintf(f, "\n");
            }
            fprintf(f, "\n");
            
            // EXPANDED: Scan ALL NetIDs (0x0 - 0x500)
            fprintf(f, "--- ALL NetIDs (0x40000000 range) ---\n");
            for (int off = 0; off < 0x500; off += 4) {
                __try {
                    int val = *(int*)(missile + off);
                    if (val > 0x40000000 && val < 0x50000000) {
                        fprintf(f, "[0x%03X] = 0x%X", off, val);
                        if (val == playerNetId) fprintf(f, " [PLAYER!]");
                        fprintf(f, "\n");
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // EXPANDED: Scan ALL Vec3 positions (0x0 - 0x800) - wider range!
            fprintf(f, "\n--- ALL Vec3 Positions (valid map coords, 0x0-0x800) ---\n");
            for (int off = 0; off < 0x800; off += 4) {
                __try {
                    float x = *(float*)(missile + off);
                    float y = *(float*)(missile + off + 4);
                    float z = *(float*)(missile + off + 8);
                    if (x > 100 && x < 16000 && z > 100 && z < 16000 && y > -200 && y < 500) {
                        fprintf(f, "[0x%03X] (%.1f, %.1f, %.1f)\n", off, x, y, z);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Also scan for string at 0x10 area (looks like spell name)
            fprintf(f, "\n--- Possible Spell Names (strings) ---\n");
            for (int off = 0; off < 0x100; off += 8) {
                __try {
                    char* str = (char*)(missile + off);
                    // Check if looks like ASCII string
                    if (str[0] >= 'A' && str[0] <= 'z' && str[1] >= 'a' && str[1] <= 'z') {
                        char name[32] = {0};
                        for (int i = 0; i < 31; i++) {
                            if (str[i] == 0 || str[i] < 32 || str[i] > 126) break;
                            name[i] = str[i];
                        }
                        if (strlen(name) > 3) {
                            fprintf(f, "[0x%03X] Direct String: \"%s\"\n", off, name);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // EXPANDED: Scan ALL pointers and try to find spell name (0x0 - 0x300)
            fprintf(f, "\n--- ALL Pointers -> SpellName ---\n");
            for (int off = 0; off < 0x300; off += 8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(missile + off);
                    if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                        // Try SpellInfo->SpellData(0x18)->Name(0x8)
                        __try {
                            uint64_t spellData = *(uint64_t*)(ptr + 0x18);
                            if (spellData > 0x10000 && spellData < 0x7FFFFFFFFFFF) {
                                uint64_t namePtr = *(uint64_t*)(spellData + 0x8);
                                if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                    char name[32] = {0};
                                    bool valid = true;
                                    for (int i = 0; i < 20; i++) {
                                        char c = *(char*)(namePtr + i);
                                        if (c == 0) break;
                                        if (c < 32 || c > 126) { valid = false; break; }
                                        name[i] = c;
                                    }
                                    if (valid && name[0] >= 'A' && name[0] <= 'Z') {
                                        fprintf(f, "[0x%03X] -> SpellInfo -> SpellData -> Name: %s\n", off, name);
                                    }
                                }
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                        
                        // Also try SpellData(0x0)->Name(0x8) directly
                        __try {
                            uint64_t namePtr = *(uint64_t*)(ptr + 0x8);
                            if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                char name[32] = {0};
                                bool valid = true;
                                for (int i = 0; i < 20; i++) {
                                    char c = *(char*)(namePtr + i);
                                    if (c == 0) break;
                                    if (c < 32 || c > 126) { valid = false; break; }
                                    name[i] = c;
                                }
                                if (valid && name[0] >= 'A' && name[0] <= 'Z') {
                                    fprintf(f, "[0x%03X] -> Direct Name@0x8: %s\n", off, name);
                                }
                            }
                        } __except(EXCEPTION_EXECUTE_HANDLER) {}
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            fprintf(f, "\n");
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "CRASH reading missile[%d]\n\n", m);
        }
    }
    
    fclose(f);
}

// Global for missile offset scan
bool doMissileOffsetScan = false;
bool doMissileDecryptionScan = false;  // NEW: Missile Decryption Scan (theo chiến lược chienluocmissile)

// ============================================================================
// MISSILE OFFSET SCAN - Tìm và verify các offsets cho Evade system
// ============================================================================
// Helper functions for safe reading (noinline to avoid C2712 in caller functions with C++ objects)
__declspec(noinline) bool ScanSafeReadFloat(uint64_t addr, float* out) {
    __try {
        if (!addr) return false;
        *out = *(float*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

__declspec(noinline) bool ScanSafeReadInt(uint64_t addr, int* out) {
    __try {
        if (!addr) return false;
        *out = *(int*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

__declspec(noinline) bool ScanSafeReadQWORD(uint64_t addr, uint64_t* out) {
    __try {
        if (!addr) return false;
        *out = *(uint64_t*)addr;
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}

inline bool ScanSafeReadVec3(uint64_t addr, float* x, float* y, float* z) {
    return ScanSafeReadFloat(addr, x) && ScanSafeReadFloat(addr + 4, y) && ScanSafeReadFloat(addr + 8, z);
}

void ScanMissileOffsets() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    if (!moduleBase) return;
    
    uint64_t localPlayer = 0;
    __try {
        localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (!localPlayer) return;
    
    int playerNetId = 0;
    __try {
        playerNetId = *(int*)(localPlayer + Offset::oObjNetId);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    
    float gameTime = 0.0f;
    __try {
        gameTime = *(float*)(moduleBase + Offset::oGametime);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    
    uint64_t missileMgr = 0;
    __try {
        missileMgr = *(uint64_t*)(moduleBase + Offset::oMissileList);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return;
    }
    if (!missileMgr) return;
    
    FILE* f = fopen("missile_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "========================================\n");
    fprintf(f, "MISSILE OFFSET SCAN - Tìm offsets cho Evade\n");
    fprintf(f, "========================================\n");
    fprintf(f, "LocalPlayer: 0x%llX\n", localPlayer);
    fprintf(f, "PlayerNetID: 0x%X\n", playerNetId);
    fprintf(f, "GameTime: %.2f\n", gameTime);
    fprintf(f, "MissileManager: 0x%llX\n", missileMgr);
    fprintf(f, "========================================\n\n");
    
    // Get missile array
    uint64_t arrayPtr = 0;
    int size = 0;
    __try {
        arrayPtr = *(uint64_t*)(missileMgr + 0x08);
        size = *(int*)(missileMgr + 0x10);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "ERROR: Cannot read missile array\n");
        fclose(f);
        return;
    }
    
    if (!arrayPtr || size <= 0 || size > 100) {
        fprintf(f, "ERROR: Invalid missile array (ptr=0x%llX, size=%d)\n", arrayPtr, size);
        fclose(f);
        return;
    }
    
    fprintf(f, "Found %d missiles\n\n", size);
    
    // Scan each missile
    for (int m = 0; m < size && m < 50; m++) {
        uint64_t missile = 0;
        // Read missile pointer (C-style, no C++ objects)
        __try {
            missile = *(uint64_t*)(arrayPtr + m * 0x8);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            continue;
        }
        
        if (!missile || missile < 0x10000) continue;
        
        // Get Name using SDK
        char mName[64] = "Unknown";
        __try {
            typedef const char*(*fnGetName)(uint64_t);
            // Manual read name logic consistent with Project style
            uint64_t nameAddr = *(uint64_t*)(missile + Offset::oObjName); // 0x68 or similar
            if (nameAddr) {
                // Inline Read String
                for (int i=0; i<63; i++) {
                    char c = *(char*)(nameAddr + i);
                    if (c == 0) break;
                    mName[i] = c;
                }
            }
        } __except(1) {}

        fprintf(f, "=== MISSILE[%d] @ 0x%llX | Name: %s ===\n", m, missile, mName);
        
        // Filter: Nếu tên là "Unknown" hoặc chứa "SRU_" (Minion/Monster visual), có thể skip nếu muốn
        // Nhưng tạm thời cứ in hết để debug.
        
        // ====================================================================
        // PHASE 1: Tìm SpellInfo pointer (so sánh với SpellInfo từ spell cast)
        // ====================================================================
        fprintf(f, "\n--- PHASE 1: Tìm SpellInfo pointer ---\n");
        
        // Get SpellInfo từ LocalPlayer spell slots để so sánh (C-style struct)
        float spellStartPos[3] = {0, 0, 0};
        float spellEndPos[3] = {0, 0, 0};
        int spellSrcIdx = 0;
        char spellName[64] = {0};
        uint64_t referenceSpellInfo = 0;
        
        uint64_t spellBook = 0;
        __try {
            spellBook = *(uint64_t*)(localPlayer + Offset::oObjSpellBook);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            spellBook = 0;
        }
            
        // First try: Get from OnCastingSpell (có thể có SpellInfo khi đang cast)
            if (spellBook) {
            uint64_t onCastingSpell = 0;
            __try {
                onCastingSpell = *(uint64_t*)(spellBook + Offset::oObjOnCastingSpell);
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                onCastingSpell = 0;
            }
            
            fprintf(f, "DEBUG: spellBook=0x%llX, onCastingSpell=0x%llX\n", spellBook, onCastingSpell);
            
            if (onCastingSpell) {
                uint64_t spellInfo = 0;
                __try {
                    spellInfo = *(uint64_t*)(onCastingSpell + Offset::oSpellSlotSpellInfo);
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    spellInfo = 0;
                }
                
                if (spellInfo && spellInfo > 0x100000) {
                    uint64_t spellData = 0;
                    __try {
                        spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        spellData = 0;
                    }
                    
                    if (spellData && spellData > 0x100000) {
                        uint64_t namePtr = 0;
                        __try {
                            namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            namePtr = 0;
                        }
                        
                        if (namePtr && namePtr > 0x10000) {
                            for (int c = 0; c < 63; c++) {
                                char ch = 0;
                                __try {
                                    ch = *(char*)(namePtr + c);
                                } __except(EXCEPTION_EXECUTE_HANDLER) {
                                    spellName[0] = 0;
                                    break;
                                }
                                if (ch == 0) break;
                                if (ch < 32 || ch > 126) { spellName[0] = 0; break; }
                                spellName[c] = ch;
                            }
                            
                            if (spellName[0]) {
                                referenceSpellInfo = spellInfo;
                                // Get spell positions
                                float sx, sy, sz, ex, ey, ez;
                                if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos, &sx) &&
                                    ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 4, &sy) &&
                                    ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 8, &sz)) {
                                    spellStartPos[0] = sx; spellStartPos[1] = sy; spellStartPos[2] = sz;
                                }
                                if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos, &ex) &&
                                    ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 4, &ey) &&
                                    ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 8, &ez)) {
                                    spellEndPos[0] = ex; spellEndPos[1] = ey; spellEndPos[2] = ez;
                                }
                                __try {
                                    spellSrcIdx = *(int*)(spellInfo + Offset::oSpellInfoSrcIndex);
                                } __except(EXCEPTION_EXECUTE_HANDLER) {
                                    spellSrcIdx = 0;
                                }
                            }
                        }
                    }
                }
            }
        }
        
        // Second try: Get from spell slots (Q, W, E, R) nếu chưa có từ OnCastingSpell
        if (!spellName[0] && spellBook) {
            // Try to get recent spell cast info (Q, W, E, R) - ưu tiên slot 1 (W) trước
            int slotOrder[4] = {1, 0, 2, 3}; // W, Q, E, R
            fprintf(f, "DEBUG: Trying spell slots...\n");
            for (int s = 0; s < 4; s++) {
                int slot = slotOrder[s];
                    uint64_t spellSlot = 0;
                    __try {
                        spellSlot = *(uint64_t*)(spellBook + Offset::oObjSpellBookSpellSlot + slot * 0x8);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                fprintf(f, "DEBUG: Slot[%d] = 0x%llX\n", slot, spellSlot);
                    if (!spellSlot) continue;
                    
                    uint64_t spellInfo = 0;
                    __try {
                        spellInfo = *(uint64_t*)(spellSlot + Offset::oSpellSlotSpellInfo);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                if (!spellInfo || spellInfo < 0x100000) continue;
                    
                    uint64_t spellData = 0;
                    __try {
                        spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                if (!spellData || spellData < 0x100000) continue;
                    
                    // Get spell name
                    uint64_t namePtr = 0;
                    __try {
                        namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                if (namePtr && namePtr > 0x10000) {
                        for (int c = 0; c < 63; c++) {
                            char ch = 0;
                            __try {
                                ch = *(char*)(namePtr + c);
                            } __except(EXCEPTION_EXECUTE_HANDLER) {
                                spellName[0] = 0;
                                break;
                            }
                            if (ch == 0) break;
                            if (ch < 32 || ch > 126) { spellName[0] = 0; break; }
                            spellName[c] = ch;
                    }
                    
                    if (spellName[0]) {
                        referenceSpellInfo = spellInfo;
                    // Get spell positions
                    float sx, sy, sz, ex, ey, ez;
                    if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos, &sx) &&
                        ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 4, &sy) &&
                        ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 8, &sz)) {
                        spellStartPos[0] = sx; spellStartPos[1] = sy; spellStartPos[2] = sz;
                    }
                    if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos, &ex) &&
                        ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 4, &ey) &&
                        ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 8, &ez)) {
                        spellEndPos[0] = ex; spellEndPos[1] = ey; spellEndPos[2] = ez;
                    }
                    __try {
                        spellSrcIdx = *(int*)(spellInfo + Offset::oSpellInfoSrcIndex);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        spellSrcIdx = 0;
                    }
                        break; // Found spell name
                    }
                }
                }
            }
            
            fprintf(f, "Reference SpellInfo: Name=\"%s\", SrcIdx=0x%X, Ptr=0x%llX\n", 
                spellName[0] ? spellName : "N/A", spellSrcIdx, referenceSpellInfo);
            if (spellStartPos[0] > 0) {
                fprintf(f, "  StartPos: (%.1f, %.1f, %.1f)\n", spellStartPos[0], spellStartPos[1], spellStartPos[2]);
            }
            if (spellEndPos[0] > 0) {
                fprintf(f, "  EndPos: (%.1f, %.1f, %.1f)\n", spellEndPos[0], spellEndPos[1], spellEndPos[2]);
            }
            if (!spellName[0]) {
                fprintf(f, "  ⚠️  No reference SpellInfo from spell slots - will try to get from indirect scan\n");
            }
            fprintf(f, "\n");
            
            // Scan for SpellInfo pointer (0x0 - 0x800) - Mở rộng phạm vi
            struct SpellInfoCandidate { uint64_t offset; uint64_t ptr; };
            SpellInfoCandidate spellInfoCandidates[200];
            int spellInfoCount = 0;
            fprintf(f, "Scanning for SpellInfo pointers (0x0 - 0x800)...\n");
            for (uint64_t off = 0x0; off <= 0x800; off += 0x8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(missile + off);
                    if (ptr < 0x100000 || ptr > 0x7FFFFFFFFFFF) continue;
                    
                    // Check if this pointer leads to SpellInfo
                    // SpellInfo + 0x18 = SpellData pointer
                    uint64_t spellData = 0;
                    __try {
                        spellData = *(uint64_t*)(ptr + Offset::oSpellInfoSpellData);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                    if (spellData < 0x100000 || spellData > 0x7FFFFFFFFFFF) continue;
                    
                    // SpellData + 0x8 = Spell name
                    uint64_t namePtr = 0;
                    __try {
                        namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                    if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) continue;
                    
                    char testName[64] = {0};
                    bool validName = false;
                    for (int c = 0; c < 63; c++) {
                        char ch = 0;
                        __try {
                            ch = *(char*)(namePtr + c);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            break;
                        }
                        if (ch == 0 && c > 3) { validName = true; break; }
                        if (ch < 32 || ch > 126) break;
                        testName[c] = ch;
                    }
                    
                    if (validName && testName[0] && spellInfoCount < 200) {
                        spellInfoCandidates[spellInfoCount].offset = off;
                        spellInfoCandidates[spellInfoCount].ptr = ptr;
                        spellInfoCount++;
                        fprintf(f, "  [0x%llX] -> SpellInfo* = 0x%llX", off, ptr);
                        fprintf(f, " -> SpellData = 0x%llX", spellData);
                        fprintf(f, " -> Name = \"%s\"", testName);
                        
                        // Verify: Compare with reference
                        if (referenceSpellInfo && ptr == referenceSpellInfo) {
                            fprintf(f, " ✅ MATCHES REFERENCE PTR!");
                        } else if (spellName[0] && strcmp(testName, spellName) == 0) {
                            fprintf(f, " ✅ MATCHES REFERENCE NAME!");
                        }
                        fprintf(f, "\n");
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            if (spellInfoCount == 0) {
                fprintf(f, "  ❌ No SpellInfo pointer found\n");
                fprintf(f, "  ⚠️  Trying alternative scan: Check pointers that might lead to SpellInfo indirectly...\n");
                
                // Alternative: Scan for pointers that might contain SpellInfo indirectly
                for (uint64_t off = 0x0; off <= 0x800; off += 0x8) {
                    __try {
                        uint64_t ptr = *(uint64_t*)(missile + off);
                        if (ptr < 0x100000 || ptr > 0x7FFFFFFFFFFF) continue;
                        
                        // Check if this pointer + offset leads to SpellInfo
                        for (uint64_t innerOff = 0x0; innerOff <= 0x100; innerOff += 0x8) {
                            __try {
                                uint64_t spellInfo = *(uint64_t*)(ptr + innerOff);
                                if (spellInfo < 0x100000 || spellInfo > 0x7FFFFFFFFFFF) continue;
                                
                                uint64_t spellData = 0;
                                __try {
                                    spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                                } __except(EXCEPTION_EXECUTE_HANDLER) {
                                    continue;
                                }
                                if (spellData < 0x100000 || spellData > 0x7FFFFFFFFFFF) continue;
                                
                                uint64_t namePtr = 0;
                                __try {
                                    namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                                } __except(EXCEPTION_EXECUTE_HANDLER) {
                                    continue;
                                }
                                if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) continue;
                                
                                char testName[64] = {0};
                                bool validName = false;
                                for (int c = 0; c < 63; c++) {
                                    char ch = 0;
                                    __try {
                                        ch = *(char*)(namePtr + c);
                                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                                        break;
                                    }
                                    if (ch == 0 && c > 3) { validName = true; break; }
                                    if (ch < 32 || ch > 126) break;
                                    testName[c] = ch;
                                }
                                
                                if (validName && testName[0]) {
                                    fprintf(f, "  [0x%llX] -> PTR[0x%llX] -> SpellInfo* = 0x%llX -> Name = \"%s\"", 
                                        off, innerOff, spellInfo, testName);
                                    
                                    // If we don't have reference SpellInfo yet, use this one
                                    if (!referenceSpellInfo && spellInfoCount == 0) {
                                        referenceSpellInfo = spellInfo;
                                        strncpy(spellName, testName, 63);
                                        spellName[63] = 0;
                                        
                                        // Get spell positions from this SpellInfo
                                        float sx, sy, sz, ex, ey, ez;
                                        if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos, &sx) &&
                                            ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 4, &sy) &&
                                            ScanSafeReadFloat(spellInfo + Offset::oSpellInfoStartPos + 8, &sz)) {
                                            spellStartPos[0] = sx; spellStartPos[1] = sy; spellStartPos[2] = sz;
                                        }
                                        if (ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos, &ex) &&
                                            ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 4, &ey) &&
                                            ScanSafeReadFloat(spellInfo + Offset::oSpellInfoEndPos + 8, &ez)) {
                                            spellEndPos[0] = ex; spellEndPos[1] = ey; spellEndPos[2] = ez;
                                        }
                                        __try {
                                            spellSrcIdx = *(int*)(spellInfo + Offset::oSpellInfoSrcIndex);
                                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                                            spellSrcIdx = 0;
                                        }
                                        fprintf(f, " ✅ USING AS REFERENCE!");
                                    }
                                    fprintf(f, "\n");
                                    
                                    if (spellInfoCount < 200) {
                                        spellInfoCandidates[spellInfoCount].offset = off;
                                        spellInfoCandidates[spellInfoCount].ptr = spellInfo;
                                        spellInfoCount++;
                                    }
                                }
                            } __except(EXCEPTION_EXECUTE_HANDLER) {}
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                if (spellInfoCount > 0) {
                    fprintf(f, "  ✅ Found %d SpellInfo candidates (indirect)\n", spellInfoCount);
                }
            } else {
                fprintf(f, "  ✅ Found %d SpellInfo candidates\n", spellInfoCount);
            }
            
            // ====================================================================
            // PHASE 1.5: Tìm SpellData DIRECT access (nếu có)
            // ====================================================================
            fprintf(f, "\n--- PHASE 1.5: Tìm SpellData DIRECT access (trực tiếp từ Missile) ---\n");
            fprintf(f, "Scanning for direct SpellData pointers (0x0 - 0x800)...\n");
            fprintf(f, "SpellData structure: SpellData + 0x8 = Name pointer\n");
            
            int spellDataDirectCount = 0;
            for (uint64_t off = 0x0; off <= 0x800; off += 0x8) {
                __try {
                    uint64_t ptr = *(uint64_t*)(missile + off);
                    if (ptr < 0x100000 || ptr > 0x7FFFFFFFFFFF) continue;
                    
                    // Check if this pointer leads directly to SpellData
                    // SpellData + 0x8 = Spell name pointer
                    uint64_t namePtr = 0;
                    __try {
                        namePtr = *(uint64_t*)(ptr + Offset::oSpellDataName);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        continue;
                    }
                    if (namePtr < 0x10000 || namePtr > 0x7FFFFFFFFFFF) continue;
                    
                    char testName[64] = {0};
                    bool validName = false;
                    for (int c = 0; c < 63; c++) {
                        char ch = 0;
                        __try {
                            ch = *(char*)(namePtr + c);
                        } __except(EXCEPTION_EXECUTE_HANDLER) {
                            break;
                        }
                        if (ch == 0 && c > 3) { validName = true; break; }
                        if (ch < 32 || ch > 126) break;
                        testName[c] = ch;
                    }
                    
                    if (validName && testName[0]) {
                        spellDataDirectCount++;
                        fprintf(f, "  [0x%llX] -> SpellData* = 0x%llX -> Name = \"%s\"", off, ptr, testName);
                        
                        // Verify: Compare với reference SpellInfo's SpellData
                        if (referenceSpellInfo) {
                            uint64_t refSpellData = 0;
                            __try {
                                refSpellData = *(uint64_t*)(referenceSpellInfo + Offset::oSpellInfoSpellData);
                            } __except(EXCEPTION_EXECUTE_HANDLER) {
                                refSpellData = 0;
                            }
                            if (refSpellData && ptr == refSpellData) {
                                fprintf(f, " ✅ MATCHES REFERENCE SpellData!");
                            } else if (spellName[0] && strcmp(testName, spellName) == 0) {
                                fprintf(f, " ✅ MATCHES REFERENCE NAME!");
                            }
                        }
                        fprintf(f, "\n");
                        
                        if (spellDataDirectCount >= 20) break; // Limit output
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            if (spellDataDirectCount == 0) {
                fprintf(f, "  ❌ No direct SpellData access found\n");
                fprintf(f, "  💡 SpellData phải được truy cập qua SpellInfo (SpellInfo + 0x18)\n");
            } else {
                fprintf(f, "  ✅ Found %d direct SpellData candidates\n", spellDataDirectCount);
            }
            
            // ====================================================================
            // PHASE 2: Tìm Position (Current position - thay đổi khi missile di chuyển)
            // ====================================================================
            fprintf(f, "\n--- PHASE 2: Tìm Position (Current Pos) ---\n");
            fprintf(f, "Scanning Vec3 positions (0x0 - 0x800)...\n");
            fprintf(f, "Filtering: X/Z > 100, Y reasonable, not (0,0,0) or (1,1,1)...\n");
            fprintf(f, "Filtering: Valid map coordinates only\n");
            
            // Use C-style array instead of std::vector
            struct PositionCandidate { uint64_t offset; float pos[3]; };
            PositionCandidate positionCandidates[500];
            int positionCount = 0;
            for (uint64_t off = 0x0; off <= 0x800; off += 0x4) {
                __try {
                    float x, y, z;
                    if (!ScanSafeReadFloat(missile + off, &x) ||
                        !ScanSafeReadFloat(missile + off + 4, &y) ||
                        !ScanSafeReadFloat(missile + off + 8, &z)) continue;
                    
                    // Filter out invalid positions
                    // Loại bỏ (0,0,0), (1,1,1), và các giá trị không hợp lệ
                    if (x == 0.0f && y == 0.0f && z == 0.0f) continue;
                    if (x == 1.0f && y == 1.0f && z == 1.0f) continue;
                    if (x < 0 || x > 30000) continue;
                    if (z < 0 || z > 30000) continue;
                    if (y < -2000 || y > 3000) continue;
                    
                    // Chỉ lấy positions hợp lệ (trong map bounds)
                    if (x > 100 && x < 20000 && z > 100 && z < 20000) {
                        // Loại bỏ filter distXZ - Position có thể có X và Z gần nhau
                        // (Missile có thể di chuyển theo đường thẳng hoặc ở gần góc map)
                        
                        if (positionCount < 500) {
                            positionCandidates[positionCount].offset = off;
                            positionCandidates[positionCount].pos[0] = x;
                            positionCandidates[positionCount].pos[1] = y;
                            positionCandidates[positionCount].pos[2] = z;
                            positionCount++;
                        }
                        
                        // Check if matches SpellInfo StartPos or EndPos
                        bool matchStart = (spellStartPos[0] > 0 && 
                            fabsf(x - spellStartPos[0]) < 200 && 
                            fabsf(z - spellStartPos[2]) < 200);
                        bool matchEnd = (spellEndPos[0] > 0 && 
                            fabsf(x - spellEndPos[0]) < 200 && 
                            fabsf(z - spellEndPos[2]) < 200);
                        
                        // Hiển thị tất cả positions hợp lệ
                        fprintf(f, "  [0x%llX]: (%.1f, %.1f, %.1f)", off, x, y, z);
                        if (matchStart) fprintf(f, " ✅ MATCHES StartPos!");
                        if (matchEnd) fprintf(f, " ✅ MATCHES EndPos!");
                        fprintf(f, "\n");
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            if (positionCount == 0) {
                fprintf(f, "  ❌ No valid positions found\n");
            } else {
                fprintf(f, "  ✅ Found %d position candidates\n", positionCount);
            }
            
            // ====================================================================
            // PHASE 2.5: DEEP SCAN - Tìm StartPos/EndPos & SpellInfo (Duyệt toàn bộ offsets)
            // ====================================================================
            fprintf(f, "\n--- PHASE 2.5: DEEP SCAN StartPos/EndPos ---\n");
            
            float missileSpellStartPos[3] = {0, 0, 0};
            float missileSpellEndPos[3] = {0, 0, 0};
            uint64_t missileSpellInfo = 0;
            char missileSpellName[64] = {0};
            uint64_t foundPatternOffset = 0;
            bool isIndirect = false;
            int startPosCount = 0; // Added declaration
            int endPosCount = 0;   // Added declaration

            fprintf(f, "Đang quét sâu để tìm SpellInfo pointer (Duyệt 0x0 - 0x800)...\n");

            // Duyệt từng offset 8-byte để tìm pointer
            for (uint64_t off1 = 0x0; off1 <= 0x800; off1 += 0x8) {
                __try {
                    uint64_t val1 = *(uint64_t*)(missile + off1);
                    if (val1 < 0x100000 || val1 > 0x7FFFFFFFFFFF) continue;

                    // Thử cả 2 trường hợp: Direct (off1 là SpellInfo) và Indirect (val1 là SpellCast dẫn tới SpellInfo)
                    uint64_t candidates[2] = { val1, 0 };
                    
                    // Kiểm tra indirect (thử các offset phổ biến trong structure trung gian)
                    uint64_t indirectOffsets[] = { 0xD8, 0x98, 0x10, 0x118, 0x128 };
                    
                    for (int i = 0; i < 6; i++) {
                        uint64_t testInfo = (i == 0) ? candidates[0] : 0;
                        uint64_t currentOff2 = 0;
                        
                        if (i > 0) {
                            currentOff2 = indirectOffsets[i-1];
                            __try { testInfo = *(uint64_t*)(val1 + currentOff2); } __except(1) { testInfo = 0; }
                        }

                        if (testInfo < 0x100000 || testInfo > 0x7FFFFFFFFFFF) continue;

                        // Verify SpellInfo structure
                        uint64_t spellData = 0;
                        // Manual SafeReadUInt64 check
                        __try { spellData = *(uint64_t*)(testInfo + Offset::oSpellInfoSpellData); } __except(1) { spellData = 0; }
                        
                        if (spellData > 0x100000) {
                            char testName[64] = {0};
                            uint64_t namePtr = 0;
                            __try { namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName); } __except(1) { namePtr = 0; }
                            
                            if (namePtr > 0x10000) {
                                // Manual ReadChars
                                bool validString = false;
                                for(int c=0; c<63; c++) {
                                     char ch = 0;
                                     __try { ch = *(char*)(namePtr + c); } __except(1) { break; }
                                     if(ch == 0) { validString = true; break; }
                                     if(ch < 32 || ch > 126) { missileSpellName[0] = 0; break; }
                                     testName[c] = ch;
                                }
                                
                                if (validString && testName[0] != '\0' && strlen(testName) > 2) {
                                    missileSpellInfo = testInfo;
                                    strncpy(missileSpellName, testName, 63);
                                    foundPatternOffset = off1;
                                    isIndirect = (i > 0);
                                    
                                    // Lấy tọa độ mẫu từ SpellInfo
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoStartPos, &missileSpellStartPos[0]);
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoStartPos + 4, &missileSpellStartPos[1]);
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoStartPos + 8, &missileSpellStartPos[2]);
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoEndPos, &missileSpellEndPos[0]);
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoEndPos + 4, &missileSpellEndPos[1]);
                                    ScanSafeReadFloat(missileSpellInfo + Offset::oSpellInfoEndPos + 8, &missileSpellEndPos[2]);

                                    fprintf(f, "  🎯 FOUND: Pattern [0x%llX]%s dẫn tới Spell: \"%s\"\n", 
                                        off1, isIndirect ? "->Indirect" : "(Direct)", missileSpellName);
                                    if (isIndirect) fprintf(f, "     Lớp trung gian: 0x%llX, Offset trong: 0x%llX\n", val1, indirectOffsets[i-1]);
                                    fprintf(f, "     SpellInfo Ptr: 0x%llX\n", missileSpellInfo);
                                    fprintf(f, "     Mẫu StartPos: (%.1f, %.1f, %.1f)\n", missileSpellStartPos[0], missileSpellStartPos[1], missileSpellStartPos[2]);
                                    fprintf(f, "     Mẫu EndPos:   (%.1f, %.1f, %.1f)\n", missileSpellEndPos[0], missileSpellEndPos[1], missileSpellEndPos[2]);
                                    goto found_spell; 
                                }
                            }
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }

            found_spell:
            if (!missileSpellInfo) {
                fprintf(f, "  ❌ Không tìm thấy SpellInfo qua Deep Scan.\n");
            }
            // End of SCAN logic replacment

            // STEP 2: Tìm tọa độ TRỰC TIẾP trong Missile structure
            fprintf(f, "\nĐang tìm Start/End Pos trực tiếp trong Missile (Duyệt 0x0 - 0x800)...\n");
            
            if (missileSpellStartPos[0] < 10 && spellStartPos[0] > 10) {
                memcpy(missileSpellStartPos, spellStartPos, sizeof(float)*3);
                fprintf(f, "  💡 Sử dụng StartPos từ local cast làm reference.\n");
            }
            if (missileSpellEndPos[0] < 10 && spellEndPos[0] > 10) {
                memcpy(missileSpellEndPos, spellEndPos, sizeof(float)*3);
                fprintf(f, "  💡 Sử dụng EndPos từ local cast làm reference.\n");
            }

            for (uint64_t off = 0x0; off <= 0x800; off += 0x4) {
                __try {
                    float x, y, z;
                    // Inline SafeReadFloat replacement
                    x = *(float*)(missile + off);
                    y = *(float*)(missile + off + 4);
                    z = *(float*)(missile + off + 8);
                    
                    if (x < 100 || x > 15000) continue; // Out of map

                    // So khớp với StartPos mẫu
                    if (missileSpellStartPos[0] > 10) {
                        float dist = sqrtf(powf(x - missileSpellStartPos[0], 2) + powf(z - missileSpellStartPos[2], 2));
                        if (dist < 5.0f) {
                            fprintf(f, "  [0x%llX] 🌟 Cực giống StartPos: (%.1f, %.1f, %.1f)\n", off, x, y, z);
                        startPosCount++;
                        } else if (dist < 50.0f) {
                            fprintf(f, "  [0x%llX] ⚠️  Gần StartPos (dist=%.1f): (%.1f, %.1f, %.1f)\n", off, dist, x, y, z);
                        }
                    }

                    // So khớp với EndPos mẫu
                    if (missileSpellEndPos[0] > 10) {
                        float dist = sqrtf(powf(x - missileSpellEndPos[0], 2) + powf(z - missileSpellEndPos[2], 2));
                        if (dist < 5.0f) {
                            fprintf(f, "  [0x%llX] 🌟 Cực giống EndPos: (%.1f, %.1f, %.1f)\n", off, x, y, z);
                        endPosCount++;
                        } else if (dist < 50.0f) {
                            fprintf(f, "  [0x%llX] ⚠️  Gần EndPos (dist=%.1f): (%.1f, %.1f, %.1f)\n", off, dist, x, y, z);
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // STEP 3: Nếu không tìm thấy match, hiển thị ALL valid Vec3 positions
            fprintf(f, "\n");
            if (startPosCount == 0 && endPosCount == 0) {
                fprintf(f, "  ⚠️  No StartPos/EndPos exact matches found.\n");
                fprintf(f, "\n  Listing ALL valid Vec3 positions in Missile structure:\n");
                
                int validPosCount = 0;
                for (uint64_t off = 0x0; off <= 0x800; off += 0x4) {
                    __try {
                        float x, y, z;
                        // Inline SafeReadFloat replacement
                        x = *(float*)(missile + off);
                        y = *(float*)(missile + off + 4);
                        z = *(float*)(missile + off + 8);
                        
                        // Filter
                        if (x == 0.0f && y == 0.0f && z == 0.0f) continue;
                        if (x < 100 || x > 20000 || z < 100 || z > 20000) continue;
                        if (y < -2000 || y > 3000) continue;
                        
                        fprintf(f, "    [0x%llX]: (%.1f, %.1f, %.1f)\n", off, x, y, z);
                        validPosCount++;
                        if (validPosCount >= 30) {
                            fprintf(f, "    ... (truncated, showing first 30)\n");
                            break;
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                fprintf(f, "\n  💡 TIP: Cast a skillshot (Jinx W, Ezreal Q, Lux Q) và scan ngay khi missile vừa tạo!\n");
                fprintf(f, "  💡 So sánh các positions với SpellInfo StartPos/EndPos để xác định offset.\n");
            } else {
                fprintf(f, "  ✅ Found %d StartPos, %d EndPos matches\n", startPosCount, endPosCount);
            }
            
            // ====================================================================
            // PHASE 3: Tìm SrcIdx (Source NetID - caster NetID)
            // ====================================================================
            fprintf(f, "\n--- PHASE 3: Tìm SrcIdx (Source NetID) ---\n");
            fprintf(f, "Looking for NetID = 0x%X (player) or SpellInfo SrcIdx = 0x%X...\n", playerNetId, spellSrcIdx);
            fprintf(f, "Scanning all NetIDs (0x0 - 0x800)...\n");
            
            // Use C-style array instead of std::vector
            struct SrcIdxCandidate { uint64_t offset; int netId; };
            SrcIdxCandidate srcIdxCandidates[200];
            int srcIdxCount = 0;
            for (uint64_t off = 0x0; off <= 0x800; off += 0x4) {
                __try {
                    int netId = *(int*)(missile + off);
                    // Valid NetID range: 0x40000000 - 0x7FFFFFFF (player/enemy NetIDs)
                    // Cũng check các NetID khác có thể hợp lệ
                    if (netId >= 0x40000000 && netId <= 0x7FFFFFFF) {
                        if (srcIdxCount < 200) {
                            srcIdxCandidates[srcIdxCount].offset = off;
                            srcIdxCandidates[srcIdxCount].netId = netId;
                            srcIdxCount++;
                        }
                        
                        bool isPlayer = (netId == playerNetId);
                        bool matchesSpellSrcIdx = (netId == spellSrcIdx && spellSrcIdx > 0);
                        
                        // Hiển thị tất cả NetIDs hợp lệ nếu là player, matches SpellInfo, hoặc trong 100 đầu tiên
                        if (isPlayer || matchesSpellSrcIdx || srcIdxCount <= 100) {
                            fprintf(f, "  [0x%llX]: 0x%X", off, netId);
                            if (isPlayer) fprintf(f, " ✅ PLAYER NETID!");
                            if (matchesSpellSrcIdx) fprintf(f, " ✅ MATCHES SpellInfo SrcIdx!");
                            fprintf(f, "\n");
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Also scan với step 0x1 để tìm NetID có thể bị misaligned (chỉ scan một số offset quan trọng)
            fprintf(f, "\nAlso scanning misaligned offsets (0x1, 0x2, 0x3, 0x5, 0x6, 0x7, 0x9, 0xA, 0xB, 0xD, 0xE, 0xF)...\n");
            uint64_t misalignedOffsets[] = {0x1, 0x2, 0x3, 0x5, 0x6, 0x7, 0x9, 0xA, 0xB, 0xD, 0xE, 0xF};
            for (int i = 0; i < 12; i++) {
                for (uint64_t base = 0x0; base <= 0x800; base += 0x10) {
                    uint64_t off = base + misalignedOffsets[i];
                    if (off > 0x800) break;
                    
                    __try {
                        int netId = *(int*)(missile + off);
                        if (netId >= 0x40000000 && netId <= 0x7FFFFFFF) {
                            bool isPlayer = (netId == playerNetId);
                            bool matchesSpellSrcIdx = (netId == spellSrcIdx && spellSrcIdx > 0);
                            if (isPlayer || matchesSpellSrcIdx) {
                                fprintf(f, "  [0x%llX] (misaligned): 0x%X", off, netId);
                                if (isPlayer) fprintf(f, " ✅ PLAYER NETID!");
                                if (matchesSpellSrcIdx) fprintf(f, " ✅ MATCHES SpellInfo SrcIdx!");
                            fprintf(f, "\n");
                        }
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
            }
            
            if (srcIdxCount == 0) {
                fprintf(f, "  ❌ No valid NetIDs found\n");
            } else {
                fprintf(f, "  ✅ Found %d NetID candidates\n", srcIdxCount);
                if (srcIdxCount > 100) {
                    fprintf(f, "  ⚠️  (Showing first 100 + player/SpellInfo matches)\n");
                }
            }
            
            // ====================================================================
            // PHASE 4: Test Current Offsets (OLD vs NEW)
            // ====================================================================
            fprintf(f, "\n--- PHASE 4: Test Current Offsets ---\n");
            fprintf(f, "Testing both OLD (deprecated) and NEW (verified) offsets:\n\n");
            
            // Test oMissileSpellInfo OLD (0x1D8 - DEPRECATED)
            fprintf(f, "=== SpellInfo Access ===\n");
            __try {
                uint64_t spellInfo = *(uint64_t*)(missile + 0x1D8); // OLD offset
                if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                    uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                    if (spellData > 0x100000) {
                        uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                        if (namePtr > 0x10000) {
                            char name[64] = {0};
                            for (int c = 0; c < 63; c++) {
                                char ch = 0;
                                __try { ch = *(char*)(namePtr + c); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
                                if (ch == 0) break;
                                if (ch < 32 || ch > 126) { name[0] = 0; break; }
                                name[c] = ch;
                            }
                            if (name[0]) {
                                fprintf(f, "  [OLD] 0x1D8 (direct): ✅ VALID -> \"%s\"\n", name);
                            } else {
                                fprintf(f, "  [OLD] 0x1D8 (direct): ⚠️  Pointer valid but name invalid\n");
                            }
                        } else {
                            fprintf(f, "  [OLD] 0x1D8 (direct): ⚠️  Invalid SpellData name pointer\n");
                        }
                    } else {
                        fprintf(f, "  [OLD] 0x1D8 (direct): ⚠️  Invalid SpellData pointer\n");
                    }
                } else {
                    fprintf(f, "  [OLD] 0x1D8 (direct): ❌ Invalid pointer (0x%llX)\n", spellInfo);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [OLD] 0x1D8 (direct): ❌ CRASH\n");
            }
            
            // Test oMissileSpellInfo NEW (0x1F0 -> 0xD8 - VERIFIED)
                                __try {
                uint64_t spellCast = *(uint64_t*)(missile + 0x1F0); // NEW offset
                if (spellCast > 0x100000 && spellCast < 0x7FFFFFFFFFFF) {
                    uint64_t spellInfo = *(uint64_t*)(spellCast + 0xD8);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                        if (spellData > 0x100000) {
                            uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                            if (namePtr > 0x10000) {
                                char name[64] = {0};
                                for (int c = 0; c < 63; c++) {
                                    char ch = 0;
                                    __try { ch = *(char*)(namePtr + c); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
                                    if (ch == 0) break;
                                    if (ch < 32 || ch > 126) { name[0] = 0; break; }
                                    name[c] = ch;
                                }
                                if (name[0]) {
                                    fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ✅ VALID -> \"%s\" ✅ USE THIS!\n", name);
                                } else {
                                    fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ⚠️  Name invalid\n");
                                }
                            } else {
                                fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ⚠️  Invalid name ptr\n");
                            }
                        } else {
                            fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ⚠️  Invalid SpellData ptr\n");
                        }
                    } else {
                        fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ⚠️  Invalid SpellInfo ptr\n");
                    }
                } else {
                    fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ❌ Invalid SpellCast ptr (0x%llX)\n", spellCast);
                }
                                } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [NEW] 0x1F0->0xD8 (indirect): ❌ CRASH\n");
            }
            
            // Test pattern 0x338 -> 0x98 (found to work for SmolderW)
            __try {
                uint64_t ptr1 = *(uint64_t*)(missile + 0x338);
                if (ptr1 > 0x100000 && ptr1 < 0x7FFFFFFFFFFF) {
                    uint64_t spellInfo = *(uint64_t*)(ptr1 + 0x98);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                        if (spellData > 0x100000) {
                            uint64_t namePtr = *(uint64_t*)(spellData + Offset::oSpellDataName);
                            if (namePtr > 0x10000) {
                                char name[64] = {0};
                                for (int c = 0; c < 63; c++) {
                                    char ch = 0;
                                    __try { ch = *(char*)(namePtr + c); } __except(EXCEPTION_EXECUTE_HANDLER) { break; }
                                if (ch == 0) break;
                                if (ch < 32 || ch > 126) { name[0] = 0; break; }
                                name[c] = ch;
                            }
                            if (name[0]) {
                                    fprintf(f, "  [NEW] 0x338->0x98 (indirect): ✅ VALID -> \"%s\" ✅ ALTERNATIVE!\n", name);
                                } else {
                                    fprintf(f, "  [NEW] 0x338->0x98 (indirect): ⚠️  Name invalid\n");
                                }
                            } else {
                                fprintf(f, "  [NEW] 0x338->0x98 (indirect): ⚠️  Invalid name ptr\n");
                            }
                        } else {
                            fprintf(f, "  [NEW] 0x338->0x98 (indirect): ⚠️  Invalid SpellData ptr\n");
                        }
                    } else {
                        fprintf(f, "  [NEW] 0x338->0x98 (indirect): ⚠️  Invalid SpellInfo ptr\n");
                    }
                } else {
                    fprintf(f, "  [NEW] 0x338->0x98 (indirect): ❌ Invalid ptr1 (0x%llX)\n", ptr1);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [NEW] 0x338->0x98 (indirect): ❌ CRASH\n");
            }
            
            // Test oMissilePosition OLD (0x180 - DEPRECATED) vs NEW (0x5A0 - VERIFIED)
            fprintf(f, "\n=== Position ===\n");
            __try {
                float x, y, z;
                if (ScanSafeReadFloat(missile + 0x180, &x) &&
                    ScanSafeReadFloat(missile + 0x180 + 4, &y) &&
                    ScanSafeReadFloat(missile + 0x180 + 8, &z)) {
                    if (x > 100 && x < 20000 && z > 100 && z < 20000) {
                        fprintf(f, "  [OLD] 0x180: ✅ (%.1f, %.1f, %.1f)\n", x, y, z);
                    } else {
                        fprintf(f, "  [OLD] 0x180: ❌ Invalid coords (%.1f, %.1f, %.1f)\n", x, y, z);
                    }
                    } else {
                    fprintf(f, "  [OLD] 0x180: ❌ Cannot read\n");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [OLD] 0x180: ❌ CRASH\n");
                    }
            
            __try {
                float x, y, z;
                if (ScanSafeReadFloat(missile + 0x5A0, &x) &&
                    ScanSafeReadFloat(missile + 0x5A0 + 4, &y) &&
                    ScanSafeReadFloat(missile + 0x5A0 + 8, &z)) {
                    if (x > 100 && x < 20000 && z > 100 && z < 20000) {
                        fprintf(f, "  [NEW] 0x5A0: ✅ (%.1f, %.1f, %.1f) ✅ USE THIS!\n", x, y, z);
                } else {
                        fprintf(f, "  [NEW] 0x5A0: ⚠️  Out of bounds (%.1f, %.1f, %.1f)\n", x, y, z);
                    }
                } else {
                    fprintf(f, "  [NEW] 0x5A0: ❌ Cannot read\n");
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  [NEW] 0x5A0: ❌ CRASH\n");
            }
            
            // Test oMissileSrcIdx (0x1D0) - VERIFIED
            fprintf(f, "\n=== Source/Target NetID ===\n");
            __try {
                int srcIdx = *(int*)(missile + Offset::oMissileSrcIdx);
                if (srcIdx >= 0x40000000 && srcIdx <= 0x7FFFFFFF) {
                    fprintf(f, "  oMissileSrcIdx (0x1D0): ✅ VALID 0x%X", srcIdx);
                    if (srcIdx == playerNetId) {
                        fprintf(f, " ✅ PLAYER!");
                    }
                    fprintf(f, "\n");
                } else {
                    fprintf(f, "  oMissileSrcIdx (0x1D0): ⚠️  Invalid NetID (0x%X)\n", srcIdx);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  oMissileSrcIdx (0x1D0): ❌ CRASH\n");
            }
            
            // Test oMissileNetId (0x20) - VERIFIED
            __try {
                int netId = *(int*)(missile + Offset::oMissileNetId);
                if (netId >= 0x40000000 && netId <= 0x7FFFFFFF) {
                    fprintf(f, "  oMissileNetId (0x20): ✅ VALID 0x%X\n", netId);
                } else {
                    fprintf(f, "  oMissileNetId (0x20): ⚠️  Invalid NetID (0x%X)\n", netId);
                }
            } __except(EXCEPTION_EXECUTE_HANDLER) {
                fprintf(f, "  oMissileNetId (0x20): ❌ CRASH\n");
            }
            
            fprintf(f, "\n");
    }
    
    fprintf(f, "========================================\n");
    fprintf(f, "SCAN COMPLETE\n");
    fprintf(f, "========================================\n");
    fprintf(f, "\nINSTRUCTIONS:\n");
    fprintf(f, "1. Cast a skillshot (Jinx W, Ezreal Q, etc.)\n");
    fprintf(f, "2. Scan ngay khi missile vừa created\n");
    fprintf(f, "3. So sánh SpellInfo pointer với SpellInfo từ spell cast\n");
    fprintf(f, "4. So sánh Position với StartPos/EndPos từ SpellInfo\n");
    fprintf(f, "5. Verify SrcIdx match với caster NetID\n");
    fprintf(f, "========================================\n");
    
    fclose(f);
}

// ============================================================================
// MISSILE DECRYPTION SCAN - Scan với chiến lược giải mã (theo chienluocmissile)
// ============================================================================
// NOTE: Missile structure có thể bị obfuscated ở offset 0x108
// Hàm giải mã: sub_296390, sub_521940, sub_17F5D70
// Để scan chính xác, cần gọi hàm giải mã trước khi đọc offsets
// Tuy nhiên, một số offsets có thể được đọc trực tiếp (không bị obfuscated)
void ScanMissileOffsetsWithDecryption() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    if (!moduleBase) return;
    
    // Use SafeRead functions instead of __try/__except to avoid C2712 with C++ objects
    uint64_t localPlayer = 0;
    if (!ScanSafeReadQWORD(moduleBase + Offset::oLocalPlayer, &localPlayer) || !localPlayer) {
        return;
    }
    
    int playerNetId = 0;
    if (!ScanSafeReadInt(localPlayer + Offset::oObjNetId, &playerNetId)) {
        return;
    }
    
    float gameTime = 0.0f;
    if (!ScanSafeReadFloat(moduleBase + Offset::oGametime, &gameTime)) {
        return;
    }
    
    uint64_t missileMgr = 0;
    if (!ScanSafeReadQWORD(moduleBase + Offset::oMissileList, &missileMgr) || !missileMgr) {
        return;
    }
    
    FILE* f = fopen("missile_decrypt_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "========================================\n");
    fprintf(f, "MISSILE DECRYPTION SCAN - Chiến lược giải mã (FULL SCAN)\n");
    fprintf(f, "========================================\n");
    fprintf(f, "LocalPlayer: 0x%llX\n", localPlayer);
    fprintf(f, "PlayerNetID: 0x%X\n", playerNetId);
    fprintf(f, "GameTime: %.2f\n", gameTime);
    fprintf(f, "MissileManager: 0x%llX\n", missileMgr);
    fprintf(f, "\n");
    fprintf(f, "DECRYPTION INFO:\n");
    fprintf(f, "- Hàm giải mã: sub_296390 (XOR/NOT), sub_521940 (wrapper), sub_17F4EE0 (search)\n");
    fprintf(f, "- Obfuscation structure: missilePtr + 0x108 (a4 + 264 trong sub_296390)\n");
    fprintf(f, "- Size field: 0x10E (v4[6] trong sub_296390)\n");
    fprintf(f, "- Data index: 0x110 (v4[8] trong sub_296390)\n");
    fprintf(f, "\n💡 NOTE: Một số offsets có thể KHÔNG bị obfuscated (có thể đọc trực tiếp)\n");
    fprintf(f, "💡 Scanner sẽ thử đọc trực tiếp, nếu không được mới cần giải mã\n");
    fprintf(f, "========================================\n\n");
    
    // Shared reference variables for coordinate verification
    float spellStartPos[3] = { 0, 0, 0 };
    float spellEndPos[3] = { 0, 0, 0 };
    int spellSrcIdx = 0;
    int spellTargetIdx = 0;
    char spellName[64] = { 0 };
    uint64_t referenceSpellInfo = 0;
    uint64_t arrayPtr = 0;
    int size = 0;

    uint64_t spellBook = 0;
    // Try pointer approach (main.cpp)
    ScanSafeReadQWORD(localPlayer + Offset::oObjSpellBook, &spellBook);
    
    // Try inline approach if pointer failed or is small
    if (spellBook < 0x10000) {
        spellBook = localPlayer + Offset::oObjSpellBook;
    }

    // PHASE 0: Get Global Reference from Spell Slots (optional but helpful)
    auto FindReferenceInSpellBook = [&](uint64_t book) -> bool {
        if (!book) return false;
        int slotOrder[4] = { 1, 0, 2, 3 }; // W, Q, E, R
        for (int s = 0; s < 4; s++) {
            uint64_t spellSlot = 0;
            // Try both pointer array and inline array
            if (ScanSafeReadQWORD(book + Offset::oObjSpellBookSpellSlot + slotOrder[s] * 0x8, &spellSlot) && spellSlot > 0x100000) {
                uint64_t spellInfo = 0;
                if (ScanSafeReadQWORD(spellSlot + Offset::oSpellSlotSpellInfo, &spellInfo) && spellInfo > 0x100000) {
                    char name[64] = { 0 };
                    MissileScanner::GetSpellNameC(spellInfo, name, 64);
                    if (name[0]) {
                        referenceSpellInfo = spellInfo;
                        strncpy_s(spellName, name, _TRUNCATE);
                        ScanSafeReadVec3(spellInfo + Offset::oSpellInfoStartPos, &spellStartPos[0], &spellStartPos[1], &spellStartPos[2]);
                        ScanSafeReadVec3(spellInfo + Offset::oSpellInfoEndPos, &spellEndPos[0], &spellEndPos[1], &spellEndPos[2]);
                        ScanSafeReadInt(spellInfo + Offset::oSpellInfoSrcIndex, &spellSrcIdx);
                        ScanSafeReadInt(spellInfo + Offset::oSpellInfoTargetIndex, &spellTargetIdx);
                        return true;
                    }
                }
            }
        }
        return false;
    };

    if (!FindReferenceInSpellBook(spellBook)) {
        fprintf(f, "  🔍 Default SpellBook failed, performing exhaustive search in local player memory (0x3000 - 0x6000)...\n");
        // BRUTE FORCE: Search localPlayer memory for SpellBook/SpellSlot candidates
        for (uint64_t off = 0x3000; off <= 0x6000; off += 0x8) {
            uint64_t candidatePtr = 0;
            if (ScanSafeReadQWORD(localPlayer + off, &candidatePtr) && candidatePtr > 0x100000) {
                // If it's a pointer, try it as spellSlot or spellBook
                if (FindReferenceInSpellBook(candidatePtr)) {
                    fprintf(f, "  ⭐ Found valid SpellBook reference at offset 0x%llX (ptr=0x%llX)\n", off, candidatePtr);
                    break;
                }
            }
        }
    }
    
    fprintf(f, "Reference SpellInfo: Name=\"%s\", SrcIdx=0x%X, TargetIdx=0x%X, Ptr=0x%llX\n",
        spellName[0] ? spellName : "N/A", spellSrcIdx, spellTargetIdx, referenceSpellInfo);
    if (spellStartPos[0] > 0) {
        fprintf(f, "  StartPos: (%.1f, %.1f, %.1f)\n", spellStartPos[0], spellStartPos[1], spellStartPos[2]);
    }
    if (spellEndPos[0] > 0) {
        fprintf(f, "  EndPos: (%.1f, %.1f, %.1f)\n", spellEndPos[0], spellEndPos[1], spellEndPos[2]);
    }
    
    // PHASE 0.1: Get Player Position for fallback matching
    float playerPos[3] = {0, 0, 0};
    ScanSafeReadVec3(localPlayer + Offset::oObjPosition, &playerPos[0], &playerPos[1], &playerPos[2]);
    fprintf(f, "Current Player Position: (%.1f, %.1f, %.1f)\n", playerPos[0], playerPos[1], playerPos[2]);
    
    // PHASE 0.2: Verify SpellInfo Coordinate Offsets (if current ones yield 0)
    if (referenceSpellInfo && (spellStartPos[0] < 1.0f || spellEndPos[0] < 1.0f)) {
        fprintf(f, "  🔍 SpellInfo Start/End Pos invalid (0.0). Searching for better offsets near 0xA4...\n");
        for (uint64_t off = 0x80; off <= 0x150; off += 0x4) {
            float x, y, z;
            if (ScanSafeReadVec3(referenceSpellInfo + off, &x, &y, &z)) {
                if (x > 100 && x < 15000 && z > 100 && z < 15000) {
                    bool matchPlayer = (fabsf(x - playerPos[0]) < 200.0f && fabsf(z - playerPos[2]) < 200.0f);
                    if (matchPlayer) {
                        fprintf(f, "  ⭐ SpellInfo Candidate @ +0x%llX: (%.1f, %.1f, %.1f) [NEAR PLAYER]\n", off, x, y, z);
                        if (spellStartPos[0] < 1.0f) {
                            memcpy(spellStartPos, &x, sizeof(float)*3);
                        }
                    } else {
                        fprintf(f, "  📍 SpellInfo Candidate @ +0x%llX: (%.1f, %.1f, %.1f)\n", off, x, y, z);
                        if (spellEndPos[0] < 1.0f) {
                             memcpy(spellEndPos, &x, sizeof(float)*3);
                        }
                    }
                }
            }
        }
    }
    
    if (spellStartPos[0] < 1.0f && playerPos[0] > 1.0f) {
        fprintf(f, "💡 NOTE: SpellInfo StartPos is still 0, using Player Position as fallback reference!\n");
        memcpy(spellStartPos, playerPos, sizeof(float) * 3);
    }

    fprintf(f, "\n");
    
    // Get missile array using safe reads
    if (!ScanSafeReadQWORD(missileMgr + 0x08, &arrayPtr) || !ScanSafeReadInt(missileMgr + 0x10, &size)) {
        fprintf(f, "ERROR: Cannot read missile array\n");
        fclose(f);
        return;
    }
    
    if (!arrayPtr || size <= 0 || size > 100) {
        fprintf(f, "ERROR: Invalid missile array (ptr=0x%llX, size=%d)\n", arrayPtr, size);
        fclose(f);
        return;
    }
    
    fprintf(f, "Found %d missiles\n\n", size);
    
    // TRACK UNIQUE MISSILES TO AVOID DUPLICATES
    std::vector<uint64_t> uniqueMissiles;
    
    // Scan each missile
    for (int m = 0; m < size && uniqueMissiles.size() < 10; m++) {
        uint64_t missile = 0;
        if (!ScanSafeReadQWORD(arrayPtr + m * 0x8, &missile)) {
            continue;
        }
        
        if (!missile || missile < 0x10000) continue;
        
        // Skip duplicate pointers
        bool isDuplicate = false;
        for (uint64_t seen : uniqueMissiles) {
            if (seen == missile) {
                isDuplicate = true;
                break;
            }
        }
        if (isDuplicate) continue;
        uniqueMissiles.push_back(missile);
        
        fprintf(f, "=== MISSILE[%d] @ 0x%llX ===\n\n", (int)uniqueMissiles.size() - 1, missile);
        
        fprintf(f, "\n");
        
        // --- PHASE 2: Multi-Pattern Probe for this Missile ---
        fprintf(f, "--- PHASE 2: Probing SpellInfo Patterns ---\n");
        uint64_t currentMissileSpellInfo = 0;
        char currentMissileName[64] = {0};
        
        uint64_t patterns[][2] = { {0x158, 0x98}, {0x728, 0x98}, {0x1F0, 0xD8}, {0xC0, 0xE8} };
        const char* patNames[] = { "0x158->0x98", "0x728->0x98", "0x1F0->0xD8", "0xC0->0xE8" };
        
        for (int i = 0; i < 4; i++) {
            uint64_t ptr = 0;
            if (ScanSafeReadQWORD(missile + patterns[i][0], &ptr) && ptr > 0x100000) {
                uint64_t spellInfo = 0;
                if (ScanSafeReadQWORD(ptr + patterns[i][1], &spellInfo) && spellInfo > 0x100000) {
                    char nameBuf[64] = {0};
                    MissileScanner::GetSpellNameC(spellInfo, nameBuf, 64);
                    if (nameBuf[0]) {
                        fprintf(f, "  ✅ Pattern FOUND via %s: \"%s\" (SpellInfo @ 0x%llX)\n", patNames[i], nameBuf, spellInfo);
                        currentMissileSpellInfo = spellInfo;
                        strncpy_s(currentMissileName, nameBuf, _TRUNCATE);
                    }
                }
            }
        }

        // Try direct access from pattern (oMissileSpellInfo_Direct = 0x260)
        if (!currentMissileSpellInfo) {
            uint64_t directInfo = 0;
            if (ScanSafeReadQWORD(missile + Offset::oMissileSpellInfo_Direct, &directInfo) && directInfo > 0x100000) {
                char nameBuf[64] = {0};
                MissileScanner::GetSpellNameC(directInfo, nameBuf, 64);
                if (nameBuf[0]) {
                    fprintf(f, "  ✅ Direct access FOUND at 0x%X: \"%s\"\n", (uint32_t)Offset::oMissileSpellInfo_Direct, nameBuf);
                    currentMissileSpellInfo = directInfo;
                    strncpy_s(currentMissileName, nameBuf, _TRUNCATE);
                }
            }
        }

        // EXHAUSTIVE SCAN for SpellInfo (If patterns fail)
        if (!currentMissileSpellInfo) {
            fprintf(f, "  🔍 Patterns failed, performing exhaustive pointer search (0x0 - 0x1000)...\n");
            for (uint64_t off = 0x0; off <= 0x1000; off += 0x8) {
                uint64_t ptr = 0;
                if (ScanSafeReadQWORD(missile + off, &ptr) && ptr > 0x100000 && ptr < 0x7FFFFFFFFFFF) {
                    char nameBuf[64] = {0};
                    MissileScanner::GetSpellNameC(ptr, nameBuf, 64);
                    if (nameBuf[0]) {
                        fprintf(f, "  ✅ Exhaustive FIND @ +0x%llX: \"%s\" (SpellInfo @ 0x%llX)\n", off, nameBuf, ptr);
                        currentMissileSpellInfo = ptr;
                        strncpy_s(currentMissileName, nameBuf, _TRUNCATE);
                        break;
                    }
                    
                    // Try as inner ptr
                    for (uint64_t innerOff : {0x18, 0x98, 0xD8, 0xE8}) {
                        uint64_t subPtr = 0;
                        if (ScanSafeReadQWORD(ptr + innerOff, &subPtr) && subPtr > 0x100000 && subPtr < 0x7FFFFFFFFFFF) {
                            MissileScanner::GetSpellNameC(subPtr, nameBuf, 64);
                            if (nameBuf[0]) {
                                fprintf(f, "  ✅ Exhaustive FIND @ +0x%llX -> +0x%llX: \"%s\"\n", off, innerOff, nameBuf);
                                currentMissileSpellInfo = subPtr;
                                strncpy_s(currentMissileName, nameBuf, _TRUNCATE);
                                break;
                            }
                        }
                    }
                    if (currentMissileSpellInfo) break;
                }
            }
        }

        if (currentMissileSpellInfo && !referenceSpellInfo) {
            // Update global reference if we found one
            referenceSpellInfo = currentMissileSpellInfo;
            strncpy_s(spellName, currentMissileName, _TRUNCATE);
            ScanSafeReadVec3(currentMissileSpellInfo + Offset::oSpellInfoStartPos, &spellStartPos[0], &spellStartPos[1], &spellStartPos[2]);
            ScanSafeReadVec3(currentMissileSpellInfo + Offset::oSpellInfoEndPos, &spellEndPos[0], &spellEndPos[1], &spellEndPos[2]);
            ScanSafeReadInt(currentMissileSpellInfo + Offset::oSpellInfoSrcIndex, &spellSrcIdx);
            ScanSafeReadInt(currentMissileSpellInfo + Offset::oSpellInfoTargetIndex, &spellTargetIdx);
        }
        
        if (currentMissileName[0]) {
            const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(currentMissileName);
            if (dbInfo) {
                fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f\n", dbInfo->speed, dbInfo->radius, dbInfo->width);
            }
        }
        fprintf(f, "\n");

        // --- PHASE 3: Brute-Force Coordinate Matching ---
        if (referenceSpellInfo) {
            fprintf(f, "--- PHASE 3: Brute-Force Coordinate Matching (Searching for Start/End Pos) ---\n");
            fprintf(f, "Searching missile object (0x0 - 0x1000) for Vec3 matching SpellInfo values...\n");
            fprintf(f, "Target Reference: Start(%.1f, %.1f, %.1f), End(%.1f, %.1f, %.1f)\n", 
                spellStartPos[0], spellStartPos[1], spellStartPos[2],
                spellEndPos[0], spellEndPos[1], spellEndPos[2]);

            for (uint64_t off = 0x0; off <= 0x1000; off += 0x4) {
                float x, y, z;
                if (ScanSafeReadVec3(missile + off, &x, &y, &z)) {
                    // Coordinates often have Y around 50-200. X and Z are in map range.
                    if (x < -2000 || x > 20000 || z < -2000 || z > 20000) continue;

                    // Calculate distance to reference points
                    float distStart = sqrtf(powf(x - spellStartPos[0], 2) + powf(z - spellStartPos[2], 2));
                    float distEnd = (spellEndPos[0] > 10.0f) ? sqrtf(powf(x - spellEndPos[0], 2) + powf(z - spellEndPos[2], 2)) : 99999.0f;

                    if (distStart < 20.0f) {
                        fprintf(f, "  [0x%llX]: ⭐ MATCHES StartPos/PlayerPos! (%.1f, %.1f, %.1f) dist=%.1f\n", off, x, y, z, distStart);
                    }
                    if (distEnd < 20.0f) {
                        fprintf(f, "  [0x%llX]: ⭐ MATCHES EndPos! (%.1f, %.1f, %.1f) dist=%.1f\n", off, x, y, z, distEnd);
                    }
                    
                    // Even if not an exact match, log it if it looks like a valid coordinate near the player
                    if (distStart < 200.0f && distStart > 20.0f) {
                        fprintf(f, "  [0x%llX]: 📍 Near Player/Start (%.1f, %.1f, %.1f) dist=%.1f\n", off, x, y, z, distStart);
                    }
                }
            }
            fprintf(f, "\n");
        }

        // --- PHASE 4: NetID/Index Matching (Searching for SrcIdx/TargetIdx) ---
        fprintf(f, "--- PHASE 4: NetID/Index Matching ---\n");
        fprintf(f, "Search Targets: Player=0x%X, SpellInfoSrc=0x%X, SpellInfoTarget=0x%X\n", 
            playerNetId, spellSrcIdx, spellTargetIdx);

        for (uint64_t off = 0x0; off <= 0x800; off += 0x4) {
            int val = 0;
            if (ScanSafeReadInt(missile + off, &val)) {
                if (val >= 0x40000000 && val <= 0x7FFFFFFF) {
                    bool match = false;
                    char matchName[128] = {0};
                    if (val == playerNetId) { strcat_s(matchName, "⭐PLAYER "); match = true; }
                    if (spellSrcIdx > 0 && val == spellSrcIdx) { strcat_s(matchName, "✅SrcIdx "); match = true; }
                    if (spellTargetIdx > 0 && val == spellTargetIdx) { strcat_s(matchName, "🎯TargetIdx "); match = true; }
                    
                    // Check for XORed/Tagged NetIDs (0x68... range seen in logs)
                    if (val >= 0x68000000 && val <= 0x69FFFFFF) {
                        strcat_s(matchName, "🆔 [TAGGED/XORED] ");
                        match = true;
                    }

                    if (match) {
                        fprintf(f, "  [0x%llX]: 🆔 NetID %s (0x%X)\n", off, matchName, val);
                    }
                }
            }
        }
        fprintf(f, "\n");

        // --- PHASE 1: Known Offsets (Comparison & Validation) ---
        fprintf(f, "--- PHASE 1: Known Offsets (Comparison & Validation) ---\n");
        fprintf(f, "💡 Comparing VERIFIED offsets vs PATTERN offsets to find which ones work\n");
        int netId = 0;
        if (ScanSafeReadInt(missile + Offset::oMissileNetId, &netId)) {
            if (netId >= 0x40000000 && netId <= 0x7FFFFFFF) {
                fprintf(f, "  oMissileNetId (0x20): ✅ 0x%X", netId);
                if (netId == playerNetId) fprintf(f, " (PLAYER)");
                fprintf(f, "\n");
            } else {
                fprintf(f, "  oMissileNetId (0x20): ⚠️  0x%X (invalid)\n", netId);
            }
        } else {
            fprintf(f, "  oMissileNetId (0x20): ❌ CRASH\n");
        }
        
        // Position - Try both old offset and pattern offset
        float x, y, z;
        bool posFound = false;
        if (ScanSafeReadFloat(missile + Offset::oMissilePosition, &x) &&
            ScanSafeReadFloat(missile + Offset::oMissilePosition + 4, &y) &&
            ScanSafeReadFloat(missile + Offset::oMissilePosition + 8, &z)) {
            if (x > 100 && x < 20000 && z > 100 && z < 20000) {
                fprintf(f, "  oMissilePosition (0x5A0): ✅ (%.1f, %.1f, %.1f)\n", x, y, z);
                posFound = true;
            }
        }
        
        // Try pattern CurPos offset
        if (!posFound && ScanSafeReadFloat(missile + Offset::oMissileCurPos, &x) &&
            ScanSafeReadFloat(missile + Offset::oMissileCurPos + 4, &y) &&
            ScanSafeReadFloat(missile + Offset::oMissileCurPos + 8, &z)) {
            if (x > 100 && x < 20000 && z > 100 && z < 20000) {
                fprintf(f, "  oMissileCurPos (0x1DC from pattern): ✅ (%.1f, %.1f, %.1f)\n", x, y, z);
                posFound = true;
            }
        }
        
        if (!posFound) {
            fprintf(f, "  oMissilePosition (0x5A0): ⚠️  Invalid\n");
            fprintf(f, "  oMissileCurPos (0x1DC): ⚠️  Invalid\n");
            // Try alternative position offsets
            fprintf(f, "    💡 Trying alternative position offsets...\n");
            for (uint64_t altOff : {0x450, 0x180, 0x120, 0x150, 0x1B0, 0x240, 0x330, 0x360, 0x2D0}) {
                float ax, ay, az;
                if (ScanSafeReadFloat(missile + altOff, &ax) &&
                    ScanSafeReadFloat(missile + altOff + 4, &ay) &&
                    ScanSafeReadFloat(missile + altOff + 8, &az)) {
                    if (ax > 100 && ax < 20000 && az > 100 && az < 20000) {
                        fprintf(f, "    ✅ Alternative position at 0x%llX: (%.1f, %.1f, %.1f)\n", altOff, ax, ay, az);
                    }
                }
            }
        }
        
        // StartPos (from pattern)
        float sx, sy, sz;
        if (ScanSafeReadFloat(missile + Offset::oMissileStartPos, &sx) &&
            ScanSafeReadFloat(missile + Offset::oMissileStartPos + 4, &sy) &&
            ScanSafeReadFloat(missile + Offset::oMissileStartPos + 8, &sz)) {
            if (sx > 100 && sx < 20000 && sz > 100 && sz < 20000) {
                fprintf(f, "  oMissileStartPos (0x2E0 from pattern): ✅ (%.1f, %.1f, %.1f)", sx, sy, sz);
                if (spellStartPos[0] > 0) {
                    float dist = sqrtf((sx-spellStartPos[0])*(sx-spellStartPos[0]) + (sy-spellStartPos[1])*(sy-spellStartPos[1]) + (sz-spellStartPos[2])*(sz-spellStartPos[2]));
                    if (dist < 5.0f) {
                        fprintf(f, " ✅ MATCHES SpellInfo StartPos (dist=%.2f)", dist);
                    }
                }
                fprintf(f, "\n");
            } else {
                fprintf(f, "  oMissileStartPos (0x2E0): ⚠️  (%.1f, %.1f, %.1f) (out of bounds)\n", sx, sy, sz);
            }
        } else {
            fprintf(f, "  oMissileStartPos (0x2E0): ❌ CRASH\n");
        }
        
        // EndPos (from pattern)
        float ex, ey, ez;
        if (ScanSafeReadFloat(missile + Offset::oMissileEndPos, &ex) &&
            ScanSafeReadFloat(missile + Offset::oMissileEndPos + 4, &ey) &&
            ScanSafeReadFloat(missile + Offset::oMissileEndPos + 8, &ez)) {
            if (ex > 100 && ex < 20000 && ez > 100 && ez < 20000) {
                fprintf(f, "  oMissileEndPos (0x2EC from pattern): ✅ (%.1f, %.1f, %.1f)", ex, ey, ez);
                if (spellEndPos[0] > 0) {
                    float dist = sqrtf((ex-spellEndPos[0])*(ex-spellEndPos[0]) + (ey-spellEndPos[1])*(ey-spellEndPos[1]) + (ez-spellEndPos[2])*(ez-spellEndPos[2]));
                    if (dist < 5.0f) {
                        fprintf(f, " ✅ MATCHES SpellInfo EndPos (dist=%.2f)", dist);
                    }
                }
                fprintf(f, "\n");
            } else {
                fprintf(f, "  oMissileEndPos (0x2EC): ⚠️  (%.1f, %.1f, %.1f) (out of bounds)\n", ex, ey, ez);
            }
        } else {
            fprintf(f, "  oMissileEndPos (0x2EC): ❌ CRASH\n");
        }
        
        // SrcIdx - Try both old offset and pattern offset
        int srcIdx = 0;
        bool srcIdxFound = false;
        if (ScanSafeReadInt(missile + Offset::oMissileSrcIdx, &srcIdx)) {
            if (srcIdx >= 0x40000000 && srcIdx <= 0x7FFFFFFF) {
                fprintf(f, "  oMissileSrcIdx (0x1D0): ✅ 0x%X", srcIdx);
                if (srcIdx == playerNetId) fprintf(f, " (PLAYER)");
                if (srcIdx == spellSrcIdx && spellSrcIdx > 0) fprintf(f, " (MATCHES SpellInfo)");
                fprintf(f, "\n");
                srcIdxFound = true;
            }
        }
        
        // Try pattern SrcIdx offset
        if (!srcIdxFound && ScanSafeReadInt(missile + Offset::oMissileSrcIdx_Alt, &srcIdx)) {
            if (srcIdx >= 0x40000000 && srcIdx <= 0x7FFFFFFF) {
                fprintf(f, "  oMissileSrcIdx (0x2C4 from pattern): ✅ 0x%X", srcIdx);
                if (srcIdx == playerNetId) fprintf(f, " (PLAYER)");
                if (srcIdx == spellSrcIdx && spellSrcIdx > 0) fprintf(f, " (MATCHES SpellInfo)");
                fprintf(f, "\n");
                srcIdxFound = true;
            }
        }
        
        if (!srcIdxFound) {
            fprintf(f, "  oMissileSrcIdx (0x1D0): ⚠️  Invalid\n");
            fprintf(f, "  oMissileSrcIdx (0x2C4): ⚠️  Invalid\n");
            // Try alternative SrcIdx offsets
            fprintf(f, "    💡 Trying alternative SrcIdx offsets...\n");
            for (uint64_t altOff : {0x1C8, 0x1C4, 0x1CC, 0x1D4, 0x1D8, 0x1DC, 0x1E0, 0x1E4, 0x2DC}) {
                int altSrcIdx = 0;
                if (ScanSafeReadInt(missile + altOff, &altSrcIdx)) {
                    if (altSrcIdx >= 0x40000000 && altSrcIdx <= 0x7FFFFFFF) {
                        fprintf(f, "    ✅ Alternative SrcIdx at 0x%llX: 0x%X", altOff, altSrcIdx);
                        if (altSrcIdx == playerNetId) fprintf(f, " (PLAYER)");
                        fprintf(f, "\n");
                    }
                }
            }
        }
        
        // DestIdx (from pattern)
        int destIdx = 0;
        if (ScanSafeReadInt(missile + Offset::oMissileDestIdx, &destIdx)) {
            if (destIdx >= 0x40000000 && destIdx <= 0x7FFFFFFF) {
                fprintf(f, "  oMissileDestIdx (0x318 from pattern): ✅ 0x%X", destIdx);
                if (destIdx == spellTargetIdx && spellTargetIdx > 0) fprintf(f, " (MATCHES SpellInfo TargetIdx)");
                fprintf(f, "\n");
            } else if (destIdx == 0) {
                fprintf(f, "  oMissileDestIdx (0x318): ⚠️  0x0 (no target - skillshot)\n");
            } else {
                fprintf(f, "  oMissileDestIdx (0x318): ⚠️  0x%X (invalid)\n", destIdx);
            }
        } else {
            fprintf(f, "  oMissileDestIdx (0x318): ❌ CRASH\n");
        }
        
        // SpellInfo Direct Access (from pattern)
        uint64_t spellInfoDirect = 0;
        if (ScanSafeReadQWORD(missile + Offset::oMissileSpellInfo_Direct, &spellInfoDirect) && 
            spellInfoDirect > 0x100000 && spellInfoDirect < 0x7FFFFFFFFFFF) {
            uint64_t spellData = 0;
            if (ScanSafeReadQWORD(spellInfoDirect + Offset::oSpellInfoSpellData, &spellData) && spellData > 0x100000) {
                uint64_t namePtr = 0;
                if (ScanSafeReadQWORD(spellData + Offset::oSpellDataName, &namePtr) && namePtr > 0x10000) {
                    char name[64] = {0};
                    bool validName = false;
                    for (int c = 0; c < 63; c++) {
                        MEMORY_BASIC_INFORMATION mbi;
                        if (VirtualQuery((LPCVOID)(namePtr + c), &mbi, sizeof(mbi)) == 0) break;
                        if (mbi.State != MEM_COMMIT) break;
                        char ch = *(char*)(namePtr + c);
                        if (ch == 0 && c > 3) { validName = true; break; }
                        if (ch < 32 || ch > 126) break;
                        name[c] = ch;
                    }
                    if (validName && name[0]) {
                        fprintf(f, "  oMissileSpellInfo_Direct (0x260 from pattern): ✅ \"%s\"\n", name);
                        // Lookup from database
                        const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(name);
                        if (dbInfo) {
                            fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f, Range=%.0f\n", 
                                dbInfo->speed, dbInfo->radius, dbInfo->width, dbInfo->range);
                        }
                    } else {
                        fprintf(f, "  oMissileSpellInfo_Direct (0x260): ⚠️  Invalid SpellData\n");
                    }
                } else {
                    fprintf(f, "  oMissileSpellInfo_Direct (0x260): ⚠️  Invalid namePtr\n");
                }
            } else {
                fprintf(f, "  oMissileSpellInfo_Direct (0x260): ⚠️  Invalid SpellData pointer\n");
            }
        } else {
            fprintf(f, "  oMissileSpellInfo_Direct (0x260): ⚠️  Invalid SpellInfo pointer\n");
        }
        
        fprintf(f, "\n");
        
        // PHASE 2: Scan SpellInfo Patterns (4 patterns)
        fprintf(f, "--- PHASE 2: SpellInfo Patterns (INDIRECT) ---\n");
        fprintf(f, "💡 Missile là gì?\n");
        fprintf(f, "   - Missile = GameObject pointer từ MissileList (oMissileList)\n");
        fprintf(f, "   - MissileList = MissileManager + 0x08 -> ArrayPtr, + 0x10 -> Size\n");
        fprintf(f, "   - Missile[%d] @ 0x%llX là missile object trong game\n", m, missile);
        fprintf(f, "   - Pattern [0x728]->[0x98] nghĩa là: missile[0x728] -> ptr[0x98] -> SpellInfo*\n");
        fprintf(f, "   - EzrealQ dùng Pattern 3 (0x728->0x98) để access SpellInfo\n\n");
        
        // Pattern 1: 0x1F0 -> 0xD8
        uint64_t spellCast = 0;
        if (ScanSafeReadQWORD(missile + 0x1F0, &spellCast) && spellCast > 0x100000 && spellCast < 0x7FFFFFFFFFFF) {
            uint64_t spellInfo = 0;
            if (ScanSafeReadQWORD(spellCast + 0xD8, &spellInfo) && spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                uint64_t spellData = 0;
                if (ScanSafeReadQWORD(spellInfo + Offset::oSpellInfoSpellData, &spellData) && spellData > 0x100000) {
                    uint64_t namePtr = 0;
                    if (ScanSafeReadQWORD(spellData + Offset::oSpellDataName, &namePtr) && namePtr > 0x10000) {
                        char name[64] = {0};
                        bool validName = false;
                        for (int c = 0; c < 63; c++) {
                            char ch = 0;
                            MEMORY_BASIC_INFORMATION mbi;
                            if (VirtualQuery((LPCVOID)(namePtr + c), &mbi, sizeof(mbi)) == 0) break;
                            if (mbi.State != MEM_COMMIT) break;
                            ch = *(char*)(namePtr + c);
                            if (ch == 0 && c > 3) { validName = true; break; }
                            if (ch < 32 || ch > 126) break;
                            name[c] = ch;
                        }
                        if (validName && name[0]) {
                            fprintf(f, "  Pattern 1 [0x1F0]->[0xD8]: ✅ \"%s\"\n", name);
                            // Lookup from database
                            const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(name);
                            if (dbInfo) {
                                fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f, Range=%.0f\n", 
                                    dbInfo->speed, dbInfo->radius, dbInfo->width, dbInfo->range);
                            }
                        }
                    }
                }
            }
        }
        
        // Pattern 2: 0x578 -> 0x98
        uint64_t ptr1 = 0;
        if (ScanSafeReadQWORD(missile + 0x578, &ptr1) && ptr1 > 0x100000 && ptr1 < 0x7FFFFFFFFFFF) {
            uint64_t spellInfo = 0;
            if (ScanSafeReadQWORD(ptr1 + 0x98, &spellInfo) && spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                uint64_t spellData = 0;
                if (ScanSafeReadQWORD(spellInfo + Offset::oSpellInfoSpellData, &spellData) && spellData > 0x100000) {
                    uint64_t namePtr = 0;
                    if (ScanSafeReadQWORD(spellData + Offset::oSpellDataName, &namePtr) && namePtr > 0x10000) {
                        char name[64] = {0};
                        bool validName = false;
                        for (int c = 0; c < 63; c++) {
                            char ch = 0;
                            MEMORY_BASIC_INFORMATION mbi;
                            if (VirtualQuery((LPCVOID)(namePtr + c), &mbi, sizeof(mbi)) == 0) break;
                            if (mbi.State != MEM_COMMIT) break;
                            ch = *(char*)(namePtr + c);
                            if (ch == 0 && c > 3) { validName = true; break; }
                            if (ch < 32 || ch > 126) break;
                            name[c] = ch;
                        }
                        if (validName && name[0]) {
                            fprintf(f, "  Pattern 2 [0x578]->[0x98]: ✅ \"%s\"\n", name);
                            // Lookup from database
                            const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(name);
                            if (dbInfo) {
                                fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f, Range=%.0f\n", 
                                    dbInfo->speed, dbInfo->radius, dbInfo->width, dbInfo->range);
                            }
                        }
                    }
                }
            }
        }
        
        // Pattern 3: 0x728 -> 0x98 (EzrealQ dùng pattern này!)
        fprintf(f, "\n  🔍 Testing Pattern 3 [0x728]->[0x98] (EzrealQ pattern)...\n");
        uint64_t ptr2 = 0;
        bool pattern3Worked = false;
        if (ScanSafeReadQWORD(missile + 0x728, &ptr2)) {
            fprintf(f, "    missile[0x728] = 0x%llX", ptr2);
            if (ptr2 > 0x100000 && ptr2 < 0x7FFFFFFFFFFF) {
                fprintf(f, " ✅ Valid pointer\n");
                uint64_t spellInfo = 0;
                if (ScanSafeReadQWORD(ptr2 + 0x98, &spellInfo)) {
                    fprintf(f, "    ptr[0x98] = 0x%llX", spellInfo);
                    if (spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                        fprintf(f, " ✅ Valid SpellInfo pointer\n");
                        uint64_t spellData = 0;
                        if (ScanSafeReadQWORD(spellInfo + Offset::oSpellInfoSpellData, &spellData) && spellData > 0x100000) {
                            uint64_t namePtr = 0;
                            if (ScanSafeReadQWORD(spellData + Offset::oSpellDataName, &namePtr) && namePtr > 0x10000) {
                                char name[64] = {0};
                                bool validName = false;
                                for (int c = 0; c < 63; c++) {
                                    char ch = 0;
                                    MEMORY_BASIC_INFORMATION mbi;
                                    if (VirtualQuery((LPCVOID)(namePtr + c), &mbi, sizeof(mbi)) == 0) break;
                                    if (mbi.State != MEM_COMMIT) break;
                                    ch = *(char*)(namePtr + c);
                                    if (ch == 0 && c > 3) { validName = true; break; }
                                    if (ch < 32 || ch > 126) break;
                                    name[c] = ch;
                                }
                                if (validName && name[0]) {
                                    fprintf(f, "  Pattern 3 [0x728]->[0x98]: ✅ \"%s\"\n", name);
                                    pattern3Worked = true;
                                    // Lookup from database
                                    const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(name);
                                    if (dbInfo) {
                                        fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f, Range=%.0f\n", 
                                            dbInfo->speed, dbInfo->radius, dbInfo->width, dbInfo->range);
                                    } else {
                                        fprintf(f, "    ⚠️  Spell \"%s\" not in database\n", name);
                                    }
                                } else {
                                    fprintf(f, " ⚠️  Invalid spell name\n");
                                }
                            } else {
                                fprintf(f, " ⚠️  Invalid namePtr (0x%llX)\n", namePtr);
                            }
                        } else {
                            fprintf(f, " ⚠️  Invalid SpellData pointer (0x%llX)\n", spellData);
                        }
                    } else {
                        fprintf(f, " ❌ Invalid SpellInfo pointer\n");
                    }
                } else {
                    fprintf(f, " ❌ Cannot read ptr[0x98]\n");
                }
            } else {
                fprintf(f, " ❌ Invalid pointer (out of range)\n");
            }
        } else {
            fprintf(f, "    missile[0x728] = ❌ CRASH or invalid\n");
        }
        
        if (!pattern3Worked) {
            fprintf(f, "  ⚠️  Pattern 3 FAILED - EzrealQ không dùng pattern này với missile này\n");
            fprintf(f, "  💡 Có thể missile này không phải EzrealQ hoặc dùng pattern khác\n");
        }
        
        // Pattern 4: 0xC0 -> 0xE8
        uint64_t ptr3 = 0;
        if (ScanSafeReadQWORD(missile + 0xC0, &ptr3) && ptr3 > 0x100000 && ptr3 < 0x7FFFFFFFFFFF) {
            uint64_t spellInfo = 0;
            if (ScanSafeReadQWORD(ptr3 + 0xE8, &spellInfo) && spellInfo > 0x100000 && spellInfo < 0x7FFFFFFFFFFF) {
                uint64_t spellData = 0;
                if (ScanSafeReadQWORD(spellInfo + Offset::oSpellInfoSpellData, &spellData) && spellData > 0x100000) {
                    uint64_t namePtr = 0;
                    if (ScanSafeReadQWORD(spellData + Offset::oSpellDataName, &namePtr) && namePtr > 0x10000) {
                        char name[64] = {0};
                        bool validName = false;
                        for (int c = 0; c < 63; c++) {
                            char ch = 0;
                            MEMORY_BASIC_INFORMATION mbi;
                            if (VirtualQuery((LPCVOID)(namePtr + c), &mbi, sizeof(mbi)) == 0) break;
                            if (mbi.State != MEM_COMMIT) break;
                            ch = *(char*)(namePtr + c);
                            if (ch == 0 && c > 3) { validName = true; break; }
                            if (ch < 32 || ch > 126) break;
                            name[c] = ch;
                        }
                        if (validName && name[0]) {
                            fprintf(f, "  Pattern 4 [0xC0]->[0xE8]: ✅ \"%s\"\n", name);
                            // Lookup from database
                            const SpellDatabase::SpellInfo* dbInfo = SpellDatabase::GetSpellInfo(name);
                            if (dbInfo) {
                                fprintf(f, "    📚 Database: Speed=%.0f, Radius=%.0f, Width=%.0f, Range=%.0f\n", 
                                    dbInfo->speed, dbInfo->radius, dbInfo->width, dbInfo->range);
                            }
                        }
                    }
                }
            }
        }
        
        fprintf(f, "\n");
        
        // PHASE 3: Scan Missing Offsets (cần tìm)
        fprintf(f, "--- PHASE 3: Missing Offsets (CẦN TÌM) ---\n");
        fprintf(f, "Scanning for: StartPos, EndPos, Speed, Radius, Width, StartTime\n\n");
        
        // Get SpellInfo for reference (to compare StartPos/EndPos)
        float refStartX = 0, refStartY = 0, refStartZ = 0;
        float refEndX = 0, refEndY = 0, refEndZ = 0;
        bool hasRefStartPos = false, hasRefEndPos = false;
        
        // Try to get SpellInfo from missile patterns
        // Using C-style arrays instead of std::string to avoid C2712 error (cannot use __try with C++ objects)
        char patternUsedBuf[64] = {0};
        uint64_t spellInfoRef = MissileScanner::GetSpellInfoFromMissileC(missile, patternUsedBuf, sizeof(patternUsedBuf));
        if (spellInfoRef) {
            char spellNameBuf[64] = {0};
            MissileScanner::GetSpellNameC(spellInfoRef, spellNameBuf, sizeof(spellNameBuf));
            if (spellNameBuf[0]) {
                fprintf(f, "✅ Found SpellInfo via %s: \"%s\"\n", patternUsedBuf, spellNameBuf);
                
                // Read StartPos/EndPos from SpellInfo for comparison
                if (ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoStartPos, &refStartX) &&
                    ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoStartPos + 4, &refStartY) &&
                    ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoStartPos + 8, &refStartZ)) {
                    hasRefStartPos = true;
                    fprintf(f, "  Reference StartPos: (%.1f, %.1f, %.1f)\n", refStartX, refStartY, refStartZ);
                }
                if (ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoEndPos, &refEndX) &&
                    ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoEndPos + 4, &refEndY) &&
                    ScanSafeReadFloat(spellInfoRef + Offset::oSpellInfoEndPos + 8, &refEndZ)) {
                    hasRefEndPos = true;
                    fprintf(f, "  Reference EndPos: (%.1f, %.1f, %.1f)\n", refEndX, refEndY, refEndZ);
                }
            }
        } else {
            fprintf(f, "⚠️  No SpellInfo found - cannot compare StartPos/EndPos\n");
            fprintf(f, "💡 TIP: Cast a skillshot from YOUR champion to get reference SpellInfo\n");
            fprintf(f, "💡 CRITICAL: Scanner needs SpellInfo reference to find StartPos/EndPos accurately\n");
            fprintf(f, "💡 Without SpellInfo, scanner can only list candidates but cannot verify matches\n");
        }
        fprintf(f, "\n");
        
        // Scan StartPos/EndPos candidates (Vec3, range 0x0-0x800, expanded range)
        fprintf(f, "StartPos/EndPos candidates (Vec3, 0x0-0x800):\n");
        int posCandidateCount = 0;
        // Using C-style arrays instead of std::vector to avoid C2712 error
        struct OffsetMatch { uint64_t offset; float distance; };
        OffsetMatch startPosMatches[50];
        OffsetMatch endPosMatches[50];
        int startPosMatchCount = 0;
        int endPosMatchCount = 0;
        
        // Also check if current position is valid to use as reference
        float currentPosX = 0, currentPosY = 0, currentPosZ = 0;
        bool hasCurrentPos = ScanSafeReadFloat(missile + Offset::oMissilePosition, &currentPosX) &&
                             ScanSafeReadFloat(missile + Offset::oMissilePosition + 4, &currentPosY) &&
                             ScanSafeReadFloat(missile + Offset::oMissilePosition + 8, &currentPosZ);
        bool currentPosValid = hasCurrentPos && currentPosX > 100 && currentPosX < 20000 && 
                               currentPosZ > 100 && currentPosZ < 20000;
        
        for (uint64_t off = 0x0; off <= 0x800 && posCandidateCount < 100; off += 0x4) {
            // Skip known offsets
            if (off == Offset::oMissilePosition || off == Offset::oMissilePosition + 4 || off == Offset::oMissilePosition + 8) {
                continue;
            }
            
            float x, y, z;
            if (ScanSafeReadFloat(missile + off, &x) &&
                ScanSafeReadFloat(missile + off + 4, &y) &&
                ScanSafeReadFloat(missile + off + 8, &z)) {
                // More lenient validation - allow positions closer to origin
                if (x > 0 && x < 20000 && z > 0 && z < 20000 && y > -2000 && y < 5000) {
                    // Skip if it's the same as current position (already known)
                    if (currentPosValid && fabsf(x - currentPosX) < 1.0f && 
                        fabsf(z - currentPosZ) < 1.0f) {
                        continue;
                    }
                    
                    if (x != 0 || y != 0 || z != 0) {
                        fprintf(f, "  [0x%llX]: (%.1f, %.1f, %.1f)", off, x, y, z);
                        
                        // Check if matches reference StartPos/EndPos
                        if (hasRefStartPos) {
                            float dist = sqrtf((x-refStartX)*(x-refStartX) + (y-refStartY)*(y-refStartY) + (z-refStartZ)*(z-refStartZ));
                            if (dist < 5.0f && startPosMatchCount < 50) {
                                startPosMatches[startPosMatchCount].offset = off;
                                startPosMatches[startPosMatchCount].distance = dist;
                                startPosMatchCount++;
                                fprintf(f, " ✅ MATCHES StartPos (dist=%.2f)", dist);
                            }
                        }
                        if (hasRefEndPos) {
                            float dist = sqrtf((x-refEndX)*(x-refEndX) + (y-refEndY)*(y-refEndY) + (z-refEndZ)*(z-refEndZ));
                            if (dist < 5.0f && endPosMatchCount < 50) {
                                endPosMatches[endPosMatchCount].offset = off;
                                endPosMatches[endPosMatchCount].distance = dist;
                                endPosMatchCount++;
                                fprintf(f, " ✅ MATCHES EndPos (dist=%.2f)", dist);
                            }
                        }
                        
                        fprintf(f, "\n");
                        posCandidateCount++;
                    }
                }
            }
        }
        
        // Show best matches - using simple bubble sort for C-style array
        if (startPosMatchCount > 0 || endPosMatchCount > 0) {
            fprintf(f, "\n🎯 BEST MATCHES:\n");
            if (startPosMatchCount > 0) {
                // Simple sort to find minimum distance
                for (int i = 0; i < startPosMatchCount - 1; i++) {
                    for (int j = i + 1; j < startPosMatchCount; j++) {
                        if (startPosMatches[j].distance < startPosMatches[i].distance) {
                            OffsetMatch tmp = startPosMatches[i];
                            startPosMatches[i] = startPosMatches[j];
                            startPosMatches[j] = tmp;
                        }
                    }
                }
                fprintf(f, "  StartPos: 0x%llX (distance: %.2f) ✅\n", startPosMatches[0].offset, startPosMatches[0].distance);
            }
            if (endPosMatchCount > 0) {
                // Simple sort to find minimum distance
                for (int i = 0; i < endPosMatchCount - 1; i++) {
                    for (int j = i + 1; j < endPosMatchCount; j++) {
                        if (endPosMatches[j].distance < endPosMatches[i].distance) {
                            OffsetMatch tmp = endPosMatches[i];
                            endPosMatches[i] = endPosMatches[j];
                            endPosMatches[j] = tmp;
                        }
                    }
                }
                fprintf(f, "  EndPos: 0x%llX (distance: %.2f) ✅\n", endPosMatches[0].offset, endPosMatches[0].distance);
            }
        } else if (hasRefStartPos || hasRefEndPos) {
            fprintf(f, "\n⚠️  No exact matches found - positions may be calculated dynamically\n");
        }
        if (posCandidateCount == 0) {
            fprintf(f, "  ⚠️  No valid position candidates found\n");
        }
        
        fprintf(f, "\n");
        
        // Scan Speed/Radius/Width candidates (float, range 0x150-0x500)
        fprintf(f, "Speed/Radius/Width candidates (float, 0x150-0x500):\n");
        fprintf(f, "💡 CANDIDATES FROM SCAN LOG:\n");
        fprintf(f, "  - MISSILE[0]: 0x4E4 = 55.86 (Radius/Width candidate) ⭐\n");
        fprintf(f, "  - MISSILE[1]: 0x220 = 3571.07 (Speed candidate)\n");
        fprintf(f, "  - MISSILE[2]: 0x2D0 = 655.66, 0x2D4 = 283.57, 0x2D8 = 678.80, 0x2DC = 121.71 ⭐, 0x2E0 = 21.71\n");
        fprintf(f, "💡 Verifying these candidates...\n\n");
        int floatCandidateCount = 0;
        // Reuse currentPosX, currentPosY, currentPosZ, hasCurrentPos from above
        
        // First, test known candidates from scan log
        uint64_t knownCandidates[] = {0x4E4, 0x220, 0x2D0, 0x2D4, 0x2D8, 0x2DC, 0x2E0};
        for (uint64_t testOff : knownCandidates) {
            float val;
            if (ScanSafeReadFloat(missile + testOff, &val)) {
                if (val > 10 && val < 5000 && val != 0) {
                    fprintf(f, "  [0x%llX]: %.2f", testOff, val);
                    if (val >= 400 && val <= 5000) {
                        fprintf(f, " (Speed candidate)");
                        if (val >= 1000 && val <= 2500) fprintf(f, " ⭐ COMMON RANGE");
                    }
                    if (val >= 10 && val <= 500) {
                        fprintf(f, " (Radius/Width candidate)");
                        if (val >= 50 && val <= 150) fprintf(f, " ⭐ COMMON RANGE");
                    }
                    fprintf(f, " ⚠️ FROM SCAN LOG\n");
                    floatCandidateCount++;
                }
            }
        }
        
        fprintf(f, "\n");
        
        for (uint64_t off = 0x150; off <= 0x500 && floatCandidateCount < 50; off += 0x4) {
            // Skip known candidates already tested
            bool skip = false;
            for (uint64_t testOff : knownCandidates) {
                if (off == testOff) { skip = true; break; }
            }
            if (skip) continue;
            float val;
            if (ScanSafeReadFloat(missile + off, &val)) {
                // Speed: 50-5000 units/s, Radius: 10-500 units, Width: 10-500 units
                if (val > 10 && val < 5000 && val != 0) {
                    // Filter out position values (if it's too close to current position, skip it)
                    bool isPositionValue = false;
                    if (hasCurrentPos) {
                        if (fabsf(val - currentPosX) < 10.0f || fabsf(val - currentPosZ) < 10.0f) {
                            isPositionValue = true;
                        }
                    }
                    
                    if (!isPositionValue) {
                        fprintf(f, "  [0x%llX]: %.2f", off, val);
                        if (val >= 400 && val <= 5000) {
                            fprintf(f, " (Speed candidate)");
                            if (val >= 1000 && val <= 2500) fprintf(f, " ⭐ COMMON RANGE");
                        }
                        if (val >= 10 && val <= 500) {
                            fprintf(f, " (Radius/Width candidate)");
                            if (val >= 50 && val <= 150) fprintf(f, " ⭐ COMMON RANGE");
                        }
                        fprintf(f, "\n");
                        floatCandidateCount++;
                    }
                }
            }
        }
        if (floatCandidateCount == 0) {
            fprintf(f, "  ⚠️  No valid float candidates found\n");
        }
        
        fprintf(f, "\n");
        
        // Decryption structure info
        fprintf(f, "--- DECRYPTION STRUCTURE INFO ---\n");
        uintptr_t obfStruct = missile + 0x108;
        MEMORY_BASIC_INFORMATION mbi2;
        if (VirtualQuery((LPCVOID)obfStruct, &mbi2, sizeof(mbi2)) != 0 && mbi2.State == MEM_COMMIT) {
            uint8_t sizeField = *(uint8_t*)(obfStruct + 0x6);
            uint8_t dataIndex = *(uint8_t*)(obfStruct + 0x8);
            uint8_t initialized = *(uint8_t*)(obfStruct + 0x5);
            fprintf(f, "Obfuscation structure @ 0x108:\n");
            fprintf(f, "  Initialized (0x10D): 0x%02X %s\n", initialized, initialized ? "(OBFUSCATED)" : "(NOT OBFUSCATED)");
            fprintf(f, "  Size field (0x10E): 0x%02X\n", sizeField);
            fprintf(f, "  Data index (0x110): 0x%02X\n", dataIndex);
        } else {
            fprintf(f, "  ⚠️  Cannot read obfuscation structure\n");
        }
        fprintf(f, "\n💡 NOTE: Hàm giải mã đã được implement (IDA::DecryptMissileByte)\n");
        fprintf(f, "💡 Scanner đọc trực tiếp - một số offsets có thể KHÔNG bị obfuscated\n");
        fprintf(f, "💡 Nếu obfuscated, có thể gọi IDA::DecryptMissileByte() để decrypt\n");
        
        fprintf(f, "\n");
    }
    
    fprintf(f, "========================================\n");
    fprintf(f, "SCAN COMPLETE\n");
    fprintf(f, "========================================\n");
    fprintf(f, "\nINSTRUCTIONS:\n");
    fprintf(f, "1. Cast a skillshot (Jinx W, Ezreal Q, Lux Q, etc.)\n");
    fprintf(f, "2. Scan NGAY khi missile vừa created (frame đầu tiên)\n");
    fprintf(f, "3. So sánh StartPos/EndPos candidates với SpellInfo StartPos/EndPos (✅ MATCHES)\n");
    fprintf(f, "4. Tính Speed từ Position delta: Scan nhiều frames, Speed = Distance(Pos1, Pos2) / deltaTime\n");
    fprintf(f, "5. So sánh Radius/Width với SpellData values (nếu có)\n");
    fprintf(f, "6. So sánh StartTime với GameTime khi missile created (diff < 0.5s)\n");
    fprintf(f, "7. So sánh DestIdx với SpellInfo TargetIndex (chỉ có với targeted spells)\n");
    fprintf(f, "\n");
    fprintf(f, "DECRYPTION NOTES:\n");
    fprintf(f, "- Hàm giải mã: sub_296390 (XOR/NOT), sub_521940 (wrapper), sub_17F4EE0 (search)\n");
    fprintf(f, "- Obfuscation structure: missilePtr + 0x108\n");
    fprintf(f, "- ✅ IMPLEMENTED: IDA::DecryptMissileByte() (simplified from sub_296390)\n");
    fprintf(f, "- ✅ IMPLEMENTED: IDA::IsMissileObfuscated() (check obfuscation status)\n");
    fprintf(f, "- Hiện tại scanner đọc trực tiếp - một số offsets có thể không bị obfuscated\n");
    fprintf(f, "- Scanner này được thiết kế để dùng ở nhiều phiên bản (offsets có thể thay đổi)\n");
    fprintf(f, "\n⚠️  ISSUE DETECTED:\n");
    fprintf(f, "- Position/SrcIdx có giá trị invalid → Có thể bị OBFUSCATED\n");
    fprintf(f, "- Không tìm thấy SpellInfo patterns → Có thể bị OBFUSCATED\n");
    fprintf(f, "- Giải pháp: Cast skillshot và scan NGAY khi missile vừa created\n");
    fprintf(f, "- Hoặc: Cần implement full decryption logic để decrypt trước khi đọc\n");
    fprintf(f, "========================================\n");
    
    fclose(f);
}

// ============================================================================
// DYNAMIC MISSILE OFFSET SCANNER - Uses MissileOffsetScanner for multi-version
// ============================================================================
// Purpose: Find missing offsets dynamically by comparing with SpellInfo
// Strategy: Match Vec3/floats in missile structure with known SpellInfo values
void ScanMissileOffsetsDynamic() {
    FILE* f = fopen("missile_dynamic_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "========================================\n");
    fprintf(f, "DYNAMIC MISSILE OFFSET SCANNER\n");
    fprintf(f, "========================================\n");
    fprintf(f, "Strategy: Compare missile structure with SpellInfo values\n");
    fprintf(f, "This scanner works across multiple game versions!\n");
    fprintf(f, "\n");
    fprintf(f, "Decryption functions (from IDA):\n");
    fprintf(f, "- sub_296390: XOR/NOT decryption (main)\n");
    fprintf(f, "- sub_521940: Wrapper function\n");
    fprintf(f, "- sub_17F5D70: Thunk to sub_17F4EE0\n");
    fprintf(f, "- sub_17F4EE0: Search function\n");
    fprintf(f, "\n");
    fprintf(f, "Obfuscation structure: missilePtr + 0x108\n");
    fprintf(f, "NOTE: Some offsets may NOT be obfuscated!\n");
    fprintf(f, "========================================\n\n");
    
    // Run the full scan using MissileOffsetScanner
    MissileScanner::ScanResults results = MissileScanner::FullScan(f);
    
    // Output final summary
    fprintf(f, "\n========================================\n");
    fprintf(f, "COPY-PASTE READY OFFSETS (for Offsets.h)\n");
    fprintf(f, "========================================\n\n");
    
    fprintf(f, "// Position Offsets\n");
    if (results.bestStartPosOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileStartPos = 0x%llX;\n", results.bestStartPosOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileStartPos = 0x0; // NOT FOUND - need more samples\n");
    }
    if (results.bestEndPosOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileEndPos = 0x%llX;\n", results.bestEndPosOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileEndPos = 0x0; // NOT FOUND - need more samples\n");
    }
    
    fprintf(f, "\n// Movement Offsets\n");
    if (results.bestSpeedOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileSpeed = 0x%llX; // NEEDS VERIFICATION\n", results.bestSpeedOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileSpeed = 0x0; // NOT FOUND\n");
    }
    
    fprintf(f, "\n// Collision Offsets\n");
    if (results.bestRadiusOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileRadius = 0x%llX; // NEEDS VERIFICATION\n", results.bestRadiusOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileRadius = 0x0; // NOT FOUND\n");
    }
    if (results.bestWidthOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileWidth = 0x%llX; // NEEDS VERIFICATION\n", results.bestWidthOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileWidth = 0x0; // NOT FOUND\n");
    }
    
    fprintf(f, "\n// Timing Offsets\n");
    if (results.bestStartTimeOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileStartTime = 0x%llX;\n", results.bestStartTimeOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileStartTime = 0x0; // NOT FOUND\n");
    }
    
    fprintf(f, "\n// Targeting Offsets\n");
    if (results.bestDestIdxOffset > 0) {
        fprintf(f, "inline constexpr uint64_t oMissileDestIdx = 0x%llX; // Only for targeted spells\n", results.bestDestIdxOffset);
    } else {
        fprintf(f, "inline constexpr uint64_t oMissileDestIdx = 0x0; // NOT FOUND (normal for skillshots)\n");
    }
    
    fprintf(f, "\n========================================\n");
    fprintf(f, "SCAN INSTRUCTIONS:\n");
    fprintf(f, "========================================\n");
    fprintf(f, "1. Cast multiple skillshots (Ezreal Q, Jinx W, Lux Q, etc.)\n");
    fprintf(f, "2. Run this scan IMMEDIATELY after missile appears\n");
    fprintf(f, "3. The scanner compares missile values with SpellInfo\n");
    fprintf(f, "4. High-confidence matches (✅) are reliable\n");
    fprintf(f, "5. Candidates (⚠️) need manual verification\n");
    fprintf(f, "6. Copy the offsets to Offsets.h when verified\n");
    fprintf(f, "========================================\n");
    
    fclose(f);
}

// ============================================================================
// BASIC ATTACK OFFSET SCANNER - Find BasicAttack state offsets
// ============================================================================
bool continuousBasicAttackScan = false;

void BasicAttackOffsetScan() {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer);
    if (!localPlayer) return;
    
    FILE* f = fopen("basicattack_offset_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== BASIC ATTACK OFFSET SCAN ===\n");
    fprintf(f, "LocalPlayer: 0x%llX\n\n", localPlayer);
    
    // Known offsets from pattern file
    fprintf(f, "=== KNOWN OFFSETS (from pattern) ===\n");
    fprintf(f, "oBasicAttackBase = 0x%llX\n", Offset::oBasicAttackBase);
    fprintf(f, "oBasicAttackOffset1 = 0x%llX\n", Offset::oBasicAttackOffset1);
    fprintf(f, "oBasicAttackOffset2 = 0x%llX\n", Offset::oBasicAttackOffset2);
    fprintf(f, "oBasicAttackRemote = 0x%llX\n", Offset::oBasicAttackRemote);
    fprintf(f, "oBasicAttackMelee = 0x%llX\n", Offset::oBasicAttackMelee);
    fprintf(f, "oObjBasicAttackCastCount = 0x%llX\n\n", Offset::oObjBasicAttackCastCount);
    
    // Try to read BasicAttack structure
    fprintf(f, "=== READING BASICATTACK STRUCTURE ===\n");
    __try {
        uint64_t baBase = *(uint64_t*)(localPlayer + Offset::oBasicAttackBase);
        fprintf(f, "LocalPlayer + 0x%llX = 0x%llX\n", Offset::oBasicAttackBase, baBase);
        
        if (baBase && baBase > 0x10000) {
            uint64_t baO1 = *(uint64_t*)(baBase + Offset::oBasicAttackOffset1);
            fprintf(f, "  + 0x%llX = 0x%llX\n", Offset::oBasicAttackOffset1, baO1);
            
            if (baO1 && baO1 > 0x10000) {
                uint64_t baO2 = *(uint64_t*)(baO1 + Offset::oBasicAttackOffset2);
                fprintf(f, "    + 0x%llX = 0x%llX\n", Offset::oBasicAttackOffset2, baO2);
                
                // Try reading remote/melee flags
                if (baO2 && baO2 > 0x10000) {
                    uint8_t remoteFlag = *(uint8_t*)(baO2 + Offset::oBasicAttackRemote);
                    uint8_t meleeFlag = *(uint8_t*)(baO2 + Offset::oBasicAttackMelee);
                    fprintf(f, "      Remote flag (0x%llX): %d\n", Offset::oBasicAttackRemote, remoteFlag);
                    fprintf(f, "      Melee flag (0x%llX): %d\n", Offset::oBasicAttackMelee, meleeFlag);
                }
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "EXCEPTION reading BasicAttack structure\n");
    }
    
    // Try to read BasicAttackCastCount
    fprintf(f, "\n=== BASICATTACK CAST COUNT ===\n");
    __try {
        uint32_t castCount = *(uint32_t*)(localPlayer + Offset::oObjBasicAttackCastCount);
        fprintf(f, "LocalPlayer + 0x%llX = %u\n", Offset::oObjBasicAttackCastCount, castCount);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        fprintf(f, "EXCEPTION reading BasicAttackCastCount\n");
    }
    
    // Scan for potential BasicAttackCastCount (look for incrementing values)
    fprintf(f, "\n=== SCANNING FOR CAST COUNT (0x5800 - 0x6000) ===\n");
    fprintf(f, "Attack 5-10 times, then scan. Look for value = attack count\n");
    for (uint64_t off = 0x5800; off < 0x6000; off += 4) {
        __try {
            uint32_t val = *(uint32_t*)(localPlayer + off);
            // Look for small values that could be attack count (1-100)
            if (val > 0 && val < 100) {
                fprintf(f, "[0x%llX] = %u\n", off, val);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    // Also scan 0x4000-0x5000 range for attack count
    fprintf(f, "\n=== SCANNING RANGE (0x4C00 - 0x5000) ===\n");
    for (uint64_t off = 0x4C00; off < 0x5000; off += 4) {
        __try {
            uint32_t val = *(uint32_t*)(localPlayer + off);
            if (val > 0 && val < 100) {
                fprintf(f, "[0x%llX] = %u\n", off, val);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    // Scan for potential BasicAttack base pointers (0x2C00 - 0x2D00)
    fprintf(f, "\n=== SCANNING FOR BA BASE POINTERS (0x2C00 - 0x2D00) ===\n");
    for (uint64_t off = 0x2C00; off < 0x2D00; off += 8) {
        __try {
            uint64_t ptr = *(uint64_t*)(localPlayer + off);
            if (ptr && ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                fprintf(f, "[0x%llX] = 0x%llX (valid pointer)\n", off, ptr);
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {}
    }
    
    fclose(f);
}

// ============================================================================
// AI MANAGER OFFSET SCANNER - Verify AiManager base and internal offsets
// ============================================================================
bool continuousAiManagerScan = false;
bool continuousAiManagerLog = false;

void AiManagerOffsetScan() {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer);
    if (!localPlayer) return;
    
    FILE* f = fopen("aimanager_offset_scan.txt", "w");
    if (!f) return;
    
    fprintf(f, "=== AI MANAGER OFFSET SCAN ===\n");
    fprintf(f, "LocalPlayer: 0x%llX\n\n", localPlayer);
    
    // Get player position for comparison
    float playerX = 0, playerY = 0, playerZ = 0;
    __try {
        playerX = *(float*)(localPlayer + 0x254);  // oObjPosition
        playerY = *(float*)(localPlayer + 0x258);
        playerZ = *(float*)(localPlayer + 0x25C);
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    fprintf(f, "PlayerPosition: (%.1f, %.1f, %.1f)\n\n", playerX, playerY, playerZ);
    
    // =========================================================================
    // TEST MULTIPLE AiManager BASE OFFSETS
    // =========================================================================
    fprintf(f, "=== TESTING AiManager BASE OFFSETS ===\n\n");
    
    uint64_t aiManagerCandidates[] = {0x41A8, 0x2C78, 0x4060, 0x2E68, 0x3110};
    const char* candidateNames[] = {"0x41A8 (old)", "0x2C78 (IDA)", "0x4060 (IDA)", "0x2E68 (BuffMgr?)", "0x3110 (SpellBook?)"};
    
    for (int i = 0; i < 5; i++) {
        uint64_t offset = aiManagerCandidates[i];
        fprintf(f, "--- Testing %s ---\n", candidateNames[i]);
        
        __try {
            uint64_t ptr = *(uint64_t*)(localPlayer + offset);
            fprintf(f, "PTR[0x%llX] = 0x%llX\n", offset, ptr);
            
            if (ptr > 0x10000 && ptr < 0x7FFFFFFFFFFF) {
                fprintf(f, "  Valid pointer! Scanning for Vec3 positions...\n");
                
                // Scan for Vec3 that matches player position (EXPANDED RANGE 0x0 - 0x600)
                for (int off = 0; off < 0x600; off += 4) {
                    __try {
                        float x = *(float*)(ptr + off);
                        float y = *(float*)(ptr + off + 4);
                        float z = *(float*)(ptr + off + 8);
                        
                        // Check if valid map coords
                        if (x > 0 && x < 16000 && z > 0 && z < 16000 && 
                            y > -500 && y < 500) {
                            
                            // Check if close to player position
                            float dist = sqrtf((x-playerX)*(x-playerX) + (z-playerZ)*(z-playerZ));
                            if (dist < 50) {
                                fprintf(f, "  [0x%03X] (%.1f, %.1f, %.1f) << EXACT MATCH!\n", off, x, y, z);
                            } else if (dist < 500) {
                                fprintf(f, "  [0x%03X] (%.1f, %.1f, %.1f) dist=%.0f\n", off, x, y, z, dist);
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                // ALSO: Check if this is actually a pointer to another structure
                fprintf(f, "  Checking nested pointers...\n");
                for (int off = 0; off < 0x100; off += 8) {
                    __try {
                        uint64_t nestedPtr = *(uint64_t*)(ptr + off);
                        if (nestedPtr > 0x10000 && nestedPtr < 0x7FFFFFFFFFFF) {
                            // Check if nested pointer contains player position
                            for (int innerOff = 0; innerOff < 0x200; innerOff += 4) {
                                __try {
                                    float x = *(float*)(nestedPtr + innerOff);
                                    float y = *(float*)(nestedPtr + innerOff + 4);
                                    float z = *(float*)(nestedPtr + innerOff + 8);
                                    
                                    if (x > 0 && x < 16000 && z > 0 && z < 16000 && y > -500 && y < 500) {
                                        float dist = sqrtf((x-playerX)*(x-playerX) + (z-playerZ)*(z-playerZ));
                                        if (dist < 50) {
                                            fprintf(f, "  PTR[0x%02X]+0x%03X = (%.1f, %.1f, %.1f) << NESTED MATCH!\n", 
                                                off, innerOff, x, y, z);
                                        }
                                    }
                                } __except(EXCEPTION_EXECUTE_HANDLER) {}
                            }
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                // Scan for velocity-like floats (small values, typically < 500)
                fprintf(f, "  Scanning for velocity/speed values...\n");
                for (int off = 0x300; off < 0x400; off += 4) {
                    __try {
                        float val = *(float*)(ptr + off);
                        if (val > 0.1f && val < 1000.0f) {
                            fprintf(f, "  [0x%03X] = %.2f (possible speed/velocity)\n", off, val);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                // Scan for IsMoving/IsDashing flags (0 or 1)
                fprintf(f, "  Scanning for bool flags...\n");
                for (int off = 0x310; off < 0x400; off += 4) {
                    __try {
                        int val = *(int*)(ptr + off);
                        if (val == 0 || val == 1) {
                            fprintf(f, "  [0x%03X] = %d (possible bool)\n", off, val);
                        }
                    } __except(EXCEPTION_EXECUTE_HANDLER) {}
                }
                
                // Check known offsets
                fprintf(f, "  Checking known AiManager offsets...\n");
                __try {
                    float* targetPos = (float*)(ptr + 0x34);
                    fprintf(f, "  TargetPos[0x34] = (%.1f, %.1f, %.1f)\n", targetPos[0], targetPos[1], targetPos[2]);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  TargetPos[0x34] = CRASH\n"); }
                
                __try {
                    float* velocity = (float*)(ptr + 0x318);
                    fprintf(f, "  Velocity[0x318] = (%.1f, %.1f, %.1f)\n", velocity[0], velocity[1], velocity[2]);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  Velocity[0x318] = CRASH\n"); }
                
                __try {
                    int isMoving = *(int*)(ptr + 0x31C);
                    fprintf(f, "  IsMoving[0x31C] = %d\n", isMoving);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  IsMoving[0x31C] = CRASH\n"); }
                
                __try {
                    float* startPath = (float*)(ptr + 0x330);
                    fprintf(f, "  StartPath[0x330] = (%.1f, %.1f, %.1f)\n", startPath[0], startPath[1], startPath[2]);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  StartPath[0x330] = CRASH\n"); }
                
                __try {
                    float* targetPosition = (float*)(ptr + 0x33C);
                    fprintf(f, "  TargetPosition[0x33C] = (%.1f, %.1f, %.1f)\n", targetPosition[0], targetPosition[1], targetPosition[2]);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  TargetPosition[0x33C] = CRASH\n"); }
                
                __try {
                    int segmentsCount = *(int*)(ptr + 0x350);
                    fprintf(f, "  SegmentsCount[0x350] = %d\n", segmentsCount);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  SegmentsCount[0x350] = CRASH\n"); }
                
                __try {
                    float dashSpeed = *(float*)(ptr + 0x360);
                    fprintf(f, "  DashSpeed[0x360] = %.2f\n", dashSpeed);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  DashSpeed[0x360] = CRASH\n"); }
                
                __try {
                    int isDashing = *(int*)(ptr + 0x384);
                    fprintf(f, "  IsDashing[0x384] = %d\n", isDashing);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  IsDashing[0x384] = CRASH\n"); }
                
                __try {
                    float* serverPos = (float*)(ptr + 0x474);
                    fprintf(f, "  ServerPos[0x474] = (%.1f, %.1f, %.1f)\n", serverPos[0], serverPos[1], serverPos[2]);
                } __except(EXCEPTION_EXECUTE_HANDLER) { fprintf(f, "  ServerPos[0x474] = CRASH\n"); }
            } else {
                fprintf(f, "  Invalid/null pointer\n");
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "  CRASH reading pointer\n");
        }
        fprintf(f, "\n");
    }
    
    // =========================================================================
    // DETAILED SCAN OF VERIFIED AIMANAGER (0x3108)
    // =========================================================================
    fprintf(f, "\n=== DETAILED AIMANAGER SCAN (0x3108) ===\n");
    
    __try {
        uint64_t aiManager = *(uint64_t*)(localPlayer + 0x3108);
        fprintf(f, "AiManager PTR: 0x%llX\n\n", aiManager);
        
        if (aiManager > 0x10000 && aiManager < 0x7FFFFFFFFFFF) {
            // Verified positions
            fprintf(f, "--- VERIFIED POSITIONS ---\n");
            __try {
                float* startPath = (float*)(aiManager + 0x1E0);
                fprintf(f, "StartPath[0x1E0] = (%.1f, %.1f, %.1f)\n", startPath[0], startPath[1], startPath[2]);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            __try {
                float* targetPos = (float*)(aiManager + 0x23C);
                fprintf(f, "TargetPos[0x23C] = (%.1f, %.1f, %.1f)\n", targetPos[0], targetPos[1], targetPos[2]);
            } __except(EXCEPTION_EXECUTE_HANDLER) {}
            
            // Scan for Velocity (Vec3 with small values, changes when moving)
            fprintf(f, "\n--- SCANNING FOR VELOCITY (Vec3 near 0x1E0-0x300) ---\n");
            fprintf(f, "Move your character and compare values!\n");
            for (int off = 0x1C0; off < 0x300; off += 4) {
                __try {
                    float x = *(float*)(aiManager + off);
                    float y = *(float*)(aiManager + off + 4);
                    float z = *(float*)(aiManager + off + 8);
                    
                    // Velocity is typically small non-zero when moving, zero when stopped
                    if ((x != 0 || z != 0) && fabsf(x) < 1000 && fabsf(z) < 1000 && fabsf(y) < 100) {
                        fprintf(f, "[0x%03X] (%.2f, %.2f, %.2f)\n", off, x, y, z);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Scan for IsMoving/IsDashing (int 0 or 1)
            fprintf(f, "\n--- SCANNING FOR BOOL FLAGS (0 or 1) ---\n");
            for (int off = 0x200; off < 0x300; off += 4) {
                __try {
                    int val = *(int*)(aiManager + off);
                    if (val == 0 || val == 1) {
                        fprintf(f, "[0x%03X] = %d\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Scan for SegmentsCount (small int 1-10)
            fprintf(f, "\n--- SCANNING FOR SEGMENT COUNT (1-20) ---\n");
            for (int off = 0x200; off < 0x300; off += 4) {
                __try {
                    int val = *(int*)(aiManager + off);
                    if (val >= 1 && val <= 20) {
                        fprintf(f, "[0x%03X] = %d\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // Scan for MoveSpeed (float ~300-500 range)
            fprintf(f, "\n--- SCANNING FOR MOVESPEED (300-600) ---\n");
            for (int off = 0x0; off < 0x300; off += 4) {
                __try {
                    float val = *(float*)(aiManager + off);
                    if (val > 250 && val < 700) {
                        fprintf(f, "[0x%03X] = %.1f\n", off, val);
                    }
                } __except(EXCEPTION_EXECUTE_HANDLER) {}
            }
            
            // RAW HEX DUMP around position offsets
            fprintf(f, "\n--- RAW HEX DUMP (0x1C0 - 0x280) ---\n");
            for (int row = 0x1C0; row < 0x280; row += 0x10) {
                fprintf(f, "[0x%03X] ", row);
                for (int col = 0; col < 0x10; col += 4) {
                    __try {
                        uint32_t val = *(uint32_t*)(aiManager + row + col);
                        fprintf(f, "%08X ", val);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        fprintf(f, "???????? ");
                    }
                }
                fprintf(f, "\n");
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    
    // =========================================================================
    // RAW HEX DUMP of best candidate
    // =========================================================================
    fprintf(f, "\n=== RAW HEX DUMP (AiManager @ 0x41A8) ===\n");
    __try {
        uint64_t aiManager = *(uint64_t*)(localPlayer + 0x41A8);
        if (aiManager > 0x10000 && aiManager < 0x7FFFFFFFFFFF) {
            for (int row = 0; row < 0x200; row += 0x10) {
                fprintf(f, "[0x%03X] ", row);
                for (int col = 0; col < 0x10; col += 4) {
                    __try {
                        uint32_t val = *(uint32_t*)(aiManager + row + col);
                        fprintf(f, "%08X ", val);
                    } __except(EXCEPTION_EXECUTE_HANDLER) {
                        fprintf(f, "???????? ");
                    }
                }
                fprintf(f, "\n");
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {}
    
    fclose(f);
}

// ============================================================================
// AI MANAGER SCAN WITH IDA DECRYPTION (NEW - Dec 2024)
// Uses exact reproduction of sub_289E40 decryption algorithm
// Offset: oObjAiManagerObf = 0x36F0
// ============================================================================
void AiManagerScanWithIDADecrypt() {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = 0;
    
    __try { localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer); } 
    __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!localPlayer) return;
    
    // Get player position for reference
    char posX[32], posY[32], posZ[32];
    __try {
        sprintf(posX, "%.1f", *(float*)(localPlayer + 0x254));
        sprintf(posY, "%.1f", *(float*)(localPlayer + 0x258));
        sprintf(posZ, "%.1f", *(float*)(localPlayer + 0x25C));
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        strcpy(posX, "N/A"); strcpy(posY, "N/A"); strcpy(posZ, "N/A");
    }
    
    // Call the comprehensive scan function
    AiManagerScan::ScanAiManagerOffsets(localPlayer, posX, posY, posZ);
}

// ============================================================================
// AI MANAGER NAVGRID-BASED SCAN (NEW - Jan 2025)
// Uses NavGrid (0x1D32A80) as reference to find path-related offsets:
// - oObjAiMgrCurrentSegment
// - oObjAiMgrNavArray
// - oObjAiMgrSegmentsCount
// - oObjAiMgrMoveVec3
// - oObjAiMgrServerPos
// ============================================================================
void RunAiManagerNavGridScan() {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = 0;
    
    __try { localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer); } 
    __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!localPlayer) return;
    
    // Call the NavGrid-based scan function
    AiManagerNavGridScan::ScanAiManagerWithNavGrid(localPlayer);
}

// ============================================================================
// SCAN OBFUSCATED OFFSET - Tìm offset obfuscated structure trực tiếp trong game
// ============================================================================
void ScanObfuscatedAiManagerOffset() {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = 0;
    
    __try { localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer); } 
    __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!localPlayer) return;
    
    // Call the scan function
    AiManagerScan::ScanObfuscatedOffset(localPlayer);
}

// ============================================================================
// SCAN HasPath IDLE vs MOVING - So sánh để tìm offset chính xác
// ============================================================================
void ScanHasPathIdleVsMoving(bool isIdle) {
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = 0;
    
    __try { localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer); } 
    __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!localPlayer) return;
    
    // Call the scan function
    AiManagerScan::ScanHasPathIdleVsMoving(localPlayer, isIdle);
}

// ============================================================================
// AI MANAGER OBFUSCATION DUMPER - Compare Direct vs Obfuscated methods
// ============================================================================
// ============================================================================
// AI MANAGER ANALYZER - Deep scan of Direct Pointer (0x3108)
// ============================================================================
// ============================================================================
// CONTINUOUS PATH LOGGER - Track movement offsets in real-time
// ============================================================================
// ============================================================================
// CONTINUOUS PATH LOGGER & PATTERN SCANNER
// ============================================================================
// ============================================================================
// AI MANAGER DIFF SCANNER - Find offsets by comparing IDLE vs MOVING states
// ============================================================================
// Usage:
// 1. Stand still -> Click Button -> Logs "Captured IDLE state..."
// 2. Move char -> Click Button -> Logs "COMPARING..." and shows differences
// ============================================================================

struct AiManagerSnapshot {
    bool valid;
    uint8_t data[0x600]; // Capture 0x600 bytes
};

static AiManagerSnapshot idleSnapshot = { false };

void DumpAiManagerObfuscated() { // Keep name for Menu compatibility
    uint64_t base = (uint64_t)GetModuleHandle(NULL);
    uint64_t localPlayer = 0;
    
    __try { localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer); } 
    __except(EXCEPTION_EXECUTE_HANDLER) { return; }
    if (!localPlayer) return;

    uint64_t aiManager = 0;
    __try { aiManager = *(uint64_t*)(localPlayer + 0x3108); } 
    __except(EXCEPTION_EXECUTE_HANDLER) {}
    
    if (!aiManager || aiManager < 0x10000) {
        // Try to log error
        FILE* f = fopen("aimanager_diff.txt", "a");
        if (f) { fprintf(f, "[ERROR] AiManager invalid (0x3108)\n"); fclose(f); }
        return;
    }

    FILE* f = fopen("aimanager_diff.txt", "a");
    if (!f) return;

    if (!idleSnapshot.valid) {
        // STEP 1: CAPTURE IDLE
        fprintf(f, "\n=== STEP 1: CAPTURED IDLE SNAPSHOT ===\n");
        fprintf(f, "Action: Now MOVE your character and click DUMP again!\n");
        
        __try {
            for (int i = 0; i < 0x600; i++) {
                idleSnapshot.data[i] = *(uint8_t*)(aiManager + i);
            }
            idleSnapshot.valid = true;
            fprintf(f, "Captured 0x600 bytes from 0x%llX\n", aiManager);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            fprintf(f, "[CRASH] Failed to read AiManager\n");
            idleSnapshot.valid = false;
        }
    } else {
        // STEP 2: COMPARE WITH MOVING
        fprintf(f, "\n=== STEP 2: COMPRARE IDLE vs CURRENT ===\n");
        fprintf(f, "Showing ONLY CHANGED values (candidates for Moving/Dashing/Pos)\n");
        fprintf(f, "----------------------------------------------------------\n");
        fprintf(f, "Offset | Type  | Idle Value    | Current Value | Diff\n");
        fprintf(f, "----------------------------------------------------------\n");
        
        int changeCount = 0;
        
        __try {
            for (int i = 0; i < 0x600; i += 4) { // Check 4-byte aligned mostly
                // Read current
                uint32_t curVal = *(uint32_t*)(aiManager + i);
                float curFloat = *(float*)(aiManager + i);
                
                // Read idle
                uint32_t idleVal = *(uint32_t*)(&idleSnapshot.data[i]);
                float idleFloat = *(float*)(&idleSnapshot.data[i]);
                
                if (curVal != idleVal) {
                    changeCount++;
                    
                    // Format output
                    fprintf(f, "0x%03X  | ", i);
                    
                    // Auto-detect type (Int vs Float)
                    bool looksLikeFloat = (curFloat > -100000.0f && curFloat < 100000.0f) && 
                                          (fabs(curFloat) > 0.001f);
                    
                    if (looksLikeFloat) {
                        fprintf(f, "FLOAT | %-13.3f | %-13.3f | %.3f", idleFloat, curFloat, curFloat - idleFloat);
                    } else {
                        fprintf(f, "INT   | %-13d | %-13d | %d", idleVal, curVal, curVal - idleVal);
                    }
                    
                    // Annotations
                    if (i == 0x1E0) fprintf(f, " (Pos?)");
                    if (i == 0x23C) fprintf(f, " (Target?)");
                    if (i == 0x368) fprintf(f, " (Speed?)");
                    
                    // Check for Bool flags (0->1 or 1->0) using Byte check
                    if (*(uint8_t*)(aiManager + i) != idleSnapshot.data[i]) {
                         uint8_t bCur = *(uint8_t*)(aiManager + i);
                         uint8_t bIdle = idleSnapshot.data[i];
                         if ((bCur == 0 || bCur == 1) && (bIdle == 0 || bIdle == 1)) {
                             fprintf(f, " [BOOL CHANGE: %d -> %d]", bIdle, bCur);
                         }
                    }
                    
                    fprintf(f, "\n");
                }
            }
        } __except(EXCEPTION_EXECUTE_HANDLER) {
             fprintf(f, "[CRASH] Comparison failed\n");
        }
        
        fprintf(f, "----------------------------------------------------------\n");
        fprintf(f, "Total Changes: %d offsets\n", changeCount);
        fprintf(f, "==========================================================\n\n");
        
        // Reset so user can try again
        idleSnapshot.valid = false;
        fprintf(f, "Snapshot RESET. Stand still and click again to restart process.\n");
    }

    fclose(f);
}

namespace DATA {
    uint64_t ModuleBase = 0;
    float* pViewMatrix = nullptr;
    float* pProjMatrix = nullptr;
    uint64_t LocalPlayer = 0;
}

// Global Vars for Hook
Present oPresent;
HWND window = NULL;
WNDPROC oWndProc;
ID3D11Device* pDevice = NULL;
ID3D11DeviceContext* pContext = NULL;
ID3D11RenderTargetView* mainRenderTargetView;

extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Forward declarations
bool IsLeagueFocused();
bool IsChatOpen();
Vector3 GetMouseWorldPosRaw();
float GetAttackDelay(uint64_t localPlayer);
float GetAttackWindup(uint64_t localPlayer);
float GetPing();

void InitImGui() {
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;
    ImGui_ImplWin32_Init(window);
    ImGui_ImplDX11_Init(pDevice, pContext);
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    // Menu Toggle
    if (uMsg == WM_KEYDOWN && wParam == VK_INSERT) {
        Menu::menuOpen = !Menu::menuOpen;
    }

    if (Menu::menuOpen) {
        if (ImGui_ImplWin32_WndProcHandler(hWnd, uMsg, wParam, lParam))
            return true;
    }

    // Manual Target Selection - Call TargetSelector::OnLeftClick()
    // WM_LBUTTONDOWN = 0x0201 = 513
    // Note: Also handled in hkPresent with GetAsyncKeyState for reliability
    if (uMsg == WM_LBUTTONDOWN && !Menu::menuOpen) {
        SDK::TargetSelector::OnLeftClick();
    }
    
    return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

bool init = false;

// Render Hook
// ============================================================================
// GLOBAL HELPERS FOR SPELL READING
// ============================================================================

static Vector3 ReadVec3(uint64_t addr) {
    Vector3 v;
    v.x = *(float*)(addr);
    v.y = *(float*)(addr + 4);
    v.z = *(float*)(addr + 8);
    return v;
}

static void WriteVec3(uint64_t addr, Vector3 v) {
    *(float*)(addr) = v.x;
    *(float*)(addr + 4) = v.y;
    *(float*)(addr + 8) = v.z;
}

HRESULT __stdcall hkPresent(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    if (!init) {
        if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void**)& pDevice))) {
            pDevice->GetImmediateContext(&pContext);
            DXGI_SWAP_CHAIN_DESC sd;
            pSwapChain->GetDesc(&sd);
            window = sd.OutputWindow;
            
            // Get Screen Size
            RECT clientRect;
            if (GetClientRect(window, &clientRect)) {
                Render::g_screenWidth = clientRect.right - clientRect.left;
                Render::g_screenHeight = clientRect.bottom - clientRect.top;
            }

            ID3D11Texture2D* pBackBuffer;
            pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)& pBackBuffer);
            pDevice->CreateRenderTargetView(pBackBuffer, NULL, &mainRenderTargetView);
            pBackBuffer->Release();
            
            oWndProc = (WNDPROC)SetWindowLongPtr(window, GWLP_WNDPROC, (LONG_PTR)WndProc);
            InitImGui();
            init = true;
        } else {
            return oPresent(pSwapChain, SyncInterval, Flags);
        }
    }

    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    
    // ESP Render Loop
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    
    // Get Local Player once for frame
    auto local = SDK::ObjectManager::GetLocalPlayer();
    
    // UPDATE GUIDED SCAN (AiManager offset scanner state machine)
    if (local && AiManagerScan::g_scanState.isActive) {
        AiManagerScan::UpdateGuidedScan(local->Address);
    }
    
    // UPDATE NAVGRID GUIDED SCAN (NavGrid-based offset scanner)
    if (local && AiManagerNavGridScan::g_navGridScanState.isActive) {
        AiManagerNavGridScan::UpdateNavGridGuidedScan(local->Address);
    }
    
    // TARGET SELECTOR CLICK HANDLER (using GetAsyncKeyState for reliability)
    static bool wasLMBPressed = false;
    bool isLMBPressed = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    
    // Detect click (rising edge)
    if (isLMBPressed && !wasLMBPressed && !Menu::menuOpen) {
        SDK::TargetSelector::OnLeftClick();
    }
    wasLMBPressed = isLMBPressed;
    
    // DEBUG: Log ESP counts once per 2 seconds
    static int logTimer = 0;
    if (logTimer++ > 200) { // Approx 2-3s at 60fps or dependent on hook speed
        logTimer = 0;
        std::ofstream log("orbwalker_debug.txt", std::ios_base::app);
        if (log.is_open()) {
            int heroCount = SDK::ObjectManager::GetHeroes().size();
            int minionCount = SDK::ObjectManager::GetMinions().size();
            log << "[ESP DEBUG] Heroes: " << heroCount << " | Minions: " << minionCount << std::endl;
            if (local) {
                log << "[LOCAL DEBUG] Range: " << local->GetAttackRange() 
                    << " | Bounding: " << local->GetBoundingRadius() << std::endl;
            } else {
                log << "[LOCAL DEBUG] LocalPlayer is NULL" << std::endl;
            }
            log.close();
        }
    }

    if (Menu::drawHeroes || Menu::drawMinions) {
        // Prepare local player for team check
        // auto local = SDK::ObjectManager::GetLocalPlayer(); // Removed
        
        if (local) {
            // Heroes
            if (Menu::drawHeroes) {
                auto heroes = SDK::ObjectManager::GetHeroes();
                for (auto hero : heroes) {
                    if (!hero->IsValid() || hero->IsDead() || !hero->IsVisible()) continue;
                    if (hero->Address == local->Address) continue; // Skip self

                    Vector3 pos = hero->GetPosition();
                    Vector2 screenPos = Render::WorldToScreen(pos);
                    
                    if (screenPos.x > 0 && screenPos.y > 0 && screenPos.x < Render::g_screenWidth && screenPos.y < Render::g_screenHeight) {
                        if (hero->GetTeam() != local->GetTeam()) {
                            // Enemy
                            draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 30.0f, IM_COL32(255, 0, 0, 255), 32, 2.0f);
                             
                            if (Menu::drawEnemyRange) {
                                float enemyRange = hero->GetAttackRange() + hero->GetBoundingRadius() + 65.0f;
                                Render::DrawCircle3D(draw, pos, 64, enemyRange, ImColor(255, 50, 50, 100), 2.0f);
                            }

                             if (Menu::showObjectNames) {
                                std::string text = hero->GetName() + " [AD: " + std::to_string((int)hero->GetAttackDamage()) + "]";
                                draw->AddText(ImVec2(screenPos.x, screenPos.y - 10), IM_COL32(255, 255, 255, 255), text.c_str());
                            }
                        } else {
                            // Ally
                             // draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 30.0f, IM_COL32(0, 255, 0, 255), 32, 2.0f);
                        }
                    }
                }
            }

            // Minions
            if (Menu::drawMinions) {
                 auto minions = SDK::ObjectManager::GetMinions();
                 for (auto minion : minions) {
                      if (!minion->IsValid() || minion->IsDead() || !minion->IsVisible()) continue;
                      
                      Vector3 pos = minion->GetPosition();
                      Vector2 screenPos = Render::WorldToScreen(pos);
                      
                      if (screenPos.x > 0 && screenPos.y > 0 && screenPos.x < Render::g_screenWidth && screenPos.y < Render::g_screenHeight) {
                           if (minion->GetTeam() != local->GetTeam()) {
                                draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 15.0f, IM_COL32(255, 255, 0, 255));
                           }
                      }
                 }
            }
            
            // Draw Range - RealAttackRange = attackRange + myBoundingRadius (NO extra 65!)
            if (Menu::drawRange) {
                 Vector3 pos = local->GetPosition();
                 float rangeWorld = local->GetRealAttackRange(); // Correct formula from leagueoflegends-master
                 Render::DrawCircle3D(draw, pos, 64, rangeWorld, ImColor(0, 255, 255, 150), 2.0f);
            }
        }
    }
    
    // Orbwalker Drawing (Moved from ThreadMatrix)
    if (Menu::orbwalkerEnabled && local) {
        if (local->IsValid()) {
            Vector3 localPos = local->GetPosition();
            float attackRange = local->GetRealAttackRange(); // Correct: attackRange + myBoundingRadius
            
            // Draw attack range
            if (Menu::drawAttackRange) {
                Render::DrawCircle3D(draw, localPos, 64, attackRange, ImColor(255, 255, 255, 100), 2.0f);
            }
            
            // Draw killable minions
            if (Menu::drawKillableMinions) {
                float myDamage = local->GetAttackDamage();
                auto minions = SDK::ObjectManager::GetAllMinions();
                
                for (auto* minion : minions) {
                    if (!minion || minion->GetTeam() == local->GetTeam()) continue;
                    if (minion->IsDead() || !minion->IsVisible()) continue;
                    
                    float hp = minion->GetHealth();
                    if (hp <= myDamage && hp > 0) {
                        Vector3 minionPos = minion->GetPosition();
                        Vector2 screenPos = Render::WorldToScreen(minionPos);
                        
                        if (screenPos.x > 0 && screenPos.y > 0 && screenPos.x < Render::g_screenWidth && screenPos.y < Render::g_screenHeight) {
                            draw->AddCircleFilled(ImVec2(screenPos.x, screenPos.y), 8.0f, ImColor(0, 255, 0, 200));
                            draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 10.0f, ImColor(255, 255, 255, 255), 32, 2.0f);
                        }
                    }
                }
                
                for (auto* minion : minions) delete minion;
            }
            
            // ============================================================================
            // Draw Selected Target (NewTargetSelector.cs: OnDraw + OnLight)
            // ============================================================================
            if (Menu::tsDrawSelected && SDK::TargetSelector::SelectedTarget != 0) {
                SDK::GameObject* selectedTarget = new SDK::GameObject(SDK::TargetSelector::SelectedTarget);
                
                if (selectedTarget && selectedTarget->IsValid() && !selectedTarget->IsDead() && selectedTarget->IsVisible()) {
                    Vector3 targetPos = selectedTarget->GetPosition();
                    Vector2 screenPos = Render::WorldToScreen(targetPos);
                    float boundingRadius = selectedTarget->GetBoundingRadius();
                    
                    if (screenPos.x > 0 && screenPos.y > 0 && 
                        screenPos.x < Render::g_screenWidth && screenPos.y < Render::g_screenHeight) {
                        
                        // Get color from menu (RGB 0-1 to 0-255)
                        ImU32 circleColor = IM_COL32(
                            (int)(Menu::tsDrawColor[0] * 255),
                            (int)(Menu::tsDrawColor[1] * 255),
                            (int)(Menu::tsDrawColor[2] * 255),
                            255
                        );
                        
                        // Draw 3D circle around target (on ground)
                        Render::DrawCircle3D(draw, targetPos, 64, boundingRadius + 20.0f, circleColor, 3.0f);
                        
                        // Draw outer glow circle (larger, semi-transparent)
                        ImU32 glowColor = IM_COL32(
                            (int)(Menu::tsDrawColor[0] * 255),
                            (int)(Menu::tsDrawColor[1] * 255),
                            (int)(Menu::tsDrawColor[2] * 255),
                            100
                        );
                        Render::DrawCircle3D(draw, targetPos, 64, boundingRadius + 40.0f, glowColor, 2.0f);
                        
                        // Draw screen-space indicator (2D circle on screen)
                        draw->AddCircle(ImVec2(screenPos.x, screenPos.y), 40.0f, circleColor, 32, 3.0f);
                        
                        // Highlight effect (pulsing inner circle)
                        if (Menu::tsHighlightSelected) {
                            static float pulse = 0.0f;
                            pulse += 0.05f;
                            if (pulse > 3.14159f * 2.0f) pulse = 0.0f;
                            float pulseSize = 35.0f + sin(pulse) * 8.0f;
                            
                            ImU32 highlightColor = IM_COL32(
                                (int)(Menu::tsDrawColor[0] * 255),
                                (int)(Menu::tsDrawColor[1] * 255),
                                (int)(Menu::tsDrawColor[2] * 255),
                                150
                            );
                            draw->AddCircle(ImVec2(screenPos.x, screenPos.y), pulseSize, highlightColor, 32, 2.0f);
                        }
                        
                        // Draw "TARGET" text above
                        ImVec2 textSize = ImGui::CalcTextSize("TARGET");
                        draw->AddText(ImVec2(screenPos.x - textSize.x / 2, screenPos.y - 60), circleColor, "TARGET");
                    }
                }
                
                delete selectedTarget;
            }
            
            // Draw Hold Position - Removed per user request
            // if (Menu::drawHoldPosition) {
            //     Render::DrawCircle3D(draw, localPos, 64, 120.0f, ImColor(255, 255, 0, 100), 2.0f);
            // }
            
            // ============================================================================
            // MISSILE DRAWING DEBUG - Shows enemy skillshots, minion/turret attacks
            // ============================================================================
            if (Menu::drawMissiles || Menu::drawMinionMissiles || Menu::drawTurretMissiles || Menu::drawChampionMissiles) {
                auto missiles = SDK::MissileManager::GetMissiles();
                
                // Draw debug info panel in top-left corner
                float debugY = 150.0f;
                char debugText[256];
                sprintf_s(debugText, "Active Missiles: %d", (int)missiles.size());
                draw->AddText(ImVec2(10, debugY), IM_COL32(255, 255, 255, 255), debugText);
                debugY += 20.0f;
                
                int missileIdx = 0;
                for (auto missile : missiles) {
                    if (!missile || !missile->IsValid()) continue;
                    
                    std::string spellName = missile->GetSpellName();
                    uint32_t netId = missile->GetNetId();
                    
                    bool isMinion = missile->IsFromMinion();
                    bool isTurret = missile->IsFromTurret();
                    bool isChampion = missile->IsChampionSpell();
                    
                    // Determine type and color
                    const char* typeStr = "UNKNOWN";
                    ImU32 textColor = IM_COL32(255, 255, 255, 255);
                    
                    if (isMinion) {
                        typeStr = "MINION";
                        textColor = IM_COL32(255, 255, 0, 255);
                    } else if (isTurret) {
                        typeStr = "TURRET";
                        textColor = IM_COL32(255, 0, 0, 255);
                    } else if (isChampion) {
                        typeStr = "CHAMPION";
                        textColor = IM_COL32(0, 255, 255, 255);
                    }
                    
                    // Check filter
                    bool shouldShow = Menu::drawMissiles;
                    if (Menu::drawMinionMissiles && isMinion) shouldShow = true;
                    if (Menu::drawTurretMissiles && isTurret) shouldShow = true;
                    if (Menu::drawChampionMissiles && isChampion) shouldShow = true;
                    
                    if (shouldShow && missileIdx < 10) {
                        sprintf_s(debugText, "[%d] %s: %s (0x%X)", missileIdx, typeStr, 
                            spellName.empty() ? "???" : spellName.c_str(), netId);
                        draw->AddText(ImVec2(10, debugY), textColor, debugText);
                        debugY += 16.0f;
                        missileIdx++;
                    }
                    
                    // Draw missile trajectory on map
                    Vector3 startPos = missile->GetStartPosition();
                    Vector3 endPos = missile->GetEndPosition();
                    Vector3 currentPos = missile->GetPosition();
                    
                    // Choose color based on missile type
                    ImU32 lineColor = IM_COL32(255, 255, 255, 180);
                    if (isMinion) lineColor = IM_COL32(255, 255, 0, 120);  // Yellow for minions
                    else if (isTurret) lineColor = IM_COL32(255, 0, 0, 120);  // Red for turrets
                    else if (isChampion) lineColor = IM_COL32(0, 255, 255, 150);  // Cyan for champion spells
                    
                    // Draw line from start to end position
                    if (startPos.x > 0 && endPos.x > 0) {
                        Vector2 startScreen = Render::WorldToScreen(startPos);
                        Vector2 endScreen = Render::WorldToScreen(endPos);
                        
                        if (startScreen.x > 0 && startScreen.y > 0 && endScreen.x > 0 && endScreen.y > 0) {
                            // Draw trajectory line
                            draw->AddLine(ImVec2(startScreen.x, startScreen.y), 
                                        ImVec2(endScreen.x, endScreen.y), 
                                        lineColor, 3.0f);
                            
                            // Draw current position as a circle
                            if (currentPos.x > 0) {
                                Vector2 currentScreen = Render::WorldToScreen(currentPos);
                                if (currentScreen.x > 0 && currentScreen.y > 0) {
                                    draw->AddCircleFilled(ImVec2(currentScreen.x, currentScreen.y), 5.0f, lineColor);
                                }
                            }
                        }
                    }
                }
                
                SDK::MissileManager::FreeMissiles(missiles);
            }
            
            // Draw Active Mode Text
            std::string modeText = "";
            bool isSpace = (GetAsyncKeyState(VK_SPACE) & 0x8000) && IsLeagueFocused();
            bool isV = (GetAsyncKeyState(0x56) & 0x8000) && IsLeagueFocused();
            bool isX = (GetAsyncKeyState(0x58) & 0x8000) && IsLeagueFocused();
            bool isC = (GetAsyncKeyState(0x43) & 0x8000) && IsLeagueFocused();
            bool isZ = (GetAsyncKeyState(0x5A) & 0x8000) && IsLeagueFocused();
            
            // Caps Lock / LMB State
            static bool capsLockToggle = false;
            // Simple toggle check (same as thread logic but here just for display)
            // Ideally we should share this state, but for display valid is ok
            if (GetAsyncKeyState(VK_CAPITAL) & 0x0001) capsLockToggle = !capsLockToggle;
            bool lmbHeld = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) && IsLeagueFocused();
            bool modifierActive = capsLockToggle || lmbHeld;
            
            if (isSpace) modeText = "COMBO";
            else if (isZ) modeText = "FLEE";
            else if (isV) modeText = "LANE CLEAR";
            else if (isX) modeText = "LAST HIT";
            else if (isC) modeText = "HARASS";
            
            if (!modeText.empty()) {
                 Vector2 screenPos = Render::WorldToScreen(localPos);
                 // Draw slightly below character or above?
                 // Typically below feet or above head. Let's do below feet.
                 if (screenPos.x > 0 && screenPos.y > 0) {
                      // Centered text
                      ImVec2 textSize = ImGui::CalcTextSize(modeText.c_str());
                      draw->AddText(ImVec2(screenPos.x - textSize.x / 2, screenPos.y + 20), IM_COL32(255, 255, 0, 255), modeText.c_str());
                }
           }
        }
    }
    
    // ============================================================================
    // SPELL DEBUG OVERLAY - Show spell info in real-time
    // ============================================================================
    if (showSpellDebugOverlay && local) {
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        uint64_t spellBook = local->Address + Offset::oObjSpellBook;
        
        ImGui::SetNextWindowPos(ImVec2(10, 300), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(280, 200), ImGuiCond_FirstUseEver);
        ImGui::Begin("Spell Debug", &showSpellDebugOverlay, ImGuiWindowFlags_NoCollapse);
        
        const char* slotNames[] = {"Q", "W", "E", "R"};
        
        for (int slot = 0; slot < 4; slot++) {
            // Read spell data (no __try in C++ with objects)
            uint64_t spellSlot = 0;
            int level = 0;
            float cooldown = 0, totalCooldown = 0, remaining = 0;
            bool valid = false;
            
            // Safe read without __try (just check pointers)
            if (spellBook) {
                spellSlot = *(uint64_t*)(spellBook + slot * 8 + Offset::oObjSpellBookSpellSlot);
                if (spellSlot && spellSlot > 0x10000) {
                    level = *(int*)(spellSlot + Offset::oSpellSlotLevel);
                    cooldown = *(float*)(spellSlot + Offset::oSpellSlotCooldown);
                    totalCooldown = *(float*)(spellSlot + Offset::oSpellSlotTotalCooldown);
                    remaining = cooldown > gameTime ? cooldown - gameTime : 0.0f;
                    valid = true;
                }
            }
            
            if (!valid || !spellSlot) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s] NULL", slotNames[slot]);
            } else if (remaining <= 0.0f && level > 0) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%s] Lv%d READY", slotNames[slot], level);
            } else if (level > 0) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[%s] Lv%d CD: %.1fs / %.1fs", 
                    slotNames[slot], level, remaining, totalCooldown);
            } else {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s] Not Learned", slotNames[slot]);
            }
        }
        
        ImGui::Separator();
        ImGui::Text("GameTime: %.1f", gameTime);
        ImGui::Text("SpellBook: 0x%llX", spellBook);
        
        ImGui::End();
    }

    // ============================================================================
    // BUFFMANAGER DEBUG OVERLAY - Shows active buffs for local player and enemies
    // ============================================================================
    if (showBuffManagerDebug && local) {
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        ImGui::SetNextWindowPos(ImVec2(10, 520), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 350), ImGuiCond_FirstUseEver);
        ImGui::Begin("BuffManager Debug", &showBuffManagerDebug, ImGuiWindowFlags_NoCollapse);
        
        // Local Player Buffs
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "=== LOCAL PLAYER BUFFS ===");
        ImGui::Separator();
        
        SDK::BuffManager localBuffMgr(local->Address);
        
        // --- ADDED DEBUG INFO ---
        ImGui::Text("BuffManager Addr: %llX", localBuffMgr.GetBuffManagerAddress());
        uint64_t arrStart = localBuffMgr.GetRawArrayStart();
        uint64_t arrEnd = localBuffMgr.GetRawArrayEnd();
        ImGui::Text("Array Start: %llX", arrStart);
        ImGui::Text("Array End:   %llX", arrEnd);
        
        if (arrStart && arrEnd && arrEnd > arrStart) {
             size_t count = (arrEnd - arrStart) / sizeof(uint64_t);
             ImGui::Text("Calculated Count: %llu", count);
        } else {
             ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid Array Pointers!");
        }
        ImGui::Separator();
        // -----------------------
        
        // Debug Button - Safe offset discovery
        if (ImGui::Button("Dump Buff Offsets to File")) {
            BuffManagerDebug();
            ImGui::OpenPopup("BuffDumpComplete");
        }
        if (ImGui::BeginPopup("BuffDumpComplete")) {
            ImGui::Text("Offset scan complete!");
            ImGui::Text("Check buff_debug.txt in game folder.");
            ImGui::EndPopup();
        }
        ImGui::Separator();
        
        // CC Status - DISABLED (uses crashy buff iteration)
        ImGui::Text("CC Status:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[Disabled - offset verification needed]");
        
        // Invulnerability Status - DISABLED
        ImGui::Text("Invuln Status:");
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[Disabled]");
        
        // Active Buffs List - DISABLED TO PREVENT CRASH
        // Use "Dump Buff Offsets to File" button for safe buff inspection
        if (ImGui::CollapsingHeader("Active Buffs (Local) [DISABLED - Use File Debug]")) {
            ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Buff iteration disabled to prevent crash.");
            ImGui::Text("Use 'Dump Buff Offsets to File' button above.");
            ImGui::Text("Check buff_debug.txt in game folder for results.");
            
            // Only show array info - no iteration
            uint64_t buffMgr = local->Address + Offset::oObjBuffManager;
            uint64_t arrStart = *(uint64_t*)(buffMgr + 0x18);
            uint64_t arrEnd = *(uint64_t*)(buffMgr + 0x20);
            ImGui::Text("Array: 0x%llX - 0x%llX", arrStart, arrEnd);
            if (arrEnd > arrStart) {
                size_t count = (arrEnd - arrStart) / sizeof(uint64_t);
                ImGui::Text("Calculated entries: %llu", count);
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "=== ENEMY HEROES BUFFS ===");
        ImGui::Separator();
        
        // Enemy Heroes Buffs - DISABLED (uses crashy buff iteration)
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "[Disabled - buff name reading causes crash]");
        ImGui::Text("Use 'Dump Buff Offsets to File' for safe debug.");
        
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "GameTime: %.1f", gameTime);
        
        ImGui::End();
    }
    
    // ============================================================================
    // SPELLDATA DEBUG OVERLAY - Shows spell info for local player and enemies
    // ============================================================================
    if (showSpellDataDebug && local) {
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        ImGui::SetNextWindowPos(ImVec2(420, 520), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(380, 350), ImGuiCond_FirstUseEver);
        ImGui::Begin("SpellData Debug", &showSpellDataDebug, ImGuiWindowFlags_NoCollapse);
        
        // Local Player Spells
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "=== LOCAL PLAYER SPELLS ===");
        ImGui::Separator();
        
        SDK::SpellBook localSpellBook(local->Address + Offset::oObjSpellBook);
        const char* slotNames[] = {"Q", "W", "E", "R", "D", "F"};
        
        for (int i = 0; i < 6; i++) {
            SDK::SpellSlot spell = localSpellBook.GetSpell(i);
            if (!spell.IsValid()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s] Invalid", slotNames[i]);
                continue;
            }
            
            int level = spell.GetLevel();
            float remaining = spell.GetRemainingCooldown(gameTime);
            
            if (level <= 0) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "[%s] Not Learned", slotNames[i]);
            } else if (remaining <= 0.0f) {
                ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "[%s] Lv%d READY", slotNames[i], level);
            } else {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "[%s] Lv%d CD: %.1fs", slotNames[i], level, remaining);
            }
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "=== ENEMY SPELLS (R ONLY) ===");
        ImGui::Separator();
        
        // Enemy Ultimate Status
        auto heroes = SDK::ObjectManager::GetHeroes();
        int myTeam = local->GetTeam();
        
        for (auto hero : heroes) {
            if (!hero || !hero->IsValid()) continue;
            if (hero->GetTeam() == myTeam) continue; // Skip allies
            if (hero->IsDead()) continue;
            
            std::string heroName = hero->GetName();
            if (heroName.empty()) continue;
            
            SDK::SpellBook enemySpellBook(hero->Address + Offset::oObjSpellBook);
            SDK::SpellSlot ultSpell = enemySpellBook.R(); // Slot 3 = R
            
            ImGui::Text("%s [R]: ", heroName.c_str());
            ImGui::SameLine();
            
            if (!ultSpell.IsValid()) {
                ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "N/A");
            } else {
                int level = ultSpell.GetLevel();
                float remaining = ultSpell.GetRemainingCooldown(gameTime);
                
                if (level <= 0) {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Not Learned");
                } else if (remaining <= 0.0f) {
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "READY!");
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.5f, 1.0f), "CD: %.0fs", remaining);
                }
            }
            
            delete hero;
        }
        
        ImGui::Spacing();
        ImGui::Separator();
        
        // SpellFactory Data Display
        if (ImGui::CollapsingHeader("Spell Config (Factory Data)")) {
            std::string localName = local->GetName();
            ImGui::Text("Champion: %s", localName.c_str());
            ImGui::Separator();
            
            bool hasConfig = false;
            for (int i = 0; i < 4; i++) {
                 SDK::Spell spell = SDK::SpellFactory::GetSpell(localName, i);
                 if (spell.Data.Type != SDK::SpellType::None) // Has config
                 {
                     hasConfig = true;
                     ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "[%s] %s Config:", slotNames[i], localName.c_str());
                     ImGui::BulletText("Range: %.0f", spell.GetRange());
                     ImGui::BulletText("Speed: %.0f", spell.GetSpeed());
                     ImGui::BulletText("Width: %.0f", spell.GetWidth());
                     ImGui::BulletText("Delay: %.2fs", spell.GetDelay());
                     
                     // Collision flags
                     std::string collStr = "";
                     if (spell.CollidesWithMinions()) collStr += "Minions, ";
                     if (spell.CollidesWithChampions()) collStr += "Champions, ";
                     if (spell.Data.IsBlockedByWindWall()) collStr += "WindWall";
                     ImGui::BulletText("Collision: %s", collStr.c_str());
                 }
            }
            
            if (!hasConfig) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "No SpellFactory config found for %s", localName.c_str());
                ImGui::Text("Logic will default to Targeted/Auto attacks.");
                ImGui::Text("Add configurations to SDK/Spell.h");
            }
        }
        
        ImGui::End();
    }

    // ============================================================================
    // SPELL DRAWING & CASTING LOGIC
    // ============================================================================
    static auto WorldToScreen = [&](Vector3 pos, Vector2& out) -> bool {
        uint64_t base = (uint64_t)GetModuleHandle(NULL);
        uint64_t viewProj = base + Offset::ViewProjectionMatrix;
        
        float* m = (float*)viewProj;
        float w = m[3] * pos.x + m[7] * pos.y + m[11] * pos.z + m[15];
        
        if (w < 0.01f) return false;
        
        float x = m[0] * pos.x + m[4] * pos.y + m[8] * pos.z + m[12];
        float y = m[1] * pos.x + m[5] * pos.y + m[9] * pos.z + m[13];
        
        // Get Screen Size (using ImGui IO)
        float width = ImGui::GetIO().DisplaySize.x;
        float height = ImGui::GetIO().DisplaySize.y;
        
        out.x = (width / 2.0f) * (1.0f + x / w);
        out.y = (height / 2.0f) * (1.0f - y / w);
        return true;
    };
    
    auto DrawWorldCircle = [&](Vector3 center, float radius, ImVec4 color, int segments = 60) {
        ImDrawList* draw = ImGui::GetBackgroundDrawList();
        std::vector<ImVec2> points;
        for (int i = 0; i <= segments; i++) {
            float theta = 2.0f * 3.1415926f * float(i) / float(segments);
            Vector3 p = center;
            p.x += radius * cosf(theta);
            p.z += radius * sinf(theta);
            
            Vector2 screen;
            if (WorldToScreen(p, screen)) {
                points.push_back(ImVec2(screen.x, screen.y));
            }
        }
        
        for (size_t i = 0; i < points.size() - 1; i++) {
            draw->AddLine(points[i], points[i+1], ImGui::ColorConvertFloat4ToU32(color), 2.0f);
        }
    };
    
    // Draw Spells if enabled
    if (showSpellDataDebug && local) {
        std::string localName = local->GetName();
        Vector3 localPos = local->GetPosition();
        
        // Get spell book from memory
        SDK::SpellBook book(local->Address + Offset::oObjSpellBook);
        
        // DEBUG: Show drawing status at fixed position
        static char debugBuf[256];
        int debugY = 400;
        
        ImGui::GetForegroundDrawList()->AddText(ImVec2(10, debugY), 0xFFFFFFFF, "=== SPELL DRAW DEBUG ==="); debugY += 16;
        sprintf_s(debugBuf, "Champion: %s", localName.c_str());
        ImGui::GetForegroundDrawList()->AddText(ImVec2(10, debugY), 0xFFFFFFFF, debugBuf); debugY += 16;
        sprintf_s(debugBuf, "LocalPos: %.0f, %.0f, %.0f", localPos.x, localPos.y, localPos.z);
        ImGui::GetForegroundDrawList()->AddText(ImVec2(10, debugY), 0xFFFFFFFF, debugBuf); debugY += 16;
        
        for (int i = 0; i < 4; i++) {
            // Check if spell is learned from memory
            SDK::SpellSlot memSpell = book.GetSpell(i);
            int spellLevel = memSpell.IsValid() ? memSpell.GetLevel() : 0;
            
            // Skip if spell not learned
            if (spellLevel <= 0) {
                sprintf_s(debugBuf, "[%d] Level=0, SKIP", i);
                ImGui::GetForegroundDrawList()->AddText(ImVec2(10, debugY), 0xFF888888, debugBuf); debugY += 16;
                continue;
            }
            
            // Priority 1: Try SpellFactory for predefined champion data
            SDK::Spell factorySpell = SDK::SpellFactory::GetSpell(localName, i);
            float range = factorySpell.GetRange();
            
            // If no factory config but spell is learned, use default visualization ranges
            if (range <= 0 && spellLevel > 0) {
                // Common default ranges for visualization (Reduced to 1150 for accuracy)
                switch (i) {
                    case 0: range = 1150;  break; // Q - Ezreal Q
                    case 1: range = 1150;  break; // W - Ezreal W
                    case 2: range = 475;   break; // E - Ezreal E
                    case 3: range = 20000; break; // R - Global
                }
            }
            
            // DEBUG: Show what we're about to draw
            sprintf_s(debugBuf, "[%d] Lv%d Range=%.0f -> DRAWING", i, spellLevel, range);
            ImGui::GetForegroundDrawList()->AddText(ImVec2(10, debugY), 0xFF00FF00, debugBuf); debugY += 16;
            
            // Define color based on slot
            ImVec4 color = ImVec4(1, 1, 1, 1);
            if (i == 0) color = ImVec4(0, 1, 1, 1); // Q Cyan
            else if (i == 1) color = ImVec4(1, 0, 1, 1); // W Magenta
            else if (i == 2) color = ImVec4(1, 1, 0, 1); // E Yellow
            else if (i == 3) color = ImVec4(1, 0, 0, 1); // R Red

            // Draw the range circle
            // NOTE: Use Render::DrawCircle3D which is verified working for attack range
            ImU32 uColor = ImGui::ColorConvertFloat4ToU32(color);
            Render::DrawCircle3D(ImGui::GetBackgroundDrawList(), localPos, 120, range, uColor, 2.0f);
            
            // Draw Info text at edge
            Vector3 edgePos = localPos;
            edgePos.x += range;
            
            // Check visibility using Render::WorldToScreen (returns Vector2)
            Vector2 screenPos = Render::WorldToScreen(edgePos);
            
            if (screenPos.x > 0 && screenPos.y > 0 && 
                screenPos.x < Render::g_screenWidth && screenPos.y < Render::g_screenHeight) {
                
                char label[32];
                const char* slotNames[] = {"Q", "W", "E", "R"};
                sprintf_s(label, "%s (%.0f)", slotNames[i], range);
                ImGui::GetBackgroundDrawList()->AddText(ImVec2(screenPos.x, screenPos.y), uColor, label);
            }
        }
    }

    // [REMOVED] EVADE SKILLSHOT DRAWING - Feature disabled (offsets not stable)

    // ============================================================================
    // SAFE BUFF OVERLAY - Render active buffs using SEH protection
    // ============================================================================
    if (g_showBuffOverlay && local) {
        // Update Local Buffs
        GetSafeBuffs(local->Address, g_buffInfos, g_buffInfoCount, 32);
        
        ImGui::SetNextWindowPos(ImVec2(500, 100), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 500), ImGuiCond_FirstUseEver);
        ImGui::Begin("Safe Buff Overlay", &g_showBuffOverlay);
        
        // --- LOCAL PLAYER ---
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.4f, 1.0f), "=== LOCAL: %s (Count: %d) ===", local->GetName().c_str(), g_buffInfoCount);
        ImGui::Separator();
        
        for (int i = 0; i < g_buffInfoCount; i++) {
            SafeBuffInfo& info = g_buffInfos[i];
            
            ImVec4 color = ImVec4(0.8f, 0.8f, 0.8f, 1.0f);
            const char* typeStr = "Unknown";
            
            switch (info.type) {
                case 1: color = ImVec4(0.4f, 1.0f, 0.4f, 1.0f); typeStr = "Internal"; break; 
                case 24: color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); typeStr = "Suppression"; break;
                case 29: color = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); typeStr = "Knockup"; break;
                case 30: color = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); typeStr = "Knockback"; break;
                case 5: color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); typeStr = "Stun"; break;
                case 11: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); typeStr = "Snare"; break;
            }
            
            ImGui::TextColored(color, "[%d:%s] %s", info.type, typeStr, info.name);
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "(x%d) %.1fs", info.stacks, info.remaining);
        }
        
        // --- ENEMY HEROES ---
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "=== ENEMY HEROES ===");
        
        auto heroes = SDK::ObjectManager::GetHeroes();
        for (auto hero : heroes) {
            if (!hero || !hero->IsValid() || hero->IsDead()) continue;
            if (hero->GetTeam() == local->GetTeam()) continue;
            
            SafeBuffInfo enemyBuffs[32];
            int enemyCount = 0;
            GetSafeBuffs(hero->Address, enemyBuffs, enemyCount, 32);
            
            if (enemyCount > 0) {
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.6f, 1.0f), "%s (Buffs: %d)", hero->GetName().c_str(), enemyCount);
                
                for (int i = 0; i < enemyCount; i++) {
                    SafeBuffInfo& info = enemyBuffs[i];
                    if (info.type == 0) continue; // Skip unknown/internal if you want cleaner look
                    
                    ImVec4 color = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
                    const char* typeStr = "?";
                    
                    switch (info.type) {
                        case 24: color = ImVec4(1.0f, 0.2f, 0.2f, 1.0f); typeStr = "SUPPRESS"; break;
                        case 29: color = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); typeStr = "KNOCKUP"; break;
                        case 30: color = ImVec4(1.0f, 0.5f, 0.2f, 1.0f); typeStr = "KNOCKBACK"; break;
                        case 5: color = ImVec4(1.0f, 0.5f, 0.0f, 1.0f); typeStr = "STUN"; break;
                        case 11: color = ImVec4(1.0f, 1.0f, 0.0f, 1.0f); typeStr = "SNARE"; break;
                    }
                    
                    if (strcmp(typeStr, "?") != 0) {
                         // Highlight CC
                         ImGui::TextColored(color, ">>> [%s] %s (%.1fs)", typeStr, info.name, info.remaining);
                    } else {
                         // Normal buff
                         ImGui::Text("    [%d] %s (%.1fs)", info.type, info.name, info.remaining);
                    }
                }
            }
        }
        
        ImGui::End();
    }

    // ============================================================================
    // DEBUG OFFSET OVERLAY - Shows all offset values for verification
    // ============================================================================
    if (Menu::showOffsetDebug && local) {
        ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(450, 600), ImGuiCond_FirstUseEver);
        ImGui::Begin("Offset Debug", &Menu::showOffsetDebug);
        
        uint64_t localAddr = local->Address;
        
        ImGui::Text("=== LOCAL PLAYER (0x%llX) ===", localAddr);
        ImGui::Separator();
        
        // Simple reads without __try (address should be valid if local exists)
        // Basic Info
        ImGui::Text("NetId (0xC4): %d", *(int*)(localAddr + 0xC4));
        ImGui::Text("Team (0x251): %d", (int)(*(uint8_t*)(localAddr + 0x251)));
        
        float px = *(float*)(localAddr + 0x254);
        float py = *(float*)(localAddr + 0x254 + 4);
        float pz = *(float*)(localAddr + 0x254 + 8);
        ImGui::Text("Position (0x254): (%.1f, %.1f, %.1f)", px, py, pz);
        
        ImGui::Spacing();
        ImGui::Text("--- Health/Mana ---");
        ImGui::Text("Health (0x10A8): %.2f", *(float*)(localAddr + 0x10A8));
        ImGui::Text("MaxHealth (0x10D0): %.2f", *(float*)(localAddr + 0x10D0));
        ImGui::Text("Mana (0x358): %.2f", *(float*)(localAddr + 0x358));
        ImGui::Text("MaxMana (0x380): %.2f", *(float*)(localAddr + 0x380));
        
        ImGui::Spacing();
        ImGui::Text("--- Combat Stats ---");
        ImGui::Text("AttackRange (0x181C): %.2f", *(float*)(localAddr + 0x181C));
        ImGui::Text("BaseAD (0x17D4): %.2f", *(float*)(localAddr + 0x17D4));
        ImGui::Text("BonusAD (0x1730): %.2f", *(float*)(localAddr + 0x1730));
        ImGui::Text("MoveSpeed (0x1814): %.2f", *(float*)(localAddr + 0x1814));
        ImGui::Text("Armor (0x17FC): %.2f", *(float*)(localAddr + 0x17FC));
        ImGui::Text("MagicResist (0x1804): %.2f", *(float*)(localAddr + 0x1804));
        
        ImGui::Spacing();
        ImGui::Text("--- Status Flags ---");
        ImGui::Text("Dead (0x250): %d", (int)(*(uint8_t*)(localAddr + 0x250)));
        ImGui::Text("Visible (0x300): %d", (int)(*(uint8_t*)(localAddr + 0x300)));
        ImGui::Text("Targetable_old (0x458): %d", (int)(*(uint8_t*)(localAddr + 0x458)));
        ImGui::Text("Targetable_new (0xEC8): %d", (int)(*(uint8_t*)(localAddr + 0xEC8)));
        
        ImGui::Spacing();
        ImGui::Text("--- Bounding Radius (Function) ---");
        float boundingRadius = local->GetBoundingRadius();
        ImGui::Text("GetBoundingRadius(): %.2f", boundingRadius);
        
        ImGui::Spacing();
        ImGui::Text("--- Attack Timing (Function) ---");
        float attackDelay = GetAttackDelay(localAddr);
        float attackWindup = GetAttackWindup(localAddr);
        ImGui::Text("GetAttackDelay(): %.3f sec", attackDelay);
        ImGui::Text("GetAttackWindup(): %.3f sec", attackWindup);
        
        ImGui::Spacing();
        ImGui::Text("=== MISSILES ===");
        ImGui::Separator();
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t missileManager = *(uint64_t*)(moduleBase + Offset::oMissileList);
        if (missileManager) {
            uint64_t arrayPtr = *(uint64_t*)(missileManager + 0x08);
            int size = *(int*)(missileManager + 0x10);
            if (size > 100) size = 100;
            if (size < 0) size = 0;
            
            ImGui::Text("Missile Count: %d", size);
            
            for (int i = 0; i < size && i < 5; i++) {
                uint64_t mAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                if (!mAddr) continue;
                
                ImGui::Text("Missile[%d] @ 0x%llX", i, mAddr);
                
                uint64_t spellInfo = *(uint64_t*)(mAddr + 0x2F0);
                if (spellInfo) {
                    ImGui::Text("  SpellInfo (0x2F0): 0x%llX", spellInfo);
                    int srcIdx = *(int*)(spellInfo + 0x88);
                    int tgtIdx = *(int*)(spellInfo + 0xE0);
                    ImGui::Text("  SrcIndex (0x88): %d", srcIdx);
                    ImGui::Text("  TgtIndex (0xE0): %d", tgtIdx);
                } else {
                    ImGui::TextColored(ImVec4(1,0,0,1), "  SpellInfo: NULL");
                }
            }
        } else {
            ImGui::TextColored(ImVec4(1,0,0,1), "MissileManager: NULL");
        }
        
        ImGui::End();
    }

    // ============================================================================
    // CONTINUOUS MISSILE LOGGING - Call external C-style function (FAST for enemy spell capture)
    // ============================================================================
    if (Menu::continuousMissileLog) {
        static int logCounter = 0;
        logCounter++;
        if (logCounter >= 5) { // Very fast - every 5 frames to catch enemy spells
            logCounter = 0;
            ContinuousMissileLog();
        }
    }
    
    // ============================================================================
    // CONTINUOUS TURRET LOGGING - Find turret target offset
    // ============================================================================
    if (Menu::continuousTurretLog) {
        static int turretLogCounter = 0;
        turretLogCounter++;
        if (turretLogCounter >= 60) { // ~1 sec at 60fps
            turretLogCounter = 0;
            ContinuousTurretLog();
        }
    }
    
    // ============================================================================
    // CONTINUOUS SPELL DUMP - Verify spell offsets in real-time
    // ============================================================================
    if (continuousSpellDump) {
        static int spellDumpCounter = 0;
        spellDumpCounter++;
        if (spellDumpCounter >= 120) { // ~2 sec at 60fps
            spellDumpCounter = 0;
            SpellDebug();
        }
    }
    
    // ============================================================================
    // CONTINUOUS MISSILE SCAN - Find missile offsets (FAST - every frame when missiles exist)
    // ============================================================================
    if (continuousMissileScan) {
        static int missileScanCounter = 0;
        missileScanCounter++;
        if (missileScanCounter >= 5) { // Very fast - every 5 frames to catch spell missiles
            missileScanCounter = 0;
            MissileOffsetScan();
        }
    }
    
    // CONTINUOUS MISSILE DECRYPTION SCAN
    // ============================================================================
    if (doMissileDecryptionScan) {
        static int decryptScanCounter = 0;
        decryptScanCounter++;
        if (decryptScanCounter >= 30) { // Every 30 frames (slower than continuous scan)
            decryptScanCounter = 0;
            ScanMissileOffsetsWithDecryption();
        }
    }
    
        
    // ============================================================================
    // CONTINUOUS AI MANAGER SCAN & LOG - Real-time debugging
    // ============================================================================
    if (continuousAiManagerScan) {
        static int aiScanCounter = 0;
        aiScanCounter++;
        if (aiScanCounter >= 30) { // ~0.5 sec at 60fps
            aiScanCounter = 0;
            AiManagerOffsetScan();
        }
    }
    
    if (continuousAiManagerLog && local) {
        static FILE* logFile = nullptr;
        static int frameCounter = 0;
        static uint64_t lastAiMgr = 0;
        
        frameCounter++;
        if (frameCounter >= 10) { // Log every ~0.16 sec
            frameCounter = 0;
            
            if (!logFile) {
                logFile = fopen("aimanager_continuous_log.txt", "w");
                if (logFile) {
                    fprintf(logFile, "=== AI MANAGER CONTINUOUS LOG ===\n");
                    fprintf(logFile, "Format: Frame | StartPath | TargetPos | Distance | [0x214] [0x21C] [0x224] [0x268] [0x2B0] [0x210] [0x228] [0x2D8] [0x2E8] | DashSpeed[0x360] [0x364] [0x368] | Extended bools [0x300-0x400] | BuffManager info\n\n");
                }
            }
            
            if (logFile) {
                uint64_t base = (uint64_t)GetModuleHandle(NULL);
                uint64_t localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer);
                if (localPlayer) {
                    uint64_t aiMgr = *(uint64_t*)(localPlayer + 0x3108);
                    if (aiMgr > 0x10000 && aiMgr < 0x7FFFFFFFFFFF) {
                        float startX = *(float*)(aiMgr + 0x1E0);
                        float startY = *(float*)(aiMgr + 0x1E4);
                        float startZ = *(float*)(aiMgr + 0x1E8);
                        float targetX = *(float*)(aiMgr + 0x23C);
                        float targetY = *(float*)(aiMgr + 0x240);
                        float targetZ = *(float*)(aiMgr + 0x244);
                        
                        // Read ALL bool candidates
                        int val214 = *(int*)(aiMgr + 0x214);
                        int val21C = *(int*)(aiMgr + 0x21C);
                        int val224 = *(int*)(aiMgr + 0x224);
                        int val268 = *(int*)(aiMgr + 0x268);
                        int val2B0 = *(int*)(aiMgr + 0x2B0);
                        int val210 = *(int*)(aiMgr + 0x210);
                        int val228 = *(int*)(aiMgr + 0x228);
                        int val2D8 = *(int*)(aiMgr + 0x2D8);
                        int val2E8 = *(int*)(aiMgr + 0x2E8);
                        
                        // Read ALL dash speed candidates
                        float dashSpeed360 = *(float*)(aiMgr + 0x360);
                        float dashSpeed364 = *(float*)(aiMgr + 0x364);
                        float dashSpeed368 = *(float*)(aiMgr + 0x368);
                        
                        // Calculate distance
                        float dx = targetX - startX;
                        float dz = targetZ - startZ;
                        float dist = sqrtf(dx*dx + dz*dz);
                        
                        // Log frame data with ALL important values
                        fprintf(logFile, "%06d | (%.1f,%.1f,%.1f) | (%.1f,%.1f,%.1f) | %.1f | %d %d %d %d %d %d %d | %.2f %.2f %.2f | [0x228]=%d [0x2D8]=%d [0x2E8]=%d",
                            GetTickCount(), startX, startY, startZ, targetX, targetY, targetZ, dist,
                            val214, val21C, val224, val268, val2B0, val210, val228, val2D8, val2E8,
                            dashSpeed360, dashSpeed364, dashSpeed368);
                        
                        // Log extended bool candidates for dash detection
                        for (int off = 0x300; off < 0x400; off += 4) {
                            int val = *(int*)(aiMgr + off);
                            if (val == 0 || val == 1) {
                                fprintf(logFile, " [0x%03X]=%d", off, val);
                            }
                        }
                        
                        // Log BuffManager info
                        uint64_t buffMgr = *(uint64_t*)(localPlayer + 0x2E68);
                        if (buffMgr > 0x10000 && buffMgr < 0x7FFFFFFFFFFF) {
                            uint64_t buffArray = *(uint64_t*)(buffMgr + 0x18);
                            uint64_t buffArrayEnd = *(uint64_t*)(buffMgr + 0x20);
                            if (buffArray && buffArrayEnd && buffArray < buffArrayEnd) {
                                int buffCount = (buffArrayEnd - buffArray) / 8;
                                fprintf(logFile, " | BuffCount=%d", buffCount);
                                
                                // Scan for dash-related buffs
                                for (int i = 0; i < (std::min)(buffCount, 5); i++) {
                                    uint64_t buffInst = *(uint64_t*)(buffArray + i * 8);
                                    if (buffInst > 0x10000 && buffInst < 0x7FFFFFFFFFFF) {
                                        uint64_t buffScript = *(uint64_t*)(buffInst + 0x10);
                                        if (buffScript > 0x10000 && buffScript < 0x7FFFFFFFFFFF) {
                                            uint64_t namePtr = *(uint64_t*)(buffScript + 0x8);
                                            if (namePtr > 0x10000 && namePtr < 0x7FFFFFFFFFFF) {
                                                char buffName[32] = {0};
                                                for (int j = 0; j < 31; j++) {
                                                    char c = *(char*)(namePtr + j);
                                                    if (c == 0) break;
                                                    buffName[j] = c;
                                                }
                                                fprintf(logFile, " [%s]", buffName);
                                            }
                                        }
                                    }
                                }
                            }
                        }
                        fprintf(logFile, "\n");
                        
                        fflush(logFile); // Force write immediately
                    }
                }
            }
        }
    }
    
    // ============================================================================
    // REALTIME AI MANAGER OVERLAY - Shows live values on screen
    // ============================================================================
    if (continuousAiManagerScan && local) {
        uint64_t base = (uint64_t)GetModuleHandle(NULL);
        uint64_t localPlayer = *(uint64_t*)(base + Offset::oLocalPlayer);
        if (localPlayer) {
            // Read values safely (no __try needed - just null check)
            uint64_t aiMgr = *(uint64_t*)(localPlayer + 0x3108);
            if (aiMgr > 0x10000 && aiMgr < 0x7FFFFFFFFFFF) {
                float startX = *(float*)(aiMgr + 0x1E0);
                float startY = *(float*)(aiMgr + 0x1E4);
                float startZ = *(float*)(aiMgr + 0x1E8);
                float targetX = *(float*)(aiMgr + 0x23C);
                float targetY = *(float*)(aiMgr + 0x240);
                float targetZ = *(float*)(aiMgr + 0x244);
                
                // Read multiple potential IsMoving/IsDashing offsets
                int val214 = *(int*)(aiMgr + 0x214);
                int val21C = *(int*)(aiMgr + 0x21C);
                int val224 = *(int*)(aiMgr + 0x224);
                int val268 = *(int*)(aiMgr + 0x268);
                int val2B0 = *(int*)(aiMgr + 0x2B0);
                int val210 = *(int*)(aiMgr + 0x210);
                
                // Read dash speed candidates
                float dashSpeed360 = *(float*)(aiMgr + 0x360);
                float dashSpeed364 = *(float*)(aiMgr + 0x364);
                float dashSpeed368 = *(float*)(aiMgr + 0x368);
                
                // Calculate if moving (startPath != targetPos)
                float dx = targetX - startX;
                float dz = targetZ - startZ;
                float dist = sqrtf(dx*dx + dz*dz);
                bool isMovingCalc = dist > 5.0f;
                
                ImGui::SetNextWindowPos(ImVec2(10, 400));
                ImGui::Begin("AiManager Live", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_AlwaysAutoResize);
                ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== AiManager LIVE ===");
                ImGui::Text("StartPath: (%.1f, %.1f, %.1f)", startX, startY, startZ);
                ImGui::Text("TargetPos: (%.1f, %.1f, %.1f)", targetX, targetY, targetZ);
                ImGui::Text("Distance: %.1f", dist);
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "=== BOOL CANDIDATES ===");
                ImGui::Text("[0x210]: %d", val210);
                ImGui::Text("[0x214]: %d", val214);
                ImGui::Text("[0x21C]: %d", val21C);
                ImGui::Text("[0x224]: %d", val224);
                ImGui::Text("[0x268]: %d", val268);
                ImGui::Text("[0x2B0]: %d", val2B0);
                
                ImGui::Separator();
                if (isMovingCalc) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), ">>> MOVING (calc) <<<");
                } else {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "STOPPED");
                }
                
                // Calculate velocity from position change
                static struct Vec3 { float x, y, z; } lastPos = {startX, startY, startZ};
                static float lastTime = GetTickCount() / 1000.0f;
                float currentTime = GetTickCount() / 1000.0f;
                float deltaTime = currentTime - lastTime;
                
                struct Vec3 velocity = {0, 0, 0};
                float speed = 0;
                if (deltaTime > 0) {
                    velocity.x = (startX - lastPos.x) / deltaTime;
                    velocity.y = (startY - lastPos.y) / deltaTime;
                    velocity.z = (startZ - lastPos.z) / deltaTime;
                    speed = sqrtf(velocity.x*velocity.x + velocity.z*velocity.z);
                }
                
                // Detect dash by speed threshold
                bool isDashing = speed > 1000.0f;
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 0, 1), "=== VELOCITY ANALYSIS ===");
                ImGui::Text("Speed: %.1f units/sec", speed);
                ImGui::Text("Velocity: (%.1f, %.1f, %.1f)", velocity.x, velocity.y, velocity.z);
                
                if (isDashing) {
                    ImGui::TextColored(ImVec4(1, 0, 1, 1), ">>> DASHING DETECTED! <<<");
                } else if (isMovingCalc) {
                    ImGui::TextColored(ImVec4(0, 1, 0, 1), "Normal Movement");
                } else {
                    ImGui::TextColored(ImVec4(1, 0.5f, 0, 1), "Stationary");
                }
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "=== DASH SPEED CANDIDATES ===");
                ImGui::Text("[0x360]: %.2f", dashSpeed360);
                ImGui::Text("[0x364]: %.2f", dashSpeed364);
                ImGui::Text("[0x368]: %.2f", dashSpeed368);
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "=== EXTENDED BOOL SCAN (0x300-0x400) ===");
                for (int off = 0x300; off < 0x400; off += 4) {
                    int val = *(int*)(aiMgr + off);
                    if (val == 0 || val == 1) {
                        ImGui::Text("[0x%03X]: %d", off, val);
                    }
                }
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "=== BUFF MANAGER SCAN ===");
                uint64_t buffMgr = *(uint64_t*)(localPlayer + 0x2E68);
                if (buffMgr > 0x10000 && buffMgr < 0x7FFFFFFFFFFF) {
                    uint64_t buffArray = *(uint64_t*)(buffMgr + 0x18);
                    if (showBuffManagerDebug) {
                        ImGui::Begin("BuffManager Debug", &showBuffManagerDebug);
                        
                        // Fix 1: Declare gameTime
                        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
                        float gameTime = *(float*)(moduleBase + Offset::oGametime);

                        // Fix 2: localPlayer is uint64_t (address), so pass it directly
                        SDK::BuffManager buffMgr(localPlayer);
                        ImGui::Text("BuffManager Addr: %llX", buffMgr.GetBuffManagerAddress());
                        
                        uint64_t arrStart = buffMgr.GetRawArrayStart();
                        uint64_t arrEnd = buffMgr.GetRawArrayEnd();
                        ImGui::Text("Array Start: %llX", arrStart);
                        ImGui::Text("Array End:   %llX", arrEnd);
                        
                        if (arrStart && arrEnd && arrEnd > arrStart) {
                            size_t count = (arrEnd - arrStart) / sizeof(uint64_t);
                            ImGui::Text("Calculated Count: %d", (int)count);
                        } else {
                            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Invalid Array Pointers!");
                        }

                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(0, 1, 0, 1), "--- LOCAL PLAYER BUFFS ---");
                        
                        ImGui::Text("CC Status: %s", buffMgr.IsImmobile(gameTime) ? "IMMOBILE" : "NONE");
                        ImGui::Text("Invuln Status: %s", buffMgr.IsInvulnerable(gameTime) ? "YES" : "None");
                        
                        if (ImGui::TreeNode("Active Buffs (Local)")) {
                            auto buffs = buffMgr.GetBuffs();
                            if (buffs.empty()) {
                                ImGui::TextColored(ImVec4(1, 0, 0, 1), "No active buffs found");
                            }
                            for (const auto& buff : buffs) {
                                if (buff.IsActive(gameTime)) {
                                    ImGui::Text("%s (Type: %d, Stacks: %d, Time: %.1f)", 
                                        buff.GetName().c_str(), 
                                        (int)buff.GetType(), 
                                        buff.GetStackCount(), 
                                        buff.GetRemainingTime(gameTime));
                                }
                            }
                            ImGui::TreePop();
                        }
                        
                        ImGui::Separator();
                        ImGui::TextColored(ImVec4(1, 0, 0, 1), "--- ENEMY HEROES BUFFS ---");
                        
                        // Fix 3: Get enemy heroes locally since 'enemyHeroes' is not defined in this scope
                        auto heroes = SDK::ObjectManager::GetHeroes();
                        uint64_t myTeam = *(uint8_t*)(localPlayer + 0x251); // Read team directly from localPlayer address
                        
                        for (auto* hero : heroes) {
                            if (hero && hero->IsVisible() && hero->GetTeam() != myTeam) {
                                 SDK::BuffManager heroBuffs(hero->Address);
                                 std::string status = "Normal";
                                 ImVec4 color = ImVec4(1, 1, 1, 1);
                                 
                                 if (heroBuffs.IsKnockedUp(gameTime)) { status = "KNOCKED UP"; color = ImVec4(1, 0, 0, 1); }
                                 else if (heroBuffs.IsStunned(gameTime)) { status = "STUNNED"; color = ImVec4(1, 0, 0, 1); }
                                 else if (heroBuffs.IsInvulnerable(gameTime)) { status = "INVULNERABLE"; color = ImVec4(1, 1, 0, 1); }
                                 else if (heroBuffs.IsUntargetable(gameTime)) { status = "UNTARGETABLE"; color = ImVec4(1, 1, 0, 1); }
                                 
                                 ImGui::Text("%s: ", hero->GetName().c_str());
                                 ImGui::SameLine();
                                 ImGui::TextColored(color, "%s", status);
                            }
                            // Don't delete hero here if the list owns them, but SDK::ObjectManager usually returns new instances or direct pointers. 
                            // Standard pattern in this codebase seems to be we might need to delete if GetHeroes returns new'd objects.
                            // Checking GetHeroes implementation would be good, but assuming standard behavior from other snippets:
                            // "for (auto h : heroes) delete h;" was seen in target selector snippet. 
                            // BUT, let's look at how I used it in other places.
                            // In snippet 2016 (line 2016-2058), I iterate and then `delete hero`.
                            // So I should clean up.
                            delete hero;
                        }
                        
                        ImGui::End();
                    }
                } else {
                    ImGui::Text("BuffManager: Invalid");
                }
                
                ImGui::Separator();
                ImGui::TextColored(ImVec4(1, 1, 1, 1), "=== DASH DETECTION ===");
                ImGui::Text("Threshold: >1000 units/sec = DASH");
                ImGui::Text("Current: %.1f units/sec", speed);
                
                // Update last position
                lastPos.x = startX;
                lastPos.y = startY;
                lastPos.z = startZ;
                lastTime = currentTime;
                
                ImGui::End();
            }
        }
    }

    // ============================================================================
    // DRAW AI STATE DEBUG - COMPREHENSIVE VISUALIZATION
    // Shows ALL AiManager properties for verification
    // ============================================================================
    if (Menu::bDrawAiState) {
        uint64_t base = (uint64_t)GetModuleHandle(NULL);
        uint64_t localAddr = *(uint64_t*)(base + Offset::oLocalPlayer);
        
        if (localAddr) {
             // Create proper GameObject wrapper
             SDK::GameObject localObj(localAddr);
             SDK::AiManager ai = localObj.GetAiManager();
             
             if (ai.IsValid()) {
                 // Get all AiManager properties
                 Vector3 currentPos = localObj.GetPosition();
                 Vector3 serverPos = ai.GetServerPosition();
                 Vector3 targetPos = ai.GetTargetPosition();
                 Vector3 startPath = ai.GetStartPath();
                 Vector3 endPath = ai.GetEndPath();
                 Vector3 velocity = ai.GetVelocity();
                 Vector3 moveDir = ai.GetMoveDirection();
                 
                 float speed = ai.GetSpeed();
                 float dashSpeed = ai.GetDashSpeed();
                 bool isMoving = ai.IsMoving();
                 bool isDashing = ai.IsDashing();
                 bool hasPath = ai.HasPath();
                 int currentSeg = ai.GetCurrentSegment();
                 int segCount = ai.GetSegmentsCount();
                 
                 // ================================================================
                 // DRAW OVERLAY PANEL (Top-left corner)
                 // ================================================================
                 ImGui::SetNextWindowPos(ImVec2(10, 200), ImGuiCond_FirstUseEver);
                 ImGui::SetNextWindowSize(ImVec2(380, 0), ImGuiCond_FirstUseEver);
                 ImGui::Begin("AiManager Debug [15.11 Compat]", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
                 
                 // STATE SECTION
                 ImGui::TextColored(ImVec4(1, 1, 0, 1), "=== MOVEMENT STATE ===");
                 if (isDashing) {
                     ImGui::TextColored(ImVec4(1, 0, 1, 1), ">> DASHING <<");
                 } else if (isMoving) {
                     ImGui::TextColored(ImVec4(0, 1, 0, 1), "MOVING");
                 } else {
                     ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "IDLE");
                 }
                 ImGui::SameLine();
                 ImGui::Text("| HasPath: %s", hasPath ? "YES" : "NO");
                 
                 ImGui::Separator();
                 
                 // POSITION SECTION (0x330, 0x33C, 0x474)
                 ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== POSITIONS ===");
                 ImGui::Text("ServerPos (0x474): (%.1f, %.1f, %.1f)", serverPos.x, serverPos.y, serverPos.z);
                 ImGui::Text("StartPath (0x330): (%.1f, %.1f, %.1f)", startPath.x, startPath.y, startPath.z);
                 ImGui::Text("EndPath   (0x33C): (%.1f, %.1f, %.1f)", endPath.x, endPath.y, endPath.z);
                 ImGui::Text("TargetPos (0x34):  (%.1f, %.1f, %.1f)", targetPos.x, targetPos.y, targetPos.z);
                 
                 ImGui::Separator();
                 
                 // VELOCITY SECTION (0x318)
                 ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== VELOCITY (0x318) ===");
                 ImGui::Text("Velocity: (%.1f, %.1f, %.1f)", velocity.x, velocity.y, velocity.z);
                 ImGui::Text("Speed: %.1f u/s", speed);
                 ImGui::Text("MoveDir (0x480): (%.2f, %.2f, %.2f)", moveDir.x, moveDir.y, moveDir.z);
                 
                 ImGui::Separator();
                 
                 // PATH SEGMENTS SECTION (0x320, 0x348, 0x350)
                 ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== PATH SEGMENTS ===");
                 ImGui::Text("CurrentSegment (0x320): %d", currentSeg);
                 ImGui::Text("SegmentsCount (0x350):  %d", segCount);
                 ImGui::Text("NavArray (0x348): [Simulated - %d points]", segCount);
                 
                 ImGui::Separator();
                 
                 // DASH SECTION (0x360, 0x384)
                 ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== DASH INFO ===");
                 ImGui::Text("IsDashing (0x384): %s", isDashing ? "TRUE" : "FALSE");
                 ImGui::Text("DashSpeed (0x360): %.1f", dashSpeed);
                 if (isDashing) {
                     Vector3 dashEnd = ai.GetDashEndPos(0.4f);
                     ImGui::Text("DashEndPos (est): (%.1f, %.1f, %.1f)", dashEnd.x, dashEnd.y, dashEnd.z);
                 }
                 
                 ImGui::Separator();
                 
                 // PREDICTION SECTION
                 ImGui::TextColored(ImVec4(0, 1, 1, 1), "=== PREDICTION ===");
                 Vector3 pred1s = ai.PredictPositionWithWallCheck(1.0f); // With wall checking
                 Vector3 pred2s = ai.PredictPositionWithWallCheck(2.0f); // With wall checking
                 ImGui::Text("Pos in 1.0s (wall-check): (%.1f, %.1f, %.1f)", pred1s.x, pred1s.y, pred1s.z);
                 ImGui::Text("Pos in 2.0s (wall-check): (%.1f, %.1f, %.1f)", pred2s.x, pred2s.y, pred2s.z);
                 
                 // Also show basic prediction for comparison
                 Vector3 pred1sBasic = ai.PredictPosition(1.0f);
                 Vector3 pred2sBasic = ai.PredictPosition(2.0f);
                 ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Pos in 1.0s (basic): (%.1f, %.1f, %.1f)", pred1sBasic.x, pred1sBasic.y, pred1sBasic.z);
                 ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1), "Pos in 2.0s (basic): (%.1f, %.1f, %.1f)", pred2sBasic.x, pred2sBasic.y, pred2sBasic.z);
                 
                 ImGui::Separator();
                 
                 // DEBUG INFO
                 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "[Simulation Mode - Physics Based]");
                 ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1), "AiMgr: 0x%llX | Owner: 0x%llX", ai.m_Address, ai.m_OwnerAddress);
                 
                 ImGui::End();
                 
                 // ================================================================
                 // DRAW ON PLAYER (World Space) - Using new DrawAiManagerDebug
                 // ================================================================
                 Render::DrawAiManagerDebug(ImGui::GetBackgroundDrawList(), &localObj);
             } else {
                 // AiManager invalid
                 Vector3 pos = localObj.GetPosition();
                 Vector2 screenPos = Render::WorldToScreen({pos.x, pos.y + 120.0f, pos.z});
                 if (screenPos.x > 0) {
                     ImDrawList* draw = ImGui::GetBackgroundDrawList();
                     draw->AddText(ImVec2(screenPos.x, screenPos.y), IM_COL32(255, 100, 100, 255), "AiManager: INVALID");
                     
                     char buf[64];
                     uint64_t aiAddr = localObj.GetAiManagerRaw();
                     sprintf(buf, "AiMgr Addr: 0x%llX", aiAddr);
                     draw->AddText(ImVec2(screenPos.x, screenPos.y + 15), IM_COL32(200, 200, 200, 255), buf);
                 }
             }
        }
    }

    // Render Menu
    Menu::Render();

    // Render ImGui
    ImGui::Render();
    pContext->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
    
    if (local) delete local; // Cleanup properly
    
    return oPresent(pSwapChain, SyncInterval, Flags);
}

// Thread to update Matrix (WorldToScreen Logic)
// Also acting as a test bed for SDK for now

DWORD WINAPI ThreadMatrix(LPVOID lpParam) {
    // Wait for ModuleBase to be ready? Or just get it here.
    while(!DATA::ModuleBase) {
        DATA::ModuleBase = (uint64_t)GetModuleHandleA(NULL);
        Sleep(100);
    }
    
    // Test SDK
    static bool sdkTested = false;
    
    while (true) {
        // Update Matrix logic
        if(!DATA::pViewMatrix) {
             DATA::pViewMatrix = (float*)(DATA::ModuleBase + Offset::ViewProjectionMatrix);
             DATA::pProjMatrix = (float*)(DATA::ModuleBase + Offset::ViewProjectionMatrix + Offset::projMatrix);
        }

        if (DATA::pViewMatrix && DATA::pProjMatrix) {
            memcpy(Render::viewMatrix, DATA::pViewMatrix, sizeof(Render::viewMatrix));
            memcpy(Render::projMatrix, DATA::pProjMatrix, sizeof(Render::projMatrix));
            Render::MultiplyMatrices(Render::viewProjMatrix, Render::viewMatrix, 4, 4, Render::projMatrix, 4, 4);
        }

        // Simple Print Test
        if (!sdkTested) {
             SDK::GameObject* local = SDK::ObjectManager::GetLocalPlayer();
             if (local) {
                 float hp = local->GetHealth();
                 // Print using standard cout or DBG_LOG if we had one.
                 // Since we deleted DBG_LOG, let's just inspect in debugger or add a simple check.
                 if (hp > 0) {
                      // It works
                 }
                 sdkTested = true;
             }
        }
        
        // Matrix update only - Orbwalker runs in its own thread now
        Sleep(10);
    }
    return 0;
}

// ============================================================================
// ORBWALKER THREADS (Like Backup Architecture)
// ============================================================================

// OLD TIMING NAMESPACE - DEPRECATED, using OrbwalkerTiming now
// Kept for backward compatibility with any remaining code
namespace OrbTiming {
    int LastAATick = 0;
    int LastMoveTime = 0;
}

// Helper: Get object position
Vector3 GetObjPositionRaw(uint64_t obj) {
    if (!obj) return Vector3(0, 0, 0);
    return *(Vector3*)(obj + Offset::oObjPosition);
}

// Helper: Get mouse world position (raw memory read)
Vector3 GetMouseWorldPosRaw() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    uint64_t hudInstance = *(uint64_t*)(moduleBase + Offset::oHudInstance);
    if (!hudInstance) return Vector3(0, 0, 0);
    
    uint64_t input = *(uint64_t*)(hudInstance + Offset::oHudInstanceInput);
    if (!input) return Vector3(0, 0, 0);
    
    Vector3 mousePos;
    mousePos.x = *(float*)(input + Offset::oHudMouseVec3);
    mousePos.y = *(float*)(input + Offset::oHudMouseVec3 + 0x4);
    mousePos.z = *(float*)(input + Offset::oHudMouseVec3 + 0x8);
    
    if (mousePos.x < -1000.0f || mousePos.x > 20000.0f ||
        mousePos.z < -1000.0f || mousePos.z > 20000.0f) {
        return Vector3(0, 0, 0);
    }
    return mousePos;
}

// Helper: Get attack delay in SECONDS (like leagueoflegends-master)
float GetAttackDelay(uint64_t localPlayer) {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    typedef float(__cdecl* fnGetAttackDelay)(uint64_t);
    fnGetAttackDelay getAttackDelay = (fnGetAttackDelay)(moduleBase + Offset::Function::AttackDelay);
    
    __try {
        return getAttackDelay(localPlayer);
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return 1.0f; // Default 1 second
    }
}

// Helper: Get attack windup in SECONDS using FUNCTION CALL (like leagueoflegends-master)
// leagueoflegends-master: fnGetAttackWindup(this, 0x40)
float GetAttackWindup(uint64_t localPlayer) {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    typedef float(__cdecl* fnGetAttackWindup)(uint64_t, int);
    fnGetAttackWindup getAttackWindup = (fnGetAttackWindup)(moduleBase + Offset::Function::oGetAttackWindup);
    
    __try {
        return getAttackWindup(localPlayer, 0x40); // 0x40 flag like leagueoflegends-master
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0.3f; // Default 300ms
    }
}

// Helper: Get ping in MILLISECONDS using FUNCTION CALL
// Returns ping in ms (convert to seconds when needed: ping / 1000.0f)
float GetPing() {
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    typedef float(__cdecl* fnGetPing)();
    fnGetPing getPing = (fnGetPing)(moduleBase + Offset::Function::GetPing);
    
    __try {
        return getPing(); // Returns ping in milliseconds
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return 30.0f; // Default 30ms ping
    }
}

// LEGACY: For backward compatibility with code using milliseconds
float GetAttackDelayRaw(uint64_t localPlayer) {
    return GetAttackDelay(localPlayer) * 1000.0f;
}

float GetAttackWindupRaw(uint64_t localPlayer) {
    return GetAttackWindup(localPlayer) * 1000.0f;
}

// Helper: Check if valid target (raw)
bool IsValidTargetRaw(uint64_t obj, uint64_t localPlayer) {
    if (!obj || obj == localPlayer) return false;
    
    // Check team
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    uint8_t objTeam = *(uint8_t*)(obj + Offset::TeamID);
    if (myTeam == objTeam) return false;
    
    // Check dead
    uint8_t dead = *(uint8_t*)(obj + Offset::oDead);
    if (dead != 0) return false;
    
    // Check health
    float health = *(float*)(obj + Offset::oHealth);
    if (health <= 0) return false;
    
    // Check visible
    uint8_t visible = *(uint8_t*)(obj + Offset::oVisibility);
    if (visible == 0) return false;
    
    // Check targetable
    uint8_t targetable = *(uint8_t*)(obj + Offset::oTargetable);
    if (targetable == 0) return false;
    
    return true;
}

// Helper: Get player's bounding radius using FUNCTION CALL (like leagueoflegends-master)
// This gives accurate values instead of reading from potentially wrong offset
float GetMyBoundingRadius(uint64_t localPlayer) {
    if (!localPlayer) return 65.0f; // Default
    
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    typedef float(__fastcall* fnGetBoundingRadius)(uint64_t obj);
    fnGetBoundingRadius getBoundingRadius = (fnGetBoundingRadius)(moduleBase + Offset::Function::oGetBoundingRadius);
    
    __try {
        float r = getBoundingRadius(localPlayer);
        // Sanity check: bounding radius should be between 0 and 500
        if (r < 0.0f || r > 500.0f || r != r) return 65.0f; // r != r checks for NaN
        return r;
    }
    __except(EXCEPTION_EXECUTE_HANDLER) {
        return 65.0f; // Return default on any exception
    }
}

// Helper: Get RealAttackRange (following leagueoflegends-master pattern)
// RealAttackRange = baseAttackRange + myBoundingRadius
float GetRealAttackRangeRaw(uint64_t localPlayer) {
    if (!localPlayer) return 550.0f; // Default ADC range
    float baseRange = *(float*)(localPlayer + Offset::RangeAttack);
    float myRadius = GetMyBoundingRadius(localPlayer);
    return baseRange + myRadius;
}

// Helper: Get best hero target in range (using SDK)
// Following leagueoflegends-master pattern:
// - Use RealAttackRange (attackRange + myBoundingRadius)
// - Check: RealAttackRange + targetBoundingRadius >= distance
uint64_t GetBestHeroTargetRaw(uint64_t moduleBase, uint64_t localPlayer, float realAttackRange) {
    auto heroes = SDK::ObjectManager::GetHeroes();
    if (heroes.empty()) return 0;
    
    Vector3 myPos = GetObjPositionRaw(localPlayer);
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    
    uint64_t bestTarget = 0;
    float lowestHealth = 999999.0f;
    
    for (auto* hero : heroes) {
        if (!hero || hero->Address == localPlayer) continue;
        if (hero->GetTeam() == myTeam) continue;
        if (hero->IsDead() || hero->GetHealth() <= 0) continue;
        if (!hero->IsVisible() || !hero->IsTargetable()) continue;
        
        Vector3 objPos = hero->GetPosition();
        float dist = myPos.Distance(objPos);
        
        // leagueoflegends-master pattern: realAttackRange + targetBoundingRadius >= dist
        float effectiveRange = realAttackRange + hero->GetBoundingRadius();
        if (dist > effectiveRange) continue;
        
        float hp = hero->GetHealth();
        if (hp < lowestHealth) {
            lowestHealth = hp;
            bestTarget = hero->Address;
        }
    }
    
    for (auto* hero : heroes) delete hero;
    return bestTarget;
}

// Helper: Check if jungle plant
bool IsJunglePlant(const std::string& name) {
    return (name.find("SRU_Plant") != std::string::npos);
}

// Helper: Check if epic monster (Baron, Dragon, Herald, Atakhan)
bool IsEpicMonster(const std::string& name) {
    return (name.find("SRU_Baron") != std::string::npos ||    // Baron
            name.find("SRU_Dragon") != std::string::npos ||   // Dragon
            name.find("SRU_RiftHerald") != std::string::npos || // Rift Herald
            name.find("SRU_Atakhan") != std::string::npos);   // Atakhan
}

// Helper: Check if large jungle monster (buffs and large camps)
bool IsLargeJungleMonster(const std::string& name) {
    return (name.find("SRU_Red") != std::string::npos ||      // Red Buff
            name.find("SRU_Blue") != std::string::npos ||     // Blue Buff
            name.find("SRU_Krug") != std::string::npos ||     // Big Krug
            name.find("SRU_Gromp") != std::string::npos ||    // Gromp
            name.find("SRU_Murkwolf") != std::string::npos || // Big Wolf
            name.find("SRU_Razorbeak") != std::string::npos); // Big Raptor
}

// Helper: Check if small jungle monster
bool IsSmallJungleMonster(const std::string& name) {
    return (name.find("SRU_KrugMini") != std::string::npos ||
            name.find("SRU_MurkwolfMini") != std::string::npos ||
            name.find("SRU_RazorbeakMini") != std::string::npos ||
            name.find("SRU_Crab") != std::string::npos);
}

// Helper: Get minion type (4=Melee, 5=Ranged, 6=Cannon, 7=Super)
int GetMinionType(uint64_t minionAddr) {
    if (!minionAddr) return 0;
    return *(int*)(minionAddr + Offset::LaneMinionType);
}

// Helper: Simple health prediction (accounts for attack travel time)
float PredictHealth(uint64_t unitAddr, float myDamage, float travelTime = 0.25f) {
    if (!unitAddr) return 0;
    
    float currentHP = *(float*)(unitAddr + Offset::oHealth);
    
    // Simple prediction: assume unit loses HP at constant rate
    // In real implementation, would check incoming projectiles
    // For now, just return current HP - this is placeholder
    return currentHP;
}

// Helper: Smart Farm Target Selection (Includes ShouldWait logic)
// Returns target address, or 0 if no target or if we should wait
uint64_t GetBestFarmTargetSmart(uint64_t moduleBase, uint64_t localPlayer, float range, float myDamage, bool laneClearMode, bool* outShouldWait) {
    *outShouldWait = false;
    
    // Get all minions/monsters
    auto minions = SDK::ObjectManager::GetAllMinions();
    if (minions.empty()) return 0;
    
    Vector3 myPos = GetObjPositionRaw(localPlayer);
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    float myBoundingRadius = GetMyBoundingRadius(localPlayer);
    
    SDK::GameObject* killableTarget = nullptr;
    SDK::GameObject* laneClearTarget = nullptr;
    
    float lowestKillableHP = 999999.0f;
    float highestLaneClearHP = -1.0f; // For pushing, we might want highest HP or closest? Usually closest or cannon.
    // Simple logic: Prioritize Cannon > Super > Melee > Ranged
    int priorityScore = -1;
    
    bool anyMinionAlmostDead = false;
    
    for (auto* minion : minions) {
        if (!minion || minion->Address == localPlayer) continue;
        if (minion->GetTeam() == myTeam) continue;
        if (minion->IsDead() || minion->GetHealth() <= 0) continue;
        if (!minion->IsVisible() || !minion->IsTargetable()) continue;
        
        Vector3 objPos = minion->GetPosition();
        float dist = myPos.Distance(objPos);
        // FIXED: effectiveRange = attackRange + myBoundingRadius + targetBoundingRadius
        float effectiveRange = range + myBoundingRadius + minion->GetBoundingRadius();
        if (dist > effectiveRange) continue;
        
        std::string name = minion->GetName();
        bool isPlant = IsJunglePlant(name);
        bool isSmallJungle = IsSmallJungleMonster(name);
        
        // Filter options
        if (isPlant && !Menu::attackJunglePlants) continue;
        if (isSmallJungle && !Menu::prioritizeSmallJungle) continue; // Logic might vary
        
        float health = minion->GetHealth(); // Use SDK method
        
        // Check if killable
        if (health <= myDamage) {
            if (health < lowestKillableHP) {
                lowestKillableHP = health;
                killableTarget = minion;
            }
            continue; // Ensure we pick this as killable, don't consider for wait logic (it IS the target)
        }
        
        // Check if almost dead (ShouldWait logic)
        // If minion health < 2.0 * damage, it will be killable soon.
        // We shouldn't attack another full hp minion if this one is dying.
        // BUT only checking standard minions, not jungle/turrets usually
        if (!isPlant && health < myDamage * 2.5f) { // 2.5x buffer for safety
             anyMinionAlmostDead = true;
        }
        
        // Lane Clear Logic (finding a target if no killable)
        if (laneClearMode) {
             // Priority: Epic > Large > Cannon > Super > Melee > Ranged
             int currentScore = 0;
             if (IsEpicMonster(name)) currentScore = 100;
             else if (IsLargeJungleMonster(name)) currentScore = 90;
             else if (IsSmallJungleMonster(name)) currentScore = 80; // Only if enabled
             else {
                 int type = GetMinionType(minion->Address);
                 if (type == 6) currentScore = 50; // Cannon
                 else if (type == 7) currentScore = 40; // Super
                 else if (type == 4) currentScore = 30; // Melee
                 else if (type == 5) currentScore = 10; // Ranged
             }
             
             if (currentScore > priorityScore) {
                 priorityScore = currentScore;
                 laneClearTarget = minion;
             } else if (currentScore == priorityScore) {
                 // Break ties with distance or health?
                 // Let's use HP (lower is better for clearing out waves? or higher for pushing? 
                 // Usually standard orbwalkers focus low HP to remove source of damage)
                 // Let's stick to standard behavior: Closest?
                 // Current simple logic: update if closer?
                 // For now, keep first found or maybe add distance check.
             }
        }
    }
    
    // Decision Phase
    
    // 1. Is there a killable minion?
    if (killableTarget) {
        *outShouldWait = false; // Kill it now!
        uint64_t addr = killableTarget->Address;
        // Clean up
        for (auto* m : minions) delete m;
        return addr;
    }
    
    // 2. No killable minion, but is one about to die?
    if (anyMinionAlmostDead) {
        *outShouldWait = true; // Wait for it!
        for (auto* m : minions) delete m;
        return 0; // Don't attack anything else
    }
    
    // 3. Lane Clear Mode: Attack push target
    if (laneClearMode && laneClearTarget) {
        *outShouldWait = false;
        uint64_t addr = laneClearTarget->Address;
        for (auto* m : minions) delete m;
        return addr;
    }
    
    // 4. Turret Logic (Lane Clear only)
    if (laneClearMode && Menu::prioritizeTurrets) {
        auto turrets = SDK::ObjectManager::GetTurrets();
        SDK::GameObject* bestTurret = nullptr;
        
        for (auto* turret : turrets) {
            if (!turret || turret->Address == localPlayer) continue;
            if (turret->GetTeam() == myTeam) continue;
            if (turret->IsDead() || turret->GetHealth() <= 0) continue;
            if (!turret->IsVisible() || !turret->IsTargetable()) continue;
            
            // Range check - FIXED: include bounding radii
            float turretDist = turret->GetPosition().Distance(myPos);
            float turretEffectiveRange = range + myBoundingRadius + turret->GetBoundingRadius();
            if (turretDist > turretEffectiveRange) continue;
            
            bestTurret = turret; // Just take the first valid one for now
            break; 
        }
        
        if (bestTurret) {
             *outShouldWait = false;
             uint64_t addr = bestTurret->Address;
             for (auto* m : minions) delete m;
             for (auto* t : turrets) delete t;
             return addr;
        }
        
        for (auto* t : turrets) delete t;
    }
    
    for (auto* m : minions) delete m;
    return 0;
}
        


// Helper: Calculate incoming minion damage (from allied minions attacking target)
float GetIncomingMinionDamage(uint64_t targetMinion, uint64_t localPlayer) {
    if (!targetMinion) return 0.0f;
    
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    Vector3 targetPos = GetObjPositionRaw(targetMinion);
    float incomingDamage = 0.0f;
    
    auto minions = SDK::ObjectManager::GetAllMinions();
    
    for (auto* minion : minions) {
        if (!minion || minion->Address == targetMinion) continue;
        if (minion->GetTeam() != myTeam) continue; // Only allied minions
        if (minion->IsDead()) continue;
        
        // Check if this minion is in range to attack target
        Vector3 minionPos = minion->GetPosition();
        float dist = minionPos.Distance(targetPos);
        float attackRange = minion->GetAttackRange() + 100.0f; // Minion attack range
        
        if (dist <= attackRange) {
            // Estimate minion damage (approximate)
            incomingDamage += minion->GetAttackDamage() * 0.8f; // 0.8 = attack speed factor
        }
    }
    
    for (auto* minion : minions) delete minion;
    return incomingDamage;
}

// Helper: Get best farm target for FREEZE mode (last hit at lowest safe HP)
uint64_t GetFreezeFarmTarget(uint64_t moduleBase, uint64_t localPlayer, float range, float myDamage) {
    Vector3 myPos = GetObjPositionRaw(localPlayer);
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    float myBoundingRadius = GetMyBoundingRadius(localPlayer);
    
    uint64_t bestTarget = 0;
    float lowestSafeHP = 999999.0f;
    
    auto minions = SDK::ObjectManager::GetAllMinions();
    
    for (auto* minion : minions) {
        if (!minion || minion->Address == localPlayer) continue;
        if (minion->GetTeam() == myTeam) continue;
        if (minion->IsDead() || minion->GetHealth() <= 0) continue;
        if (!minion->IsVisible() || !minion->IsTargetable()) continue;
        
        Vector3 objPos = minion->GetPosition();
        float dist = myPos.Distance(objPos);
        // FIXED: effectiveRange = attackRange + myBoundingRadius + targetBoundingRadius
        float effectiveRange = range + myBoundingRadius + minion->GetBoundingRadius();
        if (dist > effectiveRange) continue;
        
        int minionType = GetMinionType(minion->Address);
        if (minionType != 4 && minionType != 5 && minionType != 6 && minionType != 7) continue; // Only lane minions
        
        float health = minion->GetHealth();
        float incomingDamage = GetIncomingMinionDamage(minion->Address, localPlayer);
        
        // Calculate safe HP threshold (HP where we can still last hit before minions kill it)
        float safeThreshold = myDamage + (incomingDamage * 0.5f); // 0.5s buffer
        
        // Only last hit when HP is below safe threshold
        if (health <= safeThreshold && health > 0) {
            if (health < lowestSafeHP) {
                lowestSafeHP = health;
                bestTarget = minion->Address;
            }
        }
    }
    
    for (auto* minion : minions) delete minion;
    return bestTarget;
}

// Helper: Get best farm target for FAST CLEAR mode
// Priority: Cannon LastHit > Any LastHit > Ranged > Melee > Cannon
uint64_t GetFastClearTarget(uint64_t moduleBase, uint64_t localPlayer, float range, float myDamage) {
    Vector3 myPos = GetObjPositionRaw(localPlayer);
    uint8_t myTeam = *(uint8_t*)(localPlayer + Offset::TeamID);
    float myBoundingRadius = GetMyBoundingRadius(localPlayer);
    
    uint64_t cannonLastHit = 0;
    uint64_t anyLastHit = 0;
    uint64_t rangedPush = 0;
    uint64_t meleePush = 0;
    uint64_t cannonPush = 0;
    
    float lowestCannonLastHitHP = 999999.0f;
    float lowestLastHitHP = 999999.0f;
    float lowestRangedHP = 999999.0f;
    float lowestMeleeHP = 999999.0f;
    float lowestCannonHP = 999999.0f;
    
    auto minions = SDK::ObjectManager::GetAllMinions();
    
    for (auto* minion : minions) {
        if (!minion || minion->Address == localPlayer) continue;
        if (minion->GetTeam() == myTeam) continue;
        if (minion->IsDead() || minion->GetHealth() <= 0) continue;
        if (!minion->IsVisible() || !minion->IsTargetable()) continue;
        
        Vector3 objPos = minion->GetPosition();
        float dist = myPos.Distance(objPos);
        // FIXED: effectiveRange = attackRange + myBoundingRadius + targetBoundingRadius
        float effectiveRange = range + myBoundingRadius + minion->GetBoundingRadius();
        if (dist > effectiveRange) continue;
        
        float health = minion->GetHealth();
        float predictedHP = PredictHealth(minion->Address, myDamage);
        int minionType = GetMinionType(minion->Address);
        
        bool isCannon = (minionType == 6);
        bool isRanged = (minionType == 5);
        bool isMelee = (minionType == 4);
        bool canLastHit = (predictedHP <= myDamage);
        
        // Cannon last hit (highest priority)
        if (isCannon && canLastHit && predictedHP < lowestCannonLastHitHP) {
            lowestCannonLastHitHP = predictedHP;
            cannonLastHit = minion->Address;
        }
        
        // Any last hit
        if (canLastHit && predictedHP < lowestLastHitHP) {
            lowestLastHitHP = predictedHP;
            anyLastHit = minion->Address;
        }
        
        // Ranged push
        if (isRanged && health < lowestRangedHP) {
            lowestRangedHP = health;
            rangedPush = minion->Address;
        }
        
        // Melee push
        if (isMelee && health < lowestMeleeHP) {
            lowestMeleeHP = health;
            meleePush = minion->Address;
        }
        
        // Cannon push (lowest priority)
        if (isCannon && !canLastHit && health < lowestCannonHP) {
            lowestCannonHP = health;
            cannonPush = minion->Address;
        }
    }
    
    for (auto* minion : minions) delete minion;
    
    // Priority: Cannon LastHit > Any LastHit > Ranged > Melee > Cannon
    if (cannonLastHit) return cannonLastHit;
    if (anyLastHit) return anyLastHit;
    if (rangedPush) return rangedPush;
    if (meleePush) return meleePush;
    return cannonPush;
}
void IssueMoveRaw(uint64_t localPlayer, Vector3 pos) {
    if (!localPlayer || (pos.x == 0 && pos.z == 0)) return;
    
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    static void* spoof_trampoline = nullptr;
    if (!spoof_trampoline) {
        spoof_trampoline = mem::ScanModInternal((char*)"\xFF\x23", (char*)"xx", (char*)GetModuleHandleA(nullptr));
    }
    if (!spoof_trampoline) return;
    
    using fnIssueOrder = int64_t(__cdecl*)(uintptr_t, int, Vector3*, uintptr_t, bool, bool);
    fnIssueOrder issueOrder = (fnIssueOrder)(moduleBase + Offset::Function::oIssueOrder);
    
    Vector3 localPos = pos;
    spoof_call(spoof_trampoline, issueOrder, localPlayer, 2, &localPos, (uintptr_t)0, false, false);
}

// Helper: Issue attack order
void IssueAttackRaw(uint64_t localPlayer, uint64_t target) {
    if (!localPlayer || !target) return;
    
    uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
    static void* spoof_trampoline = nullptr;
    if (!spoof_trampoline) {
        spoof_trampoline = mem::ScanModInternal((char*)"\xFF\x23", (char*)"xx", (char*)GetModuleHandleA(nullptr));
    }
    if (!spoof_trampoline) return;
    
    using fnIssueOrder = int64_t(__cdecl*)(uintptr_t, int, Vector3*, uintptr_t, bool, bool);
    fnIssueOrder issueOrder = (fnIssueOrder)(moduleBase + Offset::Function::oIssueOrder);
    
    Vector3 targetPos = GetObjPositionRaw(target);
    spoof_call(spoof_trampoline, issueOrder, localPlayer, 3, &targetPos, target, true, false);
}

// Helper: Check if League window is focused
bool IsLeagueFocused() {
    HWND foreground = GetForegroundWindow();
    DWORD foregroundPid;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return (foregroundPid == GetCurrentProcessId());
}

// Helper: Check if chat is open using game's internal chat state
// Uses offset oChatState (0x193EB74) - value = 1 when chat is open, 0 when closed
// Helper function: Read chat state (C-style, no C++ objects to avoid unwinding issues)
extern "C" uint32_t ReadChatStateRaw(uint64_t offset) {
    __try {
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        return *(uint32_t*)(moduleBase + offset);
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        return 0xFFFFFFFF; // Error marker
    }
}

bool IsChatOpen() {
    if (!IsLeagueFocused()) {
        return false; // Not focused, chat can't be open
    }
    
    // Chỉ check nếu checkbox được bật
    if (!Menu::blockKeysWhenChatOpen) {
        return false; // Disabled, không block
    }
    
    // Read chat state using C-style function (no C++ objects)
    uint32_t chatState = ReadChatStateRaw(Menu::chatStateTestOffset);
    
    // Check for error
    if (chatState == 0xFFFFFFFF) {
        return false; // Error reading, assume chat is closed
    }
    
    // DEBUG: Log chat state để kiểm tra (outside __try block)
    static uint32_t lastChatState = 999;
    static DWORD lastLogTime = 0;
    DWORD now = GetTickCount();
    if (chatState != lastChatState && (now - lastLogTime > 1000)) { // Log mỗi 1 giây
        // OutputDebugStringA để không spam console
        char buf[256];
        sprintf_s(buf, sizeof(buf), "[DEBUG] ChatState @ 0x%llX: %u (0=closed, 1=open)\n", Menu::chatStateTestOffset, chatState);
        OutputDebugStringA(buf);
        lastChatState = chatState;
        lastLogTime = now;
    }
    
    return (chatState == 1); // Chat is open when value = 1
}

// Helper: Check if a key is a combo key (should be blocked when chat is open)
// Combo keys: Space, V, C, X, Z (multi-key combinations for combo scripts)
// Semi keys: Q, W, E, R, T, D, F, 1-6, etc. (single keys - should NOT be blocked, allow typing in chat)
inline bool IsComboKey(int vk) {
    return (vk == VK_SPACE || 
            vk == 0x56 || // V key
            vk == 0x43 || // C key
            vk == 0x58 || // X key
            vk == 0x5A);  // Z key
}

// ============================================================================
// ORBWALKER TIMING - Using leagueoflegends-master logic (ALL IN SECONDS)
// ============================================================================
namespace OrbwalkerTiming {
    float lastAttackTime = 0.0f;  // In SECONDS (gameTime when attack started)
    float lastActionTime = 0.0f;  // In SECONDS (for click delay)
    float cachedPing = 30.0f;     // Cached ping in ms (update every 500ms)
    DWORD lastPingUpdate = 0;     // Last ping update time
    
    // Get ping with caching (update every 500ms to avoid frequent calls)
    float GetCachedPing() {
        DWORD currentTime = GetTickCount();
        if (currentTime - lastPingUpdate > 500) {
            cachedPing = GetPing(); // Update ping
            lastPingUpdate = currentTime;
        }
        return cachedPing;
    }
    
    // Get ping compensation in SECONDS (ping affects timing, especially windup)
    // Higher ping = need more buffer time for attack to register on server
    float GetPingCompensation() {
        float pingMs = GetCachedPing();
        return (pingMs / 1000.0f) * 0.5f; // Half of ping in seconds (conservative estimate)
    }
    
    // leagueoflegends-master: StopOrbwalk (improved with ping compensation)
    // Returns true if we should NOT do any action (still in windup)
    bool IsWindingUp(float gameTime, float attackWindup) {
        float pingComp = GetPingCompensation();
        return gameTime < lastAttackTime + attackWindup + Menu::windupBuffer + pingComp;
    }
    
    // leagueoflegends-master: IsReloading (improved with ping compensation)
    // Returns true if we cannot attack yet (still on cooldown)
    bool IsReloading(float gameTime, float attackDelay) {
        float pingComp = GetPingCompensation();
        return gameTime < lastAttackTime + attackDelay - Menu::attackBeforeCanAttack - pingComp;
    }
    
    // leagueoflegends-master: CanDoAction (click delay check with ping compensation)
    bool CanDoAction(float gameTime) {
        float pingComp = GetPingCompensation();
        if (gameTime < lastActionTime + Menu::clickDelay + pingComp) return false;
        lastActionTime = gameTime;
        return true;
    }
}

// Orbwalker Hero Thread (SPACE key - Combo)
// REWRITTEN to use leagueoflegends-master logic exactly
DWORD WINAPI ThreadOrbwalkerHero(LPVOID lpParam) {
    Sleep(3000);
    
    while (true) {
        if (!Menu::orbwalkerEnabled) { Sleep(50); continue; }
        if (!IsLeagueFocused()) { Sleep(100); continue; }
        if (IsChatOpen()) { Sleep(50); continue; } // Don't orbwalk when chat is open
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        
        if (!localPlayer) { Sleep(100); continue; }
        if (!(GetAsyncKeyState(VK_SPACE) & 0x8000)) { Sleep(10); continue; }
        
        // Get gameTime in SECONDS (like leagueoflegends-master)
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        // Get attack timing using FUNCTION CALLS (in SECONDS)
        float attackDelay = GetAttackDelay(localPlayer);
        float attackWindup = GetAttackWindup(localPlayer);
        
        // Get RealAttackRange = attackRange + myBoundingRadius
        float realAttackRange = GetRealAttackRangeRaw(localPlayer);
        
        // leagueoflegends-master: StopOrbwalk - if still in windup, don't do anything
        if (OrbwalkerTiming::IsWindingUp(gameTime, attackWindup)) {
            Sleep(5);
            continue;
        }
        
        // leagueoflegends-master: IsReloading - if can't attack yet, just move
        bool isReloading = OrbwalkerTiming::IsReloading(gameTime, attackDelay);
        
        if (isReloading) {
            // Can't attack, just move (Idle)
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                Vector3 mousePos = GetMouseWorldPosRaw();
                if (mousePos.x != 0 || mousePos.z != 0) {
                    IssueMoveRaw(localPlayer, mousePos);
                }
            }
            Sleep(5);
            continue;
        }
        
        // Can attack - find target using TargetSelector with Menu mode
        uint64_t target = 0;
        SDK::TargetSelectorMode tsMode = SDK::Orbwalker::GetTSMode();
        SDK::GameObject* targetObj = SDK::TargetSelector::GetTarget(realAttackRange, tsMode);
        if (targetObj) {
            target = targetObj->Address;
            delete targetObj;
        }
        
        if (target) {
            // Attack target
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                IssueAttackRaw(localPlayer, target);
                OrbwalkerTiming::lastAttackTime = gameTime;  // Record attack time in SECONDS
                
                // Debug log
                std::ofstream log("attack_debug.txt", std::ios_base::app);
                if (log.is_open()) {
                    Vector3 myPos = GetObjPositionRaw(localPlayer);
                    Vector3 targetPos = GetObjPositionRaw(target);
                    float dist = myPos.Distance(targetPos);
                    log << "[ATTACK] gameTime=" << gameTime 
                        << " | dist=" << dist 
                        << " | realRange=" << realAttackRange
                        << " | attackDelay=" << attackDelay
                        << " | attackWindup=" << attackWindup
                        << std::endl;
                    log.close();
                }
            }
        } else {
            // No target, move (Idle)
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                Vector3 mousePos = GetMouseWorldPosRaw();
                if (mousePos.x != 0 || mousePos.z != 0) {
                    IssueMoveRaw(localPlayer, mousePos);
                }
            }
        }
        
        Sleep(5);
    }
    return 0;
}

// Orbwalker Minion Thread (V/X/C keys - LaneClear/LastHit/Harass)
// REWRITTEN to use leagueoflegends-master logic (SECONDS)
DWORD WINAPI ThreadOrbwalkerMinion(LPVOID lpParam) {
    Sleep(3000);
    
    while (true) {
        if (!Menu::orbwalkerEnabled) { Sleep(50); continue; }
        if (!IsLeagueFocused()) { Sleep(100); continue; }
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        
        if (!localPlayer) { Sleep(100); continue; }
        
        // Check which mode is active
        bool isLaneClear = (GetAsyncKeyState(0x56) & 0x8000); // V key
        bool isLastHit = (GetAsyncKeyState(0x58) & 0x8000);   // X key
        bool isHarass = (GetAsyncKeyState(0x43) & 0x8000);    // C key
        
        if (!isLaneClear && !isLastHit && !isHarass) { 
            Sleep(10); 
            continue; 
        }
        
        // Block combo keys (VCXZ) when chat is open (check AFTER key check)
        if (IsChatOpen()) { Sleep(50); continue; }
        
        // Get gameTime in SECONDS (like leagueoflegends-master)
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        // Get attack timing using FUNCTION CALLS (in SECONDS)
        float attackDelay = GetAttackDelay(localPlayer);
        float attackWindup = GetAttackWindup(localPlayer);
        
        // Get RealAttackRange = attackRange + myBoundingRadius
        float realAttackRange = GetRealAttackRangeRaw(localPlayer);
        float attackDamage = *(float*)(localPlayer + Offset::DamageBase) + *(float*)(localPlayer + Offset::DamageBonus);
        
        // leagueoflegends-master: StopOrbwalk - if still in windup, don't do anything
        if (OrbwalkerTiming::IsWindingUp(gameTime, attackWindup)) {
            Sleep(5);
            continue;
        }
        
        // leagueoflegends-master: IsReloading - if can't attack yet, just move
        bool isReloading = OrbwalkerTiming::IsReloading(gameTime, attackDelay);
        
        if (isReloading) {
            // Can't attack, just move (Idle)
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                Vector3 mousePos = GetMouseWorldPosRaw();
                if (mousePos.x != 0 || mousePos.z != 0) {
                    IssueMoveRaw(localPlayer, mousePos);
                }
            }
            Sleep(5);
            continue;
        }
        
        // Find target based on mode
        uint64_t target = 0;
        bool shouldWait = false;
        
        if (isHarass) {
            // Harass (C key): Attack heroes + last hit minions
            if (!Menu::farmOverHarass) {
                target = GetBestHeroTargetRaw(moduleBase, localPlayer, realAttackRange);
            }
            if (!target) {
                target = GetBestFarmTargetSmart(moduleBase, localPlayer, realAttackRange, attackDamage, false, &shouldWait);
            }
            if (!target && Menu::farmOverHarass && !shouldWait) {
                target = GetBestHeroTargetRaw(moduleBase, localPlayer, realAttackRange);
            }
        } else if (isLastHit) {
            // X key - LastHit only
            target = GetBestFarmTargetSmart(moduleBase, localPlayer, realAttackRange, attackDamage, false, &shouldWait);
        } else if (isLaneClear) {
            // V key - LaneClear (push)
            target = GetBestFarmTargetSmart(moduleBase, localPlayer, realAttackRange, attackDamage, true, &shouldWait);
        }
        
        if (target) {
            // Attack target
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                IssueAttackRaw(localPlayer, target);
                OrbwalkerTiming::lastAttackTime = gameTime;  // Record attack time in SECONDS
            }
        } else {
            // No target, move (Idle)
            if (OrbwalkerTiming::CanDoAction(gameTime)) {
                Vector3 mousePos = GetMouseWorldPosRaw();
                if (mousePos.x != 0 || mousePos.z != 0) {
                    IssueMoveRaw(localPlayer, mousePos);
                }
            }
        }
        
        Sleep(5);
    }
    return 0;
}

// Orbwalker Flee Thread (Z key - Move only, no attacks)
// REWRITTEN to use leagueoflegends-master logic (SECONDS)
DWORD WINAPI ThreadOrbwalkerFlee(LPVOID lpParam) {
    Sleep(3000);
    
    while (true) {
        if (!Menu::orbwalkerEnabled) { Sleep(50); continue; }
        if (!IsLeagueFocused()) { Sleep(100); continue; }
        
        uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
        uint64_t localPlayer = *(uint64_t*)(moduleBase + Offset::oLocalPlayer);
        
        if (!localPlayer) { Sleep(100); continue; }
        if (!(GetAsyncKeyState(0x5A) & 0x8000)) { Sleep(10); continue; } // Z key
        
        // Block combo key (Z) when chat is open (check AFTER key check)
        if (IsChatOpen()) { Sleep(50); continue; }
        
        float gameTime = *(float*)(moduleBase + Offset::oGametime);
        
        // Flee mode: Only move, no attacks
        if (OrbwalkerTiming::CanDoAction(gameTime)) {
            Vector3 mousePos = GetMouseWorldPosRaw();
            if (mousePos.x != 0 || mousePos.z != 0) {
                IssueMoveRaw(localPlayer, mousePos);
            }
        }
        
        Sleep(5);
    }
    return 0;
}

// ============================================================================
// NEW: AUTO CAST & SCAN THREAD
// ============================================================================
DWORD WINAPI ThreadAutoCastScan(LPVOID lpParam) {
    Sleep(3000);
    bool lastState = false;
    float lastCastTime = 0.0f;
    
    while (true) {
        bool currentState = Menu::autoCastAndScan;
        
        // CASE 1: Đang bật - Thực hiện Cast Spell liên tục
        if (currentState) {
            uintptr_t moduleBase = (uintptr_t)GetModuleHandle(NULL);
            float gameTime = 0.0f;
            if (moduleBase) {
                gameTime = *(float*)(moduleBase + Offset::oGametime);
            }
            
            // Cast phím W mỗi 1.5 giây (đủ thời gian cho hầu hết các skillshot)
            if (gameTime > lastCastTime + 1.5f) {
                // Sử dụng phím W là skillshot phổ biến của nhiều champs (Smolder, Jinx, Ezreal)
                keybd_event('W', 0, 0, 0);
                Sleep(50);
                keybd_event('W', 0, KEYEVENTF_KEYUP, 0);
                lastCastTime = gameTime;
            }
        }
        
        // CASE 2: Vừa bỏ check - Trigger SCAN ngay lập tức
        if (lastState && !currentState) {
            // Đợi 0.2s để missile vừa cast có thời gian spawn
            Sleep(200);
            ScanMissileOffsets();
        }
        
        lastState = currentState;
        Sleep(100);
    }
    return 0;
}

// Init Thread
DWORD WINAPI ThreadGUI(LPVOID lpReserved) {
    Render::InitCircle();
    
    bool init_hook = false;
    do {
        if (kiero::init(kiero::RenderType::D3D11) == kiero::Status::Success) {
            kiero::bind(8, (void**)& oPresent, hkPresent);
            init_hook = true;
        }
        Sleep(10);
    } while (!init_hook);
    return TRUE;
}

// Terminator Thread (Simple)
DWORD WINAPI ThreadTerminate(LPVOID lpParam) {
    while (true) {
        if (GetAsyncKeyState(VK_END) & 1) {
            kiero::shutdown();
            break;
        }
        Sleep(100);
    }
    FreeLibraryAndExitThread((HMODULE)lpParam, 0);
    return TRUE;
}

// ============================================================================
// THREAD HIDING - Evade basic anti-cheat detection
// From leagueoflegends-master
// ============================================================================
static HMODULE g_hLocalModule = nullptr;
static bool g_bEject = false;

bool WINAPI HideThread(const HANDLE hThread) noexcept
{
    __try {
        using FnSetInformationThread = NTSTATUS(NTAPI*)(HANDLE ThreadHandle, UINT ThreadInformationClass, PVOID ThreadInformation, ULONG ThreadInformationLength);
        FnSetInformationThread NtSetInformationThread = reinterpret_cast<FnSetInformationThread>(
            ::GetProcAddress(::GetModuleHandleA("ntdll.dll"), "NtSetInformationThread")
        );

        if (!NtSetInformationThread)
            return false;

        // ThreadHideFromDebugger = 0x11
        NTSTATUS status = NtSetInformationThread(hThread, 0x11u, nullptr, 0ul);
        if (status == 0x00000000)
            return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
    return false;
}

// ============================================================================
// WAIT FOR GAME READY - Don't init until game is actually loaded
// ============================================================================
bool WaitForGameReady(int timeoutSeconds = 60)
{
    std::ofstream logFile("orbwalker_debug.txt", std::ios_base::app);
    logFile << "Waiting for game to be ready..." << std::endl;
    
    int waited = 0;
    while (waited < timeoutSeconds * 10)
    {
        __try {
            float gameTime = SDK::Game::GetTime();
            if (gameTime > 3.0f) {
                logFile << "Game ready! GameTime: " << gameTime << std::endl;
                logFile.close();
                return true;
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            // Memory not ready yet
        }
        
        Sleep(100);
        waited++;
    }
    
    logFile << "Timeout waiting for game" << std::endl;
    logFile.close();
    return false;
}

// ============================================================================
// EJECT THREAD - Safe DLL unload
// ============================================================================
DWORD __stdcall EjectThread(LPVOID lpParameter)
{
    Sleep(100);
    kiero::shutdown();
    FreeLibraryAndExitThread(g_hLocalModule, 0);
    return 0;
}

// ============================================================================
// MAIN INJECTION THREAD - All initialization happens here
// ============================================================================
DWORD __stdcall OnInject(LPVOID lpReserved)
{
    // Debug File Init
    std::ofstream logFile("orbwalker_debug.txt", std::ios_base::trunc);
    logFile << "--- INTERNAL ORBWALKER DEBUG LOG START ---" << std::endl;
    logFile << "Injected successfully!" << std::endl;
    logFile.close();
    
    Sleep(100);
    
    // Hide this thread from debugger
    if (HideThread(::GetCurrentThread())) {
        std::ofstream log("orbwalker_debug.txt", std::ios_base::app);
        log << "Thread hidden from debugger" << std::endl;
        log.close();
    }
    
    // Wait for game to be ready
    if (!WaitForGameReady(60)) {
        std::ofstream log("orbwalker_debug.txt", std::ios_base::app);
        log << "Game not ready, aborting..." << std::endl;
        log.close();
        return 0;
    }
    
    // Start all worker threads
    CreateThread(nullptr, 0, ThreadGUI, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadMatrix, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadOrbwalkerHero, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadOrbwalkerMinion, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadOrbwalkerFlee, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadAutoCastScan, g_hLocalModule, 0, nullptr);
    CreateThread(nullptr, 0, ThreadTerminate, g_hLocalModule, 0, nullptr);
    
    // Wait for eject signal
    while (!g_bEject)
    {
        Sleep(5);
        if (GetAsyncKeyState(VK_DELETE) & 1) {
            g_bEject = true;
        }
    }
    
    // Clean eject
    CreateThread(nullptr, 0, EjectThread, nullptr, 0, nullptr);
    
    return 0;
}

// ============================================================================
// DLL MAIN ENTRY POINT
// ============================================================================
BOOL WINAPI DllMain(HMODULE hMod, DWORD dwReason, LPVOID lpReserved) {
    switch (dwReason)
    {
    case DLL_PROCESS_ATTACH:
        Console::Init();
        g_hLocalModule = hMod;
        DisableThreadLibraryCalls(hMod);
        CreateThread(nullptr, 0, OnInject, hMod, 0, nullptr);
        break;
    case DLL_PROCESS_DETACH:
        Console::Close();
        kiero::shutdown();
        break;
    }
    return TRUE;
}

// ============================================================================
// MANUAL MAP ENTRY POINT
// ============================================================================
extern "C" __declspec(dllexport) void ManualMapEntry(HMODULE hMod) {
    g_hLocalModule = hMod;
    DisableThreadLibraryCalls(hMod);
    CreateThread(nullptr, 0, OnInject, hMod, 0, nullptr);
}

