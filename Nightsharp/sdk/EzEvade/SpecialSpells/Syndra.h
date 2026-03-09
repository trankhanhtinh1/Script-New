#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Syndra : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "syndrae")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Syndra", true);
        if (!hero.IsValid()) {
            return;
        }

        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!spellData) {
                    return;
                }

                if (EqualsI(spellData->spellName, "syndrae")) {
                    Vec3 estart = args.StartPos;
                    Vec3 eend = args.StartPos + (args.EndPos - args.StartPos).Normalized2D() * 800.0f;

                    for (const auto& kv : s_spheres) {
                        const auto& sphere = kv.second;
                        if (!sphere.IsValid() || sphere.IsDead()) {
                            continue;
                        }

                        auto proj = SDK::GeometryAdv::ProjectOn(sphere.GetPosition().To2D(), estart.To2D(), eend.To2D());
                        if (proj.IsOnSegment && sphere.GetPosition().To2D().Distance(proj.SegmentPoint) <= sphere.GetBoundingRadius() + 155.0f) {
                            Vec3 start = sphere.GetPosition();
                            Vec3 end = args.Sender.GetServerPosition() + (sphere.GetPosition() - args.Sender.GetServerPosition()).Normalized2D() * spellData->range;
                            auto data = std::make_shared<SpellData>(spellData->Clone());
                            data->spellDelay = sphere.GetPosition().Distance2D(args.Sender.GetServerPosition()) / spellData->projectileSpeed * 1000.0f;
                            SpellDetector::CreateSpellData(args.Sender, start, end, data, sphere);
                        }
                    }

                    for (const auto& entry : s_qSpots) {
                        const Vec3 spherePos = entry.second;
                        auto proj = SDK::GeometryAdv::ProjectOn(spherePos.To2D(), estart.To2D(), eend.To2D());
                        if (proj.IsOnSegment && spherePos.To2D().Distance(proj.SegmentPoint) <= 155.0f) {
                            Vec3 start = spherePos;
                            Vec3 end = args.Sender.GetServerPosition() + (spherePos - args.Sender.GetServerPosition()).Normalized2D() * spellData->range;
                            auto data = std::make_shared<SpellData>(spellData->Clone());
                            data->spellDelay = spherePos.Distance2D(args.Sender.GetServerPosition()) / spellData->projectileSpeed * 1000.0f;
                            SpellDetector::CreateSpellData(args.Sender, start, end, data, SDK::GameObject());
                        }
                    }

                    specialSpellArgs.NoProcess = true;
                    return;
                }

                if (EqualsI(spellData->spellName, "syndraq") || EqualsI(spellData->spellName, "syndrawcast")) {
                    Vec3 end = ClipEndByRange(args.StartPos, args.EndPos, spellData->range);
                    s_qSpots[SDK::Game::GetTime()] = end;
                }
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline std::unordered_map<int, SDK::GameObject> s_spheres = {};
    static inline std::unordered_map<float, Vec3> s_qSpots = {};

    static void OnGameUpdate() {
        std::unordered_set<int> aliveSpheres;
        for (const auto& minion : SDK::ObjectManager::GetMinions()) {
            if (!minion.IsValid() || minion.IsDead()) {
                continue;
            }
            if (!Situation::CheckTeam(minion)) {
                continue;
            }
            if (!EqualsI(minion.GetChampionName(), "syndrasphere") && !EqualsI(minion.GetName(), "syndrasphere")) {
                continue;
            }
            const int id = minion.GetNetId();
            aliveSpheres.insert(id);
            s_spheres[id] = minion;
        }

        std::vector<int> removeSphere;
        for (const auto& kv : s_spheres) {
            if (aliveSpheres.find(kv.first) == aliveSpheres.end()) {
                removeSphere.push_back(kv.first);
            }
        }
        for (int id : removeSphere) {
            s_spheres.erase(id);
        }

        const float now = SDK::Game::GetTime();
        std::vector<float> removeQSpot;
        for (const auto& spot : s_qSpots) {
            if (now - spot.first >= (1.2f * 0.6f)) {
                removeQSpot.push_back(spot.first);
            }
        }
        for (float key : removeQSpot) {
            s_qSpots.erase(key);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
