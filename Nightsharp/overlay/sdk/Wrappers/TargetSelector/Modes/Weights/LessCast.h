#pragma once

#include "../IWeightItem.h"
#include <algorithm>

namespace SDK::Modes::Weights {

class LessCast : public IWeightItem {
public:
    const char* Name() const override { return "LessCast"; }
    const char* DisplayName() const override { return "Less Cast"; }
    int DefaultWeight() const override { return 10; }
    bool Inverted() const override { return true; }
    float GetValue(const AIHeroClient& hero) override {
        const float playerAP = GameObjects::Player().TotalMagicalDamage();
        return hero.Health() / (std::max)(1.0f, playerAP);
    }
};

} // namespace SDK::Modes::Weights
