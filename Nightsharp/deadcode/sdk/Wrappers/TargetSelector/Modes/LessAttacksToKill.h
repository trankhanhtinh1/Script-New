#pragma once

#include "../ITargetSelectorMode.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class LessAttacksToKill final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "lessattacks"; }
    const char* DisplayName() const override { return "Less Attacks To Kill"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType) const override {
        auto ordered = heroes;
        const auto player = ObjectManager::Player();
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                const float lhsHits = EffectiveHits(player, lhs);
                const float rhsHits = EffectiveHits(player, rhs);
                if (lhsHits != rhsHits) {
                    return lhsHits < rhsHits;
                }
                return lhs.Distance(from) < rhs.Distance(from);
            });
        return ordered;
    }

private:
    static float EffectiveHits(const AIHeroClient& player, const AIHeroClient& hero) {
        const float aaDamage = std::max(player.GetAutoAttackDamage(hero), 1.0f);
        return EffectiveHealth(hero, DamageType::Physical) / aaDamage;
    }
};

} // namespace SDK::TargetSelectorModes
