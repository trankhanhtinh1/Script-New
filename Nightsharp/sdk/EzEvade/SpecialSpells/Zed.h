#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Zed : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "ZedQ")) {
            return;
        }

        if (!s_processRegistered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                    if (!SpellNameIs(spellData, "ZedQ")) {
                        return;
                    }

                    for (auto it = ObjectTracker::ObjTracker.begin(); it != ObjectTracker::ObjTracker.end(); ) {
                        auto& info = it->second;
                        if (!EqualsI(info.Name, "Shadow")) {
                            ++it;
                            continue;
                        }

                        if (!info.UsePosition && (!info.Obj.IsValid() || info.Obj.IsDead())) {
                            const int removeId = it->first;
                            ++it;
                            DelayAction::Add(1, [removeId]() { ObjectTracker::ObjTracker.erase(removeId); });
                            continue;
                        }

                        const Vec3 start = info.UsePosition ? info.Position : info.Obj.GetPosition();
                        const Vec3 end = start.Extend(args.EndPos, spellData->range);
                        SpellDetector::CreateSpellData(args.Sender, start, end, spellData, SDK::GameObject(), 0.0f, false);
                        ++it;
                    }
                });
            s_processRegistered = true;
        }

        if (!s_missileRegistered) {
            SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
                if (_stricmp(args.SpellName.c_str(), "ZedWMissile") != 0) {
                    return;
                }

                auto caster = FindObjectByNetId(args.CasterNetId);
                if (!caster.IsValid() || !Situation::CheckTeam(caster)) {
                    return;
                }

                const int trackerId = args.MissileObj.GetNetworkId();
                if (trackerId <= 0) {
                    return;
                }
                if (ObjectTracker::ObjTracker.find(trackerId) != ObjectTracker::ObjTracker.end()) {
                    return;
                }

                ObjectTrackerInfo info("Shadow", args.EndPos);
                info.OwnerNetworkID = args.CasterNetId;
                info.UsePosition = true;
                info.Position = args.EndPos;
                ObjectTracker::ObjTracker[trackerId] = info;

                DelayAction::Add(1000, [trackerId]() {
                    ObjectTracker::ObjTracker.erase(trackerId);
                });
            });
            s_missileRegistered = true;
        }

        if (!s_updateRegistered) {
            SDK::EventSystem::OnGameUpdate([](float) {
                SyncShadowObjects();
                CleanupStaleShadows();
            });
            s_updateRegistered = true;
        }
    }

private:
    static inline bool s_processRegistered = false;
    static inline bool s_missileRegistered = false;
    static inline bool s_updateRegistered = false;

    static void SyncShadowObjects() {
        for (const auto& minion : SDK::ObjectManager::GetMinions()) {
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }
            if (!Situation::CheckTeam(minion)) {
                continue;
            }
            if (!EqualsI(minion.GetName(), "Shadow")) {
                continue;
            }

            const int netId = minion.GetNetId();
            auto it = ObjectTracker::ObjTracker.find(netId);
            if (it == ObjectTracker::ObjTracker.end()) {
                ObjectTracker::ObjTracker[netId] = ObjectTrackerInfo(minion, "Shadow");
            } else {
                it->second.Obj = minion;
                it->second.Name = "Shadow";
                it->second.UsePosition = false;
                it->second.Position = minion.GetPosition();
                it->second.Timestamp = TickNow();
            }

            for (auto& kv : ObjectTracker::ObjTracker) {
                auto& info = kv.second;
                if (!EqualsI(info.Name, "Shadow")) {
                    continue;
                }
                if (!info.UsePosition) {
                    continue;
                }
                if (info.Position.Distance2D(minion.GetPosition()) >= 5.0f) {
                    continue;
                }
                info.Name = "Shadow";
                info.UsePosition = false;
                info.Obj = minion;
                info.Position = minion.GetPosition();
                info.Timestamp = TickNow();
            }
        }
    }

    static void CleanupStaleShadows() {
        std::vector<int> removeIds;
        for (const auto& kv : ObjectTracker::ObjTracker) {
            const int id = kv.first;
            const auto& info = kv.second;
            if (!EqualsI(info.Name, "Shadow")) {
                continue;
            }

            if (info.UsePosition) {
                if (TickNow() - info.Timestamp > 1000.0f) {
                    removeIds.push_back(id);
                }
                continue;
            }

            if (!info.Obj.IsValid() || info.Obj.IsDead()) {
                removeIds.push_back(id);
            }
        }

        for (int id : removeIds) {
            ObjectTracker::ObjTracker.erase(id);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

