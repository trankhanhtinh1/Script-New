#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// ============================================================================
// Yasuo.h — C++ port of EzEvade/SpecialSpells/Yasuo.cs (48 lines)
//   Line-by-line, preserving original logic
// ============================================================================

namespace EzEvade {
namespace SpecialSpells {

class Yasuo : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 24: if (spellData.spellName == "YasuoQW" || "YasuoQ3W")
        if (spellData.spellName == "YasuoQW" || spellData.spellName == "YasuoQ3W") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Yasuo") {
                    yasuoSpellData = &spellData;
                    yasuoHero = &hero;
                    break;
                }
            }
        }
    }

    // C# lines 34-45: adjust spellDelay based on Yasuo's cast time
    void OnProcessSpell(SDK::GameObject* hero, const std::string& spellName,
                        int /*spellSlot*/, float castTime) {
        if (!hero || !hero->IsEnemy(SDK::GameObjects::Player)) return;
        if (!yasuoSpellData) return;

        // C# line 36: if (hero.IsEnemy && args.SData.Name == "YasuoQ")
        if (spellName == "YasuoQ") {
            // C# line 38: castTime = (hero.Spellbook.CastTime - Game.Time) * 1000
            float castTimeMs = castTime * 1000.0f;

            // C# line 40-43: if (castTime > 0) spellData.spellDelay = castTime
            if (castTimeMs > 0) {
                yasuoSpellData->spellDelay = castTimeMs;
            }
        }
    }

private:
    SpellData* yasuoSpellData = nullptr;
    SDK::GameObject* yasuoHero = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
