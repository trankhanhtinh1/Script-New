#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Ekko : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "EkkoR")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "EkkoR")) {
                    return;
                }

                for (const auto& minion : SDK::ObjectManager::GetMinions()) {
                    if (!minion.IsValid() || minion.IsDead()) {
                        continue;
                    }
                    if (!Situation::CheckTeam(minion)) {
                        continue;
                    }
                    if (!EqualsI(minion.GetName(), "Ekko")) {
                        continue;
                    }

                    SpellDetector::CreateSpellData(args.Sender, args.StartPos, minion.GetServerPosition(), spellData);
                }

                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

