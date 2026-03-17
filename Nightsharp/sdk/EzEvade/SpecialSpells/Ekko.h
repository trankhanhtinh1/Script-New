#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Ekko.h — C++ port of EzEvade/SpecialSpells/Ekko.cs (50 lines)

namespace EzEvade {
namespace SpecialSpells {

class Ekko : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "EkkoR") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_EkkoR(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 30-47: find Ekko ghost minion and create spell at its position
    static void ProcessSpell_EkkoR(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "EkkoR") {
            for (auto& obj : SDK::GameObjects::EnemyMinions) {
                if (obj.IsValid() && obj.GetChampionName() == "Ekko") {
                    Vec2 blinkPos = obj.GetPosition().To2D();
                    SpellDetector::CreateSpellData(hero, start, Vec3(blinkPos.x, 0, blinkPos.y), spellData);
                }
            }
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
