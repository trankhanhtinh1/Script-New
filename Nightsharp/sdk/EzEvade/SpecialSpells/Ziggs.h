#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Ziggs : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "ZiggsQ")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "ZiggsQ")) {
                    return;
                }

                const Vec2 startPos = args.Sender.GetServerPosition().To2D();
                Vec2 endPos = args.EndPos.To2D();
                const Vec2 dir = (endPos - startPos).Normalized();

                if (endPos.Distance(startPos) > 850.0f) {
                    endPos = startPos + dir * 850.0f;
                }

                SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos, args.EndPos.y), spellData,
                                               SDK::GameObject(), 0.0f, false);

                const Vec2 endPos2 = endPos + dir * 0.4f * startPos.Distance(endPos);
                SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos2, args.EndPos.y), spellData,
                                               SDK::GameObject(), 250.0f, false);

                const Vec2 endPos3 = endPos2 + dir * 0.6f * endPos.Distance(endPos2);
                SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos3, args.EndPos.y), spellData,
                                               SDK::GameObject(), 800.0f);

                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

