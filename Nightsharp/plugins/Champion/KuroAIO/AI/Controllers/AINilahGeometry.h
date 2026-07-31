#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nilah::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 600.0f;
inline constexpr float kQHalfWidth = 75.0f;
inline constexpr float kQSpeed = 2200.0f;
inline constexpr float kQDelaySeconds = 0.25f;
inline constexpr float kWDurationSeconds = 2.5f;
inline constexpr float kWAllyDurationSeconds = 1.5f;
inline constexpr float kERange = 550.0f;
inline constexpr float kEHalfWidth = 100.0f;
inline constexpr float kEDashSeconds = 0.25f;
inline constexpr float kRRadius = 450.0f;
inline constexpr float kRDurationSeconds = 1.5f;

inline bool QHits(const Vec3& origin, const Vec3& endpoint,
                  const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, origin, endpoint).Distance <=
        kQHalfWidth + std::max(0.0f, targetRadius);
}

inline float QTravelSeconds(const Vec3& origin, const Vec3& endpoint,
                            float speed = kQSpeed) {
    if (!origin.IsValid() || !endpoint.IsValid() || !std::isfinite(speed) || speed <= 0.0f)
        return 0.0f;
    return kQDelaySeconds + origin.Distance2D(endpoint) / speed;
}

inline bool WithinReach(const Vec3& origin, const Vec3& target,
                        float range, float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() && !target.IsZero() &&
        origin.Distance2D(target) <= std::max(0.0f, range) + std::max(0.0f, targetRadius) + 0.01f;
}

inline bool RAreaHits(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() && !center.IsZero() && !target.IsZero() &&
        center.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius) + 0.01f;
}

inline Vec3 DashEndpoint(const Vec3& origin, const Vec3& target,
                         float range = kERange) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(target));
}

inline bool DashSafe(const Vec3& endpoint, bool wall, bool enemyTurret,
                     int enemiesAtEndpoint, int maximumEnemies, bool fleeing,
                     bool lethalCommit = false) {
    if (!endpoint.IsValid() || endpoint.IsZero() || wall) return false;
    if (enemyTurret && !lethalCommit) return false;
    if (fleeing) return enemiesAtEndpoint <= std::max(0, maximumEnemies);
    return lethalCommit || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline bool WDefensiveValue(float healthPercent, bool incomingThreat,
                            bool allyThreatened, bool reactive) {
    if (reactive || incomingThreat || allyThreatened) return healthPercent <= 82.0f;
    return healthPercent <= 58.0f;
}

inline bool PreserveAaWindup(bool windingUp, bool reactive, bool lethal,
                             bool allyThreatened = false) {
    if (!windingUp) return false;
    if (reactive || lethal || allyThreatened) return false;
    return true;
}

struct SharedResourceState {
    bool PassiveObserved = false;
    bool AllySharing = false;
    float LastHealAmount = 0.0f;
    float LastShieldAmount = 0.0f;
    int LastAllyId = 0;
    int LastEventTick = 0;
};

inline void RecordSharedHeal(SharedResourceState& state, int allyId,
                             float healAmount, int tick) {
    state.PassiveObserved = true;
    state.AllySharing = allyId != 0;
    state.LastAllyId = allyId;
    state.LastHealAmount = std::max(0.0f, healAmount);
    state.LastEventTick = tick;
}

inline void RecordSharedShield(SharedResourceState& state, int allyId,
                               float shieldAmount, int tick) {
    state.PassiveObserved = true;
    state.AllySharing = allyId != 0;
    state.LastAllyId = allyId;
    state.LastShieldAmount = std::max(0.0f, shieldAmount);
    state.LastEventTick = tick;
}

inline bool PassiveShareValid(const SharedResourceState& state, int now,
                              int timeoutMs = 1800) {
    return state.PassiveObserved && state.LastEventTick > 0 &&
        now - state.LastEventTick <= std::max(1, timeoutMs);
}

struct RDecisionContext {
    bool MultiTarget = false;
    bool Lethal = false;
    bool HealNeeded = false;
    bool UnderEnemyTurret = false;
    bool CommitSafe = false;
    bool Manual = false;
};

inline bool ShouldCastR(const RDecisionContext& context) {
    if (context.Manual) return context.MultiTarget || context.Lethal || context.HealNeeded;
    if (context.UnderEnemyTurret && !context.Lethal) return false;
    return context.CommitSafe && (context.Lethal || context.MultiTarget || context.HealNeeded);
}

inline bool RecastWindowOpen(int now, int castTick, int durationMs = 650) {
    return castTick > 0 && now >= castTick && now <= castTick + std::max(1, durationMs);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nilah::Geometry
