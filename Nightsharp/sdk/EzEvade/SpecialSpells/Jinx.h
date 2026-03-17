#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Jinx.h — C++ port of EzEvade/SpecialSpells/Jinx.cs (48 lines)
// NOTE: JinxWMissile is commented out in original C# — preserving that.

namespace EzEvade {
namespace SpecialSpells {

class Jinx : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 24: commented out in original
        /*if (spellData.spellName == "JinxWMissile") { }*/
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
