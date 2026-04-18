#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class Aggro final : public IWeightItem {
public:
    const char* Name() const override { return "aggro"; }
    const char* DisplayName() const override { return "Aggro"; }
    float Score(const AIHeroClient& player, const AIHeroClient& target, DamageType) const override {
        const auto cast = target.Ref().GetActiveSpellCast();
        if (cast.IsValid() && cast.GetTargetIndex() == player.NetworkId()) {
            return 100.0f;
        }
        return std::max(0.0f, 1000.0f - target.Distance(player));
    }
};

} // namespace SDK::TargetSelectorModes::Weights
