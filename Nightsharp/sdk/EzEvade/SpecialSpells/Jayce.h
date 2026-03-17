#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Jayce.h — C++ port of EzEvade/SpecialSpells/Jayce.cs (166 lines)
// NOTE: In C# original, JayceShockBlastWall is commented out — preserving that.

namespace EzEvade {
namespace SpecialSpells {

class Jayce : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 24: commented out in original
        /*if (spellData.spellName == "JayceShockBlastWall") { }*/
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
