#pragma once

#include <vector>

#include "../../Enumerations/DamageType.h"
#include "../../GameObjects/GameObjects.h"
#include "../../UI/UI.h"

namespace SDK {

class ITargetSelectorMode {
public:
    virtual ~ITargetSelectorMode() = default;

    virtual const char* DisplayName() const = 0;
    virtual const char* Name() const = 0;
    virtual void AddToMenu(Menu* menu) = 0;
    virtual std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes) = 0;
};

} // namespace SDK
