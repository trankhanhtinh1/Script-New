#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Fizz : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (EqualsI(spellData.spellName, "FizzR")) {
            RegisterFizzR();
        }
        if (EqualsI(spellData.spellName, "FizzQ")) {
            RegisterFizzQ();
        }
    }

private:
    static inline bool s_fizzRRegistered = false;
    static inline bool s_fizzQRegistered = false;
    static inline bool s_missileRegistered = false;

    static float GetFizzRRadius(const SDK::MissileArgs& args) {
        const float dist = args.StartPos.Distance2D(args.EndPos);
        return dist > 910.0f ? 400.0f : (dist >= 455.0f ? 300.0f : 200.0f);
    }

    static void RegisterMissiles() {
        if (s_missileRegistered) {
            return;
        }

        SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
            if (!_stricmp(args.SpellName.c_str(), "FizzRMissile")) {
                OnFizzRMissile(args, 500.0f);
            }
        });
        SDK::EventSystem::OnMissileDeleted([](const SDK::MissileArgs& args) {
            if (!_stricmp(args.SpellName.c_str(), "FizzRMissile")) {
                OnFizzRMissile(args, 1000.0f);
            }
        });

        s_missileRegistered = true;
    }

    static void OnFizzRMissile(const SDK::MissileArgs& args, float extraEndTick) {
        auto caster = FindObjectByNetId(args.CasterNetId);
        if (!caster.IsValid() || !Situation::CheckTeam(caster)) {
            return;
        }

        auto it = SpellDetector::OnMissileSpells.find("fizzr");
        if (it == SpellDetector::OnMissileSpells.end() || !it->second) {
            return;
        }

        auto spellData = std::make_shared<SpellData>(it->second->Clone());
        const float radius = GetFizzRRadius(args);

        SpellDetector::CreateSpellData(caster, args.StartPos, args.EndPos,
                                       spellData, SDK::GameObject(),
                                       extraEndTick, true, SpellType::Circular, false, radius);
    }

    static void RegisterFizzR() {
        if (s_fizzRRegistered) {
            return;
        }

        RegisterMissiles();
        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "FizzR")) {
                    return;
                }

                Vec3 endPos = ClipEndByRange(args.StartPos, args.EndPos, spellData->range);
                const float dist = args.StartPos.Distance2D(endPos);
                const float radius = dist > 910.0f ? 400.0f : (dist >= 455.0f ? 300.0f : 200.0f);

                auto data = std::make_shared<SpellData>(spellData->Clone());
                data->secondaryRadius = radius;
                specialSpellArgs.Data = data;
            });

        s_fizzRRegistered = true;
    }

    static void RegisterFizzQ() {
        if (s_fizzQRegistered) {
            return;
        }

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "FizzQ")) {
                    return;
                }

                const auto& me = SDK::GameObjects::Player;
                if (me.IsValid() && args.TargetNetId == me.GetNetId()) {
                    SpellDetector::CreateSpellData(args.Sender, args.StartPos, args.EndPos, spellData, SDK::GameObject(), 0.0f);
                }

                specialSpellArgs.NoProcess = true;
            });

        s_fizzQRegistered = true;
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

