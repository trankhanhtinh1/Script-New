#pragma once

#include "../../GameObjects/GameObjects.h"
#include <string>

namespace SDK::Modes {

class IWeightItem {
public:
    virtual ~IWeightItem() = default;

    virtual const char* Name() const = 0;
    virtual const char* DisplayName() const = 0;
    virtual int DefaultWeight() const = 0;
    virtual bool Inverted() const { return false; }
    virtual float GetValue(const AIHeroClient& hero) = 0;
};

} // namespace SDK::Modes
