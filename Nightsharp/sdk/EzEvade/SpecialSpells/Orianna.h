#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"

// Orianna.h — C++ port of EzEvade/SpecialSpells/Orianna.cs (186 lines)

namespace EzEvade {
namespace SpecialSpells {

class Orianna : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "OrianaIzunaCommand") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Orianna") {
                    ObjectTracker::ObjectTrackerInfo info;
                    info.Name = "TheDoomBall";
                    info.OwnerNetworkID = hero.GetNetId();
                    info.obj = &hero;
                    ObjectTracker::objTracker[hero.GetNetId()] = info;

                    SpellDetector::OnProcessSpecialSpell.push_back(
                        [](SDK::GameObject* h, const Vec3& start, const Vec3& end,
                           SpellData& sd, SpecialSpellEventArgs& sa) {
                            ProcessSpell_OrianaIzunaCommand(h, start, end, sd, sa);
                        });
                    break;
                }
            }
        }
    }

    // C# lines 124-183: process OrianaIzunaCommand / Detonate / Dissonance
    static void ProcessSpell_OrianaIzunaCommand(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "OrianaIzunaCommand") {
            for (auto& entry : ObjectTracker::objTracker) {
                auto& info = entry.second;
                if (info.Name == "TheDoomBall") {
                    if (info.usePosition) {
                        SpellDetector::CreateSpellData(hero, info.position, end, spellData, nullptr, 0, false);
                        SpellDetector::CreateSpellData(hero, info.position, end, spellData, nullptr, 150, true,
                            SpellType::Circular, false, spellData.secondaryRadius);
                    } else {
                        if (info.obj && info.obj->IsValid()) {
                            SpellDetector::CreateSpellData(hero, info.obj->GetPosition(), end, spellData, nullptr, 0, false);
                            SpellDetector::CreateSpellData(hero, info.obj->GetPosition(), end, spellData, nullptr, 150, true,
                                SpellType::Circular, false, spellData.secondaryRadius);
                        }
                    }
                    info.position = end;
                    info.usePosition = true;
                }
            }
            specialArgs.noProcess = true;
        }

        if (spellData.spellName == "OrianaDetonateCommand" || spellData.spellName == "OrianaDissonanceCommand") {
            for (auto& entry : ObjectTracker::objTracker) {
                auto& info = entry.second;
                if (info.Name == "TheDoomBall") {
                    if (info.usePosition) {
                        SpellDetector::CreateSpellData(hero, info.position, info.position, spellData, nullptr, 0);
                    } else {
                        if (info.obj && info.obj->IsValid()) {
                            Vec3 pos = info.obj->GetPosition();
                            SpellDetector::CreateSpellData(hero, pos, pos, spellData, nullptr, 0);
                        }
                    }
                }
            }
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
