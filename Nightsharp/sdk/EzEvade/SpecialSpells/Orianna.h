#pragma once
#include "sdk/EzEvade/SpecialSpells/ChampionPlugin.h"
#include "sdk/EzEvade/SpecialSpells/SpecialSpellCommon.h"

namespace EzEvade {
namespace SpecialSpells {

class Orianna : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (!EqualsI(spellData.spellName, "OrianaIzunaCommand")) {
            return;
        }
        if (s_registered) {
            return;
        }

        auto hero = FindHeroByChampion("Orianna", true);
        if (!hero.IsValid()) {
            return;
        }

        s_oriannaNetId = hero.GetNetId();
        ObjectTracker::ObjTracker[s_oriannaNetId] = ObjectTrackerInfo(hero, "TheDoomBall");
        ObjectTracker::ObjTracker[s_oriannaNetId].OwnerNetworkID = s_oriannaNetId;

        SDK::EventSystem::OnBuffChanged([](const SDK::BuffChangeArgs& args) {
            if (!args.IsAdded) {
                return;
            }
            if (!EqualsI(args.BuffName, "orianaghostself")) {
                return;
            }
            if (!args.Unit.IsValid() || !Situation::CheckTeam(args.Unit)) {
                return;
            }
            auto* info = GetBallInfo();
            if (!info) {
                return;
            }
            info->UsePosition = false;
            info->Obj = args.Unit;
        });

        SDK::EventSystem::OnProcessSpellCast([](const SDK::SpellCastArgs& args) {
            if (!args.Sender.IsValid() || args.Sender.GetNetId() != s_oriannaNetId) {
                return;
            }
            if (!EqualsI(args.SpellName, "OrianaRedactCommand")) {
                return;
            }
            auto* info = GetBallInfo();
            if (!info) {
                return;
            }
            auto target = FindObjectByNetId(args.TargetNetId);
            if (target.IsValid()) {
                info->UsePosition = false;
                info->Obj = target;
            }
        });

        SDK::EventSystem::OnGameUpdate([](float) {
            for (const auto& emitter : SDK::GameObjects::ParticleEmitters) {
                if (!emitter.IsValid()) {
                    continue;
                }
                if (!Situation::CheckTeam(emitter)) {
                    continue;
                }
                const std::string name = emitter.GetName();
                if (name.find("Orianna") == std::string::npos || name.find("Ball_Flash_Reverse") == std::string::npos) {
                    continue;
                }
                auto* info = GetBallInfo();
                if (!info) {
                    continue;
                }
                auto hero = FindObjectByNetId(s_oriannaNetId);
                if (hero.IsValid()) {
                    info->UsePosition = false;
                    info->Obj = hero;
                }
            }
        });

        SpellDetector::RegisterOnProcessSpecialSpell(
            [](const SDK::SpellCastArgs& args, std::shared_ptr<SpellData> spellData, SpecialSpellEventArgs& specialSpellArgs) {
                if (!spellData) {
                    return;
                }

                auto* info = GetBallInfo();
                if (!info) {
                    return;
                }

                if (EqualsI(spellData->spellName, "OrianaIzunaCommand")) {
                    if (info->UsePosition) {
                        SpellDetector::CreateSpellData(args.Sender, info->Position, args.EndPos, spellData, SDK::GameObject(), 0.0f, false);
                        SpellDetector::CreateSpellData(args.Sender, info->Position, args.EndPos, spellData,
                                                       SDK::GameObject(), 150.0f, true, SpellType::Circular, false, spellData->secondaryRadius);
                    } else if (info->Obj.IsValid() && !info->Obj.IsDead()) {
                        SpellDetector::CreateSpellData(args.Sender, info->Obj.GetPosition(), args.EndPos, spellData, SDK::GameObject(), 0.0f, false);
                        SpellDetector::CreateSpellData(args.Sender, info->Obj.GetPosition(), args.EndPos, spellData,
                                                       SDK::GameObject(), 150.0f, true, SpellType::Circular, false, spellData->secondaryRadius);
                    }

                    info->Position = args.EndPos;
                    info->UsePosition = true;
                    specialSpellArgs.NoProcess = true;
                    return;
                }

                if (EqualsI(spellData->spellName, "OrianaDetonateCommand")
                    || EqualsI(spellData->spellName, "OrianaDissonanceCommand")) {
                    if (info->UsePosition) {
                        SpellDetector::CreateSpellData(args.Sender, info->Position, info->Position, spellData, SDK::GameObject(), 0.0f);
                    } else if (info->Obj.IsValid() && !info->Obj.IsDead()) {
                        Vec3 endPos = info->Obj.GetPosition();
                        SpellDetector::CreateSpellData(args.Sender, endPos, endPos, spellData, SDK::GameObject(), 0.0f);
                    }
                    specialSpellArgs.NoProcess = true;
                }
            });

        s_registered = true;
    }

private:
    static inline bool s_registered = false;
    static inline int s_oriannaNetId = 0;

    static ObjectTrackerInfo* GetBallInfo() {
        for (auto& kv : ObjectTracker::ObjTracker) {
            if (EqualsI(kv.second.Name, "TheDoomBall")) {
                return &kv.second;
            }
        }
        return nullptr;
    }
};

} // namespace SpecialSpells
} // namespace EzEvade

