#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Taric : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "TaricE")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Taric", true);
        if (!hero.IsValid()) {
            return;
        }

        s_taricNetId = hero.GetNetId();
        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                if (!SpellNameIs(spellData, "TaricE")) {
                    return;
                }

                for (const auto& hero : SDK::GameObjects::AllHeroes) {
                    if (!hero.IsValid() || !Situation::CheckTeam(hero)) {
                        continue;
                    }
                    if (EqualsI(hero.GetChampionName(), "Taric")) {
                        continue;
                    }
                    if (!hero.HasBuff("taricwleashactive")) {
                        continue;
                    }

                    Vec2 start = hero.GetServerPosition().To2D();
                    Vec2 direction = (args.EndPos.To2D() - start).Normalized();
                    Vec2 end = start + direction * spellData->range;
                    SpellDetector::CreateSpellData(hero, Vec3::From2D(start, hero.GetPosition().y),
                                                   Vec3::From2D(end, hero.GetPosition().y), spellData);
                    break;
                }
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline int s_taricNetId = 0;

    static void OnGameUpdate() {
        auto taric = FindObjectByNetId(s_taricNetId);
        if (taric.IsValid() && Situation::CheckTeam(taric)) {
            UpdateSpellsForUnit(taric);
        }

        for (const auto& hero : SDK::GameObjects::AllHeroes) {
            if (!hero.IsValid() || !Situation::CheckTeam(hero)) {
                continue;
            }
            if (!hero.HasBuff("taricwleashactive")) {
                continue;
            }
            UpdateSpellsForUnit(hero);
            break;
        }
    }

    static void UpdateSpellsForUnit(const SDK::GameObject& unit) {
        for (auto& kv : SpellDetector::DetectedSpells) {
            auto& spell = kv.second;
            if (spell.HeroID != unit.GetNetId() || !spell.Info) {
                continue;
            }
            if (!EqualsI(spell.Info->spellName, "tarice")) {
                continue;
            }
            spell.StartPos = unit.GetServerPosition().To2D();
            spell.EndPos = unit.GetServerPosition().To2D() + spell.Direction * spell.Info->range;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

