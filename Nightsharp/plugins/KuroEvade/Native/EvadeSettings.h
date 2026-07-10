#pragma once

#include <cstdint>

namespace Plugins::KuroEvade {

struct EvadeSettings {
    bool Enabled = true;
    bool DrawSpells = true;
    bool DodgeKeyActive = true;
    bool DodgeDangerousOnly = false;
    bool DodgeFow = true;
    bool DodgeCircular = true;
    bool ExtremeEvade = false;
    bool KurokamiPosition = true;
    bool HigherPrecision = false;
    bool PreventTower = false;
    bool PreventEnemy = true;
    bool UseEvadeSpells = true;
    bool PreferEvadeSpells = false;
    bool CalculateWindupDelay = true;
    bool CheckSpellCollision = false;
    bool ClickOnlyOnce = true;

    int EvadeMode = 2;
    int CandidateBudget = 0;
    int SpellActivationTime = 400;
    float ExtraDelay = 30.0f;
    float ExtraDist = 10.0f;
    float ExtraSpellRadius = 0.0f;
    float ExtraEvadeDistance = 100.0f;
    float ExtraAvoidDistance = 50.0f;
    float MinComfortZone = 550.0f;
    int ReactionTime = 0;
    int SpellDetectionTime = 0;
    int MinHitTime = 900;
    int DodgeInterval = 0;
    float DodgeHp = 100.0f;
    float FastActivationTime = 65.0f;
    float RejectMinDistance = 10.0f;
    float CurrentWindupDelay = 0.0f;
    std::uint32_t SpellColor = 0;
};

} // namespace Plugins::KuroEvade
