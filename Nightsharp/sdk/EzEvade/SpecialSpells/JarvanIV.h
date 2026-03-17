#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"
#include "../Utils/DelayAction.h"

// JarvanIV.h — C++ port of EzEvade/SpecialSpells/JarvanIV.cs (136 lines)

namespace EzEvade {
namespace SpecialSpells {

class JarvanIV : public ChampionPlugin {
public:
    static inline std::map<float, Vec3> _eSpots;

    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "JarvanIVDragonStrike") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    static void OnUpdate() {
        float gameTime = EvadeUtils::TickCount() / 1000.0f;
        std::vector<float> toRemove;
        for (auto& spot : _eSpots) {
            if (gameTime - spot.first >= 1.2f * 0.6f)
                toRemove.push_back(spot.first);
        }
        for (float key : toRemove) _eSpots.erase(key);
    }

    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        float gameTime = EvadeUtils::TickCount() / 1000.0f;

        if (spellData.spellName == "JarvanIVDemacianStandard") {
            Vec3 endPos = end;
            if (start.Distance(endPos) > spellData.range)
                endPos = start + (endPos - start).Normalized() * spellData.range;
            _eSpots[gameTime] = endPos;
        }

        if (spellData.spellName == "JarvanIVDragonStrike") {
            SpellData sd2;
            if (SpellDetector::onProcessSpells.count("jarvanivdragonstrike2")) {
                sd2 = SpellDetector::onProcessSpells["jarvanivdragonstrike2"];

                for (auto& entry : _eSpots) {
                    Vec3 flagPos = entry.second;
                    if (end.To2D().Distance(flagPos.To2D()) < 300) {
                        Vec2 dir = (flagPos.To2D() - start.To2D()).Normalized();
                        Vec2 endPosition = flagPos.To2D() + dir * 110;
                        SpellDetector::CreateSpellData(hero, start, Vec3(endPosition.x, 0, endPosition.y), sd2);
                        specialArgs.noProcess = true;
                        return;
                    }
                }

                for (auto& entry : ObjectTracker::objTracker) {
                    auto& info = entry.second;
                    if (info.Name == "Beacon") {
                        Vec2 objPos = info.usePosition ?
                            info.position.To2D() :
                            (info.obj ? info.obj->GetPosition().To2D() : Vec2(0,0));
                        if (end.To2D().Distance(objPos) < 300) {
                            Vec2 dir = (objPos - start.To2D()).Normalized();
                            Vec2 endPosition = objPos + dir * 110;
                            SpellDetector::CreateSpellData(hero, start, Vec3(endPosition.x, 0, endPosition.y), sd2);
                            specialArgs.noProcess = true;
                            return;
                        }
                    }
                }
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
