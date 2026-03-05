#pragma once
#include "SpellDatabaseEntry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <algorithm>

// ============================================================================
// SpellDatabase — C++ port of EnsoulSharp.SDK SpellDatabase.cs
//
// Loads spell data from Database.json (same format as EnsoulSharp).
// Provides query methods for prediction, evade, and spell tracking.
//
// Source: EnsoulSharp.SDK/Core/Wrappers/Spells/Database/SpellDatabase.cs
// Data:   sdk/Data/Database.json (copied from EnsoulSharp.SDK/Resources/Data/)
//
// Usage:
//   SDK::SpellDatabase::Init("path/to/Database.json");
//   auto* spell = SDK::SpellDatabase::GetByName("EzrealMysticShot");
//   auto spells = SDK::SpellDatabase::Get([](const SpellDatabaseEntry& e) {
//       return e.ChampionName == "Ezreal";
//   });
//
// TODO: Implement full JSON parsing when writing Evade system.
//       Currently provides the infrastructure and query API.
// ============================================================================

namespace SDK {

    class SpellDatabase {
    public:
        // ====================================================================
        // Initialize — Load Database.json
        // Call once during SDK initialization
        // ====================================================================
        static bool Init(const std::string& jsonPath = "") {
            if (s_initialized) return true;

            // TODO: Implement JSON parsing from Database.json
            // The JSON file format matches EnsoulSharp.SDK/Resources/Data/Database.json
            // Each entry has fields: ChampionName, SpellName, Slot, SpellType,
            // Range, Radius, Width, Delay, MissileSpeed, DangerValue, etc.
            //
            // For now, data must be loaded via LoadFromJson() after a JSON parser
            // is integrated (e.g. nlohmann/json, rapidjson, or custom parser)

            if (!jsonPath.empty()) {
                return LoadFromJson(jsonPath);
            }

            s_initialized = true;
            return true;
        }

        // ====================================================================
        // Get all spells (read-only)
        // ====================================================================
        static const std::vector<SpellDatabaseEntry>& Spells() {
            return s_spells;
        }

        // ====================================================================
        // Query with predicate (like C# LINQ .Where)
        // ====================================================================
        static std::vector<const SpellDatabaseEntry*> Get(
            std::function<bool(const SpellDatabaseEntry&)> predicate = nullptr)
        {
            std::vector<const SpellDatabaseEntry*> result;
            if (!predicate) {
                for (auto& s : s_spells)
                    result.push_back(&s);
            } else {
                for (auto& s : s_spells) {
                    if (predicate(s))
                        result.push_back(&s);
                }
            }
            return result;
        }

        // ====================================================================
        // Get spells by champion name
        // ====================================================================
        static std::vector<const SpellDatabaseEntry*> GetByChampion(const std::string& championName) {
            std::vector<const SpellDatabaseEntry*> result;
            for (auto& s : s_spells) {
                if (_stricmp(s.ChampionName.c_str(), championName.c_str()) == 0)
                    result.push_back(&s);
            }
            return result;
        }

        // ====================================================================
        // Get spell by internal spell name
        // ====================================================================
        static const SpellDatabaseEntry* GetByName(const std::string& spellName) {
            BuildIndexIfNeeded();

            // Check name index
            auto lower = ToLower(spellName);
            auto it = s_nameIndex.find(lower);
            if (it != s_nameIndex.end()) return it->second;

            // Check extra spell names
            for (auto& s : s_spells) {
                for (auto& extra : s.ExtraSpellNames) {
                    if (_stricmp(extra.c_str(), spellName.c_str()) == 0)
                        return &s;
                }
            }
            return nullptr;
        }

        // ====================================================================
        // Get spell by missile name
        // ====================================================================
        static const SpellDatabaseEntry* GetByMissileName(const std::string& missileSpellName) {
            BuildIndexIfNeeded();

            auto lower = ToLower(missileSpellName);

            // Check missile name index
            auto it = s_missileIndex.find(lower);
            if (it != s_missileIndex.end()) return it->second;

            // Check extra missile names
            for (auto& s : s_spells) {
                for (auto& extra : s.ExtraMissileNames) {
                    if (_stricmp(extra.c_str(), missileSpellName.c_str()) == 0)
                        return &s;
                }
            }
            return nullptr;
        }

        // ====================================================================
        // Get spell by champion + slot
        // ====================================================================
        static const SpellDatabaseEntry* GetBySlot(const std::string& championName, SpellSlotId slot) {
            for (auto& s : s_spells) {
                if (s.Slot == slot && _stricmp(s.ChampionName.c_str(), championName.c_str()) == 0)
                    return &s;
            }
            return nullptr;
        }

        // ====================================================================
        // Get spell by source object name
        // ====================================================================
        static const SpellDatabaseEntry* GetBySourceObjectName(const std::string& objectName) {
            auto lower = ToLower(objectName);
            for (auto& s : s_spells) {
                if (!s.SourceObjectName.empty()) {
                    auto srcLower = ToLower(s.SourceObjectName);
                    if (lower.find(srcLower) != std::string::npos)
                        return &s;
                }
            }
            return nullptr;
        }

        // ====================================================================
        // Get all dangerous spells (for evade system)
        // ====================================================================
        static std::vector<const SpellDatabaseEntry*> GetDangerousSpells() {
            std::vector<const SpellDatabaseEntry*> result;
            for (auto& s : s_spells) {
                if (s.IsDangerous)
                    result.push_back(&s);
            }
            return result;
        }

        // ====================================================================
        // Get all skillshot spells
        // ====================================================================
        static std::vector<const SpellDatabaseEntry*> GetSkillshots() {
            std::vector<const SpellDatabaseEntry*> result;
            for (auto& s : s_spells) {
                if (s.IsSkillshot())
                    result.push_back(&s);
            }
            return result;
        }

        // ====================================================================
        // Get entry count
        // ====================================================================
        static size_t GetCount() { return s_spells.size(); }

        // ====================================================================
        // Is loaded?
        // ====================================================================
        static bool IsInitialized() { return s_initialized; }

        // ====================================================================
        // Add entry manually (for custom/test entries)
        // ====================================================================
        static void AddEntry(const SpellDatabaseEntry& entry) {
            s_spells.push_back(entry);
            s_indexBuilt = false;  // Force index rebuild
        }

    private:
        // ====================================================================
        // JSON Loading — TODO: Implement with JSON parser
        // ====================================================================
        static bool LoadFromJson(const std::string& jsonPath) {
            // TODO: Implement JSON parsing
            // 
            // Recommended: Use nlohmann/json (single header) or rapidjson
            //
            // Example with nlohmann/json:
            //
            // #include <nlohmann/json.hpp>
            // using json = nlohmann::json;
            //
            // std::ifstream file(jsonPath);
            // if (!file.is_open()) return false;
            //
            // json data = json::parse(file);
            // for (auto& item : data) {
            //     SpellDatabaseEntry entry;
            //     entry.ChampionName = item.value("ChampionName", "");
            //     entry.SpellName = item.value("SpellName", "");
            //     entry.Slot = ParseSlot(item.value("Slot", "Q"));
            //     entry.Type = ParseSpellType(item.value("SpellType", "SkillshotLine"));
            //     entry.Range = item.value("Range", INT_MAX);
            //     entry.Radius = item.value("Radius", 0);
            //     entry.Width = item.value("Width", 50);
            //     entry.Delay = item.value("Delay", 250);
            //     entry.MissileSpeed = item.value("MissileSpeed", 1000);
            //     entry.DangerValue = item.value("DangerValue", 1);
            //     entry.IsDangerous = item.value("IsDangerous", false);
            //     entry.FixedRange = item.value("FixedRange", false);
            //     entry.MissileSpellName = item.value("MissileSpellName", "");
            //     // ... parse remaining fields ...
            //     
            //     // Parse collision objects (array of strings → bitmask)
            //     if (item.contains("CollisionObjects")) {
            //         for (auto& col : item["CollisionObjects"]) {
            //             std::string s = col.get<std::string>();
            //             if (s == "Minions") entry.CollisionObjects |= CollisionMinions;
            //             if (s == "Heroes")  entry.CollisionObjects |= CollisionHeroes;
            //             // etc.
            //         }
            //     }
            //     
            //     s_spells.push_back(entry);
            // }
            // 
            // s_initialized = true;
            // return true;

            (void)jsonPath;
            return false;  // Not yet implemented
        }

        // ====================================================================
        // Index Building (lazy, built on first query)
        // ====================================================================
        static void BuildIndexIfNeeded() {
            if (s_indexBuilt) return;

            s_nameIndex.clear();
            s_missileIndex.clear();

            for (auto& s : s_spells) {
                // Name index
                if (!s.SpellName.empty()) {
                    s_nameIndex[ToLower(s.SpellName)] = &s;
                }

                // Missile name index
                if (!s.MissileSpellName.empty()) {
                    s_missileIndex[ToLower(s.MissileSpellName)] = &s;
                }
            }

            s_indexBuilt = true;
        }

        // ====================================================================
        // String helpers
        // ====================================================================
        static std::string ToLower(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }

        // ====================================================================
        // Parse helpers (for JSON deserialization)
        // ====================================================================
        static SpellSlotId ParseSlot(const std::string& s) {
            if (s == "Q") return SpellSlotId::Q;
            if (s == "W") return SpellSlotId::W;
            if (s == "E") return SpellSlotId::E;
            if (s == "R") return SpellSlotId::R;
            return SpellSlotId::Q;
        }

        static SpellType ParseSpellType(const std::string& s) {
            if (s == "SkillshotCircle")        return SpellType::SkillshotCircle;
            if (s == "SkillshotMissileCircle") return SpellType::SkillshotMissileCircle;
            if (s == "SkillshotLine")          return SpellType::SkillshotLine;
            if (s == "SkillshotMissileLine")   return SpellType::SkillshotMissileLine;
            if (s == "SkillshotCone")          return SpellType::SkillshotCone;
            if (s == "SkillshotMissileCone")   return SpellType::SkillshotMissileCone;
            if (s == "SkillshotMissileArc")    return SpellType::SkillshotMissileArc;
            if (s == "SkillshotRing")          return SpellType::SkillshotRing;
            if (s == "SkillshotArc")           return SpellType::SkillshotArc;
            if (s == "Targeted")               return SpellType::Targeted;
            if (s == "TargetedMissile")        return SpellType::TargetedMissile;
            if (s == "Toggled")                return SpellType::Toggled;
            if (s == "Activated")              return SpellType::Activated;
            if (s == "Passive")                return SpellType::Passive;
            if (s == "Position")               return SpellType::Position;
            if (s == "Vector")                 return SpellType::Vector;
            return SpellType::SkillshotLine;
        }

        static BuffType ParseBuffType(const std::string& s) {
            if (s == "Stun")       return BuffType::Stun;
            if (s == "Slow")       return BuffType::Slow;
            if (s == "Snare")      return BuffType::Snare;
            if (s == "Silence")    return BuffType::Silence;
            if (s == "Taunt")      return BuffType::Taunt;
            if (s == "Fear")       return BuffType::Fear;
            if (s == "Charm")      return BuffType::Charm;
            if (s == "Suppression")return BuffType::Suppression;
            if (s == "Blind")      return BuffType::Blind;
            if (s == "Polymorph")  return BuffType::Polymorph;
            if (s == "Knockup")    return BuffType::Stun;      // Knockup mapped to Stun
            if (s == "Asleep")     return BuffType::Asleep;
            if (s == "Grounded")   return BuffType::Grounded;
            if (s == "NearSight")  return BuffType::NearSight;
            if (s == "Flee")       return BuffType::Flee;
            return BuffType::Internal;
        }

        static CastType ParseCastType(const std::string& s) {
            if (s == "EnemyChampions") return CastType::EnemyChampions;
            if (s == "EnemyMinions")   return CastType::EnemyMinions;
            if (s == "EnemyTurrets")   return CastType::EnemyTurrets;
            if (s == "AllyChampions")  return CastType::AllyChampions;
            if (s == "AllyMinions")    return CastType::AllyMinions;
            if (s == "AllyTurrets")    return CastType::AllyTurrets;
            if (s == "HeroPets")       return CastType::HeroPets;
            if (s == "Position")       return CastType::Position;
            if (s == "Direction")      return CastType::Direction;
            if (s == "Self")           return CastType::Self;
            if (s == "Charging")       return CastType::Charging;
            if (s == "Toggle")         return CastType::Toggle;
            if (s == "Channel")        return CastType::Channel;
            if (s == "Activate")       return CastType::Activate;
            return CastType::Position;
        }

        static SpellTags ParseSpellTag(const std::string& s) {
            if (s == "Damage")                  return SpellTags::Damage;
            if (s == "AoE")                     return SpellTags::AoE;
            if (s == "AppliesOnHitEffects")     return SpellTags::AppliesOnHitEffects;
            if (s == "CrowdControl")            return SpellTags::CrowdControl;
            if (s == "Shield")                  return SpellTags::Shield;
            if (s == "Heal")                    return SpellTags::Heal;
            if (s == "Stasis")                  return SpellTags::Stasis;
            if (s == "LeavesMark")              return SpellTags::LeavesMark;
            if (s == "CanDetonateMark")         return SpellTags::CanDetonateMark;
            if (s == "Transformation")          return SpellTags::Transformation;
            if (s == "Dash")                    return SpellTags::Dash;
            if (s == "Blink")                   return SpellTags::Blink;
            if (s == "Teleport")                return SpellTags::Teleport;
            if (s == "DamageAmplifier")         return SpellTags::DamageAmplifier;
            if (s == "DefensiveBuff")           return SpellTags::DefensiveBuff;
            if (s == "MovementSpeedAmplifier")  return SpellTags::MovementSpeedAmplifier;
            if (s == "AttackSpeedAmplifier")    return SpellTags::AttackSpeedAmplifier;
            if (s == "AttackRangeModifier")     return SpellTags::AttackRangeModifier;
            if (s == "SpellShield")             return SpellTags::SpellShield;
            if (s == "RemoveCrowdControl")      return SpellTags::RemoveCrowdControl;
            if (s == "GrantsVision")            return SpellTags::GrantsVision;
            if (s == "Interruptable")           return SpellTags::Interruptable;
            return SpellTags::Damage;
        }

        // ====================================================================
        // Static Data
        // ====================================================================
        static inline bool s_initialized = false;
        static inline bool s_indexBuilt = false;
        static inline std::vector<SpellDatabaseEntry> s_spells;
        static inline std::unordered_map<std::string, const SpellDatabaseEntry*> s_nameIndex;
        static inline std::unordered_map<std::string, const SpellDatabaseEntry*> s_missileIndex;
    };

} // namespace SDK
