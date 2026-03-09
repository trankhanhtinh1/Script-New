#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"

namespace EzEvade {
namespace SpecialSpells {

// C# source currently keeps Jayce special handling commented out.
class Jayce : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData&) override {
        // Intentionally empty to preserve upstream behavior.
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

