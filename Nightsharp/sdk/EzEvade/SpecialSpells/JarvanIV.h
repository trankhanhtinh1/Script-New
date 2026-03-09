#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class JarvanIV : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "JarvanIVDragonStrike")
            && !EqualsI(spellData.spellName, "JarvanIVDemacianStandard")) {
            return;
        }

        auto hero = FindHeroByChampion("JarvanIV", true);
        if (!hero.IsValid()) {
            return;
        }

        if (!s_updateRegistered) {
            SDK::EventSystem::OnGameUpdate([](float) {
                OnGameUpdate();
            });
            s_updateRegistered = true;
        }

        if (!s_specialRegistered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                    if (!spellData) {
                        return;
                    }

                    if (EqualsI(spellData->spellName, "JarvanIVDemacianStandard")) {
                        Vec3 endPos = ClipEndByRange(args.StartPos, args.EndPos, spellData->range);
                        s_eSpots[SDK::Game::GetTime()] = endPos;
                        ObjectTracker::AddObjTrackerPosition("Beacon", endPos, 1000.0f);
                        return;
                    }

                    if (!EqualsI(spellData->spellName, "JarvanIVDragonStrike")) {
                        return;
                    }

                    std::shared_ptr<SpellData> comboData = spellData;
                    auto it = SpellDetector::OnProcessSpells.find("jarvanivdragonstrike2");
                    if (it != SpellDetector::OnProcessSpells.end() && it->second) {
                        comboData = it->second;
                    }

                    for (const auto& entry : s_eSpots) {
                        const Vec3& flagPos = entry.second;
                        if (args.EndPos.To2D().Distance(flagPos.To2D()) < 300.0f) {
                            const Vec2 dir = (flagPos.To2D() - args.StartPos.To2D()).Normalized();
                            const Vec2 endPos = flagPos.To2D() + dir * 110.0f;
                            SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos, args.StartPos.y), comboData);
                            specialSpellArgs.NoProcess = true;
                            return;
                        }
                    }

                    for (auto& kv : ObjectTracker::ObjTracker) {
                        auto& info = kv.second;
                        if (!EqualsI(info.Name, "Beacon")) {
                            continue;
                        }

                        if (!info.UsePosition && (!info.Obj.IsValid() || info.Obj.IsDead())) {
                            const int netId = info.Obj.GetNetId();
                            DelayAction::Add(1, [netId]() {
                                ObjectTracker::RemoveByNetId(netId);
                            });
                            continue;
                        }

                        Vec2 beaconPos = info.UsePosition ? info.Position.To2D() : info.Obj.GetPosition().To2D();
                        if (args.EndPos.To2D().Distance(beaconPos) < 300.0f) {
                            const Vec2 dir = (beaconPos - args.StartPos.To2D()).Normalized();
                            const Vec2 endPos = beaconPos + dir * 110.0f;
                            SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos, args.StartPos.y), comboData);
                            specialSpellArgs.NoProcess = true;
                            return;
                        }
                    }
                });
            s_specialRegistered = true;
        }
    }

private:
    static inline bool s_updateRegistered = false;
    static inline bool s_specialRegistered = false;
    static inline std::unordered_map<float, Vec3> s_eSpots = {};

    static void OnGameUpdate() {
        std::vector<float> removeList;
        const float now = SDK::Game::GetTime();
        for (const auto& spot : s_eSpots) {
            if (now - spot.first >= (1.2f * 0.6f)) {
                removeList.push_back(spot.first);
            }
        }
        for (float key : removeList) {
            s_eSpots.erase(key);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

