#pragma once

#include "../../generated/InterruptableSpellData.generated.h"
#include "Interrupter.h"

namespace SDK::Events {

    using InterruptableSpell = SDK::Interrupter;
    using InterruptableSpellData = Generated::InterruptableSpellData::InterruptableEntry;
    using InterruptableTargetEventArgs = SDK::Interrupter::InterruptSpellArgs;

    inline bool AddOnInterruptableTarget(SDK::Interrupter::Handler handler) {
        return SDK::Interrupter::AddOnInterruptableTarget(handler);
    }

    inline bool OnInterruptableTarget(SDK::Interrupter::Handler handler) {
        return AddOnInterruptableTarget(handler);
    }

    inline InterruptableTargetEventArgs GetInterruptableTargetData(const AIHeroClient& target) {
        return SDK::Interrupter::GetInterruptableTargetData(target);
    }

    inline std::vector<SDK::Interrupter::ActiveSpellEntry> CastingInterruptableSpell() {
        return SDK::Interrupter::CastingInterruptableSpell();
    }

    inline bool IsCastingInterruptableSpell(const AIHeroClient& target, bool checkMovementInterruption = false) {
        return SDK::Interrupter::IsCastingInterruptableSpell(target, checkMovementInterruption);
    }

    inline const InterruptableSpellData* GlobalInterruptableSpells() {
        return Generated::InterruptableSpellData::kGlobalInterruptables;
    }

    inline int GlobalInterruptableSpellsCount() {
        return Generated::InterruptableSpellData::kGlobalInterruptableCount;
    }

    inline const InterruptableSpellData* ChampionInterruptableSpells() {
        return Generated::InterruptableSpellData::kChampionInterruptables;
    }

    inline int ChampionInterruptableSpellsCount() {
        return Generated::InterruptableSpellData::kChampionInterruptableCount;
    }
}
