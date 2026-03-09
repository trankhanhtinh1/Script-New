#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Azir : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "AzirQWrapper")) {
            return;
        }

        auto hero = FindHeroByChampion("Azir", true);
        if (!hero.IsValid()) {
            return;
        }

        if (!s_updateRegistered) {
            SDK::EventSystem::OnGameUpdate([](float) {
                RefreshSoldiers();
            });
            s_updateRegistered = true;
        }

        if (!s_spellRegistered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                    if (!SpellNameIs(spellData, "AzirQWrapper")) {
                        return;
                    }

                    RefreshSoldiers();
                    for (const auto& kv : s_soldiers) {
                        const auto& soldier = kv.second;
                        if (!soldier.IsValid() || soldier.IsDead()) {
                            continue;
                        }

                        const float maxSlideRange = 875.0f + args.Sender.GetPosition().Distance2D(soldier.GetPosition());
                        const Vec3 start = soldier.GetPosition();
                        const Vec3 end = ClipEndByRange(start, args.EndPos, maxSlideRange);
                        SpellDetector::CreateSpellData(args.Sender, start, end, spellData, soldier);
                    }

                    specialSpellArgs.NoProcess = true;
                });

            s_spellRegistered = true;
        }
    }

private:
    static inline bool s_updateRegistered = false;
    static inline bool s_spellRegistered = false;
    static inline std::unordered_map<int, SDK::GameObject> s_soldiers = {};

    static void RefreshSoldiers() {
        std::unordered_set<int> alive;
        for (const auto& minion : SDK::ObjectManager::GetMinions()) {
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }
            if (!Situation::CheckTeam(minion)) {
                continue;
            }

            const std::string lowerName = ToLower(minion.GetName());
            if (lowerName != "azirsoldier") {
                continue;
            }

            const int id = minion.GetNetId();
            alive.insert(id);
            s_soldiers[id] = minion;
        }

        std::vector<int> toRemove;
        for (const auto& kv : s_soldiers) {
            if (alive.find(kv.first) == alive.end()) {
                toRemove.push_back(kv.first);
            }
        }
        for (int id : toRemove) {
            s_soldiers.erase(id);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
