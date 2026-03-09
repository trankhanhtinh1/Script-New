#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Sion : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "SionR")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Sion", true);
        if (!hero.IsValid()) {
            return;
        }
        s_heroNetId = hero.GetNetId();

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!SpellNameIs(spellData, "SionR")) {
                    return;
                }

                auto data = std::make_shared<SpellData>(spellData->Clone());
                data->projectileSpeed = args.Sender.GetMoveSpeed();
                specialSpellArgs.Data = data;
            });

        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline int s_heroNetId = 0;

    static void OnGameUpdate() {
        auto hero = FindObjectByNetId(s_heroNetId);
        if (!hero.IsValid()) {
            return;
        }

        for (auto& kv : SpellDetector::DetectedSpells) {
            auto& spell = kv.second;
            if (spell.HeroID != s_heroNetId || !spell.Info || !EqualsI(spell.Info->spellName, "SionR")) {
                continue;
            }

            const Vec2 facingPos = hero.GetServerPosition().To2D() + hero.GetDirection().To2D().Perpendicular();
            const Vec2 endPos = hero.GetServerPosition().To2D() + (facingPos - hero.GetServerPosition().To2D()).Normalized() * 450.0f;

            spell.StartPos = hero.GetServerPosition().To2D();
            spell.EndPos = endPos;

            if (TickNow() - spell.StartTime >= 1000.0f) {
                auto data = std::make_shared<SpellData>(spell.Info->Clone());
                SpellDetector::CreateSpellData(hero, hero.GetServerPosition(), Vec3::From2D(endPos, hero.GetPosition().y),
                                               data, SDK::GameObject(), 0.0f, false, SpellType::Line, false);
                spell.StartTime = TickNow();
                break;
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

