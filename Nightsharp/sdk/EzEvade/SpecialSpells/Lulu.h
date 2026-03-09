#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Lulu : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "LuluQ")) {
            return;
        }

        if (!s_registered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                    if (!SpellNameIs(spellData, "LuluQ")) {
                        return;
                    }

                    for (auto& entry : ObjectTracker::ObjTracker) {
                        auto& info = entry.second;
                        if (!EqualsI(info.Name, "RobotBuddy")) {
                            continue;
                        }
                        if (!info.Obj.IsValid() || info.Obj.IsDead() || info.Obj.IsVisible()) {
                            continue;
                        }

                        Vec3 endPos = info.Obj.GetPosition().Extend(args.EndPos, spellData->range);
                        SpellDetector::CreateSpellData(args.Sender, info.Obj.GetPosition(), endPos, spellData, SDK::GameObject(), 0.0f, false);
                    }
                });
            s_registered = true;
        }

        EnsureLuluPix();
    }

private:
    static inline bool s_registered = false;
    static inline bool s_pendingRefresh = false;

    static void EnsureLuluPix() {
        bool found = false;
        for (const auto& minion : SDK::ObjectManager::GetMinions()) {
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }
            if (!Situation::CheckTeam(minion)) {
                continue;
            }

            const std::string lowerName = ToLower(minion.GetName());
            const std::string lowerBase = ToLower(minion.GetChampionName());
            if (lowerName.find("lulufaerie") == std::string::npos && lowerBase.find("lulufaerie") == std::string::npos) {
                continue;
            }

            found = true;
            if (ObjectTracker::ObjTracker.find(minion.GetNetId()) == ObjectTracker::ObjTracker.end()) {
                ObjectTracker::ObjTracker[minion.GetNetId()] = ObjectTrackerInfo(minion, "RobotBuddy");
            } else {
                auto& info = ObjectTracker::ObjTracker[minion.GetNetId()];
                info.Obj = minion;
                info.Name = "RobotBuddy";
                info.UsePosition = false;
            }
        }

        if (!found && !s_pendingRefresh) {
            s_pendingRefresh = true;
            DelayAction::Add(5000, []() {
                s_pendingRefresh = false;
                EnsureLuluPix();
            });
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
