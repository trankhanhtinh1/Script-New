#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class AbilityPower final : public IWeightItem {
public:
    const char* Name() const override { return "abilitypower"; }
    const char* DisplayName() const override { return "Ability Power"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return target.AbilityPower();
    }
};

} // namespace SDK::TargetSelectorModes::Weights
