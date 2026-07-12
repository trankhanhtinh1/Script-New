#pragma once

#include "../../Core/Objects.h"
#include "../../Enumerations/DamageType.h"

#include <vector>

namespace SDK {

class ITargetSelector {
public:
    virtual ~ITargetSelector() = default;

    virtual AIHeroClient GetSelectedTarget() const = 0;
    virtual void SetTarget(const AIHeroClient& target) = 0;
    virtual AIHeroClient GetTarget(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr) = 0;
    virtual std::vector<AIHeroClient> GetTargets(
        float range,
        DamageType damageType = DamageType::True,
        bool ignoreShields = true,
        const Vector3& from = Vector3(),
        const std::vector<AIHeroClient>* ignoreChampions = nullptr) = 0;
    virtual int GetPriority(const AIHeroClient& target) const = 0;
    virtual void Suspend() = 0;
    virtual void Resume() = 0;
};

} // namespace SDK
