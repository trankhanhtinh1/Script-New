#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Spells/ObjectTracker.h"

// Lux.h — C++ port of EzEvade/SpecialSpells/Lux.cs (59 lines)

namespace EzEvade {
namespace SpecialSpells {

class Lux : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "LuxMaliceCannon") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Lux") {
                    luxHero = &hero;
                    luxSpellData = &spellData;
                    ObjectTracker::HuiTrackerForceLoad();
                    break;
                }
            }
        }
    }

    // OnCreate — detect LuxR laser beam when Lux is invisible
    void OnCreateObject(SDK::GameObject* obj) {
        if (!obj || !luxHero || !luxSpellData) return;
        std::string name = obj->GetChampionName();
        if (name.find("Lux") != std::string::npos && name.find("R_mis_beam_middle") != std::string::npos) {
            if (luxHero->IsVisible()) return;
            auto dir = ObjectTracker::GetLastHiuOrientation();
            Vec2 pos = obj->GetPosition().To2D();
            Vec2 pos1 = pos - dir * 1750;
            Vec2 pos2 = pos + dir * 1750;
            SpellDetector::CreateSpellData(luxHero,
                Vec3(pos1.x, 0, pos1.y), Vec3(pos2.x, 0, pos2.y), *luxSpellData, nullptr, 0);
        }
    }

private:
    SDK::GameObject* luxHero = nullptr;
    SpellData* luxSpellData = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
