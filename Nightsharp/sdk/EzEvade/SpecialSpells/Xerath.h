#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Xerath.h — C++ port of EzEvade/SpecialSpells/Xerath.cs (48 lines)
// NOTE: Handler is commented out in original C# — preserving that.

namespace EzEvade {
namespace SpecialSpells {

class Xerath : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "xeratharcanopulse2") {
            // C# line 26: handler commented out in original
            //SpellDetector::OnProcessSpecialSpell.push_back(ProcessSpell_XerathArcanopulse2);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
