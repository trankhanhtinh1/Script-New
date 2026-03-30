#pragma once

#include "../IWeightItem.h"
#include "../../../../Core/Game.h"

namespace SDK::TargetSelectorModes::Weights {

class ShortDistanceCursor final : public IWeightItem {
public:
    const char* Name() const override { return "shortdistancecursor"; }
    const char* DisplayName() const override { return "Short Distance Cursor"; }
    float Score(const AIHeroClient&, const AIHeroClient& target, DamageType) const override {
        return 1000.0f / std::max(target.Distance(Game::CursorPos()), 1.0f);
    }
};

} // namespace SDK::TargetSelectorModes::Weights
