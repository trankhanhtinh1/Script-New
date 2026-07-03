#pragma once

#include "../IWeightItem.h"
#include "../../Damages/Damage.h"
#include <algorithm>

namespace SDK::Modes::Weights {

class LessAttack : public IWeightItem {
public:
    const char* Name() const override { return "LessAttack"; }
    const char* DisplayName() const override { return "Less Attack"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.Health() / (std::max)(1.0f, Damage::GetAutoAttackDamage(GameObjects::Player(), hero));
    }
};

} // namespace SDK::Modes::Weights
