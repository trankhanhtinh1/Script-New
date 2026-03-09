#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Ahri : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "AhriOrbofDeception2")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Ahri", true);
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

        for (auto& entry : SpellDetector::DetectedSpells) {
            auto& spell = entry.second;
            if (spell.HeroID != s_heroNetId || !spell.Info) {
                continue;
            }
            if (!EqualsI(spell.Info->spellName, "AhriOrbofDeception2")) {
                continue;
            }

            spell.EndPos = hero.GetServerPosition().To2D();
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

