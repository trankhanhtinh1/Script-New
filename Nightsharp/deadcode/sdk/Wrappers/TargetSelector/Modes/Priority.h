#pragma once

#include "../ITargetSelectorMode.h"
#include "../../../../generated/PriorityData.generated.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class Priority final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "priority"; }
    const char* DisplayName() const override { return "Priority"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType damageType) const override {
        auto ordered = heroes;
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                const float leftScore = static_cast<float>(GetPriority(lhs)) * 100000.0f
                                      - EffectiveHealth(lhs, damageType)
                                      - (SafeDistance(lhs, from) * 0.5f);
                const float rightScore = static_cast<float>(GetPriority(rhs)) * 100000.0f
                                       - EffectiveHealth(rhs, damageType)
                                       - (SafeDistance(rhs, from) * 0.5f);
                return leftScore > rightScore;
            });
        return ordered;
    }

    static int GetPriority(const AIHeroClient& hero) {
        const std::string champion = hero.CharacterName();
        if (champion.empty()) {
            return 2;
        }

        auto hasName = [&](const char* const* arr) -> bool {
            for (int i = 0; arr[i] != nullptr; ++i) {
                if (_stricmp(arr[i], champion.c_str()) == 0) {
                    return true;
                }
            }
            return false;
        };

        if (hasName(Generated::PriorityData::kTier4)) return 4;
        if (hasName(Generated::PriorityData::kTier3)) return 3;
        if (hasName(Generated::PriorityData::kTier2)) return 2;
        if (hasName(Generated::PriorityData::kTier1)) return 1;
        return 2;
    }
};

} // namespace SDK::TargetSelectorModes
