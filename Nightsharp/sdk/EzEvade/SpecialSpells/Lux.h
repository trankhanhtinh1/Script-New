#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Lux : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "LuxMaliceCannon")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Lux", true);
        if (!hero.IsValid()) {
            return;
        }

        s_luxNetId = hero.GetNetId();
        SDK::EventSystem::OnGameUpdate([](float) {
            OnGameUpdate();
        });
        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline int s_luxNetId = 0;
    static inline std::unordered_set<int> s_handledEmitters = {};

    static void OnGameUpdate() {
        if (s_luxNetId <= 0) {
            return;
        }

        auto luxHero = FindObjectByNetId(s_luxNetId);
        if (!luxHero.IsValid()) {
            return;
        }

        for (const auto& emitter : SDK::GameObjects::ParticleEmitters) {
            if (!emitter.IsValid()) {
                continue;
            }
            if (!Situation::CheckTeam(emitter)) {
                continue;
            }

            const int netId = emitter.GetNetId();
            if (s_handledEmitters.find(netId) != s_handledEmitters.end()) {
                continue;
            }

            const std::string name = emitter.GetName();
            if (name.find("Lux") == std::string::npos || name.find("R_mis_beam_middle") == std::string::npos) {
                continue;
            }

            s_handledEmitters.insert(netId);
            if (luxHero.IsVisible()) {
                continue;
            }

            std::vector<ObjectTrackerInfo*> hiuList;
            for (auto& kv : ObjectTracker::ObjTracker) {
                if (EqualsI(kv.second.Name, "hiu")) {
                    hiuList.push_back(&kv.second);
                }
            }
            if (hiuList.size() < 2) {
                continue;
            }

            Vec2 dir = ObjectTracker::GetLastHiuOrientation();
            if (dir.IsZero()) {
                continue;
            }

            Vec2 pos1 = emitter.GetPosition().To2D() - dir * 1750.0f;
            Vec2 pos2 = emitter.GetPosition().To2D() + dir * 1750.0f;

            auto it = SpellDetector::OnProcessSpells.find("luxmalicecannon");
            if (it != SpellDetector::OnProcessSpells.end() && it->second) {
                SpellDetector::CreateSpellData(luxHero, Vec3::From2D(pos1, emitter.GetPosition().y),
                                               Vec3::From2D(pos2, emitter.GetPosition().y), it->second);
            }

            for (auto* tracker : hiuList) {
                if (tracker && tracker->Obj.IsValid()) {
                    const int removeId = tracker->Obj.GetNetId();
                    DelayAction::Add(1, [removeId]() {
                        ObjectTracker::RemoveByNetId(removeId);
                    });
                }
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

