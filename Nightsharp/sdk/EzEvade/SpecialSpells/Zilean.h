#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Zilean : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "ZileanQ")) {
            return;
        }

        auto hero = FindHeroByChampion("Zilean", true);
        if (!hero.IsValid()) {
            return;
        }

        if (!s_updateRegistered) {
            SDK::EventSystem::OnGameUpdate([](float) {
                OnGameUpdate();
            });
            s_updateRegistered = true;
        }

        if (!s_spellRegistered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                    if (!SpellNameIs(spellData, "ZileanQ")) {
                        return;
                    }

                    Vec3 end = args.EndPos;
                    if (args.StartPos.Distance2D(end) > spellData->range) {
                        end = args.StartPos + (args.EndPos - args.StartPos).Normalized2D() * spellData->range;
                    }

                    for (const auto& kv : s_bombs) {
                        const auto& bomb = kv.second;
                        if (!bomb.IsValid() || bomb.IsDead() || !bomb.IsVisible()) {
                            continue;
                        }

                        auto newData = std::make_shared<SpellData>(spellData->Clone());
                        newData->radius = 350.0f;

                        if (end.Distance2D(bomb.GetPosition()) <= newData->radius) {
                            SpellDetector::CreateSpellData(args.Sender, args.Sender.GetServerPosition(), bomb.GetPosition(),
                                                           newData, SDK::GameObject(), 0.0f, true, SpellType::Circular,
                                                           false, newData->radius);
                            SpellDetector::CreateSpellData(args.Sender, args.Sender.GetServerPosition(), end,
                                                           newData, SDK::GameObject(), 0.0f, true, SpellType::Circular,
                                                           false, newData->radius);
                            specialSpellArgs.NoProcess = true;
                        }
                    }

                    for (const auto& spot : s_qSpots) {
                        const Vec3& bombPosition = spot.second;
                        auto newData = std::make_shared<SpellData>(spellData->Clone());
                        newData->radius = 350.0f;

                        if (end.Distance2D(bombPosition) <= newData->radius && s_qSpots.size() > 1) {
                            SpellDetector::CreateSpellData(args.Sender, args.Sender.GetServerPosition(), bombPosition,
                                                           newData, SDK::GameObject(), 0.0f, true, SpellType::Circular,
                                                           false, newData->radius);
                            SpellDetector::CreateSpellData(args.Sender, args.Sender.GetServerPosition(), end,
                                                           newData, SDK::GameObject(), 0.0f, true, SpellType::Circular,
                                                           false, newData->radius);
                            specialSpellArgs.NoProcess = true;
                        }
                    }

                    s_qSpots[SDK::Game::GetTime()] = end;
                });
            s_spellRegistered = true;
        }
    }

private:
    static inline const char* kObjName = "TimeBombGround";
    static inline bool s_updateRegistered = false;
    static inline bool s_spellRegistered = false;
    static inline std::unordered_map<int, SDK::GameObject> s_bombs = {};
    static inline std::unordered_map<float, Vec3> s_qSpots = {};

    static void OnGameUpdate() {
        UpdateBombObjects();

        std::vector<int> removeBombs;
        for (const auto& kv : s_bombs) {
            const auto& bomb = kv.second;
            if (!bomb.IsValid() || bomb.IsDead() || !bomb.IsVisible()) {
                removeBombs.push_back(kv.first);
            }
        }
        for (int id : removeBombs) {
            s_bombs.erase(id);
        }

        const float now = SDK::Game::GetTime();
        std::vector<float> removeSpots;
        for (const auto& spot : s_qSpots) {
            if (now - spot.first >= (2.5f * 0.6f)) {
                removeSpots.push_back(spot.first);
            }
        }
        for (float key : removeSpots) {
            s_qSpots.erase(key);
        }
    }

    static void UpdateBombObjects() {
        std::unordered_set<int> alive;
        for (const auto& emitter : SDK::GameObjects::ParticleEmitters) {
            if (!emitter.IsValid() || emitter.IsDead()) {
                continue;
            }
            if (!Situation::CheckTeam(emitter)) {
                continue;
            }
            if (emitter.GetName().find(kObjName) == std::string::npos) {
                continue;
            }

            const int netId = emitter.GetNetId();
            if (netId <= 0) {
                continue;
            }

            alive.insert(netId);
            if (s_bombs.find(netId) == s_bombs.end()) {
                RemovePairsNear(emitter.GetPosition());
                s_bombs[netId] = emitter;
            } else {
                s_bombs[netId] = emitter;
            }
        }

        std::vector<int> removeIds;
        for (const auto& kv : s_bombs) {
            if (alive.find(kv.first) == alive.end()) {
                removeIds.push_back(kv.first);
            }
        }
        for (int id : removeIds) {
            s_bombs.erase(id);
        }
    }

    static void RemovePairsNear(const Vec3& pos) {
        std::vector<float> removeKeys;
        for (const auto& kv : s_qSpots) {
            if (kv.second.Distance2D(pos) <= 30.0f) {
                removeKeys.push_back(kv.first);
            }
        }
        for (float key : removeKeys) {
            s_qSpots.erase(key);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

