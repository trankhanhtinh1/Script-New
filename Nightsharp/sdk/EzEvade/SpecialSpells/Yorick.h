#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Yorick.h — C++ port of EzEvade/SpecialSpells/Yorick.cs (43 lines)

namespace EzEvade {
namespace SpecialSpells {

class Yorick : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "YorickE") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 23-39: YorickE — splash at target location
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "YorickE") {
            Vec3 endPos = end;
            Vec3 startPos = start;
            Vec3 direction = (endPos - startPos).Normalized();

            if (startPos.Distance(endPos) > spellData.range)
                endPos = startPos + (endPos - startPos).Normalized() * spellData.range;

            Vec3 spellStart = endPos.Extend(hero->GetPosition(), 100);
            Vec3 spellEnd = spellStart + direction * 1;

            SpellDetector::CreateSpellData(hero, spellStart, spellEnd, spellData);
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
