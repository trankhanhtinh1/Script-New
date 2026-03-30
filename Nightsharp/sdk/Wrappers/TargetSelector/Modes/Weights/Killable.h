#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class Killable final : public IWeightItem {
public:
    const char* Name() const override { return "killable"; }
    const char* DisplayName() const override { return "Killable"; }
    float Score(const AIHeroClient& player, const AIHeroClient& target, DamageType damageType) const override {
        const float aa = std::max(player.GetAutoAttackDamage(target), 1.0f);
        const float hits = ITargetSelectorMode::EffectiveHealth(target, damageType) / aa;
        return 1000.0f / std::max(hits, 1.0f);
    }
};

} // namespace SDK::TargetSelectorModes::Weights
