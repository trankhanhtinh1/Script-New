#pragma once

#include "../../../../SDK/SDK.h"

namespace Plugins::KuroAIO {

inline bool IsComboMode() {
    return Orbwalker::ActiveMode() == OrbwalkingMode::Combo;
}

inline bool IsHarassMode() {
    return Orbwalker::ActiveMode() == OrbwalkingMode::Harass;
}

inline bool IsClearMode() {
    return Orbwalker::ActiveMode() == OrbwalkingMode::LaneClear;
}

inline bool IsLastHitMode() {
    return Orbwalker::ActiveMode() == OrbwalkingMode::LastHit;
}

inline bool CanAttack(int maxWaitMs = 250) {
    if (Orbwalker::CanAttack()) {
        return true;
    }
    const int cd = Orbwalker::AttackCooldownRemaining();
    return cd >= 0 && cd <= maxWaitMs;
}

} // namespace Plugins::KuroAIO
