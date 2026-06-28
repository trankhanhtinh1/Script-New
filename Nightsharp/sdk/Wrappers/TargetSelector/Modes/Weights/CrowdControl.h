#pragma once

#include "../IWeightItem.h"
#include "../../../../Core/CoreBuffs.h"

namespace SDK::Modes::Weights {

class CrowdControl : public IWeightItem {
public:
    const char* Name() const override { return "CrowdControl"; }
    const char* DisplayName() const override { return "Crowd Control"; }
    int DefaultWeight() const override { return 10; }
    float GetValue(const AIHeroClient& hero) override {
        float ccScore = 0.0f;
        uintptr_t addr = hero.Address();
        // Stun (5), Taunt (8), Polymorph (9), Snare (11), Charm (22), Suppression (24), Knockup (29)
        if (CoreBuffs::HasBuffType(addr, 5)) ccScore += 3.0f;
        if (CoreBuffs::HasBuffType(addr, 8)) ccScore += 2.5f;
        if (CoreBuffs::HasBuffType(addr, 9)) ccScore += 2.5f;
        if (CoreBuffs::HasBuffType(addr, 11)) ccScore += 2.0f;
        if (CoreBuffs::HasBuffType(addr, 22)) ccScore += 3.0f;
        if (CoreBuffs::HasBuffType(addr, 24)) ccScore += 3.0f;
        if (CoreBuffs::HasBuffType(addr, 29)) ccScore += 3.0f;
        if (CoreBuffs::HasBuffType(addr, 10)) ccScore += 1.0f; // Slow
        return ccScore;
    }
};

} // namespace SDK::Modes::Weights
