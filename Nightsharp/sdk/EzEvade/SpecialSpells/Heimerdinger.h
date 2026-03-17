#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Heimerdinger.h — C++ port of EzEvade/SpecialSpells/Heimerdinger.cs (58 lines)

namespace EzEvade {
namespace SpecialSpells {

class Heimerdinger : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "HeimerdingerTurretEnergyBlast"
            || spellData.spellName == "HeimerdingerTurretBigEnergyBlast") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "HeimerdingerTurretEnergyBlast"
            || spellData.spellName == "HeimerdingerTurretBigEnergyBlast") {
            SpellDetector::CreateSpellData(hero, start, end, spellData);
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
