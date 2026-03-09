#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"

namespace EzEvade {
namespace SpecialSpells {

// C# source keeps Jinx special handling disabled (commented out).
class Jinx : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData&) override {
        // Intentionally empty to preserve upstream behavior.
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

