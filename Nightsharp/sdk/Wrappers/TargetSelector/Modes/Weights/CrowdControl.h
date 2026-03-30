#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class CrowdControl final : public IWeightItem {
public:
    const char* Name() const override { return "crowdcontrol"; }
    const char* DisplayName() const override { return "Crowd Control"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return target.Ref().IsCrowdControlled() ? 100.0f : 0.0f;
    }
};

} // namespace SDK::TargetSelectorModes::Weights
