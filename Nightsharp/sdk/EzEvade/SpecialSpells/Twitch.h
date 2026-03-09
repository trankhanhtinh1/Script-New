#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Twitch : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "TwitchSprayandPrayAttack")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                if (!SpellNameIs(spellData, "TwitchSprayandPrayAttack")) {
                    return;
                }

                auto target = FindObjectByNetId(args.TargetNetId);
                if (!target.IsValid()) {
                    return;
                }

                Vec3 start = args.Sender.GetServerPosition();
                Vec3 end = args.Sender.GetServerPosition() + (target.GetPosition() - args.Sender.GetServerPosition()) * spellData->range;
                auto data = std::make_shared<SpellData>(spellData->Clone());
                data->spellDelay = args.Sender.AttackCastDelay() * 1000.0f;
                SpellDetector::CreateSpellData(args.Sender, start, end, data);
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

