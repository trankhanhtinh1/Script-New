#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Graves.h — C++ port of EzEvade/SpecialSpells/Graves.cs (55 lines)

namespace EzEvade {
namespace SpecialSpells {

class Graves : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "GravesQLineSpell") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 24-52: GravesQ — perpendicular detonation zone
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "GravesQLineSpell") {
            SpellData newData = spellData;
            newData.isPerpendicular = true;
            newData.secondaryRadius = 255.0f;
            newData.updatePosition = false;
            newData.extraEndTime = 1300;

            Vec3 endPos = end;
            Vec3 startPos = start;

            if (endPos.Distance(startPos) > newData.range)
                endPos = startPos + (endPos - startPos).Normalized() * newData.range;
            if (endPos.Distance(startPos) < newData.range)
                endPos = startPos + (endPos - startPos).Normalized() * newData.range;

            SpellDetector::CreateSpellData(hero, hero->GetPosition(), endPos, newData);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
