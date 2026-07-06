#pragma once

// DamageLibrary — zero-runtime-parse using constexpr struct-array data
// (see `sdk/Data/DamageData.h` for the full 173-champion patch-26.11 dataset).
// Replaces old nlohmann::json-based init that crashed in manual-map context.

#include "../../Data/DamageData.h"
#include "../../Enumerations/DamageType.h"
#include "../../Enumerations/SpellSlot.h"
#include "../../Core/Objects.h"
#include "../../GameObjects/InventorySlot.h"

#include <algorithm>
#include <cstring>
#include <string>

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

#ifndef NIGHTSHARP_SDK_DAMAGE_STAGE_DEFINED
#define NIGHTSHARP_SDK_DAMAGE_STAGE_DEFINED
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
#endif

class DamageLibrary {
public:

    static float GetSpellDamage(const AIBaseClient& source,
                                const AIBaseClient& target,
                                SpellSlot slot,
                                DamageStage stage = DamageStage::Default) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }

        const DamageData::StageEntry* entry = FindStageEntry(source, slot, stage);
        if (!entry) {
            return 0.0f;
        }

        const int spellLevel = std::max(1, source.GetSpell(slot).Level());
        const int championLevel = std::max(1, source.Level());

        float physical = 0.0f;
        float magical = 0.0f;
        float trueDamage = 0.0f;

        const auto accumulate = [&](uint8_t type, float raw) {
            switch (type) {
            case 0: physical += raw; break;   // Physical
            case 1: magical += raw; break;    // Magical
            case 2: physical += raw * 0.5f; magical += raw * 0.5f; break; // Mixed
            case 3: default: trueDamage += raw; break; // True
            }
        };

        // Base damages
        accumulate(entry->Type, PickArr(entry->Damages, entry->DamageCount, spellLevel - 1));

        // Damages per level
        if (entry->DmgPerLvlCount > 0) {
            accumulate(entry->Type, PickArr(entry->DamagesPerLvl, entry->DmgPerLvlCount, championLevel - 1));
        }

        // Scale per target missing health
        if (entry->ScaleMissHpCount > 0) {
            const float missingHealth = std::max(0.0f, target.MaxHealth() - target.Health());
            const float rawScale = missingHealth * PickArr(entry->ScaleMissHp, entry->ScaleMissHpCount, spellLevel - 1);
            accumulate(entry->Type, entry->MaxScaleMissHp > 0.0f
                ? std::min(rawScale, entry->MaxScaleMissHp)
                : rawScale);
        }

        // Scale per crit chance
        if (entry->ScalePerCrit > 0.0f) {
            accumulate(entry->Type, source.Crit() * entry->ScalePerCrit);
        }

        // Bonus scaling
        for (int i = 0; i < entry->BonusCount && i < DamageData::kMaxBonuses; ++i) {
            const auto& bonus = entry->Bonuses[i];
            const float scaleValue = ResolveScaling(bonus, source, target);
            accumulate(bonus.Type, scaleValue * PickArr(bonus.Percentages, bonus.Count, spellLevel - 1));
        }

        return source.CalculatePhysicalDamage(target, physical) +
               source.CalculateMagicDamage(target, magical) +
               trueDamage;
    }

    static DamageType GetSpellDamageType(const AIBaseClient& source,
                                         SpellSlot slot,
                                         DamageStage stage = DamageStage::Default) {
        const DamageData::StageEntry* entry = FindStageEntry(source, slot, stage);
        return entry ? MapDamageType(entry->Type) : DamageType::Physical;
    }

    static float GetAutoAttackDamage(const AIBaseClient& source,
                                     const AIBaseClient& target) {
        if (!source.IsValid() || !target.IsValid()) {
            return 0.0f;
        }
        return source.CalculatePhysicalDamage(target, source.AD());
    }

private:
    static float PickArr(const float* arr, int count, int index) {
        if (count <= 0) return 0.0f;
        const int safe = (index < 0) ? 0 : (index >= count ? count - 1 : index);
        return arr[safe];
    }

    static uint8_t MapSlot(SpellSlot slot) {
        switch (slot) {
        case SpellSlot::Q: return 0;
        case SpellSlot::W: return 1;
        case SpellSlot::E: return 2;
        case SpellSlot::R: return 3;
        default: return 255;
        }
    }

    static DamageType MapDamageType(uint8_t type) {
        switch (type) {
        case 0: return DamageType::Physical;
        case 1: return DamageType::Magical;
        case 2: return DamageType::Mixed;
        case 3: default: return DamageType::True;
        }
    }

    static const DamageData::StageEntry* FindStageEntry(const AIBaseClient& source,
                                                        SpellSlot slot,
                                                        DamageStage stage) {
        if (!source.IsValid()) {
            return nullptr;
        }

        std::string characterName = source.CharacterName();
        if (characterName.empty()) {
            char directName[64] = {};
            if (::Core::Objects::ReadCharacterName(
                    source.Address(),
                    directName,
                    static_cast<int>(sizeof(directName)))) {
                characterName = directName;
            }
        }

        const auto* champ = DamageData::FindChampion(characterName.c_str());
        if (!champ) {
            return nullptr;
        }

        const uint8_t slotIdx = MapSlot(slot);
        if (slotIdx > 3) {
            return nullptr;
        }

        const DamageData::SlotEntry* slotEntry = nullptr;
        for (int i = 0; i < champ->SlotCount; ++i) {
            if (champ->Slots[i].Slot == slotIdx) {
                slotEntry = &champ->Slots[i];
                break;
            }
        }
        if (!slotEntry || slotEntry->StageCount == 0) {
            return nullptr;
        }

        const uint8_t stageIdx = static_cast<uint8_t>(stage);
        const DamageData::StageEntry* entry = &slotEntry->Stages[0];
        for (int i = 0; i < slotEntry->StageCount; ++i) {
            if (slotEntry->Stages[i].Stage == stageIdx) {
                entry = &slotEntry->Stages[i];
                break;
            }
        }
        return entry;
    }

    static float ResolveScaling(const DamageData::BonusEntry& bonus,
                                const AIBaseClient& source,
                                const AIBaseClient& target) {
        const AIBaseClient& scaler = bonus.Target == 1 ? target : source;
        switch (bonus.Scaling) {
        case 1:  return scaler.AD();                     // AttackPoints       (total AD)
        case 2:  return scaler.BonusAttackDamage();       // BonusAttackPoints  (kept long: no short alias)
        case 3:  return scaler.AP();                      // AbilityPoints      (= ability power)
        case 4:  return scaler.BonusHealth();              // BonusHealth
        case 5:  return scaler.Health();                  // CurrentHealth
        case 6:  return scaler.MaxHealth();               // MaxHealth
        case 7:  return std::max(0.0f, scaler.MaxHealth() - scaler.Health()); // MissingHealth
        case 8:  return scaler.MaxMana();                 // MaxMana
        case 9:  return scaler.Armor();                   // Armor
        case 10: return scaler.Lethality();               // PhysicalLethality
        default: return 0.0f;
        }
    }
};

inline float AIBaseClient::GetSpellDamage(const AIBaseClient& target, SpellSlot slot) const {
    return DamageLibrary::GetSpellDamage(*this, target, slot);
}

} // namespace SDK
