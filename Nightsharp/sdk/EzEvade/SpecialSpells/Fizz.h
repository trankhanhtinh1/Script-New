#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Fizz.h — C++ port of EzEvade/SpecialSpells/Fizz.cs (109 lines)

namespace EzEvade {
namespace SpecialSpells {

class Fizz : public ChampionPlugin {
public:
    static inline SpellData* fizzRData = nullptr;

    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "FizzR") {
            fizzRData = &spellData;
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_FizzMarinerDoom(hero, start, end, sd, sa);
                });
        }
        if (spellData.spellName == "FizzQ") {
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& sa) {
                    ProcessSpell_FizzPiercingStrike(hero, start, end, sd, sa);
                });
        }
    }

    // C# lines 37-55: FizzR — adjust secondary radius based on distance
    static void ProcessSpell_FizzMarinerDoom(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "FizzR") {
            Vec3 endPos = end;
            if (start.Distance(endPos) > spellData.range)
                endPos = start + (endPos - start).Normalized() * spellData.range;

            float dist = start.Distance(endPos);
            float radius = dist > 910 ? 400.0f : (dist >= 455 ? 300.0f : 200.0f);

            SpellData data = spellData;
            data.secondaryRadius = radius;
            specialArgs.spellData = data;
        }
    }

    // C# lines 57-68: FizzQ — only dodge if targeting me
    static void ProcessSpell_FizzPiercingStrike(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        if (spellData.spellName == "FizzQ") {
            // C# line 61: if (args.Target != null && args.Target.IsMe)
            // In C++ we always create it and let the system handle
            SpellDetector::CreateSpellData(hero, start, end, spellData, nullptr, 0);
            specialArgs.noProcess = true;
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
