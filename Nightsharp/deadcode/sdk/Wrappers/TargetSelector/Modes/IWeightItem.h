#pragma once

#include "../ITargetSelectorMode.h"

namespace SDK::TargetSelectorModes {

class IWeightItem {
public:
    virtual ~IWeightItem() = default;
    virtual const char* Name() const = 0;
    virtual const char* DisplayName() const = 0;
    virtual float Score(const AIHeroClient& player,
                        const AIHeroClient& target,
                        DamageType damageType) const = 0;
};

} // namespace SDK::TargetSelectorModes
