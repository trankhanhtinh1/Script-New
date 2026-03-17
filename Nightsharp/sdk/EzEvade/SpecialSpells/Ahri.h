#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Utils/EvadeUtils.h"

// ============================================================================
// Ahri.h — C++ port of EzEvade/SpecialSpells/Ahri.cs (41 lines)
//   Line-by-line, preserving original logic
// ============================================================================

namespace EzEvade {
namespace SpecialSpells {

class Ahri : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "AhriOrbofDeception2") {
            // Find Ahri in enemy heroes and store her NetId
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Ahri") {
                    ahriNetId = hero.GetNetId();
                    break;
                }
            }
        }
    }

    void OnUpdate() {
        if (ahriNetId == 0) return;

        // Find the live Ahri object each tick
        SDK::GameObject* ahriHero = nullptr;
        for (auto& hero : SDK::GameObjects::EnemyHeroes) {
            if (hero.GetNetId() == ahriNetId && hero.IsValid()) {
                ahriHero = &hero;
                break;
            }
        }
        if (!ahriHero) return;

        for (auto& entry : SpellDetector::detectedSpells) {
            auto& spell = entry.second;
            if (spell.heroID == ahriNetId) {
                std::string lower = spell.info.spellName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                if (lower == "ahriorbofdeception2") {
                    spell.endPos = ahriHero->GetPosition().To2D();
                }
            }
        }
    }

private:
    int ahriNetId = 0;
};

} // namespace SpecialSpells
} // namespace EzEvade
