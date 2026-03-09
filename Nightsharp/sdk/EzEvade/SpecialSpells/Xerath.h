#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"

namespace EzEvade {
namespace SpecialSpells {

// C# source keeps Xerath special handler disabled (commented out).
class Xerath : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData&) override {
        // Intentionally empty to preserve upstream behavior.
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

