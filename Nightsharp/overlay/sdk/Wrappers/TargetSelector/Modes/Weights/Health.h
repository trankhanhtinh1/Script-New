#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class Health : public IWeightItem {
public:
    const char* Name() const override { return "Health"; }
    const char* DisplayName() const override { return "Health"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.Health();
    }
};

} // namespace SDK::Modes::Weights
