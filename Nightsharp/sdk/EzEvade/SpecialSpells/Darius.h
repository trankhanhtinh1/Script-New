#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Darius : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "DariusCleave")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Darius", true);
        if (!hero.IsValid()) {
            return;
        }

        s_heroNetId = hero.GetNetId();
        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });
        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline int s_heroNetId = 0;

    static void OnGameUpdate() {
        if (s_heroNetId <= 0) {
            return;
        }

        auto hero = FindObjectByNetId(s_heroNetId);
        if (!hero.IsValid()) {
            return;
        }

        for (auto& kv : SpellDetector::DetectedSpells) {
            auto& spell = kv.second;
            if (spell.HeroID != s_heroNetId || !spell.Info) {
                continue;
            }
            if (!EqualsI(spell.Info->spellName, "DariusCleave")) {
                continue;
            }

            const Vec2 heroPos = hero.GetServerPosition().To2D();
            spell.StartPos = heroPos;
            spell.EndPos = heroPos + spell.Direction * spell.Info->range;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

