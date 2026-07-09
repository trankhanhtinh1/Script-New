#pragma once

#include "CastType.h"
#include "EvadeType.h"
#include "MoveSpeedAmount.h"
#include "SpellTargets.h"
#include "UseSpellFunc.h"

#include "../../../SDK/SDK.h"

#include <string>
#include <vector>

namespace Plugins::KuroEvade {

struct EvadeSpellData {
    CastType CastTypeValue = CastType::Position;
    std::string ChampionName;
    std::string CheckSpellName;
    int DangerLevel = 0;
    int Delay = 0;
    EvadeType EvadeTypeValue = EvadeType::Blink;
    bool FixedRange = false;
    bool IsBehindTarget = false;
    bool IsInfrontTarget = false;
    bool IsItem = false;
    bool IsReversed = false;
    bool IsSpecial = false;
    bool IsSummonerSpell = false;
    int ItemId = 0;
    int MaxRange = 0;
    int MinRange = 0;
    bool HasMoveSpeedAmount = false;
    MoveSpeedAmount MoveSpeedTotalAmount;
    std::string Name;
    SDK::SpellSlot Slot = SDK::SpellSlot::Unknown;
    int Speed = 0;
    UseSpellFunc UseSpell;
    std::vector<SpellTargets> ValidTargets;
    bool IsEnabled = true;
    int IndexUseWhen = 2;
};

} // namespace Plugins::KuroEvade
