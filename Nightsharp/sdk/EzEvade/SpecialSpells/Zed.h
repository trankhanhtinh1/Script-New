#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"
#include "../Utils/DelayAction.h"

// Zed.h — C++ port of EzEvade/SpecialSpells/Zed.cs (125 lines)

namespace EzEvade {
namespace SpecialSpells {

class Zed : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "ZedQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_ZedShuriken(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 33-53: OnCreate shadow object
    static void OnCreateObject(SDK::GameObject* obj) {
        if (!obj) return;
        if (obj->GetChampionName() == "Shadow" && obj->IsEnemy(SDK::GameObjects::Player)) {
            if (ObjectTracker::objTracker.find(obj->GetNetId()) == ObjectTracker::objTracker.end()) {
                ObjectTracker::ObjectTrackerInfo info;
                info.obj = obj;
                info.Name = "Shadow";
                ObjectTracker::objTracker[obj->GetNetId()] = info;

                // Check existing position-based shadows for matching
                for (auto& entry : ObjectTracker::objTracker) {
                    auto& existing = entry.second;
                    if (existing.Name == "Shadow" && existing.usePosition &&
                        existing.position.Distance(obj->GetPosition()) < 5) {
                        existing.usePosition = false;
                        existing.obj = obj;
                    }
                }
            }
        }
    }

    // C# lines 56-62: OnDelete shadow
    static void OnDeleteObject(SDK::GameObject* obj) {
        if (!obj) return;
        if (obj->GetChampionName() == "Shadow") {
            ObjectTracker::objTracker.erase(obj->GetNetId());
        }
    }

    // C# lines 64-97: ProcessSpell — ZedQ fires from shadows too
    static void ProcessSpell_ZedShuriken(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "ZedQ") {
            for (auto& entry : ObjectTracker::objTracker) {
                auto& info = entry.second;
                if (info.Name == "Shadow") {
                    if (!info.usePosition && (!info.obj || !info.obj->IsValid())) {
                        EzEvade::DelayAction::Add(1, [id = entry.first]() {
                            ObjectTracker::objTracker.erase(id);
                        });
                        continue;
                    }

                    if (!info.usePosition) {
                        Vec3 endPos = info.obj->GetPosition().Extend(end, spellData.range);
                        SpellDetector::CreateSpellData(hero, info.obj->GetPosition(), endPos, spellData, nullptr, 0, false);
                    } else {
                        Vec3 endPos = info.position.Extend(end, spellData.range);
                        SpellDetector::CreateSpellData(hero, info.position, endPos, spellData, nullptr, 0, false);
                    }
                }
            }
        }
    }

    // C# lines 100-122: SpellMissile_ZedShadowDash — track W missile for shadow positioning
    static void OnMissileCreate(SDK::Missile* missile) {
        if (!missile) return;
        if (missile->GetSpellName() == "ZedWMissile") {
            int netId = missile->GetNetworkId();
            if (ObjectTracker::objTracker.find(netId) == ObjectTracker::objTracker.end()) {
                ObjectTracker::ObjectTrackerInfo info;
                info.Name = "Shadow";
                info.OwnerNetworkID = missile->GetCasterNetId();
                info.usePosition = true;
                info.position = missile->GetEndPos();
                ObjectTracker::objTracker[netId] = info;

                EzEvade::DelayAction::Add(1000, [netId]() {
                    ObjectTracker::objTracker.erase(netId);
                });
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
