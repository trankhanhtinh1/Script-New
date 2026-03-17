#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"
#include "../Utils/DelayAction.h"

// Lulu.h — C++ port of EzEvade/SpecialSpells/Lulu.cs (80 lines)

namespace EzEvade {
namespace SpecialSpells {

class Lulu : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "LuluQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_LuluQ(hero, start, end, sd, sa);
                });
            // C# line 27: GetLuluPix() — find pix minion
            GetLuluPix();
        }
    }

    static void GetLuluPix() {
        for (auto& obj : SDK::GameObjects::EnemyMinions) {
            if (obj.IsValid() && obj.GetChampionName() == "lulufaerie") {
                if (ObjectTracker::objTracker.find(obj.GetNetId()) == ObjectTracker::objTracker.end()) {
                    ObjectTracker::ObjectTrackerInfo info;
                    info.obj = &obj;
                    info.Name = "RobotBuddy";
                    ObjectTracker::objTracker[obj.GetNetId()] = info;
                }
            }
        }
    }

    static void ProcessSpell_LuluQ(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "LuluQ") {
            for (auto& entry : ObjectTracker::objTracker) {
                auto& info = entry.second;
                if (info.Name == "RobotBuddy") {
                    if (!info.obj || !info.obj->IsValid()) continue;
                    Vec3 objPos = info.obj->GetPosition();
                    Vec3 endPos2 = objPos.Extend(end, spellData.range);
                    SpellDetector::CreateSpellData(hero, objPos, endPos2, spellData, nullptr, 0, false);
                }
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
