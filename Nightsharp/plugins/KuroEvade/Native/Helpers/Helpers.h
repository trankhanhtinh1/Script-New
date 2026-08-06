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
    static constexpr uint32_t kSivirE = SDK::Utils::HashName("SivirE");
    static constexpr uint32_t kBlackShield = SDK::Utils::HashName("BlackShield");
    static constexpr uint32_t kNocturneShit = SDK::Utils::HashName("NocturneShit");
    static constexpr uint32_t kNocturneW = SDK::Utils::HashName("NocturneW");
    const uint32_t spellHash = SDK::Utils::HashName(last.Name.c_str());
    return spellHash == kSivirE ||
           spellHash == kBlackShield ||
           spellHash == kNocturneShit ||
           spellHash == kNocturneW;
}

} // namespace Plugins::KuroEvade::Helpers
