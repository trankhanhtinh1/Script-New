#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class ShortDistancePlayer final : public IWeightItem {
public:
    const char* Name() const override { return "shortdistanceplayer"; }
    const char* DisplayName() const override { return "Short Distance Player"; }
    float Score(const AIHeroClient& player, const AIHeroClient& target, DamageType) const override {
        return 1000.0f / std::max(target.Distance(player), 1.0f);
    }
};

} // namespace SDK::TargetSelectorModes::Weights
