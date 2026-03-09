#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Malzahar : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "MalzaharQ")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "MalzaharQ")) {
                    return;
                }

                const Vec2 direction = (args.EndPos.To2D() - args.StartPos.To2D()).Normalized();
                const Vec2 pDirection = direction.Perpendicular();
                const Vec2 targetPoint = args.EndPos.To2D();

                const Vec2 pos1 = targetPoint - pDirection * spellData->sideRadius;
                const Vec2 pos2 = targetPoint + pDirection * spellData->sideRadius;

                SpellDetector::CreateSpellData(args.Sender, Vec3::From2D(pos1, args.EndPos.y), Vec3::From2D(pos2, args.EndPos.y),
                                               spellData, SDK::GameObject(), 0.0f, false);
                SpellDetector::CreateSpellData(args.Sender, Vec3::From2D(pos2, args.EndPos.y), Vec3::From2D(pos1, args.EndPos.y),
                                               spellData, SDK::GameObject(), 0.0f);

                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

