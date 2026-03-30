#pragma once

#include "../../../Backup/Script-New/Nightsharp/libs/nlohmann/json.hpp"
#include "../../Data/26.6.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/SpellSlot.h"
#include "../../Core/Objects.h"

#include <algorithm>
#include <new>
#include <string>
#include <unordered_map>
#include <vector>

namespace SDK {

enum class DamageScalingTarget {
    Source,
    Target
};

enum class DamageScalingType {
    Unknown,
    AttackPoints,
    BonusAttackPoints,
    AbilityPoints,
    BonusHealth,
    CurrentHealth,
    MaxHealth,
    MissingHealth,
    MaxMana,
    Armor,
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

class DamageLibrary {
public:
    struct BonusScaling {
        DamageType Type = DamageType::Physical;
        DamageScalingTarget Target = DamageScalingTarget::Source;
        DamageScalingType Scaling = DamageScalingType::Unknown;
        std::vector<float> Percentages = {};
    };

    struct SpellEntry {
        DamageStage Stage = DamageStage::Default;
        DamageType Type = DamageType::True;
        std::vector<float> Damages = {};
        std::vector<float> DamagesPerLevel = {};
        std::vector<float> ScalePerTargetMissingHealth = {};
        float MaxScaleTargetMissingHealth = 0.0f;
        float ScalePerCritChance = 0.0f;
        std::vector<BonusScaling> Bonuses = {};
    };

    struct ChampionDamage {
        std::unordered_map<SpellSlot, std::vector<SpellEntry>> Spells = {};
    };

    static bool Initialize() {
        if (s_collection) {
            return true;
        }

        auto* storage = new(std::nothrow) std::unordered_map<std::string, ChampionDamage>();
        if (!storage) {
            return false;
        }

        try {
            const auto root = nlohmann::json::parse(Data::Patch26_6::Json());
            for (auto championIt = root.begin(); championIt != root.end(); ++championIt) {
                ChampionDamage champion = {};
                for (auto spellIt = championIt.value().begin(); spellIt != championIt.value().end(); ++spellIt) {
                    const SpellSlot slot = ParseSlot(spellIt.key());
                    if (slot == SpellSlot::Unknown || !spellIt.value().is_array()) {
                        continue;
                    }

                    auto& entries = champion.Spells[slot];
                    for (const auto& stageNode : spellIt.value()) {
                        SpellEntry entry = {};
                        entry.Stage = ParseStage(stageNode.value("Stage", "Default"));

                        const auto spellDataIt = stageNode.find("SpellData");
                        if (spellDataIt == stageNode.end() || !spellDataIt->is_object()) {
                            continue;
                        }

                        const auto& spellData = *spellDataIt;
                        entry.Type = ParseDamageType(spellData.value("DamageType", "True"));
                        entry.Damages = ParseFloatArray(spellData, "Damages");
                        entry.DamagesPerLevel = ParseFloatArray(spellData, "DamagesPerLvl");
                        entry.ScalePerTargetMissingHealth = ParseFloatArray(spellData, "ScalePerTargetMissHealth");
                        entry.MaxScaleTargetMissingHealth = spellData.value("MaxScaleTargetMissHealth", 0.0f);
                        entry.ScalePerCritChance = spellData.value("ScalePerCritChance", 0.0f);

                        const auto bonusIt = spellData.find("BonusDamages");
                        if (bonusIt != spellData.end() && bonusIt->is_array()) {
                            for (const auto& bonusNode : *bonusIt) {
                                BonusScaling bonus = {};
                                bonus.Type = ParseDamageType(bonusNode.value("DamageType", "True"));
                                bonus.Target = ParseScalingTarget(bonusNode.value("ScalingTarget", "Source"));
                                bonus.Scaling = ParseScalingType(bonusNode.value("ScalingType", ""));
                                bonus.Percentages = ParseFloatArray(bonusNode, "DamagePercentages");
                                entry.Bonuses.push_back(std::move(bonus));
                            }
                        }

                        entries.push_back(std::move(entry));
                    }
                }

                (*storage)[championIt.key()] = std::move(champion);
            }
        } catch (...) {
            delete storage;
            return false;
        }

        s_collection = storage;
        return true;
    }

    static void Shutdown() {
        delete s_collection;
        s_collection = nullptr;
    }

    static bool HasChampion(const std::string& championName) {
        return Initialize() && s_collection->find(championName) != s_collection->end();
    }

    static float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot,
                                DamageStage stage = DamageStage::Default) {
        if (!source.IsValid() || !target.IsValid() || !Initialize()) {
            return 0.0f;
        }

        const auto it = s_collection->find(source.CharacterName());
        if (it == s_collection->end()) {
            return 0.0f;
        }

        const auto slotIt = it->second.Spells.find(slot);
        if (slotIt == it->second.Spells.end() || slotIt->second.empty()) {
            return 0.0f;
        }

        const SpellEntry* entry = nullptr;
        for (const auto& candidate : slotIt->second) {
            if (candidate.Stage == stage) {
                entry = &candidate;
                break;
            }
        }
        if (!entry) {
            entry = &slotIt->second.front();
        }

        const int spellLevel = std::max(1, source.GetSpell(slot).Level());
        const int championLevel = std::max(1, source.Level());

        float physical = 0.0f;
        float magical = 0.0f;
        float trueDamage = 0.0f;

        const auto accumulate = [&](DamageType type, float raw) {
            switch (type) {
            case DamageType::Physical:
                physical += raw;
                break;
            case DamageType::Magical:
                magical += raw;
                break;
            case DamageType::Mixed:
                physical += raw * 0.5f;
                magical += raw * 0.5f;
                break;
            case DamageType::True:
            default:
                trueDamage += raw;
                break;
            }
        };

        accumulate(entry->Type, Pick(entry->Damages, spellLevel - 1));
        if (!entry->DamagesPerLevel.empty()) {
            accumulate(entry->Type, Pick(entry->DamagesPerLevel, championLevel - 1));
        }

        if (!entry->ScalePerTargetMissingHealth.empty()) {
            const float missingHealth = std::max(0.0f, target.MaxHealth() - target.Health());
            const float rawScale = missingHealth * Pick(entry->ScalePerTargetMissingHealth, spellLevel - 1);
            accumulate(entry->Type, entry->MaxScaleTargetMissingHealth > 0.0f
                ? std::min(rawScale, entry->MaxScaleTargetMissingHealth)
                : rawScale);
        }

        if (entry->ScalePerCritChance > 0.0f) {
            accumulate(entry->Type, source.Crit() * entry->ScalePerCritChance);
        }

        for (const auto& bonus : entry->Bonuses) {
            const float scaleValue = ResolveScalingValue(bonus, source, target);
            accumulate(bonus.Type, scaleValue * Pick(bonus.Percentages, spellLevel - 1));
        }

        return source.CalculatePhysicalDamage(target, physical) +
               source.CalculateMagicDamage(target, magical) +
               trueDamage;
    }

private:
    static float Pick(const std::vector<float>& values, int index) {
        if (values.empty()) {
            return 0.0f;
        }
        const int safe = std::clamp(index, 0, static_cast<int>(values.size()) - 1);
        return values[safe];
    }

    static std::vector<float> ParseFloatArray(const nlohmann::json& node, const char* key) {
        std::vector<float> out;
        const auto it = node.find(key);
        if (it == node.end() || !it->is_array()) {
            return out;
        }
        out.reserve(it->size());
        for (const auto& value : *it) {
            out.push_back(value.get<float>());
        }
        return out;
    }

    static SpellSlot ParseSlot(const std::string& value) {
        if (value == "Q") return SpellSlot::Q;
        if (value == "W") return SpellSlot::W;
        if (value == "E") return SpellSlot::E;
        if (value == "R") return SpellSlot::R;
        return SpellSlot::Unknown;
    }

    static DamageStage ParseStage(const std::string& value) {
        if (value == "WayBack") return DamageStage::WayBack;
        if (value == "Detonation") return DamageStage::Detonation;
        if (value == "DamagePerTick") return DamageStage::DamagePerTick;
        if (value == "DamagePerTime") return DamageStage::DamagePerTime;
        if (value == "DamagePerHalfSecond") return DamageStage::DamagePerHalfSecond;
        if (value == "DamagePerQuarterSecond") return DamageStage::DamagePerQuarterSecond;
        if (value == "DamagePerSecond") return DamageStage::DamagePerSecond;
        if (value == "SingleTotal") return DamageStage::SingleTotal;
        if (value == "SecondForm") return DamageStage::SecondForm;
        if (value == "ThirdForm") return DamageStage::ThirdForm;
        if (value == "SecondCast") return DamageStage::SecondCast;
        if (value == "ThirdCast") return DamageStage::ThirdCast;
        if (value == "Buff") return DamageStage::Buff;
        if (value == "Empowered") return DamageStage::Empowered;
        if (value == "EmpoweredDamagePerSecond") return DamageStage::EmpoweredDamagePerSecond;
        if (value == "EmpoweredDamagePerHalfSecond") return DamageStage::EmpoweredDamagePerHalfSecond;
        if (value == "EmpoweredDamagePerQuarterSecond") return DamageStage::EmpoweredDamagePerQuarterSecond;
        return DamageStage::Default;
    }

    static DamageType ParseDamageType(const std::string& value) {
        if (value == "Physical") return DamageType::Physical;
        if (value == "Magical") return DamageType::Magical;
        if (value == "Mixed") return DamageType::Mixed;
        return DamageType::True;
    }

    static DamageScalingTarget ParseScalingTarget(const std::string& value) {
        return value == "Target" ? DamageScalingTarget::Target : DamageScalingTarget::Source;
    }

    static DamageScalingType ParseScalingType(const std::string& value) {
        if (value == "AttackPoints") return DamageScalingType::AttackPoints;
        if (value == "BonusAttackPoints") return DamageScalingType::BonusAttackPoints;
        if (value == "AbilityPoints") return DamageScalingType::AbilityPoints;
        if (value == "BonusHealth") return DamageScalingType::BonusHealth;
        if (value == "CurrentHealth") return DamageScalingType::CurrentHealth;
        if (value == "MaxHealth") return DamageScalingType::MaxHealth;
        if (value == "MissingHealth") return DamageScalingType::MissingHealth;
        if (value == "MaxMana") return DamageScalingType::MaxMana;
        if (value == "Armor") return DamageScalingType::Armor;
        if (value == "PhysicalLethality") return DamageScalingType::PhysicalLethality;
        return DamageScalingType::Unknown;
    }

    static float ResolveScalingValue(const BonusScaling& bonus,
                                     const AIBaseClient& source,
                                     const AIBaseClient& target) {
        const AIBaseClient& scaler = bonus.Target == DamageScalingTarget::Target ? target : source;
        switch (bonus.Scaling) {
        case DamageScalingType::AttackPoints:
            return scaler.TotalAttackDamage();
        case DamageScalingType::BonusAttackPoints:
            return scaler.BonusAttackDamage();
        case DamageScalingType::AbilityPoints:
            return scaler.AbilityPower();
        case DamageScalingType::BonusHealth:
            return scaler.MaxHealth();
        case DamageScalingType::CurrentHealth:
            return scaler.Health();
        case DamageScalingType::MaxHealth:
            return scaler.MaxHealth();
        case DamageScalingType::MissingHealth:
            return std::max(0.0f, scaler.MaxHealth() - scaler.Health());
        case DamageScalingType::MaxMana:
            return scaler.MaxMana();
        case DamageScalingType::Armor:
            return scaler.Armor();
        case DamageScalingType::PhysicalLethality:
            return scaler.Lethality();
        case DamageScalingType::Unknown:
        default:
            return 0.0f;
        }
    }

    static inline std::unordered_map<std::string, ChampionDamage>* s_collection = nullptr;
};

} // namespace SDK
