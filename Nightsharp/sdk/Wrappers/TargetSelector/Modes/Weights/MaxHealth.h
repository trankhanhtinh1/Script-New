#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class MaxHealth : public IWeightItem {
public:
    const char* Name() const override { return "MaxHealth"; }
    const char* DisplayName() const override { return "Max Health"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.MaxHealth();
    }
};

} // namespace SDK::Modes::Weights
