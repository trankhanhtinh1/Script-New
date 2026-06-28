#pragma once

#include "../../Enumerations/DamageType.h"
#include "../../Core/Objects.h"

namespace SDK::DamageMastery {

inline float GetOutgoingDamageModifier(const AIHeroClient&, const AIBaseClient&, DamageType) {
    return 1.0f;
}

inline float GetIncomingDamageModifier(const AIBaseClient& target, DamageType) {
    return target.IsInvulnerable() ? 0.0f : 1.0f;
}

inline float ApplyOutgoing(const AIHeroClient& source,
                           const AIBaseClient& target,
                           DamageType damageType,
                           float damage) {
    return damage * GetOutgoingDamageModifier(source, target, damageType);
}

inline float ApplyIncoming(const AIBaseClient& target,
                           DamageType damageType,
                           float damage) {
    return damage * GetIncomingDamageModifier(target, damageType);
}

inline float Apply(const AIHeroClient& source,
                   const AIBaseClient& target,
                   DamageType damageType,
                   float damage) {
    return ApplyIncoming(target, damageType, ApplyOutgoing(source, target, damageType, damage));
}

} // namespace SDK::DamageMastery
