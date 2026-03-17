#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Syndra.h — C++ port of EzEvade/SpecialSpells/Syndra.cs (154 lines)

namespace EzEvade {
namespace SpecialSpells {

class Syndra : public ChampionPlugin {
public:
    static inline std::vector<SDK::GameObject*> _spheres;
    static inline std::map<float, Vec3> _qSpots;

    void LoadSpecialSpell(SpellData& spellData) override {
        std::string lower = spellData.spellName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "syndrae") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    static void OnUpdate() {
        // Remove dead spheres
        _spheres.erase(std::remove_if(_spheres.begin(), _spheres.end(),
            [](SDK::GameObject* s) { return !s || !s->IsValid(); }), _spheres.end());

        // Expire old Q spots
        float gameTime = EvadeUtils::TickCount() / 1000.0f;
        std::vector<float> toRemove;
        for (auto& spot : _qSpots) {
            if (gameTime - spot.first >= 1.2f * 0.6f)
                toRemove.push_back(spot.first);
        }
        for (float key : toRemove) _qSpots.erase(key);
    }

    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        std::string lower = spellData.spellName;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

        if (lower == "syndrae") {
            Vec2 estart = start.To2D();
            Vec2 eend = start.To2D() + (end.To2D() - start.To2D()).Normalized() * 800;

            // Check existing spheres
            for (auto* sphere : _spheres) {
                if (!sphere || !sphere->IsValid()) continue;
                Vec2 spherePos = sphere->GetPosition().To2D();
                auto proj = Vec2_ProjectOn(spherePos, estart, eend);
                if (proj.isOnSegment && spherePos.Distance(proj.segmentPoint) <= 155 + 65) {
                    Vec3 sPos = sphere->GetPosition();
                    Vec3 ePos = hero->GetPosition() + (sPos - hero->GetPosition()).Normalized() * spellData.range;
                    SpellData data = spellData;
                    data.spellDelay = sPos.Distance(hero->GetPosition()) / spellData.projectileSpeed * 1000;
                    SpellDetector::CreateSpellData(hero, sPos, ePos, data, sphere);
                }
            }

            // Check Q spots
            for (auto& entry : _qSpots) {
                Vec3 spherePos3D = entry.second;
                Vec2 spherePos = spherePos3D.To2D();
                auto proj = Vec2_ProjectOn(spherePos, estart, eend);
                if (proj.isOnSegment && spherePos.Distance(proj.segmentPoint) <= 155) {
                    Vec3 ePos = hero->GetPosition() + (spherePos3D - hero->GetPosition()).Normalized() * spellData.range;
                    SpellData data = spellData;
                    data.spellDelay = spherePos3D.Distance(hero->GetPosition()) / spellData.projectileSpeed * 1000;
                    SpellDetector::CreateSpellData(hero, spherePos3D, ePos, data, nullptr);
                }
            }

            specialArgs.noProcess = true;
        }

        if (lower == "syndraq" || lower == "syndrawcast") {
            Vec3 endPos = end;
            if (start.Distance(endPos) > spellData.range)
                endPos = start + (endPos - start).Normalized() * spellData.range;
            float gameTime = EvadeUtils::TickCount() / 1000.0f;
            _qSpots[gameTime] = endPos;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
