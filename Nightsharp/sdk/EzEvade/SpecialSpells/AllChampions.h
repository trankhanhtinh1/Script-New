#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"
#include <Windows.h>

namespace EzEvade {
namespace SpecialSpells {

class AllChampions : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!s_updateRegistered) {
            SDK::EventSystem::OnGameUpdate([](float) {
                OnGameUpdate();
            });
            s_updateRegistered = true;
        }

        if (spellData.hasTrap) {
            s_trackTraps = true;
        }

        if (spellData.isThreeWay && !s_threeWayRegistered) {
            SpellDetector::RegisterOnProcessSpecialSpell(
                [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs&) {
                    if (!spellData || !spellData->isThreeWay) {
                        return;
                    }

                    const Vec2 endPos2 = MathUtils::RotateVector(args.StartPos.To2D(), args.EndPos.To2D(), spellData->angle);
                    SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos2, args.EndPos.y),
                                                   spellData, SDK::GameObject(), 0.0f, false);

                    const Vec2 endPos3 = MathUtils::RotateVector(args.StartPos.To2D(), args.EndPos.To2D(), -spellData->angle);
                    SpellDetector::CreateSpellData(args.Sender, args.StartPos, Vec3::From2D(endPos3, args.EndPos.y),
                                                   spellData, SDK::GameObject(), 0.0f, false);
                });
            s_threeWayRegistered = true;
        }
    }

private:
    static inline bool s_updateRegistered = false;
    static inline bool s_threeWayRegistered = false;
    static inline bool s_trackTraps = false;
    static inline bool s_prevLeftDown = false;
    static inline std::unordered_map<int, SDK::GameObject> s_trapObjects = {};

    static void OnGameUpdate() {
        ProcessClickRemove();
        if (s_trackTraps) {
            TrackTrapObjects();
            ValidateTrackedTraps();
        }
    }

    static void ProcessClickRemove() {
        if (!ObjectCache::Menu.GetBool("ClickRemove", true)) {
            s_prevLeftDown = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
            return;
        }

        const bool leftDown = ((GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0);
        const bool clicked = leftDown && !s_prevLeftDown;
        s_prevLeftDown = leftDown;

        if (!clicked) {
            return;
        }

        const Vec2 cursor = SDK::Game::GetMouseWorldPos().To2D();
        std::vector<int> toDelete;
        for (const auto& kv : SpellDetector::DetectedSpells) {
            const int id = kv.first;
            const Spell& spell = kv.second;
            if (!spell.Info) {
                continue;
            }
            if (!Position::InSkillShot(cursor, spell, 50.0f + spell.Info->radius, false)) {
                continue;
            }
            if (spell.Info->range > 9000.0f || spell.Info->spellName.find("_trap") != std::string::npos) {
                toDelete.push_back(id);
            }
        }

        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { SpellDetector::DeleteSpell(id); });
        }
    }

    static void TrackTrapObjects() {
        std::unordered_set<int> aliveTrapIds;

        auto processObject = [&](const SDK::GameObject& obj) {
            if (!obj.IsValid() || obj.IsDead()) {
                return;
            }
            if (!Situation::CheckTeam(obj)) {
                return;
            }

            std::shared_ptr<SpellData> trapData = nullptr;

            const std::string lowerName = ToLower(obj.GetName());
            auto it = SpellDetector::OnProcessTraps.find(lowerName);
            if (it != SpellDetector::OnProcessTraps.end() && it->second) {
                trapData = it->second;
            }

            if (!trapData) {
                const std::string lowerBase = ToLower(obj.GetChampionName());
                auto it2 = SpellDetector::OnProcessTraps.find(lowerBase);
                if (it2 != SpellDetector::OnProcessTraps.end() && it2->second) {
                    trapData = it2->second;
                }
            }

            if (!trapData) {
                return;
            }

            const int netId = obj.GetNetId();
            if (netId <= 0) {
                return;
            }

            aliveTrapIds.insert(netId);
            s_trapObjects[netId] = obj;

            if (s_seenTrapIds.find(netId) != s_seenTrapIds.end()) {
                return;
            }

            s_seenTrapIds.insert(netId);

            auto cloned = std::make_shared<SpellData>(trapData->Clone());
            if (cloned->spellName.find("_trap") == std::string::npos) {
                cloned->spellName += "_trap";
            }

            SpellDetector::CreateSpellData(obj, obj.GetPosition(), obj.GetPosition(), cloned, obj, 1337.0f);
        };

        for (const auto& emitter : SDK::GameObjects::ParticleEmitters) {
            processObject(emitter);
        }

        for (const auto& minion : SDK::ObjectManager::GetMinions()) {
            processObject(minion);
        }

        std::vector<int> stale;
        for (const auto& kv : s_trapObjects) {
            if (aliveTrapIds.find(kv.first) == aliveTrapIds.end()) {
                stale.push_back(kv.first);
            }
        }

        for (int netId : stale) {
            RemoveTrapByNetId(netId);
        }
    }

    static void ValidateTrackedTraps() {
        std::vector<int> toDelete;
        for (auto& kv : SpellDetector::DetectedSpells) {
            const int id = kv.first;
            Spell& spell = kv.second;
            if (!spell.Info) {
                continue;
            }
            if (spell.Info->spellName.find("_trap") == std::string::npos) {
                continue;
            }
            if (!spell.SpellObject.IsValid() || spell.SpellObject.IsDead()) {
                toDelete.push_back(id);
                spell.SpellObject = SDK::GameObject();
            }
        }

        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { SpellDetector::DeleteSpell(id); });
        }
    }

    static void RemoveTrapByNetId(int netId) {
        if (netId <= 0) {
            return;
        }

        std::vector<int> toDelete;
        for (auto& kv : SpellDetector::DetectedSpells) {
            const int id = kv.first;
            Spell& spell = kv.second;
            if (!spell.Info) {
                continue;
            }
            if (!spell.SpellObject.IsValid()) {
                continue;
            }
            if (spell.SpellObject.GetNetId() != netId) {
                continue;
            }
            toDelete.push_back(id);
            spell.SpellObject = SDK::GameObject();
        }

        for (int id : toDelete) {
            DelayAction::Add(1, [id]() { SpellDetector::DeleteSpell(id); });
        }

        s_trapObjects.erase(netId);
        s_seenTrapIds.erase(netId);
    }

    static inline std::unordered_set<int> s_seenTrapIds = {};
};

} // namespace SpecialSpells
} // namespace EzEvade

