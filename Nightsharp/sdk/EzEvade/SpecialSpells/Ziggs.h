#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Ziggs.h — C++ port of EzEvade/SpecialSpells/Ziggs.cs (56 lines)

namespace EzEvade {
namespace SpecialSpells {

class Ziggs : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "ZiggsQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_ZiggsQ(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 30-53: ZiggsQ — 3 bounces with different ranges and delays
    static void ProcessSpell_ZiggsQ(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "ZiggsQ") {
            Vec2 startPos = hero->GetPosition().To2D();
            Vec2 endPos = end.To2D();
            Vec2 dir = (endPos - startPos).Normalized();

            // First bounce cap at 850
            if (endPos.Distance(startPos) > 850) {
                endPos = startPos + dir * 850;
            }

            // Bounce 1
            SpellDetector::CreateSpellData(hero, start, Vec3(endPos.x, 0, endPos.y), spellData, nullptr, 0, false);

            // Bounce 2
            Vec2 endPos2 = endPos + dir * 0.4f * startPos.Distance(endPos);
            SpellDetector::CreateSpellData(hero, start, Vec3(endPos2.x, 0, endPos2.y), spellData, nullptr, 250, false);

            // Bounce 3
            Vec2 endPos3 = endPos2 + dir * 0.6f * endPos.Distance(endPos2);
            SpellDetector::CreateSpellData(hero, start, Vec3(endPos3.x, 0, endPos3.y), spellData, nullptr, 800);

            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
