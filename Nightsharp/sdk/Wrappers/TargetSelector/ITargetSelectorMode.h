#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"

#include <algorithm>
#include <vector>

namespace SDK::TargetSelectorModes {

class ITargetSelectorMode {
public:
    virtual ~ITargetSelectorMode() = default;

    virtual const char* Name() const = 0;
    virtual const char* DisplayName() const = 0;
    virtual void AddToMenu(Menu*) const {}

    virtual std::vector<AIHeroClient> OrderChampions(const std::vector<AIHeroClient>& heroes,
                                                     const Vector3& from,
                                                     DamageType damageType) const = 0;

    static float EffectiveHealth(const AIHeroClient& hero, DamageType damageType) {
        const float rawHealth = hero.Health() + hero.Ref().GetTotalShield();
        switch (damageType) {
        case DamageType::Physical:
            return rawHealth * (100.0f + std::max(hero.Armor(), 0.0f)) / 100.0f;
        case DamageType::Magical:
            return rawHealth * (100.0f + std::max(hero.SpellBlock(), 0.0f)) / 100.0f;
        case DamageType::Mixed:
            return rawHealth * (100.0f + std::max((hero.Armor() + hero.SpellBlock()) * 0.5f, 0.0f)) / 100.0f;
        case DamageType::True:
        default:
            return rawHealth;
        }
    }

    static float SafeDistance(const AIHeroClient& hero, const Vector3& from) {
        return std::max(hero.Distance(from), 1.0f);
    }
};

} // namespace SDK::TargetSelectorModes
