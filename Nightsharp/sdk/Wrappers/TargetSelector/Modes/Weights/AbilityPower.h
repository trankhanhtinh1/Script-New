#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class AbilityPower : public IWeightItem {
public:
    const char* Name() const override { return "AbilityPower"; }
    const char* DisplayName() const override { return "Ability Power"; }
    int DefaultWeight() const override { return 10; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.TotalMagicalDamage();
    }
};

} // namespace SDK::Modes::Weights
