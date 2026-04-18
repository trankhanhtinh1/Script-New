#pragma once

#include "../../core/CoreAPI.h"
#include "../Core/Objects.h"
#include "../Math/Prediction/Movement.h"

#include <initializer_list>

namespace SDK::Utils::StatusCheck {

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

} // namespace SDK::Utils::StatusCheck
