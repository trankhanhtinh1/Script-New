#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Viktor : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "ViktorDeathRay3")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
            if (!EqualsI(args.SpellName, "viktoreaugmissile")) {
                return;
            }

            auto caster = FindObjectByNetId(args.CasterNetId);
            if (!caster.IsValid() || !Situation::CheckTeam(caster)) {
                return;
            }

            auto it = SpellDetector::OnMissileSpells.find("viktordeathray3");
            if (it == SpellDetector::OnMissileSpells.end() || !it->second) {
                return;
            }

            auto newData = std::make_shared<SpellData>(it->second->Clone());
            const float missileDist = args.EndPos.To2D().Distance(args.StartPos.To2D());
            newData->spellDelay = missileDist / 1.5f + 1000.0f;

            SpellDetector::CreateSpellData(caster, args.StartPos, args.EndPos, newData);
        });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

