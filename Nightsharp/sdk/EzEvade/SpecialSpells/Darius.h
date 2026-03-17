#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// ============================================================================
// Darius.h — C++ port of EzEvade/SpecialSpells/Darius.cs (42 lines)
//   Line-by-line, preserving original logic
// ============================================================================

namespace EzEvade {
namespace SpecialSpells {

class Darius : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 19: if (spellData.spellName == "DariusCleave")
        if (spellData.spellName == "DariusCleave") {
            // C# line 21-25: find Darius hero
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Darius") {
                    dariusHero = &hero;
                    break;
                }
            }
        }
    }

    // C# lines 29-38: Game_OnUpdate — update DariusCleave positions
    void OnUpdate() {
        if (!dariusHero || !dariusHero->IsValid()) return;

        for (auto& entry : SpellDetector::detectedSpells) {
            auto& spell = entry.second;
            if (spell.heroID == dariusHero->GetNetId()) {
                if (spell.info.spellName == "DariusCleave") {
                    // C# line 35-36: update start/end to hero position
                    spell.startPos = dariusHero->GetPosition().To2D();
                    spell.endPos = dariusHero->GetPosition().To2D() +
                                   spell.direction * spell.info.range;
                }
            }
        }
    }

private:
    SDK::GameObject* dariusHero = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
