#pragma once

#include "../ITargetSelectorMode.h"
#include "../../../Enumerations/SpellSlot.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class LessCastsToKill final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "lesscasts"; }
    const char* DisplayName() const override { return "Less Casts To Kill"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3& from,
                                             DamageType) const override {
        auto ordered = heroes;
        const auto player = ObjectManager::Player();
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                const float lhsCasts = EffectiveCasts(player, lhs);
                const float rhsCasts = EffectiveCasts(player, rhs);
                if (lhsCasts != rhsCasts) {
                    return lhsCasts < rhsCasts;
                }
                return lhs.Distance(from) < rhs.Distance(from);
            });
        return ordered;
    }

private:
    static float EffectiveCasts(const AIHeroClient& player, const AIHeroClient& hero) {
        float best = 0.0f;
        best = std::max(best, player.GetSpellDamage(hero, SpellSlot::Q));
        best = std::max(best, player.GetSpellDamage(hero, SpellSlot::W));
        best = std::max(best, player.GetSpellDamage(hero, SpellSlot::E));
        best = std::max(best, player.GetSpellDamage(hero, SpellSlot::R));
        best = std::max(best, player.GetAutoAttackDamage(hero));
        if (best <= 0.0f) {
            return EffectiveHealth(hero, DamageType::True);
        }
        return EffectiveHealth(hero, DamageType::True) / best;
    }
};

} // namespace SDK::TargetSelectorModes
