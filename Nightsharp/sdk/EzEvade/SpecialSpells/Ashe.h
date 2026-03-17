#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"
#include "../Utils/MathUtilsEz.h"

// ============================================================================
// Ashe.h — C++ port of EzEvade/SpecialSpells/Ashe.cs (46 lines)
//   Line-by-line, preserving original logic
// ============================================================================

namespace EzEvade {
namespace SpecialSpells {

class Ashe : public ChampionPlugin {
public:
    void LoadSpecialSpell(SpellData& spellData) override {
        // C# line 24: if (spellData.spellName == "Volley")
        if (spellData.spellName == "Volley") {
            // C# line 26: register OnProcessSpecialSpell
            SpellDetector::OnProcessSpecialSpell.push_back(
                [](SDK::GameObject* hero, const Vec3& start, const Vec3& end,
                   SpellData& sd, SpecialSpellEventArgs& specialArgs) {
                    ProcessSpell_AsheVolley(hero, start, end, sd, specialArgs);
                });
        }
    }

    // C# lines 30-43: ProcessSpell_AsheVolley
    static void ProcessSpell_AsheVolley(SDK::GameObject* hero, const Vec3& start,
        const Vec3& end, SpellData& spellData, SpecialSpellEventArgs& specialArgs)
    {
        // C# line 32: if (spellData.spellName == "Volley")
        if (spellData.spellName == "Volley") {
            // C# line 34-41: create 9 arrows (-4 to +4)
            for (int i = -4; i < 5; i++) {
                // C# line 36: RotateVector
                Vec2 endPos2 = EzMathUtils::RotateVector(
                    start.To2D(), end.To2D(), i * spellData.angle);
                Vec3 endPos3D = Vec3(endPos2.x, 0, endPos2.y);

                // C# line 37-40: skip i==0 (already created by main detection)
                if (i != 0) {
                    SpellDetector::CreateSpellData(hero, start, endPos3D, spellData, nullptr, 0, false);
                }
            }
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
