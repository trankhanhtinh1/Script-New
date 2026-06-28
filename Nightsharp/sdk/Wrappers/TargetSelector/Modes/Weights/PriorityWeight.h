#pragma once

#include "../IWeightItem.h"
#include "../Priority.h"

namespace SDK::Modes::Weights {

class PriorityWeight : public IWeightItem {
public:
    const char* Name() const override { return "Priority"; }
    const char* DisplayName() const override { return "Priority"; }
    int DefaultWeight() const override { return 10; }
    float GetValue(const AIHeroClient& hero) override {
        auto* inst = Priority::Instance();
        if (inst) {
            return static_cast<float>(inst->GetHeroPriority(hero));
        }
        return 1.0f;
    }
};

} // namespace SDK::Modes::Weights
