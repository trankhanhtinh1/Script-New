#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class FocusMe final : public IWeightItem {
public:
    const char* Name() const override { return "focusme"; }
    const char* DisplayName() const override { return "Focus Me"; }
    float Score(const AIHeroClient& player, const AIHeroClient& target, DamageType) const override {
        return target.InAutoAttackRange(player) ? 100.0f : 0.0f;
    }
};

} // namespace SDK::TargetSelectorModes::Weights
