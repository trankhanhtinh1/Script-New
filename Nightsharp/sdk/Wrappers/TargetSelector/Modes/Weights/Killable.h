#pragma once

#include "../IWeightItem.h"
#include "../../Damages/Damage.h"

namespace SDK::Modes::Weights {

class Killable : public IWeightItem {
public:
    const char* Name() const override { return "aa-killable"; }
    const char* DisplayName() const override { return "AA Killable"; }
    int DefaultWeight() const override { return 20; }
    float GetValue(const AIHeroClient& hero) override {
        return hero.Health() < Damage::GetAutoAttackDamage(GameObjects::Player(), hero) ? 1.0f : 0.0f;
    }
};

} // namespace SDK::Modes::Weights
