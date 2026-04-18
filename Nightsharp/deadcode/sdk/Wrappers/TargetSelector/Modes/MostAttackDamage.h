#pragma once

#include "../ITargetSelectorMode.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class MostAttackDamage final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "mostad"; }
    const char* DisplayName() const override { return "Most AD"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType) const override {
        auto ordered = heroes;
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                const float leftScore = lhs.TotalAttackDamage() - (SafeDistance(lhs, from) * 0.1f);
                const float rightScore = rhs.TotalAttackDamage() - (SafeDistance(rhs, from) * 0.1f);
                return leftScore > rightScore;
            });
        return ordered;
    }
};

} // namespace SDK::TargetSelectorModes
