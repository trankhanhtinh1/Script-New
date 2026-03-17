#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Utils/EvadeUtils.h"

// Sion.h — C++ port of EzEvade/SpecialSpells/Sion.cs (64 lines)

namespace EzEvade {
namespace SpecialSpells {

class Sion : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "SionR") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Sion") {
                    sionHero = &hero;
                    SpellDetector::OnProcessSpecialSpell.push_back(
                        [](SDK::GameObject* h, const Vec3& start, const Vec3& end,
                           SpellData& sd, SpecialSpellEventArgs& sa) {
                            ProcessSpell(h, start, end, sd, sa);
                        });
                    break;
                }
            }
        }
    }

    // C# lines 35-41: set projectile speed to hero's move speed
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "SionR") {
            spellData.projectileSpeed = hero->GetMoveSpeed();
            specialArgs.spellData = spellData;
        }
    }

    // C# lines 44-60: update SionR positions as he moves
    void OnUpdate() {
        if (!sionHero || !sionHero->IsValid()) return;

        for (auto& entry : SpellDetector::detectedSpells) {
            auto& spell = entry.second;
            if (spell.heroID == sionHero->GetNetId() && spell.info.spellName == "SionR") {
                Vec2 heroPos = sionHero->GetPosition().To2D();
                Vec2 facingDir = sionHero->GetDirection().To2D().Perpendicular();
                Vec2 endPos = heroPos + facingDir.Normalized() * 450;

                spell.startPos = heroPos;
                spell.endPos = endPos;

                if (EvadeUtils::TickCount() - spell.startTime >= 1000) {
                    SpellDetector::CreateSpellData(sionHero, sionHero->GetPosition(),
                        Vec3(endPos.x, 0, endPos.y), spell.info, nullptr, 0, false, SpellType::Line, false);
                    spell.startTime = EvadeUtils::TickCount();
                    break;
                }
            }
        }
    }

private:
    SDK::GameObject* sionHero = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
