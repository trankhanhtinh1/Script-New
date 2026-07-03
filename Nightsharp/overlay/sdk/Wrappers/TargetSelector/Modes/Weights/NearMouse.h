#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class NearMouse : public IWeightItem {
public:
    const char* Name() const override { return "NearMouse"; }
    const char* DisplayName() const override { return "Near Mouse"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.Distance(Game::CursorPosition());
    }
};

} // namespace SDK::Modes::Weights
