#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"

namespace EzEvade {
namespace SpecialSpells {

// C# handler updates spell delay dynamically from cast-time context.
// Current SDK cast args do not expose full charge-up cast-time state,
// so this plugin intentionally keeps registration minimal.
class Yasuo : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData&) override {
        // Intentionally empty until cast-time exposure is expanded.
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

