#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Lucian : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "LucianQ")) {
            return;
        }
        if (s_registered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "LucianQ")) {
                    return;
                }

                auto target = FindObjectByNetId(args.TargetNetId);
                if (!target.IsValid()) {
                    return;
                }

                const float spellDelaySec = (350.0f - ObjectCache::GamePing) / 1000.0f;
                Vec3 walkDir = (target.GetServerPosition() - target.GetPosition()).Normalized2D();
                Vec3 predictedPos = target.GetPosition() + walkDir * (target.GetMoveSpeed() * spellDelaySec);

                SpellDetector::CreateSpellData(args.Sender, args.StartPos, predictedPos, spellData, SDK::GameObject(), 0.0f);
                specialSpellArgs.NoProcess = true;
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
};

} // namespace SpecialSpells
} // namespace EzEvade

