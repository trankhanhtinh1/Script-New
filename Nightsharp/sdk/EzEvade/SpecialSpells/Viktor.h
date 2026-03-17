#pragma once
#include "ChampionPlugin.h"
#include "../Spells/SpellDetector.h"

// Viktor.h — C++ port of EzEvade/SpecialSpells/Viktor.cs (53 lines)

namespace EzEvade {
namespace SpecialSpells {

class Viktor : public ChampionPlugin {
public:
    static inline SpellData* viktorData = nullptr;

    void LoadSpecialSpell(SpellData& spellData) override {
        if (spellData.spellName == "ViktorDeathRay3") {
            viktorData = &spellData;
            // OnCreate missile callback would be registered here
        }
    }

    // C# lines 30-49: OnCreate — detect ViktorE augmented laser
    static void OnMissileCreate(SDK::Missile* missile) {
        if (!missile || !viktorData) return;
        std::string name = missile->GetSpellName();
        std::transform(name.begin(), name.end(), name.begin(), ::tolower);

        if (name == "viktoreaugmissile") {
            SpellData newData = *viktorData;
            float missileDist = missile->GetEndPos().To2D().Distance(missile->GetStartPos().To2D());
            newData.spellDelay = missileDist / 1.5f + 1000;
            SpellDetector::CreateSpellData(nullptr,
                missile->GetStartPos(), missile->GetEndPos(), newData, nullptr, 0, false);
        }
    }
};

} // namespace SpecialSpells
} // namespace EzEvade
