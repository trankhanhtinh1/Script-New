#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::Modes {

class LeastHealth : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Least Health"; }
    const char* Name() const override { return "least-health"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        auto result = heroes;
        std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.Health() < b.Health();
        });
        return result;
    }
};

} // namespace SDK::Modes
