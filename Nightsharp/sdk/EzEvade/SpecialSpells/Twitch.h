#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Twitch.h — C++ port of EzEvade/SpecialSpells/Twitch.cs (47 lines)

namespace EzEvade {
namespace SpecialSpells {

class Twitch : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "TwitchSprayandPrayAttack") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 29-44: TwitchR auto-attack as skillshot
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "TwitchSprayandPrayAttack") {
            Vec3 heroPos = hero->GetPosition();
            Vec3 endPos = heroPos + (end - heroPos) * spellData.range;

            SpellData data = spellData;
            data.spellDelay = hero->AttackCastDelay() * 1000;

            SpellDetector::CreateSpellData(hero, heroPos, endPos, data);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
