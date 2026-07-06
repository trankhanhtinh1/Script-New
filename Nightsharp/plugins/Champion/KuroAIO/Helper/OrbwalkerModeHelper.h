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

} // namespace Plugins::KuroAIO
