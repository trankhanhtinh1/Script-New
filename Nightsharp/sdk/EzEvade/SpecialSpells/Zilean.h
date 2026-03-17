#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Utils/EvadeUtils.h"

// Zilean.h — C++ port of EzEvade/SpecialSpells/Zilean.cs (117 lines)

namespace EzEvade {
namespace SpecialSpells {

class Zilean : public ChampionPlugin {
public:
    static inline std::vector<SDK::GameObject*> _bombs;
    static inline std::map<float, Vec3> _qSpots;

    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "ZileanQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell(hero, start, end, sd, sa);
                });
        }
    }

    static void OnUpdate() {
        // Remove invalid bombs
        _bombs.erase(std::remove_if(_bombs.begin(), _bombs.end(),
            [](SDK::GameObject* b) { return !b || !b->IsValid(); }), _bombs.end());

        // Expire old Q spots
        float gameTime = EvadeUtils::TickCount() / 1000.0f;
        std::vector<float> toRemove;
        for (auto& spot : _qSpots) {
            if (gameTime - spot.first >= 2.5f * 0.6f)
                toRemove.push_back(spot.first);
        }
        for (float key : toRemove) _qSpots.erase(key);
    }

    // C# lines 70-106: ZileanQ — chain explosion with double bombs
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "ZileanQ") {
            Vec3 endPos = end;
            if (start.Distance(endPos) > spellData.range)
                endPos = start + (endPos - start).Normalized() * spellData.range;

            // Check existing bombs for chain explosion
            for (auto* bomb : _bombs) {
                if (!bomb || !bomb->IsValid()) continue;
                SpellData newData = spellData;
                newData.radius = 350;
                if (endPos.Distance(bomb->GetPosition()) <= newData.radius) {
                    SpellDetector::CreateSpellData(hero, hero->GetPosition(), bomb->GetPosition(), newData,
                        nullptr, 0, true, SpellType::Circular, false, newData.radius);
                    SpellDetector::CreateSpellData(hero, hero->GetPosition(), endPos, newData,
                        nullptr, 0, true, SpellType::Circular, false, newData.radius);
                    specialArgs.noProcess = true;
                }
            }

            // Check Q spots for chain
            for (auto& bombPos : _qSpots) {
                SpellData newData = spellData;
                newData.radius = 350;
                if (endPos.Distance(bombPos.second) <= newData.radius && _qSpots.size() > 1) {
                    SpellDetector::CreateSpellData(hero, hero->GetPosition(), bombPos.second, newData,
                        nullptr, 0, true, SpellType::Circular, false, newData.radius);
                    SpellDetector::CreateSpellData(hero, hero->GetPosition(), endPos, newData,
                        nullptr, 0, true, SpellType::Circular, false, newData.radius);
                    specialArgs.noProcess = true;
                }
            }

            float gameTime = EvadeUtils::TickCount() / 1000.0f;
            _qSpots[gameTime] = endPos;
        }
    }

    static void RemovePairsNear(const Vec3& pos) {
        std::vector<float> toRemove;
        for (auto& pair : _qSpots) {
            if (pair.second.Distance(pos) <= 30)
                toRemove.push_back(pair.first);
        }
        for (float key : toRemove) _qSpots.erase(key);
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
