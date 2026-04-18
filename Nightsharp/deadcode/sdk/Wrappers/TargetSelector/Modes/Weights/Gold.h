#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class Gold final : public IWeightItem {
public:
    const char* Name() const override { return "gold"; }
    const char* DisplayName() const override { return "Gold Value"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return target.TotalAttackDamage() + target.AbilityPower() + target.Armor() + target.SpellBlock();
    }
};

} // namespace SDK::TargetSelectorModes::Weights
