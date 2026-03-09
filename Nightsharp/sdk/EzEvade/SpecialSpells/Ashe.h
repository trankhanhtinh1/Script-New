#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Ashe : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "Volley")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                if (!SpellNameIs(spellData, "Volley")) {
                    return;
                }

                for (int i = -4; i < 5; ++i) {
                    if (i == 0) {
                        continue;
                    }

                    Vec3 endPos = Vec3::From2D(
                        MathUtils::RotateVector(args.StartPos.To2D(), args.EndPos.To2D(), (float)i * spellData->angle),
                        args.EndPos.y);

                    SpellDetector::CreateSpellData(args.Sender, args.StartPos, endPos, spellData, SDK::GameObject(), 0.0f, false);
                }
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

