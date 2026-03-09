#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Yorick : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "YorickE")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "YorickE")) {
                    return;
                }

                Vec3 end = ClipEndByRange(args.StartPos, args.EndPos, spellData->range);
                Vec3 direction = (end - args.StartPos).Normalized2D();

                Vec3 spellStart = end.Extend(args.Sender.GetServerPosition(), 100.0f);
                Vec3 spellEnd = spellStart + direction * 1.0f;

                SpellDetector::CreateSpellData(args.Sender, spellStart, spellEnd, spellData);
                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

