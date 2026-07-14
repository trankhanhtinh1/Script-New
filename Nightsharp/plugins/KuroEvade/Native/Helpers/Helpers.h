#pragma once

#include "../../../../SDK/SDK.h"

namespace Plugins::KuroEvade::Helpers {

inline bool IsSpellShielded(const SDK::AIHeroClient& unit) {
    if (!unit.IsValid()) {
        return false;
    }
    if (SDK::HasBuffOfType(unit, SDK::BuffType::SpellShield) ||
        SDK::HasBuffOfType(unit, SDK::BuffType::SpellImmunity)) {
        return true;
    }

    const SDK::LastCastedSpellEntry last =
        SDK::LastCast::GetLastCastedSpell(unit);
    if (!last.IsValid ||
        SDK::Variables::TickCount() - static_cast<int>(last.StartTime) >= 300) {
        return false;
    }
    return _stricmp(last.Name.c_str(), "SivirE") == 0 ||
           _stricmp(last.Name.c_str(), "BlackShield") == 0 ||
           _stricmp(last.Name.c_str(), "NocturneShit") == 0 ||
           _stricmp(last.Name.c_str(), "NocturneW") == 0;
}

} // namespace Plugins::KuroEvade::Helpers
