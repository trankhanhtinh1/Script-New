#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::Modes {

class MostAbilityPower : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Most Ability Power"; }
    const char* Name() const override { return "most-ability-power"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        auto result = heroes;
        std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.TotalMagicalDamage() > b.TotalMagicalDamage();
        });
        return result;
    }
};

} // namespace SDK::Modes
