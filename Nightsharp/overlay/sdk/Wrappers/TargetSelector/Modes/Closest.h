#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::Modes {

class Closest : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Closest"; }
    const char* Name() const override { return "closest"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        auto result = heroes;
        std::sort(result.begin(), result.end(), [](const AIHeroClient& a, const AIHeroClient& b) {
            return a.Distance(GameObjects::Player()) < b.Distance(GameObjects::Player());
        });
        return result;
    }
};

} // namespace SDK::Modes
