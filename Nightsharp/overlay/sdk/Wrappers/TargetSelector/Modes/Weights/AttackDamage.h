#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class AttackDamage : public IWeightItem {
public:
    const char* Name() const override { return "AttackDamage"; }
    const char* DisplayName() const override { return "Attack Damage"; }
    int DefaultWeight() const override { return 10; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.TotalAttackDamage();
    }
};

} // namespace SDK::Modes::Weights
