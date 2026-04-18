#pragma once

#include "../core/CoreAPI.h"
#include "../sdk/Core/Objects.h"
#include "../sdk/Enumerations/DamageType.h"
#include "../sdk/Utils/Invulnerable.h"
#include "../sdk/Utils/StatusCheck.h"

#include <initializer_list>

namespace Plugins::Compat {

inline constexpr int kBuffTypeStun        = SDK::Utils::StatusCheck::kBuffTypeStun;
inline constexpr int kBuffTypeSnare       = SDK::Utils::StatusCheck::kBuffTypeSnare;
inline constexpr int kBuffTypeSuppression = SDK::Utils::StatusCheck::kBuffTypeSuppression;
inline constexpr int kBuffTypeCharm       = SDK::Utils::StatusCheck::kBuffTypeCharm;
inline constexpr int kBuffTypeKnockup     = SDK::Utils::StatusCheck::kBuffTypeKnockup;

inline bool HasBuffType(const SDK::AIBaseClient& target, int type) {
    return SDK::Utils::StatusCheck::HasBuffType(target, type);
}

inline bool HasAnyBuffType(const SDK::AIBaseClient& target, std::initializer_list<int> buffTypes) {
    return SDK::Utils::StatusCheck::HasAnyBuffType(target, buffTypes);
}

inline bool HasMovementLock(const SDK::AIBaseClient& target) {
    return SDK::Utils::StatusCheck::HasMovementLock(target);
}

inline bool IsZombieLike(const SDK::AIHeroClient& target) {
    return SDK::Utils::StatusCheck::IsZombieLike(target);
}

inline bool IsProtectedFromSpell(const SDK::AIHeroClient& target,
                                 SDK::DamageType damageType,
                                 float damage = -1.0f) {
    return target.IsValid() && SDK::Utils::Invulnerable::Check(target, damageType, false, damage);
}

inline bool CastIgnite(const SDK::AIHeroClient& target) {
    const auto player = SDK::ObjectManager::Player();
    if (!player.IsValid() || !target.IsValid()) {
        return false;
    }

    const SDK::SpellSlot igniteSlot = player.GetSpellSlot("SummonerDot");
    if (igniteSlot == SDK::SpellSlot::Unknown) {
        return false;
    }

    const auto ignite = player.GetSpell(igniteSlot);
    if (!ignite.IsValid() || !ignite.IsReady()) {
        return false;
    }

    if (!target.IsValidTarget(ignite.CastRange())) {
        return false;
    }

    return CoreAPI::Control::CastSpell(static_cast<int>(igniteSlot),
                                       player.Position(),
                                       target.Position(),
                                       static_cast<uint32_t>(target.NetworkId()));
}

} // namespace Plugins::Compat
