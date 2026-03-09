#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Heimerdinger : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "HeimerdingerTurretEnergyBlast")
            && !EqualsI(spellData.spellName, "HeimerdingerTurretBigEnergyBlast")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!spellData) {
                    return;
                }
                if (!EqualsI(spellData->spellName, "HeimerdingerTurretEnergyBlast")
                    && !EqualsI(spellData->spellName, "HeimerdingerTurretBigEnergyBlast")) {
                    return;
                }

                SpellDetector::CreateSpellData(args.Sender, args.StartPos, args.EndPos, spellData);
                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

