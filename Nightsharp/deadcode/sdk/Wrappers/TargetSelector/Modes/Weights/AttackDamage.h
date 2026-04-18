#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class AttackDamage final : public IWeightItem {
public:
    const char* Name() const override { return "attackdamage"; }
    const char* DisplayName() const override { return "Attack Damage"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return target.TotalAttackDamage();
    }
};

} // namespace SDK::TargetSelectorModes::Weights
