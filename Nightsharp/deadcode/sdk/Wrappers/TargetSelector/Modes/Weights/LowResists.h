#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class LowResists final : public IWeightItem {
public:
    const char* Name() const override { return "lowresists"; }
    const char* DisplayName() const override { return "Low Resists"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return 1000.0f / std::max(25.0f, target.Armor() + target.SpellBlock() + 25.0f);
    }
};

} // namespace SDK::TargetSelectorModes::Weights
