#pragma once

#include "../ITargetSelectorMode.h"
#include "../Damages/Damage.h"

namespace SDK::Modes {

class LessAttacksToKill : public ITargetSelectorMode {
public:
    const char* DisplayName() const override { return "Less Attacks To Kill"; }
    const char* Name() const override { return "less-attacks-to-kill"; }

    void AddToMenu(Menu* menu) override {
    }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) override {
        const auto& player = GameObjects::Player();
        auto result = heroes;
        std::sort(result.begin(), result.end(), [&player](const AIHeroClient& a, const AIHeroClient& b) {
            const float aAttacks = a.Health() / std::max(1.0f, Damage::GetAutoAttackDamage(player, a));
            const float bAttacks = b.Health() / std::max(1.0f, Damage::GetAutoAttackDamage(player, b));
            return aAttacks < bAttacks;
        });
        return result;
    }
};

} // namespace SDK::Modes
