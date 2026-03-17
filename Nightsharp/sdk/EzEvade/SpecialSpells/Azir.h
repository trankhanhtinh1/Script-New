#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"
#include "../Utils/DelayAction.h"

// Azir.h — C++ port of EzEvade/SpecialSpells/Azir.cs (117 lines)

namespace EzEvade {
namespace SpecialSpells {

class Azir : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "AzirQWrapper") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Azir") {
                    azirHero = &hero;
                    ObjectTracker::ObjectTrackerInfo info;
                    info.Name = "AzirQSoldier";
                    info.OwnerNetworkID = hero.GetNetId();
                    ObjectTracker::objTracker[hero.GetNetId()] = info;

                    SpellDetector::OnProcessSpecialSpell.push_back(
                        [](SDK::GameObject* h, const Vec3& start, const Vec3& end,
                           SpellData& sd, SpecialSpellEventArgs& sa) {
                            ProcessSpell_AzirSoldier(h, start, end, sd, sa);
                        });
                    break;
                }
            }
        }
    }

    static void ProcessSpell_AzirSoldier(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "AzirQWrapper") {
            for (auto& entry : ObjectTracker::objTracker) {
                auto& info = entry.second;
                if (info.Name == "AzirQSoldier") {
                    for (auto& objEntry : info.objList) {
                        auto* soldier = objEntry.second;
                        if (!soldier || !soldier->IsValid()) continue;

                        float maxRange = 875 + hero->GetPosition().Distance(soldier->GetPosition());
                        Vec3 soldierPos = soldier->GetPosition();
                        Vec3 endPos = end;
                        if (soldierPos.Distance(endPos) > maxRange)
                            endPos = soldierPos + (endPos - soldierPos).Normalized() * maxRange;

                        SpellDetector::CreateSpellData(hero, soldierPos, endPos, spellData, soldier);
                    }
                }
            }
            specialArgs.noProcess = true;
        }
    }

private:
    SDK::GameObject* azirHero = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
