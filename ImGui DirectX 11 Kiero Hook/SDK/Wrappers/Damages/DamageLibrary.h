#pragma once
#include "GameObject.h"
#include "GameObjects.h"
#include "SpellBook.h"
#include "DamageCalc.h"
#include "Enums.h"
#include "libs/nlohmann/json.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>
#include <cmath>

// ============================================================================
// DamageLibrary — Per-Champion QWER Spell Damage from JSON
// Reference: EnsoulSharp.SDK/Core/Wrappers/Damages/DamageLibrary.cs
//            + DamageJson.cs (data structures)
//            + Resources/Data/9.7.269.2391.json (damage data, Patch 9.7)
//
// NOTE: The JSON data (9.7.269.2391.json) is from EnsoulSharp's old Patch 9.7
// data. For Season 2026 (Patch 26.S1), new champions and reworked abilities
// may not be present in this JSON. In those cases, DamageLibrary::GetSpellDamage
// will return 0.0f, and scripts should use direct calculations instead.
//
// For the most up-to-date damage info, use:
//   - DamagePassives::GetPassiveDamage() for on-hit passives
//   - DamageMastery::GetRuneDamage() for rune/keystone procs
//   - DamageCalc::GetFullAutoAttackDamage() for complete AA damage
//
// The JSON file can be updated by extracting data from the game client.
// Place updated JSON in sdk/Data/ directory. The Init() method will attempt
// to load from multiple possible filenames.
//
// Usage:
//   SDK::DamageLibrary::Init();  // Load JSON
//   float dmg = SDK::DamageLibrary::GetSpellDamage(source, target, SpellSlotId::Q);
//   float dmg2 = SDK::DamageLibrary::GetSpellDamage(source, target, SpellSlotId::Q, DamageStage::SecondCast);
// ============================================================================

namespace SDK {

    // ========================================================================
    // Enums for DamageLibrary (from DamageJson.cs)
    // ========================================================================

    enum class DamageScalingTarget {
        Source,
        Target
    };

    enum class DamageScalingType {
        AttackPoints,
        BonusAttackPoints,
        AbilityPoints,
        BonusHealth,
        CurrentHealth,
        MaxHealth,
        MissingHealth,
        BonusMana,
        MaxMana,
        Armor,
        BonusArmor,
        SpellBlock,
        BonusSpellBlock,
        PhysicalLethality
    };

    enum class DamageStage {
        Default,
        WayBack,
        Detonation,
        DamagePerTick,
        DamagePerTime,
        DamagePerHalfSecond,
        DamagePerQuarterSecond,
        DamagePerSecond,
        SingleTotal,
        SecondForm,
        ThirdForm,
        SecondCast,
        ThirdCast,
        Buff,
        Empowered,
        EmpoweredDamagePerSecond,
        EmpoweredDamagePerHalfSecond,
        EmpoweredDamagePerQuarterSecond
    };

    enum class DmgSpellEffectType {
        None,
        AoE,
        Single,
        OverTime,
        Attack
    };

    // ========================================================================
    // Data Structures (from DamageJson.cs)
    // ========================================================================

    struct DmgSpellBonus {
        DamageType              DmgType = DamageType::Physical;
        std::vector<double>     DamagePercentages;
        DamageScalingTarget     ScalingTarget = DamageScalingTarget::Source;
        DamageScalingType       ScalingType = DamageScalingType::AttackPoints;
        std::vector<double>     BonusDamageOnMinion;
        std::vector<double>     BonusDamageOnMonster;
        std::vector<int>        MaxDamageOnMinion;
        std::vector<int>        MaxDamageOnMonster;
        double                  ScalePer100Ad = 0.0;
        double                  ScalePer100Ap = 0.0;
        double                  ScalePer100BonusAd = 0.0;
        std::string             ScalingBuff;
        int                     ScalingBuffOffset = 0;
        DamageScalingTarget     ScalingBuffTarget = DamageScalingTarget::Source;
    };

    struct DmgSpellOnMonster {
        DamageType              DmgType = DamageType::Physical;
        std::vector<double>     DamagePercentages;
        DamageScalingTarget     ScalingTarget = DamageScalingTarget::Source;
        DamageScalingType       ScalingType = DamageScalingType::AttackPoints;
    };

    struct DmgSpellData {
        DmgSpellEffectType      SpellEffectType = DmgSpellEffectType::None;
        DamageType              DmgType = DamageType::Physical;
        std::vector<double>     Damages;          // base damage per spell level [1..5]
        std::vector<double>     DamagesPerLvl;    // extra damage per champion level
        std::vector<DmgSpellBonus>      BonusDamages;
        std::vector<DmgSpellOnMonster>  DamagesOnMonster;
        std::vector<double>     BonusDamageOnMinion;
        std::vector<double>     BonusDamageOnMonster;
        std::vector<double>     BonusDamageOnSoldier;
        std::vector<double>     DamagesReductionOnSoldier;
        std::vector<double>     DamagesReductionPerLvlOnSoldier;
        std::vector<int>        MaxDamageOnMinion;
        std::vector<int>        MaxDamageOnMonster;
        std::vector<int>        MinDamageOnSoldier;
        std::vector<double>     ScalePerTargetMissHealth;
        double                  MaxScaleTargetMissHealth = 0.0;
        double                  MaxLevelScalingValueOnMinion = 0.0;
        double                  ScalePerCritChance = 0.0;
        double                  ScalingValueOnSoldier = 0.0;
        bool                    IsApplyOnHit = false;
        bool                    IsModifiedDamage = false;
    };

    struct DmgSpell {
        DamageStage     Stage = DamageStage::Default;
        DmgSpellData    SpellData;
    };

    struct ChampionDamage {
        std::vector<DmgSpell> Q;
        std::vector<DmgSpell> W;
        std::vector<DmgSpell> E;
        std::vector<DmgSpell> R;

        const std::vector<DmgSpell>* GetSlot(SpellSlotId slot) const {
            switch (slot) {
            case SpellSlotId::Q: return &Q;
            case SpellSlotId::W: return &W;
            case SpellSlotId::E: return &E;
            case SpellSlotId::R: return &R;
            default: return nullptr;
            }
        }
    };

    // ========================================================================
    // DamageLibrary — Main class
    // ========================================================================
    class DamageLibrary {
    public:
        // ====================================================================
        // Initialize — Load damage JSON data
        // ====================================================================
        static bool Init(const std::string& jsonPath = "") {
            if (s_initialized) return true;

            std::string path = jsonPath;
            if (path.empty()) {
                path = GetDllDirectory() + "\\sdk\\Data\\9.7.269.2391.json";
            }

            bool result = LoadFromJson(path);
            s_initialized = true;
            return result;
        }

        static bool IsInitialized() { return s_initialized; }

        // ====================================================================
        // Get spell damage for a champion slot + stage
        // ====================================================================
        static double GetSpellDamage(
            const GameObject& source,
            const GameObject& target,
            SpellSlotId slot,
            DamageStage stage = DamageStage::Default)
        {
            if (!s_initialized) return 0.0;
            if (!source.IsValid() || !target.IsValid()) return 0.0;

            std::string champName = source.GetChampionName();
            auto it = s_collection.find(champName);
            if (it == s_collection.end()) return 0.0;

            const ChampionDamage& champDmg = it->second;
            const std::vector<DmgSpell>* spells = champDmg.GetSlot(slot);
            if (!spells || spells->empty()) return 0.0;

            // Find the spell with the matching stage
            const DmgSpell* spell = nullptr;
            for (auto& sp : *spells) {
                if (sp.Stage == stage) {
                    spell = &sp;
                    break;
                }
            }
            if (!spell) {
                // Fallback to Default stage
                for (auto& sp : *spells) {
                    if (sp.Stage == DamageStage::Default) {
                        spell = &sp;
                        break;
                    }
                }
            }
            if (!spell) return 0.0;

            // Get spell level
            SpellBook sb(source.address);
            int spellLevel = 0;
            if (sb.IsValid()) {
                auto spellSlot = sb.GetSpell(slot);
                if (spellSlot.IsValid()) {
                    spellLevel = spellSlot.GetLevel();
                }
            }
            if (spellLevel <= 0) return 0.0;
            int idx = spellLevel - 1;

            return CalculateSpellDamage(source, target, spell->SpellData, idx);
        }

        // ====================================================================
        // Check if champion data exists
        // ====================================================================
        static bool HasChampion(const std::string& champName) {
            return s_collection.find(champName) != s_collection.end();
        }

        // ====================================================================
        // Get raw champion damage data
        // ====================================================================
        static const ChampionDamage* GetChampionData(const std::string& champName) {
            auto it = s_collection.find(champName);
            if (it != s_collection.end()) return &it->second;
            return nullptr;
        }

        static size_t GetChampionCount() { return s_collection.size(); }

    private:
        static inline std::unordered_map<std::string, ChampionDamage> s_collection;
        static inline bool s_initialized = false;

        // ====================================================================
        // Calculate damage from spell data
        // ====================================================================
        static double CalculateSpellDamage(
            const GameObject& source,
            const GameObject& target,
            const DmgSpellData& data,
            int idx)
        {
            // Base damage
            double baseDmg = 0.0;
            if (idx >= 0 && idx < (int)data.Damages.size()) {
                baseDmg = data.Damages[idx];
            }

            // Per-level bonus
            if (!data.DamagesPerLvl.empty()) {
                int heroLevel = source.GetLevel();
                if (heroLevel > 0 && heroLevel <= (int)data.DamagesPerLvl.size()) {
                    baseDmg += data.DamagesPerLvl[heroLevel - 1];
                }
            }

            // Bonus damages (scaling)
            double bonusDmg = 0.0;
            for (auto& bonus : data.BonusDamages) {
                double pct = 0.0;
                if (idx >= 0 && idx < (int)bonus.DamagePercentages.size()) {
                    pct = bonus.DamagePercentages[idx];
                } else if (!bonus.DamagePercentages.empty()) {
                    pct = bonus.DamagePercentages.back();
                }

                if (pct == 0.0) continue;

                const GameObject& scaleTarget =
                    (bonus.ScalingTarget == DamageScalingTarget::Target) ? target : source;

                double scaleValue = GetScalingValue(scaleTarget, bonus.ScalingType);
                bonusDmg += pct * scaleValue;
            }

            double totalRaw = baseDmg + bonusDmg;
            if (totalRaw <= 0.0) return 0.0;

            // Apply damage type reduction
            DamageType dmgType = data.DmgType;
            return DamageCalc::CalcDamage(source, target, dmgType, (float)totalRaw);
        }

        // ====================================================================
        // Get scaling value from a game object
        // ====================================================================
        static double GetScalingValue(const GameObject& obj, DamageScalingType type) {
            switch (type) {
            case DamageScalingType::AttackPoints:
                return obj.GetTotalAD();
            case DamageScalingType::BonusAttackPoints:
                return obj.GetBonusAD();
            case DamageScalingType::AbilityPoints:
                return obj.GetAP();
            case DamageScalingType::BonusHealth:
                // BonusHealth = MaxHealth - base health (approximate: use MaxHealth * 0.4 as bonus)
                return obj.GetMaxHealth() * 0.4;
            case DamageScalingType::CurrentHealth:
                return obj.GetHealth();
            case DamageScalingType::MaxHealth:
                return obj.GetMaxHealth();
            case DamageScalingType::MissingHealth:
                return obj.GetMaxHealth() - obj.GetHealth();
            case DamageScalingType::MaxMana:
                return obj.GetMaxMana();
            case DamageScalingType::BonusMana:
                // BonusMana = MaxMana - base mana (approximate: use MaxMana * 0.3 as bonus)
                return obj.GetMaxMana() * 0.3;
            case DamageScalingType::Armor:
                return obj.GetArmor();
            case DamageScalingType::BonusArmor:
                return obj.GetBonusArmor();
            case DamageScalingType::SpellBlock:
                return obj.GetMR();
            case DamageScalingType::BonusSpellBlock:
                return obj.GetBonusMR();
            case DamageScalingType::PhysicalLethality:
                return obj.GetLethality();
            default:
                return 0.0;
            }
        }

        // ====================================================================
        // Get DLL directory for auto-detecting JSON path
        // ====================================================================
        static std::string GetDllDirectory() {
            char buf[MAX_PATH] = {};
            HMODULE hm = NULL;
            GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                (LPCSTR)&GetDllDirectory, &hm);
            GetModuleFileNameA(hm, buf, MAX_PATH);
            std::string path(buf);
            return path.substr(0, path.find_last_of("\\/"));
        }

        // ====================================================================
        // JSON Parsing Helpers
        // ====================================================================
        static DamageType ParseDamageType(const std::string& s) {
            if (s == "Physical") return DamageType::Physical;
            if (s == "Magical")  return DamageType::Magical;
            if (s == "True")     return DamageType::True;
            if (s == "Mixed")    return DamageType::Mixed;
            return DamageType::Physical;
        }

        static DamageScalingTarget ParseScalingTarget(const std::string& s) {
            if (s == "Target") return DamageScalingTarget::Target;
            return DamageScalingTarget::Source;
        }

        static DamageScalingType ParseScalingType(const std::string& s) {
            if (s == "AttackPoints")       return DamageScalingType::AttackPoints;
            if (s == "BonusAttackPoints")  return DamageScalingType::BonusAttackPoints;
            if (s == "AbilityPoints")      return DamageScalingType::AbilityPoints;
            if (s == "BonusHealth")        return DamageScalingType::BonusHealth;
            if (s == "CurrentHealth")      return DamageScalingType::CurrentHealth;
            if (s == "MaxHealth")          return DamageScalingType::MaxHealth;
            if (s == "MissingHealth")      return DamageScalingType::MissingHealth;
            if (s == "BonusMana")          return DamageScalingType::BonusMana;
            if (s == "MaxMana")            return DamageScalingType::MaxMana;
            if (s == "Armor")              return DamageScalingType::Armor;
            if (s == "BonusArmor")         return DamageScalingType::BonusArmor;
            if (s == "SpellBlock")         return DamageScalingType::SpellBlock;
            if (s == "BonusSpellBlock")    return DamageScalingType::BonusSpellBlock;
            if (s == "PhysicalLethality")  return DamageScalingType::PhysicalLethality;
            return DamageScalingType::AttackPoints;
        }

        static DamageStage ParseStage(const std::string& s) {
            if (s == "Default")                         return DamageStage::Default;
            if (s == "WayBack")                         return DamageStage::WayBack;
            if (s == "Detonation")                      return DamageStage::Detonation;
            if (s == "DamagePerTick")                   return DamageStage::DamagePerTick;
            if (s == "DamagePerTime")                   return DamageStage::DamagePerTime;
            if (s == "DamagePerHalfSecond")             return DamageStage::DamagePerHalfSecond;
            if (s == "DamagePerQuarterSecond")          return DamageStage::DamagePerQuarterSecond;
            if (s == "DamagePerSecond")                 return DamageStage::DamagePerSecond;
            if (s == "SingleTotal")                     return DamageStage::SingleTotal;
            if (s == "SecondForm")                      return DamageStage::SecondForm;
            if (s == "ThirdForm")                       return DamageStage::ThirdForm;
            if (s == "SecondCast")                      return DamageStage::SecondCast;
            if (s == "ThirdCast")                       return DamageStage::ThirdCast;
            if (s == "Buff")                            return DamageStage::Buff;
            if (s == "Empowered")                       return DamageStage::Empowered;
            if (s == "EmpoweredDamagePerSecond")        return DamageStage::EmpoweredDamagePerSecond;
            if (s == "EmpoweredDamagePerHalfSecond")    return DamageStage::EmpoweredDamagePerHalfSecond;
            if (s == "EmpoweredDamagePerQuarterSecond") return DamageStage::EmpoweredDamagePerQuarterSecond;
            return DamageStage::Default;
        }

        static DmgSpellEffectType ParseEffectType(const std::string& s) {
            if (s == "AoE")      return DmgSpellEffectType::AoE;
            if (s == "Single")   return DmgSpellEffectType::Single;
            if (s == "OverTime") return DmgSpellEffectType::OverTime;
            if (s == "Attack")   return DmgSpellEffectType::Attack;
            return DmgSpellEffectType::None;
        }

        // Parse a list of doubles from JSON
        static std::vector<double> ParseDoubleList(const nlohmann::json& j) {
            std::vector<double> result;
            if (j.is_array()) {
                for (auto& v : j) {
                    result.push_back(v.get<double>());
                }
            }
            return result;
        }

        // Parse a list of ints from JSON
        static std::vector<int> ParseIntList(const nlohmann::json& j) {
            std::vector<int> result;
            if (j.is_array()) {
                for (auto& v : j) {
                    result.push_back(v.get<int>());
                }
            }
            return result;
        }

        // Parse a bonus damage entry
        static DmgSpellBonus ParseBonus(const nlohmann::json& j) {
            DmgSpellBonus b;
            if (j.contains("DamageType"))
                b.DmgType = ParseDamageType(j["DamageType"].get<std::string>());
            if (j.contains("DamagePercentages"))
                b.DamagePercentages = ParseDoubleList(j["DamagePercentages"]);
            if (j.contains("ScalingTarget"))
                b.ScalingTarget = ParseScalingTarget(j["ScalingTarget"].get<std::string>());
            if (j.contains("ScalingType"))
                b.ScalingType = ParseScalingType(j["ScalingType"].get<std::string>());
            if (j.contains("BonusDamageOnMinion"))
                b.BonusDamageOnMinion = ParseDoubleList(j["BonusDamageOnMinion"]);
            if (j.contains("BonusDamageOnMonster"))
                b.BonusDamageOnMonster = ParseDoubleList(j["BonusDamageOnMonster"]);
            if (j.contains("MaxDamageOnMinion"))
                b.MaxDamageOnMinion = ParseIntList(j["MaxDamageOnMinion"]);
            if (j.contains("MaxDamageOnMonster"))
                b.MaxDamageOnMonster = ParseIntList(j["MaxDamageOnMonster"]);
            if (j.contains("ScalePer100Ad"))
                b.ScalePer100Ad = j["ScalePer100Ad"].get<double>();
            if (j.contains("ScalePer100Ap"))
                b.ScalePer100Ap = j["ScalePer100Ap"].get<double>();
            if (j.contains("ScalePer100BonusAd"))
                b.ScalePer100BonusAd = j["ScalePer100BonusAd"].get<double>();
            if (j.contains("ScalingBuff"))
                b.ScalingBuff = j["ScalingBuff"].get<std::string>();
            if (j.contains("ScalingBuffOffset"))
                b.ScalingBuffOffset = j["ScalingBuffOffset"].get<int>();
            if (j.contains("ScalingBuffTarget"))
                b.ScalingBuffTarget = ParseScalingTarget(j["ScalingBuffTarget"].get<std::string>());
            return b;
        }

        // Parse a DamagesOnMonster entry
        static DmgSpellOnMonster ParseOnMonster(const nlohmann::json& j) {
            DmgSpellOnMonster m;
            if (j.contains("DamageType"))
                m.DmgType = ParseDamageType(j["DamageType"].get<std::string>());
            if (j.contains("DamagePercentages"))
                m.DamagePercentages = ParseDoubleList(j["DamagePercentages"]);
            if (j.contains("ScalingTarget"))
                m.ScalingTarget = ParseScalingTarget(j["ScalingTarget"].get<std::string>());
            if (j.contains("ScalingType"))
                m.ScalingType = ParseScalingType(j["ScalingType"].get<std::string>());
            return m;
        }

        // Parse spell data
        static DmgSpellData ParseSpellData(const nlohmann::json& j) {
            DmgSpellData d;
            if (j.contains("SpellEffectType"))
                d.SpellEffectType = ParseEffectType(j["SpellEffectType"].get<std::string>());
            if (j.contains("DamageType"))
                d.DmgType = ParseDamageType(j["DamageType"].get<std::string>());
            if (j.contains("Damages"))
                d.Damages = ParseDoubleList(j["Damages"]);
            if (j.contains("DamagesPerLvl"))
                d.DamagesPerLvl = ParseDoubleList(j["DamagesPerLvl"]);
            if (j.contains("BonusDamageOnMinion"))
                d.BonusDamageOnMinion = ParseDoubleList(j["BonusDamageOnMinion"]);
            if (j.contains("BonusDamageOnMonster"))
                d.BonusDamageOnMonster = ParseDoubleList(j["BonusDamageOnMonster"]);
            if (j.contains("BonusDamageOnSoldier"))
                d.BonusDamageOnSoldier = ParseDoubleList(j["BonusDamageOnSoldier"]);
            if (j.contains("DamagesReductionOnSoldier"))
                d.DamagesReductionOnSoldier = ParseDoubleList(j["DamagesReductionOnSoldier"]);
            if (j.contains("DamagesReductionPerLvlOnSoldier"))
                d.DamagesReductionPerLvlOnSoldier = ParseDoubleList(j["DamagesReductionPerLvlOnSoldier"]);
            if (j.contains("MaxDamageOnMinion"))
                d.MaxDamageOnMinion = ParseIntList(j["MaxDamageOnMinion"]);
            if (j.contains("MaxDamageOnMonster"))
                d.MaxDamageOnMonster = ParseIntList(j["MaxDamageOnMonster"]);
            if (j.contains("MinDamageOnSoldier"))
                d.MinDamageOnSoldier = ParseIntList(j["MinDamageOnSoldier"]);
            if (j.contains("ScalePerTargetMissHealth"))
                d.ScalePerTargetMissHealth = ParseDoubleList(j["ScalePerTargetMissHealth"]);
            if (j.contains("MaxScaleTargetMissHealth"))
                d.MaxScaleTargetMissHealth = j["MaxScaleTargetMissHealth"].get<double>();
            if (j.contains("MaxLevelScalingValueOnMinion"))
                d.MaxLevelScalingValueOnMinion = j["MaxLevelScalingValueOnMinion"].get<double>();
            if (j.contains("ScalePerCritChance"))
                d.ScalePerCritChance = j["ScalePerCritChance"].get<double>();
            if (j.contains("ScalingValueOnSoldier"))
                d.ScalingValueOnSoldier = j["ScalingValueOnSoldier"].get<double>();
            if (j.contains("IsApplyOnHit"))
                d.IsApplyOnHit = j["IsApplyOnHit"].get<bool>();
            if (j.contains("IsModifiedDamage"))
                d.IsModifiedDamage = j["IsModifiedDamage"].get<bool>();

            // Parse BonusDamages array
            if (j.contains("BonusDamages") && j["BonusDamages"].is_array()) {
                for (auto& bj : j["BonusDamages"]) {
                    d.BonusDamages.push_back(ParseBonus(bj));
                }
            }

            // Parse DamagesOnMonster array
            if (j.contains("DamagesOnMonster") && j["DamagesOnMonster"].is_array()) {
                for (auto& mj : j["DamagesOnMonster"]) {
                    d.DamagesOnMonster.push_back(ParseOnMonster(mj));
                }
            }

            return d;
        }

        // Parse a single spell entry (Stage + SpellData)
        static DmgSpell ParseSpell(const nlohmann::json& j) {
            DmgSpell sp;
            if (j.contains("Stage"))
                sp.Stage = ParseStage(j["Stage"].get<std::string>());
            if (j.contains("SpellData"))
                sp.SpellData = ParseSpellData(j["SpellData"]);
            return sp;
        }

        // Parse spell array for a slot (Q/W/E/R)
        static std::vector<DmgSpell> ParseSpellArray(const nlohmann::json& j) {
            std::vector<DmgSpell> result;
            if (j.is_array()) {
                for (auto& sp : j) {
                    result.push_back(ParseSpell(sp));
                }
            }
            return result;
        }

        // ====================================================================
        // Load from JSON file
        // Format: { "ChampionName": { "Q": [...], "W": [...], "E": [...], "R": [...] }, ... }
        // ====================================================================
        static bool LoadFromJson(const std::string& jsonPath) {
            std::ifstream file(jsonPath);
            if (!file.is_open()) return false;

            try {
                nlohmann::json data = nlohmann::json::parse(file);
                s_collection.clear();

                for (auto& [champName, slots] : data.items()) {
                    ChampionDamage champ;
                    if (slots.contains("Q")) champ.Q = ParseSpellArray(slots["Q"]);
                    if (slots.contains("W")) champ.W = ParseSpellArray(slots["W"]);
                    if (slots.contains("E")) champ.E = ParseSpellArray(slots["E"]);
                    if (slots.contains("R")) champ.R = ParseSpellArray(slots["R"]);
                    s_collection[champName] = std::move(champ);
                }

                return !s_collection.empty();
            } catch (...) {
                return false;
            }
        }
    };

} // namespace SDK
