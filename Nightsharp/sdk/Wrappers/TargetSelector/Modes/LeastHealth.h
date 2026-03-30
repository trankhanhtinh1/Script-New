#pragma once

#include "../ITargetSelectorMode.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class LeastHealth final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "leasthealth"; }
    const char* DisplayName() const override { return "Least Health"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType damageType) const override {
        auto ordered = heroes;
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                const float leftScore = EffectiveHealth(lhs, damageType) + (SafeDistance(lhs, from) * 0.15f);
                const float rightScore = EffectiveHealth(rhs, damageType) + (SafeDistance(rhs, from) * 0.15f);
                return leftScore < rightScore;
            });
        return ordered;
    }
};

} // namespace SDK::TargetSelectorModes
