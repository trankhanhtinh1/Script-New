#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::Modes {

class LessCastsToKill : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Less Casts To Kill"; }
    const char* Name() const override { return "less-casts-to-kill"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        const auto& player = GameObjects::Player();
        const float playerAP = player.TotalMagicalDamage();
        auto result = heroes;
        std::sort(result.begin(), result.end(), [&player, playerAP](const AIHeroClient& a, const AIHeroClient& b) {
            const float aCasts = a.Health() / std::max(1.0f, playerAP);
            const float bCasts = b.Health() / std::max(1.0f, playerAP);
            return aCasts < bCasts;
        });
        return result;
    }
};

} // namespace SDK::Modes
