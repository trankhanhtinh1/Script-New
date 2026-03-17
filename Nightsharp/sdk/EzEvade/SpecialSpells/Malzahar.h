#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Malzahar.h — C++ port of EzEvade/SpecialSpells/Malzahar.cs (50 lines)

namespace EzEvade {
namespace SpecialSpells {

class Malzahar : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "MalzaharQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 30-47: MalzaharQ — two perpendicular lines
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "MalzaharQ") {
            Vec2 direction = (end.To2D() - start.To2D()).Normalized();
            Vec2 pDirection = direction.Perpendicular();
            Vec2 targetPoint = end.To2D();

            Vec2 pos1 = targetPoint - pDirection * spellData.sideRadius;
            Vec2 pos2 = targetPoint + pDirection * spellData.sideRadius;

            SpellDetector::CreateSpellData(hero,
                Vec3(pos1.x, 0, pos1.y), Vec3(pos2.x, 0, pos2.y), spellData, nullptr, 0, false);
            SpellDetector::CreateSpellData(hero,
                Vec3(pos2.x, 0, pos2.y), Vec3(pos1.x, 0, pos1.y), spellData, nullptr, 0);

            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
