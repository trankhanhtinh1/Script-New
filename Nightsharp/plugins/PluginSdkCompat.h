#pragma once

#include "../core/CoreAPI.h"
#include "../sdk/Core/Objects.h"
#include "../sdk/Enumerations/DamageType.h"
#include "../sdk/Math/Prediction/Movement.h"
#include "../sdk/Utils/Invulnerable.h"

#include <initializer_list>

namespace Plugins::Compat {

inline constexpr int kBuffTypeStun = SDK::Prediction::Movement::detail::kBuffTypeStun;
inline constexpr int kBuffTypeSnare = SDK::Prediction::Movement::detail::kBuffTypeSnare;
inline constexpr int kBuffTypeSuppression = SDK::Prediction::Movement::detail::kBuffTypeSuppression;
inline constexpr int kBuffTypeCharm = SDK::Prediction::Movement::detail::kBuffTypeCharm;
inline constexpr int kBuffTypeKnockup = SDK::Prediction::Movement::detail::kBuffTypeKnockup;

inline bool HasBuffType(const SDK::AIBaseClient& target, int type) {
    return target.IsValid() && CoreAPI::Buffs::HasBuffType(target.Address(), type);
}

inline bool HasAnyBuffType(const SDK::AIBaseClient& target, std::initializer_list<int> buffTypes) {
    if (!target.IsValid()) {
        return false;
    }

    for (const int buffType : buffTypes) {
        if (CoreAPI::Buffs::HasBuffType(target.Address(), buffType)) {
            return true;
        }
    }
    return false;
}

inline bool HasMovementLock(const SDK::AIBaseClient& target) {
    return !target.CanMove() ||
           HasAnyBuffType(target, {kBuffTypeStun, kBuffTypeSnare, kBuffTypeSuppression, kBuffTypeCharm, kBuffTypeKnockup});
}

inline bool IsZombieLike(const SDK::AIHeroClient& target) {
    return target.IsValid() &&
           (target.HasBuff("KarthusDeathDefiedBuff") ||
            target.HasBuff("sionpassivezombie") ||
            target.HasBuff("SionPassiveZombie"));
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
