#pragma once

#include "../ITargetSelectorMode.h"
#include "../../../Core/Game.h"

#include <algorithm>

namespace SDK::TargetSelectorModes {

class NearMouse final : public ITargetSelectorMode {
public:
    const char* Name() const override { return "nearmouse"; }
    const char* DisplayName() const override { return "Near Mouse"; }

    std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                             const Vector3&,
                                             DamageType) const override {
        auto ordered = heroes;
        const Vector3 cursor = Game::CursorPos();
        std::stable_sort(ordered.begin(), ordered.end(),
            [&](const AIHeroClient& lhs, const AIHeroClient& rhs) {
                return lhs.Distance(cursor) < rhs.Distance(cursor);
            });
        return ordered;
    }
};

} // namespace SDK::TargetSelectorModes
