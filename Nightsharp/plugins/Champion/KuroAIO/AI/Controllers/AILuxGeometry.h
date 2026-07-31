#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Lux::Geometry {

using Vec3 = ::Vec3;

inline constexpr float kQRange = 1175.0f;
inline constexpr float kQWidth = 80.0f;
inline constexpr float kQSpeed = 1200.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRange = 1200.0f;
inline constexpr float kWWidth = 150.0f;
inline constexpr float kWShieldDuration = 2.5f;
inline constexpr float kERange = 1100.0f;
inline constexpr float kERadius = 295.0f;
inline constexpr float kEZoneSeconds = 5.0f;
inline constexpr float kRRange = 3340.0f;
inline constexpr float kRWidth = 190.0f;
inline constexpr float kRSpeed = 3000.0f;
inline constexpr float kRDelay = 1.375f;
inline constexpr float kPassiveSeconds = 6.0f;

inline float PointSegmentDistance(const Vec3& point, const Vec3& start,
                                  const Vec3& end) {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float lengthSquared = dx * dx + dz * dz;
    if (lengthSquared <= 0.001f) return point.Distance2D(start);
    const float t = std::clamp(
        ((point.x - start.x) * dx + (point.z - start.z) * dz) /
            lengthSquared,
        0.0f, 1.0f);
    return point.Distance2D({start.x + dx * t, start.y,
                             start.z + dz * t});
}

inline bool LineContains(const Vec3& start, const Vec3& end,
                         const Vec3& target, float targetRadius = 0.0f,
                         float width = kQWidth) {
    return PointSegmentDistance(target, start, end) <=
        std::max(0.0f, targetRadius) + std::max(0.0f, width) * 0.5f;
}

// Q can root at most the first two valid units. The caller passes the
// predicted target contact and the first intervening unit contact. Equality
// is rejected: a blocker at the same contact point owns the collision.
inline bool QFirstCollisionRoot(float targetContactDistance,
                                float firstCollisionDistance,
                                float targetRadius = 0.0f,
                                float blockerRadius = 0.0f) {
    if (targetContactDistance < 0.0f) return false;
    if (firstCollisionDistance < 0.0f) return true;
    return targetContactDistance + std::max(0.0f, targetRadius) <
        firstCollisionDistance - std::max(0.0f, blockerRadius);
}

inline bool QRootAllowed(bool predictionHit, bool projectileWall,
                         bool firstCollisionIsTarget, bool attackWindingUp,
                         bool lethal) {
    if (!predictionHit || projectileWall || !firstCollisionIsTarget) return false;
    return !attackWindingUp || lethal;
}

inline float PassiveDamage(int championLevel, float abilityPower) {
    const float level = std::clamp(static_cast<float>(championLevel), 1.0f, 18.0f);
    return 20.0f + (level - 1.0f) * 10.0f + std::max(0.0f, abilityPower) * 0.10f;
}

struct PassiveMarkState {
    bool Marked = false;
    bool Confirmed = false;
    float ExpiresAt = 0.0f;
};

inline bool PassiveMarkActive(const PassiveMarkState& state, float now) {
    return state.Marked && state.ExpiresAt > now;
}

inline PassiveMarkState ApplyPassiveMark(const PassiveMarkState& state,
                                         float now,
                                         float duration = kPassiveSeconds) {
    PassiveMarkState next = state;
    next.Marked = true;
    next.Confirmed = false;
    next.ExpiresAt = now + std::max(0.0f, duration);
    return next;
}

inline PassiveMarkState ConsumePassiveMark(const PassiveMarkState& state,
                                           float now) {
    if (!PassiveMarkActive(state, now)) return {};
    return {};
}

inline float WShieldAmount(int spellRank, float abilityPower, bool returned) {
    static constexpr float base[] = {0.0f, 25.0f, 40.0f, 55.0f, 70.0f,
                                     85.0f, 100.0f};
    const int rank = std::clamp(spellRank, 0, 6);
    const float amount = base[rank] + std::max(0.0f, abilityPower) * 0.40f;
    return amount * (returned ? 2.0f : 1.0f);
}

inline bool WReturnShieldWorthwhile(bool allyValid, bool returnPredicted,
                                    bool allyLowHealth, bool incomingThreat,
                                    bool manual) {
    if (!allyValid || !returnPredicted) return false;
    return manual || incomingThreat || allyLowHealth;
}

inline bool ZoneContains(const Vec3& center, const Vec3& target,
                         float targetRadius = 0.0f) {
    return center.Distance2D(target) <= kERadius + std::max(0.0f, targetRadius);
}

struct EZoneState {
    bool Active = false;
    bool DetonationReady = false;
    bool SlowObserved = false;
    float ExpiresAt = 0.0f;
    Vec3 Center = {};
};

inline EZoneState StartZone(const Vec3& center, float now,
                            float duration = kEZoneSeconds) {
    EZoneState next{};
    next.Active = !center.IsZero() && center.IsValid();
    next.DetonationReady = next.Active;
    next.ExpiresAt = now + std::max(0.0f, duration);
    next.Center = center;
    return next;
}

inline bool ZoneExpired(const EZoneState& zone, float now) {
    return !zone.Active || zone.ExpiresAt <= now;
}

inline bool ShouldDetonateZone(const EZoneState& zone, const Vec3& target,
                              float targetRadius, bool lethal,
                              bool rootedOrSlowed, bool aboutToExpire,
                              bool projectileWall, bool underTurret) {
    if (!zone.Active || !zone.DetonationReady || projectileWall || underTurret ||
        !ZoneContains(zone.Center, target, targetRadius)) return false;
    return lethal || aboutToExpire || rootedOrSlowed;
}

inline float SlowPercent(int spellRank) {
    static constexpr float values[] = {0.0f, 20.0f, 25.0f, 30.0f, 35.0f,
                                       40.0f, 45.0f};
    return values[std::clamp(spellRank, 0, 6)];
}

struct RLineContext {
    bool TargetValid = false;
    bool PredictionVeryHigh = false;
    bool ProjectileWall = false;
    bool LineSafe = false;
    bool Lethal = false;
    bool Manual = false;
    bool ChannelActive = false;
    bool UnderTurret = false;
    int NearbyEnemies = 0;
    int MinimumTargets = 1;
    float Distance = 0.0f;
    float Range = kRRange;
};

inline bool RLineInRange(const RLineContext& context) {
    return context.TargetValid && context.Distance <= context.Range;
}

inline bool ShouldCastR(const RLineContext& context) {
    if (!RLineInRange(context) || !context.PredictionVeryHigh ||
        context.ProjectileWall || !context.LineSafe || context.ChannelActive ||
        context.UnderTurret) return false;
    if (context.Manual) return true;
    if (!context.Lethal || context.NearbyEnemies > 0) return false;
    return context.MinimumTargets <= 1;
}

inline bool PreserveManualChannel(bool channelActive, bool localThreat,
                                  bool manualStarted) {
    return channelActive && (manualStarted || !localThreat);
}

inline bool SafeBeamLine(bool projectileWall, bool underTurret,
                         int nearbyEnemies, int maximumNearbyEnemies,
                         bool lethal) {
    if (projectileWall || underTurret) return false;
    return lethal || nearbyEnemies <= std::max(0, maximumNearbyEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Lux::Geometry
