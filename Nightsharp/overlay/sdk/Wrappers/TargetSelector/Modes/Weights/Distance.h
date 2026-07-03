#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class Distance : public IWeightItem {
public:
    const char* Name() const override { return "Distance"; }
    const char* DisplayName() const override { return "Distance"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.Distance(GameObjects::Player());
    }
};

} // namespace SDK::Modes::Weights
