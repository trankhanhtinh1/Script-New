#pragma once

#include "../IWeightItem.h"

namespace SDK::TargetSelectorModes::Weights {

class TeamFocus final : public IWeightItem {
public:
    const char* Name() const override { return "teamfocus"; }
    const char* DisplayName() const override { return "Team Focus"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        float score = 0.0f;
        for (const auto& ally : ObjectManager::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsDead()) {
                continue;
            }
            if (ally.Distance(target) <= 900.0f) {
                score += 100.0f;
            }
        }
        return score;
    }
};

} // namespace SDK::TargetSelectorModes::Weights
