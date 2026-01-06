#pragma once
#include <Windows.h>
#include <cstdint>
#include <string>
#include <vector>
#include "Offsets.h"
#include "../Vector.h"

namespace SDK
{
    // Missile/Projectile class for turret aggro detection
    // Based on leagueoflegends-master and NewOrbwalker.cs patterns
    class Missile
    {
    public:
        uint64_t Address;

        Missile(uint64_t addr) : Address(addr) {}

        bool IsValid() {
            return Address != 0;
        }

        // Get SpellInfo pointer from missile (INDIRECT ACCESS)
        // Structure: Missile[0x1F0] -> SpellCast* -> SpellCast[0xD8] -> SpellInfo*
        uint64_t GetSpellInfo() {
            if (!Address) return 0;
            __try {
                // Step 1: Get SpellCast pointer from Missile
                uint64_t spellCast = *(uint64_t*)(Address + Offset::oMissileSpellCast);
                if (!spellCast || spellCast < 0x10000) return 0;
                
                // Step 2: Get SpellInfo pointer from SpellCast
                uint64_t spellInfo = *(uint64_t*)(spellCast + Offset::oSpellCastSpellInfo);
                return spellInfo;
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        // Get source object index (who fired this missile)
        // Read from SpellInfo + oSpellInfoSrcIndex
        int GetSourceIndex() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return 0;
            __try {
                return *(int*)(spellInfo + Offset::oSpellInfoSrcIndex);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        // Get target object network ID (who is being targeted)
        // Read from SpellInfo + oSpellInfoTargetIndex
        int GetTargetIndex() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return 0;
            __try {
                return *(int*)(spellInfo + Offset::oSpellInfoTargetIndex);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return 0;
            }
        }

        // Get start position from SpellInfo
        Vector3 GetStartPos() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return Vector3(0, 0, 0);
            __try {
                return *(Vector3*)(spellInfo + Offset::oSpellInfoStartPos);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return Vector3(0, 0, 0);
            }
        }

        // Get end position from SpellInfo
        Vector3 GetEndPos() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return Vector3(0, 0, 0);
            __try {
                return *(Vector3*)(spellInfo + Offset::oSpellInfoEndPos);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return Vector3(0, 0, 0);
            }
        }

        // Check if this is an auto attack (not a spell)
        bool IsAutoAttack() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return false;
            __try {
                return *(bool*)(spellInfo + Offset::oSpellInfoIsAuto);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return false;
            }
        }

        // Get missile name (from SpellInfo -> SpellData -> Name)
        std::string GetName() {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo) return "";
            
            __try {
                // SpellInfo -> SpellData at offset 0x0
                uint64_t spellData = *(uint64_t*)(spellInfo + Offset::oSpellInfoSpellData);
                if (!spellData) return "";
                
                // SpellData -> Name at offset 0x8
                char* namePtr = (char*)(spellData + Offset::oSpellDataName);
                if (!namePtr) return "";
                
                return std::string(namePtr);
            }
            __except(EXCEPTION_EXECUTE_HANDLER) {
                return "";
            }
        }

        // Check if this missile is from a turret
        bool IsTurretShot() {
            std::string name = GetName();
            // Turret missile names contain "Turret" or specific patterns
            return (name.find("TurretAttack") != std::string::npos ||
                    name.find("Turret") != std::string::npos ||
                    name.find("Obelisk") != std::string::npos);  // Nexus turret
        }
    };

    // MissileManager - Get all active missiles
    class MissileManager
    {
    public:
        static uint64_t GetModuleBase() {
            return (uint64_t)GetModuleHandle(NULL);
        }

        static std::vector<Missile*> GetMissiles() {
            std::vector<Missile*> missiles;
            uint64_t missileManager = *(uint64_t*)(GetModuleBase() + Offset::oMissileList);
            if (!missileManager) return missiles;

            // Standard list structure
            uint64_t arrayPtr = *(uint64_t*)(missileManager + 0x08);
            int size = *(int*)(missileManager + 0x10);

            if (size > 500 || size < 0) size = 0;

            for (int i = 0; i < size; i++) {
                uint64_t objAddr = *(uint64_t*)(arrayPtr + (i * 0x8));
                if (objAddr) {
                    missiles.push_back(new Missile(objAddr));
                }
            }
            return missiles;
        }

        // Check if local player has turret aggro (turret is shooting at us)
        static bool HasTurretAggro(int localNetId) {
            auto missiles = GetMissiles();
            bool hasAggro = false;

            for (auto* missile : missiles) {
                if (missile->IsTurretShot() && missile->GetTargetIndex() == localNetId) {
                    hasAggro = true;
                    break;
                }
            }

            // Cleanup
            for (auto* m : missiles) delete m;
            return hasAggro;
        }

        // Check if a specific minion has turret aggro
        static bool MinionHasTurretAggro(int minionNetId) {
            auto missiles = GetMissiles();
            bool hasAggro = false;

            for (auto* missile : missiles) {
                if (missile->IsTurretShot() && missile->GetTargetIndex() == minionNetId) {
                    hasAggro = true;
                    break;
                }
            }

            // Cleanup
            for (auto* m : missiles) delete m;
            return hasAggro;
        }

        // Count turret shots targeting a minion (for farm under turret logic)
        static int CountTurretShotsOnMinion(int minionNetId) {
            auto missiles = GetMissiles();
            int count = 0;

            for (auto* missile : missiles) {
                if (missile->IsTurretShot() && missile->GetTargetIndex() == minionNetId) {
                    count++;
                }
            }

            // Cleanup
            for (auto* m : missiles) delete m;
            return count;
        }
    };
}
