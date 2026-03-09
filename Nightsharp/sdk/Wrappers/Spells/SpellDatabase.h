#pragma once
#include "SpellDatabaseEntry.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>

// Use nlohmann/json for parsing Database.json
#include "libs/nlohmann/json.hpp"

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
//   SDK::SpellDatabase::Init();  // Auto-finds Database.json next to DLL
//   auto* spell = SDK::SpellDatabase::GetByName("EzrealMysticShot");
//   auto spells = SDK::SpellDatabase::GetByChampion("Ezreal");
// ============================================================================

namespace SDK {

    class SpellDatabase {
    public:
        // ====================================================================
        // Initialize — Load Database.json
        // Call once during SDK initialization.
        // If jsonPath is empty, auto-detects path relative to DLL location.
        // ====================================================================
        static bool Init(const std::string& jsonPath = "") {
            if (s_initialized && !s_spells.empty()) {
                return true;
            }

            // Retry loading if previously initialized with an empty DB.
            for (const auto& path : BuildCandidatePaths(jsonPath)) {
                if (path.empty()) {
                    continue;
                }
                if (!FileExists(path)) {
                    continue;
                }
                if (LoadFromJson(path)) {
                    s_initialized = true;
                    return true;
                }
            }

            // If file not found/parsing failed, keep initialized state but empty DB.
            s_initialized = true;
            return false;
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
        // Get DLL directory path
        // ====================================================================
        static std::string GetDllDirectory() {
            char path[MAX_PATH] = { 0 };
            HMODULE hm = NULL;
            // Get handle to this DLL module
            if (GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&GetDllDirectory, &hm) == 0)
            {
                return "";
            }
            if (GetModuleFileNameA(hm, path, sizeof(path)) == 0) {
                return "";
            }
            // Remove filename, keep directory
            std::string dir(path);
            size_t pos = dir.find_last_of("\\/");
            if (pos != std::string::npos)
                dir = dir.substr(0, pos);
            return dir;
        }

        static bool FileExists(const std::string& path) {
            std::ifstream in(path, std::ios::binary);
            return in.good();
        }

        static std::string ParentDirectory(const std::string& path) {
            const size_t pos = path.find_last_of("\\/");
            if (pos == std::string::npos) {
                return "";
            }
            return path.substr(0, pos);
        }

        static std::vector<std::string> BuildCandidatePaths(const std::string& explicitPath) {
            std::vector<std::string> candidates;
            candidates.reserve(32);
            if (!explicitPath.empty()) {
                candidates.push_back(explicitPath);
            }

            const std::string dllDir = GetDllDirectory();
            if (!dllDir.empty()) {
                // Common layouts when DLL is injected from build output or project root.
                candidates.push_back(dllDir + "\\sdk\\Data\\Database.json");
                candidates.push_back(dllDir + "\\Data\\Database.json");
                candidates.push_back(dllDir + "\\Database.json");
                candidates.push_back(dllDir + "\\Nightsharp\\sdk\\Data\\Database.json");

                // Walk up parent directories to support x64\\Release / bin layouts.
                std::string cur = dllDir;
                for (int i = 0; i < 8 && !cur.empty(); ++i) {
                    candidates.push_back(cur + "\\sdk\\Data\\Database.json");
                    candidates.push_back(cur + "\\Nightsharp\\sdk\\Data\\Database.json");
                    cur = ParentDirectory(cur);
                }
            }

            // Deduplicate while preserving order.
            std::set<std::string> seen;
            std::vector<std::string> unique;
            unique.reserve(candidates.size());
            for (const auto& p : candidates) {
                if (p.empty()) {
                    continue;
                }
                if (seen.insert(p).second) {
                    unique.push_back(p);
                }
            }
            return unique;
        }

        // ====================================================================
        // JSON Loading — Full implementation using nlohmann/json
        // ====================================================================
        static bool LoadFromJson(const std::string& jsonPath) {
            using json = nlohmann::json;

            std::ifstream file(jsonPath);
            if (!file.is_open()) {
                // File not found — this is not fatal, just means no spell data
                return false;
            }

            try {
                json data = json::parse(file);

                if (!data.is_array()) {
                    return false;
                }

                s_spells.clear();
                s_spells.reserve(data.size());

                for (auto& item : data) {
                    SpellDatabaseEntry entry;

                    // ---- Champion & Spell Identity ----
                    entry.ChampionName      = GetString(item, "ChampionName");
                    entry.SpellName         = GetString(item, "SpellName");
                    entry.Slot              = ParseSlot(GetString(item, "Slot", "Q"));
                    entry.Type              = ParseSpellType(GetString(item, "SpellType", "SkillshotLine"));

                    // ---- Skillshot Geometry ----
                    entry.Range             = GetInt(item, "Range", INT_MAX);
                    entry.Radius            = GetInt(item, "Radius", 0);
                    entry.Width             = GetInt(item, "Width", 50);
                    entry.Angle             = GetInt(item, "Angle", 45);
                    entry.ArcAngle          = GetInt(item, "ArcAngle", 0);
                    entry.RingRadius        = GetInt(item, "RingRadius", 0);
                    entry.ExtraRange        = GetInt(item, "ExtraRange", 0);
                    entry.FixedRange        = GetBool(item, "FixedRange", false);
                    entry.AvoidMaxRangeReduction = GetBool(item, "AvoidMaxRangeReduction", false);

                    // ---- Timing ----
                    entry.Delay             = GetInt(item, "Delay", 250);
                    entry.MissileSpeed      = GetInt(item, "MissileSpeed", 1000);
                    entry.MissileAccel      = GetInt(item, "MissileAccel", 0);
                    entry.MissileMinSpeed   = GetInt(item, "MissileMinSpeed", 0);
                    entry.MissileMaxSpeed   = GetInt(item, "MissileMaxSpeed", 0);
                    entry.MissileDelayed    = GetBool(item, "MissileDelayed", false);
                    entry.MissileFollowsCaster = GetBool(item, "MissileFollowsCaster", false);

                    // ---- Missile Identification ----
                    entry.MissileSpellName  = GetString(item, "MissileSpellName");
                    entry.ExtraMissileNames = GetStringArray(item, "ExtraMissileNames");
                    entry.ExtraSpellNames   = GetStringArray(item, "ExtraSpellNames");

                    // ---- Collision (array of strings → bitmask) ----
                    entry.CollisionObjects  = ParseCollisionObjects(item);

                    // ---- Danger Assessment ----
                    entry.DangerValue       = GetInt(item, "DangerValue", 1);
                    entry.IsDangerous       = GetBool(item, "IsDangerous", false);

                    // ---- Cast Type & Tags ----
                    entry.CastTypes         = ParseCastTypes(item);
                    entry.Tags              = ParseSpellTags(item);

                    // ---- Buff Application ----
                    entry.AppliedBuffsOnAllies  = ParseBuffTypes(item, "AppliedBuffsOnAllies");
                    entry.AppliedBuffsOnEnemies = ParseBuffTypes(item, "AppliedBuffsOnEnemies");
                    entry.AppliedBuffsOnSelf    = ParseBuffTypes(item, "AppliedBuffsOnSelf");
                    entry.AppliedBuffOnSelfName = GetString(item, "AppliedBuffOnSelfName");
                    entry.AppliedBuffOnAllyName = GetString(item, "AppliedBuffOnAllyName");
                    entry.AppliedBuffOnEnemyName = GetString(item, "AppliedBuffOnEnemyName");
                    entry.AppliedBuffName       = GetString(item, "AppliedBuffName");

                    // ---- Source Object ----
                    entry.SourceObjectName  = GetString(item, "SourceObjectName");
                    entry.FromObjects       = GetStringArray(item, "FromObjects");
                    entry.FromObject        = GetString(item, "FromObject");

                    // ---- Misc ----
                    entry.ResetsAutoAttackTimer = GetBool(item, "ResetsAutoAttackTimer", false);
                    entry.CanBeRemoved      = GetBool(item, "CanBeRemoved", false);
                    entry.ForceRemove       = GetBool(item, "ForceRemove", false);
                    entry.ToggleParticleName = GetString(item, "ToggleParticleName");
                    entry.MinChannelDuration = GetInt(item, "MinChannelDuration", 0);
                    entry.MaxChannelDuration = GetInt(item, "MaxChannelDuration", 0);

                    s_spells.push_back(std::move(entry));
                }

                s_indexBuilt = false;  // Force rebuild indices
                s_initialized = true;
                return true;
            }
            catch (const json::exception& /*e*/) {
                // JSON parse error
                return false;
            }
            catch (...) {
                return false;
            }
        }

        // ====================================================================
        // JSON Helper Functions — safe value extraction
        // ====================================================================

        static std::string GetString(const nlohmann::json& obj, const char* key, const char* def = "") {
            if (obj.contains(key) && !obj[key].is_null()) {
                if (obj[key].is_string())
                    return obj[key].get<std::string>();
            }
            return def ? def : "";
        }

        static int GetInt(const nlohmann::json& obj, const char* key, int def = 0) {
            if (obj.contains(key) && !obj[key].is_null()) {
                if (obj[key].is_number())
                    return obj[key].get<int>();
            }
            return def;
        }

        static bool GetBool(const nlohmann::json& obj, const char* key, bool def = false) {
            if (obj.contains(key) && !obj[key].is_null()) {
                if (obj[key].is_boolean())
                    return obj[key].get<bool>();
            }
            return def;
        }

        static std::vector<std::string> GetStringArray(const nlohmann::json& obj, const char* key) {
            std::vector<std::string> result;
            if (obj.contains(key) && obj[key].is_array()) {
                for (auto& elem : obj[key]) {
                    if (elem.is_string())
                        result.push_back(elem.get<std::string>());
                }
            }
            return result;
        }

        // ====================================================================
        // Parse CollisionObjects: ["YasuoWall", "Minions", "Heroes"] → bitmask
        // ====================================================================
        static int ParseCollisionObjects(const nlohmann::json& item) {
            int flags = CollisionNone;
            if (!item.contains("CollisionObjects") || !item["CollisionObjects"].is_array())
                return flags;

            for (auto& col : item["CollisionObjects"]) {
                if (!col.is_string()) continue;
                std::string s = col.get<std::string>();

                if (s == "Minions")       flags |= CollisionMinions;
                else if (s == "Heroes")   flags |= CollisionHeroes;
                else if (s == "YasuoWall") flags |= CollisionYasuoWall;
                else if (s == "BraumShield") flags |= CollisionBraumShield;
                else if (s == "Walls")    flags |= CollisionWalls;
            }
            return flags;
        }

        // ====================================================================
        // Parse CastType array: ["Position", "EnemyChampions"] → vector<CastType>
        // ====================================================================
        static std::vector<CastType> ParseCastTypes(const nlohmann::json& item) {
            std::vector<CastType> result;
            if (!item.contains("CastType") || !item["CastType"].is_array())
                return result;

            for (auto& elem : item["CastType"]) {
                if (elem.is_string())
                    result.push_back(ParseCastType(elem.get<std::string>()));
            }
            return result;
        }

        // ====================================================================
        // Parse SpellTags array: ["Damage", "CrowdControl"] → vector<SpellTags>
        // ====================================================================
        static std::vector<SpellTags> ParseSpellTags(const nlohmann::json& item) {
            std::vector<SpellTags> result;
            if (!item.contains("SpellTags") || !item["SpellTags"].is_array())
                return result;

            for (auto& elem : item["SpellTags"]) {
                if (elem.is_string())
                    result.push_back(ParseSpellTag(elem.get<std::string>()));
            }
            return result;
        }

        // ====================================================================
        // Parse BuffType arrays (AppliedBuffsOnAllies, etc.)
        // ====================================================================
        static std::vector<BuffType> ParseBuffTypes(const nlohmann::json& item, const char* key) {
            std::vector<BuffType> result;
            if (!item.contains(key) || !item[key].is_array())
                return result;

            for (auto& elem : item[key]) {
                if (elem.is_string())
                    result.push_back(ParseBuffType(elem.get<std::string>()));
            }
            return result;
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
