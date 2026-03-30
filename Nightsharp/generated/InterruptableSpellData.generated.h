#pragma once
#include "../sdk/Enumerations/DangerLevel.h"
#include "../sdk/Enumerations/SpellSlot.h"

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

} // namespace SDK::Generated::InterruptableSpellData
