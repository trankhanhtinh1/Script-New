#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Taric.h — C++ port of EzEvade/SpecialSpells/Taric.cs (76 lines)

namespace EzEvade {
namespace SpecialSpells {

class Taric : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "TaricE") {
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                if (hero.GetChampionName() == "Taric") {
                    taricHero = &hero;
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

    // C# lines 31-57: OnUpdate — update TaricE position following hero + partner
    void OnUpdate() {
        if (!taricHero || !taricHero->IsValid()) return;

        for (auto& entry : SpellDetector::detectedSpells) {
            auto& spell = entry.second;
            std::string lower = spell.info.spellName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (spell.heroID == taricHero->GetNetId() && lower == "tarice") {
                spell.startPos = taricHero->GetPosition().To2D();
                spell.endPos = taricHero->GetPosition().To2D() + spell.direction * spell.info.range;
            }
        }
    }

    // C# lines 59-73: ProcessSpell — also create TaricE from partner position
    static void ProcessSpell(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "TaricE") {
            // Find partner with taricwleashactive buff
            for (auto& h : SDK::GameObjects::EnemyHeroes) {
                if (h.GetChampionName() != "Taric" && h.HasBuff("taricwleashactive")) {
                    Vec2 partnerStart = h.GetPosition().To2D();
                    Vec2 direction = (end.To2D() - partnerStart).Normalized();
                    Vec2 partnerEnd = partnerStart + direction * spellData.range;
                    SpellDetector::CreateSpellData(&h,
                        Vec3(partnerStart.x, 0, partnerStart.y),
                        Vec3(partnerEnd.x, 0, partnerEnd.y), spellData);
                    break;
                }
            }
        }
    }

private:
    SDK::GameObject* taricHero = nullptr;
};

} // namespace SpecialSpells
} // namespace EzEvade
