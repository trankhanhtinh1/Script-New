#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class LowHealth final : public IWeightItem {
public:
    const char* Name() const override { return "lowhealth"; }
    const char* DisplayName() const override { return "Low Health"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        const float maxHealth = std::max(target.MaxHealth(), 1.0f);
        return (1.0f - (target.Health() / maxHealth)) * 1000.0f;
    }
};

} // namespace SDK::TargetSelectorModes::Weights
