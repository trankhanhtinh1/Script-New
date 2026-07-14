#pragma once

#include "../CastType.h"
#include "../EvadeType.h"
#include "../SpellTargets.h"
#include "../../../../SDK/Enumerations/SpellSlot.h"

#include <string>
#include <vector>

namespace Plugins::KuroEvade::Database {

// Canonical player-evade spell model. This is the native equivalent of the
// supplied EvadeSpellData class; no imported/compatibility copy is retained.
struct EvadeSpellData {
    CastType CastTypeValue = CastType::Position;
    std::string ChampionName;
    std::string CheckSpellName;
    int DangerLevel = 1;
    float Delay = 250.0f;
    EvadeType EvadeTypeValue = EvadeType::Blink;
    bool FixedRange = false;
    bool IsBehindTarget = false;
    bool IsInfrontTarget = false;
    bool IsItem = false;
    bool IsReversed = false;
    bool IsSpecial = false;
    bool IsSummonerSpell = false;
    bool CanShieldAllies = false;
    bool Untargetable = false;
    int ItemId = 0;
    float MaxRange = 0.0f;
    float MinRange = 0.0f;
    std::string Name;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    float Speed = 0.0f;
    std::vector<float> SpeedArray;
    std::vector<SpellTargets> ValidTargets;
    bool IsEnabled = true;
    int IndexUseWhen = 2;

    bool IsTargeted() const {
        return !ValidTargets.empty();
    }
};

} // namespace Plugins::KuroEvade::Database
