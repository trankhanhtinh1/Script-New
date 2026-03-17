#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Helpers/ObjectCache.h"

// Lucian.h — C++ port of EzEvade/SpecialSpells/Lucian.cs (51 lines)

namespace EzEvade {
namespace SpecialSpells {

class Lucian : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "LucianQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_LucianQ(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 30-48: LucianQ — predict target position
    static void ProcessSpell_LucianQ(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "LucianQ") {
            // C# line 38: spellDelay adjusted for ping
            float spellDelay = (350.0f - ObjectCache::gamePing) / 1000.0f;
            // Simplified: use end position as-is since we don't have target object
            SpellDetector::CreateSpellData(hero, start, end, spellData, nullptr, 0);
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
