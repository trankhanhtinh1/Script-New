#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <string>
#include <vector>
#include <set>
#include <algorithm>
#include "Offsets.h"

namespace SDK
{
    // ============================================================================
    // BUFF TYPES (From EnsoulSharp.SDK)
    // ============================================================================
    enum class BuffType : int
    {
        Internal = 0,
        Aura = 1,
        CombatEnchancer = 2,
        CombatDehancer = 3,
        SpellShield = 4,
        Stun = 5,
        Invisibility = 6,
        Silence = 7,
        Taunt = 8,
        Berserk = 9,
        Polymorph = 10,
        Slow = 11,
        Snare = 12,
        Damage = 13,
        Heal = 14,
        Haste = 15,
        SpellImmunity = 16,
        PhysicalImmunity = 17,
        Invulnerability = 18,
        AttackSpeedSlow = 19,
        NearSight = 20,
        Fear = 21,
        Charm = 22,
        Poison = 23,
        Suppression = 24,
        Blind = 25,
        Counter = 26,
        Currency = 27,
        Shred = 28,
        Flee = 29,
        Knockup = 30,
        Knockback = 31,
        Disarm = 32,
        Grounded = 33,
        Drowsy = 34,
        Asleep = 35,
        Obscured = 36,
        ClickProofToEnemies = 37,
        UnKillable = 38
    };

    // ============================================================================
    // BUFF INSTANCE - Represents a single buff on a unit
    // ============================================================================
    class BuffInstance
    {
    public:
        uint64_t Address;
        
        BuffInstance(uint64_t address) : Address(address) {}
        BuffInstance() : Address(0) {}
        
        bool IsValid() const { return Address != 0; }
        
        // Get buff name from BuffScript
        // Pattern discovered via debug: BuffInstance+0x10 -> BuffScript+0x08 = char* name
        std::string GetName() const {
            if (!IsValid()) return "";
            
            // Read BuffScript pointer at +0x10
            uint64_t buffScript = *(uint64_t*)(Address + Offset::oBuffInstanceScript);
            if (!buffScript || buffScript < 0x10000 || buffScript > 0x7FFFFFFFFFFF) return "";
            
            // Read name string pointer at BuffScript+0x08
            char* namePtr = *(char**)(buffScript + Offset::oBuffScriptName);
            if (!namePtr || (uint64_t)namePtr < 0x10000 || (uint64_t)namePtr > 0x7FFFFFFFFFFF) return "";
            
            // Safe string read with length limit
            std::string result;
            for (int i = 0; i < 64; i++) {
                char c = namePtr[i];
                if (c == 0) break;
                if (c < 32 || c > 126) return ""; // Invalid char = bad pointer
                result += c;
            }
            return result;
        }
        
        BuffType GetType() const {
            if (!IsValid()) return BuffType::Internal;
            return (BuffType)(*(int*)(Address + Offset::oBuffInstanceType));
        }
        
        float GetStartTime() const {
            if (!IsValid()) return 0.0f;
            return *(float*)(Address + Offset::oBuffInstanceStartTime);
        }
        
        float GetEndTime() const {
            if (!IsValid()) return 0.0f;
            return *(float*)(Address + Offset::oBuffInstanceEndTime);
        }
        
        int GetStackCount() const {
            if (!IsValid()) return 0;
            return *(int*)(Address + Offset::oBuffInstanceStackCount);
        }
        
        int GetCount() const {
            if (!IsValid()) return 0;
            return *(int*)(Address + Offset::oBuffInstanceCount);
        }
        
        bool IsActive(float gameTime) const {
            if (!IsValid()) return false;
            // Use StackCount (verified: 1 for active buffs) instead of Count (often 0)
            return GetStackCount() > 0 && GetEndTime() > gameTime;
        }
        
        float GetRemainingTime(float gameTime) const {
            if (!IsValid()) return 0.0f;
            float remaining = GetEndTime() - gameTime;
            return remaining > 0 ? remaining : 0.0f;
        }
    };

    // ============================================================================
    // BUFF MANAGER - Manages all buffs on a GameObject
    // ============================================================================
    class BuffManager
    {
    public:
        uint64_t ObjectAddress;
        
        BuffManager(uint64_t objAddress) : ObjectAddress(objAddress) {}
        BuffManager() : ObjectAddress(0) {}
        
        bool IsValid() const { return ObjectAddress != 0; }
        
        // Get BuffManager pointer from GameObject
        // 0x2E68 is likely an offset to an embedded structure (LEA), not a pointer (MOV).
        // If we dereference it, we get the vtable (0x7FF...) which is wrong.
        uint64_t GetBuffManagerAddress() const {
            if (!IsValid()) return 0;
            return ObjectAddress + Offset::oObjBuffManager;
        }

        // Debug: Get raw array start
        uint64_t GetRawArrayStart() const {
            uint64_t buffMgr = GetBuffManagerAddress();
            if (!buffMgr) return 0;
            return *(uint64_t*)(buffMgr + Offset::oBuffManagerArray);
        }

        // Debug: Get raw array end
        uint64_t GetRawArrayEnd() const {
            uint64_t buffMgr = GetBuffManagerAddress();
            if (!buffMgr) return 0;
            return *(uint64_t*)(buffMgr + Offset::oBuffManagerArrayEnd);
        }
        
        // Get all active buffs
        std::vector<BuffInstance> GetBuffs() const {
            std::vector<BuffInstance> buffs;
            if (!IsValid()) return buffs;
            
            uint64_t buffMgr = GetBuffManagerAddress();
            if (!buffMgr) return buffs;
            
            uint64_t arrayStart = *(uint64_t*)(buffMgr + Offset::oBuffManagerArray);
            uint64_t arrayEnd = *(uint64_t*)(buffMgr + Offset::oBuffManagerArrayEnd);
            
            if (!arrayStart || !arrayEnd || arrayEnd <= arrayStart) return buffs;
            
            size_t count = (arrayEnd - arrayStart) / sizeof(uint64_t);
            if (count > 256) count = 256; // Safety limit
            
            for (size_t i = 0; i < count; i++) {
                uint64_t buffPtr = *(uint64_t*)(arrayStart + i * sizeof(uint64_t));
                if (buffPtr) {
                    buffs.push_back(BuffInstance(buffPtr));
                }
            }
            
            return buffs;
        }
        
        // Check if unit has a buff by name
        bool HasBuff(const std::string& buffName, float gameTime) const {
            auto buffs = GetBuffs();
            for (const auto& buff : buffs) {
                if (buff.IsActive(gameTime) && buff.GetName() == buffName) {
                    return true;
                }
            }
            return false;
        }
        
        // Check if unit has any buff by type
        bool HasBuffType(BuffType type, float gameTime) const {
            auto buffs = GetBuffs();
            for (const auto& buff : buffs) {
                if (buff.IsActive(gameTime) && buff.GetType() == type) {
                    return true;
                }
            }
            return false;
        }
        
        // Get buff by name (returns first match)
        BuffInstance GetBuff(const std::string& buffName, float gameTime) const {
            auto buffs = GetBuffs();
            for (const auto& buff : buffs) {
                if (buff.IsActive(gameTime) && buff.GetName() == buffName) {
                    return buff;
                }
            }
            return BuffInstance(0);
        }
        
        // Get remaining time for specific buff
        float GetBuffRemainingTime(const std::string& buffName, float gameTime) const {
            BuffInstance buff = GetBuff(buffName, gameTime);
            return buff.GetRemainingTime(gameTime);
        }

        // ============================================================================
        // CC DETECTION METHODS
        // ============================================================================
        
        // Check if unit is stunned
        bool IsStunned(float gameTime) const {
            return HasBuffType(BuffType::Stun, gameTime);
        }
        
        // Check if unit is snared/rooted
        bool IsSnared(float gameTime) const {
            return HasBuffType(BuffType::Snare, gameTime);
        }
        
        // Check if unit is taunted
        bool IsTaunted(float gameTime) const {
            return HasBuffType(BuffType::Taunt, gameTime);
        }
        
        // Check if unit is charmed
        bool IsCharmed(float gameTime) const {
            return HasBuffType(BuffType::Charm, gameTime);
        }
        
        // Check if unit is feared
        bool IsFeared(float gameTime) const {
            return HasBuffType(BuffType::Fear, gameTime);
        }
        
        // Check if unit is sleeped
        bool IsAsleep(float gameTime) const {
            return HasBuffType(BuffType::Asleep, gameTime);
        }
        
        // Check if unit is silenced
        bool IsSilenced(float gameTime) const {
            return HasBuffType(BuffType::Silence, gameTime);
        }
        
        // Check if unit is suppressed
        bool IsSuppressed(float gameTime) const {
            return HasBuffType(BuffType::Suppression, gameTime);
        }
        
        // Check if unit is knocked up
        bool IsKnockedUp(float gameTime) const {
            return HasBuffType(BuffType::Knockup, gameTime);
        }
        
        // Check if unit is knocked back
        bool IsKnockedBack(float gameTime) const {
            return HasBuffType(BuffType::Knockback, gameTime);
        }
        
        // Check if unit is grounded (cannot dash)
        bool IsGrounded(float gameTime) const {
            return HasBuffType(BuffType::Grounded, gameTime);
        }
        
        // Check if unit has ANY hard CC (cannot move or attack)
        bool HasHardCC(float gameTime) const {
            return IsStunned(gameTime) || 
                   IsTaunted(gameTime) || 
                   IsCharmed(gameTime) || 
                   IsFeared(gameTime) || 
                   IsAsleep(gameTime) || 
                   IsSuppressed(gameTime) ||
                   IsKnockedUp(gameTime) ||
                   IsKnockedBack(gameTime);
        }
        
        // Check if unit cannot move (hard CC or snare)
        bool IsImmobile(float gameTime) const {
            return HasHardCC(gameTime) || IsSnared(gameTime);
        }
        
        // Get remaining immobile time
        float GetImmobileTime(float gameTime) const {
            float maxTime = 0.0f;
            auto buffs = GetBuffs();
            
            for (const auto& buff : buffs) {
                if (!buff.IsActive(gameTime)) continue;
                
                BuffType type = buff.GetType();
                bool isImmobilizing = (type == BuffType::Stun) ||
                                      (type == BuffType::Snare) ||
                                      (type == BuffType::Taunt) ||
                                      (type == BuffType::Charm) ||
                                      (type == BuffType::Fear) ||
                                      (type == BuffType::Asleep) ||
                                      (type == BuffType::Suppression) ||
                                      (type == BuffType::Knockup) ||
                                      (type == BuffType::Knockback);
                                      
                if (isImmobilizing) {
                    float remaining = buff.GetRemainingTime(gameTime);
                    if (remaining > maxTime) {
                        maxTime = remaining;
                    }
                }
            }
            
            return maxTime;
        }
        
        // ============================================================================
        // INVULNERABILITY DETECTION
        // ============================================================================
        
        // Known invulnerability buff names
        static const std::set<std::string>& GetInvulnerabilityBuffNames() {
            static std::set<std::string> buffNames = {
                // General
                "zhonyasringshield",          // Zhonya's Hourglass
                "ChronoShift",                // Zilean R
                "BardRStasis",                // Bard R
                "KindredRNoDeathBuff",        // Kindred R
                "TaricR",                     // Taric R
                "UndyingRage",                // Tryndamere R (unkillable at 1 HP)
                "JudicatorIntervention",      // Kayle R (old name)
                "KayleR",                     // Kayle R
                
                // Spell Shields
                "sivaboriumonself",           // Sivir E
                "nocturneshroudofdarkness",   // Nocturne W
                "maborweivestwistingspell",   // Morgana E (CC immunity only)
                "bansheesveil",               // Banshee's Veil
                
                // Physical Immunity
                "ShenWBuff",                  // Shen W
                "JaxCounterStrike",           // Jax E (dodge)
                "PantheonPassiveShield",      // Pantheon passive
                "FioraW",                     // Fiora W
                
                // Untargetable
                "fizzeplayful",               // Fizz E
                "elisespiderformsuperiorspider", // Elise spider untargetable
                "VladimirSanguinePool",       // Vladimir W
                "YiMeditate",                 // Master Yi W damage reduction
                "EkkoRAttackSpeed",           // Ekko R untargetable
                "AlphaStrikeChannelBuff",     // Master Yi Q
                "SamiraW",                    // Samira W (destroys projectiles)
            };
            return buffNames;
        }
        
        // Check for Tryndamere undying rage (special case - unkillable below 30 HP)
        bool HasTryndamereUlt(float gameTime, float currentHealth) const {
            if (HasBuff("UndyingRage", gameTime)) {
                // Tryndamere cannot die while buff active
                // He can still take damage but won't go below ~30 HP
                return currentHealth <= 100.0f; 
            }
            return false;
        }
        
        // Check if unit is invulnerable (can't take damage)
        bool IsInvulnerable(float gameTime) const {
            // Check buff type first
            if (HasBuffType(BuffType::Invulnerability, gameTime)) {
                return true;
            }
            
            // Check known buff names
            const auto& buffNames = GetInvulnerabilityBuffNames();
            for (const auto& buffName : buffNames) {
                if (HasBuff(buffName, gameTime)) {
                    return true;
                }
            }
            
            return false;
        }
        
        // Check if unit is untargetable
        bool IsUntargetable(float gameTime) const {
            // These are buffs that make unit untargetable
            static std::set<std::string> untargetableBuffs = {
                "fizzeplayful",               // Fizz E
                "VladimirSanguinePool",       // Vladimir W
                "AlphaStrikeChannelBuff",     // Master Yi Q
                "EkkoRAttackSpeed",           // Ekko R
                "zhonyasringshield",          // Zhonya
                "BardRStasis",                // Bard R
            };
            
            for (const auto& buffName : untargetableBuffs) {
                if (HasBuff(buffName, gameTime)) {
                    return true;
                }
            }
            
            return false;
        }
        
        // Check for spell shields
        bool HasSpellShield(float gameTime) const {
            if (HasBuffType(BuffType::SpellShield, gameTime)) {
                return true;
            }
            
            static std::set<std::string> spellShieldBuffs = {
                "sivaboriumonself",
                "nocturneshroudofdarkness",
                "bansheesveil",
                "EdgeOfNightActive",
            };
            
            for (const auto& buffName : spellShieldBuffs) {
                if (HasBuff(buffName, gameTime)) {
                    return true;
                }
            }
            
            return false;
        }
        
        // Check if unit is in zombie state (Sion passive, Karthus passive, Kog'Maw passive)
        bool IsZombie(float gameTime) const {
            static std::set<std::string> zombieBuffs = {
                "SionPassiveZombie",          // Sion passive
                "KarthusDeath",               // Karthus passive
                "KogMawIcathianSurprise",     // Kog'Maw passive
            };
            
            for (const auto& buffName : zombieBuffs) {
                if (HasBuff(buffName, gameTime)) {
                    return true;
                }
            }
            
            return false;
        }
        
        // ============================================================================
        // UTILITY METHODS
        // ============================================================================
        
        // Check if target is a valid target (for TargetSelector)
        bool IsValidBuffState(float gameTime) const {
            // Not valid if untargetable, invulnerable, or zombie
            return !IsUntargetable(gameTime) && 
                   !IsInvulnerable(gameTime) && 
                   !IsZombie(gameTime);
        }
        
        // Get buff stack count by name
        int GetBuffStackCount(const std::string& buffName, float gameTime) const {
            BuffInstance buff = GetBuff(buffName, gameTime);
            return buff.IsValid() ? buff.GetStackCount() : 0;
        }
        
        // ============================================================================
        // SPECIAL ABILITY BUFF CHECKS (For Collision.h integration)
        // ============================================================================
        
        // Check if unit has Braum's Unbreakable (E) shield active
        // Used by BraumShieldTracker for skillshot collision
        bool HasBraumShield(float gameTime) const {
            return HasBuff("BraumShieldRaise", gameTime) || 
                   HasBuff("braumshieldraise", gameTime);
        }
        
        // Check if unit cannot be hit by skillshots (various protections)
        bool IsSkillshotImmune(float gameTime) const {
            return IsInvulnerable(gameTime) ||
                   IsUntargetable(gameTime) ||
                   HasSpellShield(gameTime);
        }
        
        // Debug: Print all active buffs
        std::string DebugBuffList(float gameTime) const {
            std::string result = "Active Buffs:\n";
            auto buffs = GetBuffs();
            
            for (const auto& buff : buffs) {
                if (buff.IsActive(gameTime)) {
                    std::string name = buff.GetName();
                    if (!name.empty()) {
                        result += "  - " + name + 
                                  " (Type: " + std::to_string((int)buff.GetType()) + 
                                  ", Stacks: " + std::to_string(buff.GetStackCount()) +
                                  ", Remaining: " + std::to_string(buff.GetRemainingTime(gameTime)) + "s)\n";
                    }
                }
            }
            
            return result;
        }
    };
}
