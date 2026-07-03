#pragma once

#include "../IWeightItem.h"

namespace SDK::Modes::Weights {

class Aggro : public IWeightItem {
public:
    const char* Name() const override { return "Aggro"; }
    const char* DisplayName() const override { return "Aggro"; }
    int DefaultWeight() const override { return 10; }
    float GetValue(const AIHeroClient& hero) override {
        float aggroCount = 0.0f;
        for (const auto& t : GameObjects::AllyTurrets()) {
            if (!t.IsDead() && t.Distance(hero) <= t.AttackRange() + 100.0f) {
                aggroCount += 3.0f;
            }
        }
        for (const auto& m : GameObjects::AllyMinions()) {
            if (!m.IsDead() && m.Distance(hero) <= m.AttackRange() + 100.0f) {
                aggroCount += 1.0f;
            }
        }
        return aggroCount;
    }
};

} // namespace SDK::Modes::Weights
