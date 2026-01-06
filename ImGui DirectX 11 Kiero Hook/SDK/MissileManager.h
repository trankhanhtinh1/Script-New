#pragma once
#include "Offsets.h"
#include "../Vector.h"
#include <vector>
#include <string>
#include <Windows.h>

namespace SDK
{
    // Helper function to safely check if memory is readable
    inline bool IsReadable(uint64_t addr, size_t size = 4) {
        if (!addr || addr < 0x10000) return false;
        return !IsBadReadPtr((void*)addr, size);
    }

    // ============================================================================
    // Missile Object - Represents a projectile in the game
    // ============================================================================
    class Missile
    {
    public:
        uint64_t Address;

        Missile(uint64_t addr) : Address(addr) {}

        bool IsValid() const {
            if (!Address || Address < 0x10000) return false;
            if (!IsReadable(Address + Offset::oMissileNetId, 4)) return false;
            return true;
        }

        // Get Missile NetID
        uint32_t GetNetId() const {
            if (!IsReadable(Address + Offset::oMissileNetId, 4)) return 0;
            return *(uint32_t*)(Address + Offset::oMissileNetId);
        }

        // Get Current Position - PTR[0x028] + 0x088
        Vector3 GetPosition() const {
            if (!Address || Address < 0x10000) return Vector3(0, 0, 0);
            
            // Follow pointer at 0x028 to SpellInfo structure
            if (IsReadable(Address + 0x028, 8)) {
                uint64_t spellInfo = *(uint64_t*)(Address + 0x028);
                if (spellInfo && spellInfo > 0x10000 && IsReadable(spellInfo + 0x088, 12)) {
                    float x = *(float*)(spellInfo + 0x088);
                    float y = *(float*)(spellInfo + 0x088 + 4);
                    float z = *(float*)(spellInfo + 0x088 + 8);
                    return Vector3(x, y, z);
                }
            }
            return Vector3(0, 0, 0);
        }

        // Get Start Position - PTR[0x028] + 0x0F0
        Vector3 GetStartPosition() const {
            if (!Address || Address < 0x10000) return Vector3(0, 0, 0);
            
            // Follow pointer at 0x028 to SpellInfo structure
            if (IsReadable(Address + 0x028, 8)) {
                uint64_t spellInfo = *(uint64_t*)(Address + 0x028);
                if (spellInfo && spellInfo > 0x10000 && IsReadable(spellInfo + 0x0F0, 12)) {
                    float x = *(float*)(spellInfo + 0x0F0);
                    float y = *(float*)(spellInfo + 0x0F0 + 4);
                    float z = *(float*)(spellInfo + 0x0F0 + 8);
                    return Vector3(x, y, z);
                }
            }
            return Vector3(0, 0, 0);
        }

        // Get End Position - PTR[0x028] + 0x0FC
        Vector3 GetEndPosition() const {
            if (!Address || Address < 0x10000) return Vector3(0, 0, 0);
            
            // Follow pointer at 0x028 to SpellInfo structure
            if (IsReadable(Address + 0x028, 8)) {
                uint64_t spellInfo = *(uint64_t*)(Address + 0x028);
                if (spellInfo && spellInfo > 0x10000 && IsReadable(spellInfo + 0x0FC, 12)) {
                    float x = *(float*)(spellInfo + 0x0FC);
                    float y = *(float*)(spellInfo + 0x0FC + 4);
                    float z = *(float*)(spellInfo + 0x0FC + 8);
                    return Vector3(x, y, z);
                }
            }
            return Vector3(0, 0, 0);
        }

        // Get Caster NetID - PTR[0x1D8] + 0x110 (from log analysis)
        uint32_t GetCasterNetId() const {
            if (!Address || Address < 0x10000) return 0;
            
            // Method 1: PTR[0x1D8] + 0x110 (found in logs for player missiles)
            if (IsReadable(Address + 0x1D8, 8)) {
                uint64_t spellInfo = *(uint64_t*)(Address + 0x1D8);
                if (spellInfo && spellInfo > 0x10000 && IsReadable(spellInfo + 0x110, 4)) {
                    uint32_t netId = *(uint32_t*)(spellInfo + 0x110);
                    if (netId > 0x40000000) return netId; // Valid missile NetID range
                }
            }
            
            // Method 2: PTR[0x0C8] + 0x0B0 (alternative found in logs)
            if (IsReadable(Address + 0x0C8, 8)) {
                uint64_t ptr = *(uint64_t*)(Address + 0x0C8);
                if (ptr && ptr > 0x10000 && IsReadable(ptr + 0x0B0, 4)) {
                    uint32_t netId = *(uint32_t*)(ptr + 0x0B0);
                    if (netId > 0x40000000) return netId;
                }
            }
            
            // Method 3: PTR[0x0C8] + 0x0E0 (another alternative)
            if (IsReadable(Address + 0x0C8, 8)) {
                uint64_t ptr = *(uint64_t*)(Address + 0x0C8);
                if (ptr && ptr > 0x10000 && IsReadable(ptr + 0x0E0, 4)) {
                    uint32_t netId = *(uint32_t*)(ptr + 0x0E0);
                    if (netId > 0x40000000) return netId;
                }
            }
            
            return 0;
        }

        // Get Source (Caster) NetID - using corrected offsets
        uint32_t GetSourceNetId() const {
            return GetCasterNetId();
        }

        // Get SpellInfo pointer - using 0x1D8 offset
        uint64_t GetSpellInfo() const {
            if (!IsReadable(Address + 0x1D8, 8)) return 0;
            return *(uint64_t*)(Address + 0x1D8);
        }

        // Get SpellData pointer - from SpellInfo + 0x18
        uint64_t GetSpellData() const {
            uint64_t spellInfo = GetSpellInfo();
            if (!spellInfo || spellInfo < 0x10000) return 0;
            if (!IsReadable(spellInfo + 0x18, 8)) return 0;
            return *(uint64_t*)(spellInfo + 0x18);
        }

        // Get Spell Name - From SpellInfo -> SpellData -> Name (offset 0x1D8 from scan)
        std::string GetSpellName() const {
            if (!Address || Address < 0x10000) return "";
            
            // Method 1: SpellInfo at 0x1D8 -> SpellData at 0x18 -> Name at 0x8 (from scan)
            if (IsReadable(Address + 0x1D8, 8)) {
                uint64_t spellInfo = *(uint64_t*)(Address + 0x1D8);
                if (spellInfo && spellInfo > 0x10000 && IsReadable(spellInfo + 0x18, 8)) {
                    uint64_t spellData = *(uint64_t*)(spellInfo + 0x18);
                    if (spellData && spellData > 0x10000 && IsReadable(spellData + 0x8, 8)) {
                        uint64_t namePtr = *(uint64_t*)(spellData + 0x8);
                        if (namePtr && namePtr > 0x10000 && IsReadable(namePtr, 64)) {
                            char name[64] = { 0 };
                            for (int i = 0; i < 63; i++) {
                                char c = *(char*)(namePtr + i);
                                if (c == 0 || c < 32 || c > 126) break;
                                name[i] = c;
                            }
                            if (strlen(name) > 2) {
                                return std::string(name);
                            }
                        }
                    }
                }
            }
            
            // Method 2: Try direct string offsets
            uint64_t nameOffsets[] = { 0x040, 0x0C0, 0x0F0 };
            for (uint64_t offset : nameOffsets) {
                if (IsReadable(Address + offset, 32)) {
                    char* directName = (char*)(Address + offset);
                    if ((directName[0] >= 'A' && directName[0] <= 'Z') || 
                        (directName[0] >= 'a' && directName[0] <= 'z')) {
                        char name[64] = { 0 };
                        for (int i = 0; i < 63; i++) {
                            char c = directName[i];
                            if (c == 0 || c < 32 || c > 126) break;
                            name[i] = c;
                        }
                        if (strlen(name) > 3) {
                            return std::string(name);
                        }
                    }
                }
            }
            
            return "";
        }

        // Check if missile is from minion
        bool IsFromMinion() const {
            std::string name = GetSpellName();
            if (name.empty()) return false;
            
            // Minion attack patterns
            if (name.find("SRU_Order") != std::string::npos) return true;
            if (name.find("Minion") != std::string::npos) return true;
            
            return false;
        }

        // Check if missile is from turret
        bool IsFromTurret() const {
            std::string name = GetSpellName();
            if (name.empty()) return false;
            
            // Turret attack patterns
            if (name.find("Turret") != std::string::npos) return true;
            if (name.find("TurretAttack") != std::string::npos) return true;
            
            return false;
        }

        // Check if missile is a champion spell
        bool IsChampionSpell() const {
            std::string name = GetSpellName();
            if (name.empty()) return false;
            
            // Not minion or turret = champion spell
            return !IsFromMinion() && !IsFromTurret();
        }
    };

    // ============================================================================
    // Missile Manager - Manages all active missiles in the game
    // ============================================================================
    class MissileManager
    {
    public:
        // Get all active missiles (enemy skillshots, minion attacks, turret attacks)
        // MissileManager contains ENEMY missiles - useful for dodging and farm prediction
        static std::vector<Missile*> GetMissiles() {
            std::vector<Missile*> missiles;
            
            uint64_t moduleBase = (uint64_t)GetModuleHandle(NULL);
            if (!moduleBase) return missiles;
            
            if (!IsReadable(moduleBase + Offset::oMissileManager, 8)) return missiles;
            uint64_t managerPtr = *(uint64_t*)(moduleBase + Offset::oMissileManager);
            if (!managerPtr || managerPtr < 0x10000) return missiles;
            
            // Missile list structure:
            // managerPtr + 0x08 = array pointer
            // managerPtr + 0x10 = size
            if (!IsReadable(managerPtr + 0x08, 8)) return missiles;
            if (!IsReadable(managerPtr + 0x10, 4)) return missiles;
            
            uint64_t arrayPtr = *(uint64_t*)(managerPtr + 0x08);
            int size = *(int*)(managerPtr + 0x10);
            
            if (!arrayPtr || arrayPtr < 0x10000 || size <= 0 || size > 100) {
                return missiles;
            }
            
            for (int i = 0; i < size && i < 100; i++) {
                if (!IsReadable(arrayPtr + i * 0x8, 8)) continue;
                
                uint64_t missileAddr = *(uint64_t*)(arrayPtr + i * 0x8);
                if (missileAddr && missileAddr > 0x10000) {
                    Missile* missile = new Missile(missileAddr);
                    uint32_t netId = missile->GetNetId();
                    // Valid NetID range for missiles: 0x40000000+
                    if (netId > 0x40000000) {
                        missiles.push_back(missile);
                    } else {
                        delete missile;
                    }
                }
            }
            
            return missiles;
        }

        // Get missiles targeting a specific NetID
        static std::vector<Missile*> GetMissilesTargetingNetId(uint32_t targetNetId) {
            std::vector<Missile*> result;
            auto missiles = GetMissiles();
            
            for (auto missile : missiles) {
                // For now, return all missiles - target detection needs SpellInfo parsing
                result.push_back(missile);
            }
            
            return result;
        }

        // Cleanup missiles vector
        static void FreeMissiles(std::vector<Missile*>& missiles) {
            for (auto m : missiles) {
                delete m;
            }
            missiles.clear();
        }
    };
}
