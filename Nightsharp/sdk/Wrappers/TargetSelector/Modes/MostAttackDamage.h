#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::Modes {

class MostAttackDamage : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Most Attack Damage"; }
    const char* Name() const override { return "most-attack-damage"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        auto result = heroes;
        std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.TotalAttackDamage() > b.TotalAttackDamage();
        });
        return result;
    }
};

} // namespace SDK::Modes
