#pragma once

#include "../ITargetSelectorMode.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class Closest final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "closest"; }
    const char* DisplayName() const override { return "Closest"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType) const override {
        auto ordered = heroes;
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                return lhs.Distance(from) < rhs.Distance(from);
            });
        return ordered;
    }
};

} // namespace SDK::TargetSelectorModes
