#pragma once
#include "../Enumerations/DangerLevel.h"
#include "../Enumerations/SpellSlot.h"

#include <cctype>

namespace SDK::Generated::InterruptableSpellData {

struct InterruptableEntry {
    const char* ChampionName;
    SDK::DangerLevel DangerLevel;
    bool MovementInterrupts;
    const char* Name;
    SDK::SpellSlot Slot;
};

inline constexpr int kChampionInterruptableCount = 30;
inline constexpr int kGlobalInterruptableCount = 1;

inline const InterruptableEntry kChampionInterruptables[] = {
    InterruptableEntry{ "Caitlyn", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "FiddleSticks", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::W },
    InterruptableEntry{ "FiddleSticks", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Galio", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Janna", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Jhin", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Karthus", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Katarina", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Lucian", SDK::DangerLevel::High, false, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Malzahar", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "MasterYi", SDK::DangerLevel::Low, true, "", SDK::SpellSlot::W },
    InterruptableEntry{ "MissFortune", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Nunu", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Pantheon", SDK::DangerLevel::Low, true, "", SDK::SpellSlot::E },
    InterruptableEntry{ "Pantheon", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Pyke", SDK::DangerLevel::Low, false, "", SDK::SpellSlot::Q },
    InterruptableEntry{ "Quinn", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Samira", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Shen", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Sion", SDK::DangerLevel::Low, true, "", SDK::SpellSlot::Q },
    InterruptableEntry{ "TahmKench", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "TwistedFate", SDK::DangerLevel::Medium, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Varus", SDK::DangerLevel::Medium, false, "", SDK::SpellSlot::Q },
    InterruptableEntry{ "Velkoz", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Vi", SDK::DangerLevel::Medium, false, "", SDK::SpellSlot::Q },
    InterruptableEntry{ "Warwick", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Xerath", SDK::DangerLevel::Medium, false, "", SDK::SpellSlot::Q },
    InterruptableEntry{ "Xerath", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Yuumi", SDK::DangerLevel::High, true, "", SDK::SpellSlot::R },
    InterruptableEntry{ "Zac", SDK::DangerLevel::Low, true, "", SDK::SpellSlot::E }
};

inline const InterruptableEntry kGlobalInterruptables[] = {
    InterruptableEntry{ "", SDK::DangerLevel::Medium, true, "summonerteleport", SDK::SpellSlot::Unknown }
};

namespace detail {
    inline char Lower(char c) {
        return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }

    inline bool EqualsIgnoreCase(const char* a, const char* b) {
        if (!a || !b) {
            return false;
        }

        while (*a && *b) {
            if (Lower(*a++) != Lower(*b++)) {
                return false;
            }
        }

        return *a == 0 && *b == 0;
    }

    inline bool HasText(const char* value) {
        return value && value[0] != 0;
    }
} // namespace detail

inline const InterruptableEntry* FindGlobalBySpellName(const char* spellName) {
    if (!spellName || !spellName[0]) {
        return nullptr;
    }

    for (int i = 0; i < kGlobalInterruptableCount; ++i) {
        if (detail::HasText(kGlobalInterruptables[i].Name) &&
            detail::EqualsIgnoreCase(kGlobalInterruptables[i].Name, spellName)) {
            return &kGlobalInterruptables[i];
        }
    }

    return nullptr;
}

inline const InterruptableEntry* FindChampionByNameAndSlot(const char* championName, SDK::SpellSlot slot) {
    if (!championName || !championName[0]) {
        return nullptr;
    }

    for (int i = 0; i < kChampionInterruptableCount; ++i) {
        const auto& entry = kChampionInterruptables[i];
        if (entry.Slot == slot && detail::EqualsIgnoreCase(entry.ChampionName, championName)) {
            return &entry;
        }
    }

    return nullptr;
}

inline const InterruptableEntry* FindBySpell(const char* championName, SDK::SpellSlot slot, const char* spellName) {
    if (const auto* global = FindGlobalBySpellName(spellName)) {
        return global;
    }

    return FindChampionByNameAndSlot(championName, slot);
}

} // namespace SDK::Generated::InterruptableSpellData
