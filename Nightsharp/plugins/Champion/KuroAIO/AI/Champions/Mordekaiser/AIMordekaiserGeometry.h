#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Mordekaiser::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kPassiveRadius = 375.0f;
inline constexpr float kPassivePulseSeconds = 0.25f;
inline constexpr int kPassiveDurationMs = 4000;
inline constexpr float kQRange = 675.0f;
inline constexpr float kQHalfWidth = 86.0f;
inline constexpr float kQDelay = 0.50f;
inline constexpr float kQIsolationMultiplier = 1.40f;
inline constexpr float kWRange = 125.0f;
inline constexpr float kERange = 700.0f;
inline constexpr float kEHalfAngleDegrees = 22.5f;
inline constexpr float kEWidth = 200.0f;
inline constexpr float kESpeed = 3000.0f;
inline constexpr float kETravelSeconds = 0.25f;
inline constexpr int kRDurationMs = 7000;
inline constexpr float kRRange = 650.0f;
inline constexpr float kRStatStealPercent = 10.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float LevelValue(int level, float first, float last) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return first + (last - first) * t;
}

inline constexpr float PassiveRawDamagePerPulse(int level, float abilityPower,
                                                  float targetMaximumHealth,
                                                  bool monster = false) {
    const float percent = LevelValue(level, 0.01f, 0.05f);
    const float healthPart = std::max(0.0f, targetMaximumHealth) * percent;
    const float base = LevelValue(level, 5.0f, 15.0f) +
        0.30f * std::max(0.0f, abilityPower) + healthPart;
    return monster ? std::min(base, 40.0f + 0.01f * std::max(0.0f, targetMaximumHealth)) : base;
}

inline constexpr float PassiveMovementSpeedPercent(int level) {
    (void)level;
    return 3.0f;
}

inline constexpr float QRawDamage(int rank, float totalAttackDamage, float abilityPower,
                                  bool isolated = false) {
    const float normal = RankValue(rank, {80.0f, 110.0f, 140.0f, 170.0f, 200.0f}) +
        RankValue(rank, {0.60f, 0.65f, 0.70f, 0.75f, 0.80f}) *
            std::max(0.0f, totalAttackDamage) +
        0.60f * std::max(0.0f, abilityPower);
    return isolated ? normal * kQIsolationMultiplier : normal;
}

inline constexpr float QIsolationBonus(float normalDamage) {
    return std::max(0.0f, normalDamage) * (kQIsolationMultiplier - 1.0f);
}

inline constexpr float WStoredResource(float damageDealt, float damageTaken) {
    return 0.35f * std::max(0.0f, damageDealt) +
        0.15f * std::max(0.0f, damageTaken);
}

inline constexpr float WShieldConversion(int rank, float storedResource) {
    return std::max(0.0f, storedResource) *
        RankValue(rank, {0.40f, 0.425f, 0.45f, 0.475f, 0.50f});
}

inline constexpr float WHealConversion(int rank, float remainingShield) {
    return std::max(0.0f, remainingShield) *
        RankValue(rank, {0.20f, 0.225f, 0.25f, 0.275f, 0.30f});
}

inline bool CircleHits(const Vec3& center, const Vec3& target, float radius,
                       float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= std::max(0.0f, radius) + std::max(0.0f, targetRadius);
}

inline bool QHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                  float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T <= 1.0f && projection.Distance <=
        kQHalfWidth + std::max(0.0f, targetRadius) &&
        origin.Distance2D(target) <= kQRange + std::max(0.0f, targetRadius);
}

inline bool IsolatedQ(const Vec3& impact, const Vec3& target, float targetRadius,
                      const std::array<Vec3, 16>& otherPositions,
                      const std::array<float, 16>& otherRadii, int otherCount) {
    if (!CircleHits(impact, target, kQHalfWidth, targetRadius)) return false;
    const int count = std::clamp(otherCount, 0, static_cast<int>(otherPositions.size()));
    for (int i = 0; i < count; ++i) {
        if (CircleHits(impact, otherPositions[i], kQHalfWidth, otherRadii[i])) return false;
    }
    return true;
}

inline bool ConeContains(const Vec3& origin, const Vec3& directionPoint,
                         const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !directionPoint.IsValid() || !target.IsValid()) return false;
    const Vec3 forward = Direction2D(origin, directionPoint);
    const Vec3 toTarget = Direction2D(origin, target);
    if (forward.IsZero() || toTarget.IsZero()) return false;
    const float distance = origin.Distance2D(target);
    if (distance > kERange + std::max(0.0f, targetRadius)) return false;
    const float dot = std::clamp(forward.x * toTarget.x + forward.z * toTarget.z, -1.0f, 1.0f);
    const float angle = std::acos(dot) * 57.2957795f;
    const float radiusAngle = std::asin(std::clamp(
        std::max(0.0f, targetRadius) / std::max(1.0f, distance), -1.0f, 1.0f)) * 57.2957795f;
    return angle <= kEHalfAngleDegrees + radiusAngle;
}

inline Vec3 PullEndpoint(const Vec3& origin, const Vec3& target, float pullDistance = 325.0f) {
    if (!origin.IsValid() || !target.IsValid()) return {};
    const Vec3 direction = Direction2D(target, origin);
    if (direction.IsZero()) return target;
    return target + direction * std::min(std::max(0.0f, pullDistance), target.Distance2D(origin));
}

inline bool ProjectileReaches(const Vec3& origin, const Vec3& target,
                              float delay = kETravelSeconds) {
    if (!origin.IsValid() || !target.IsValid()) return false;
    return origin.Distance2D(target) <= kERange + 1.0f && delay >= 0.0f;
}

struct RealmCommitContext {
    bool TargetValid = false;
    bool TargetInRange = false;
    bool TargetProtected = false;
    bool PlayerMobilityLocked = false;
    bool UnderEnemyTurret = false;
    bool LethalWithoutRealm = false;
    bool Defensive = false;
    bool Manual = false;
    bool TargetLowHealth = false;
    bool TargetAlreadyInRealm = false;
    int NearbyEnemies = 0;
    int MaximumNearbyEnemies = 2;
};

inline bool SafeRealmCommit(const RealmCommitContext& context) {
    if (!context.TargetValid || !context.TargetInRange || context.TargetProtected ||
        context.PlayerMobilityLocked || context.TargetAlreadyInRealm) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.LethalWithoutRealm && !context.Manual)
        return false;
    if (!context.Defensive && !context.LethalWithoutRealm && !context.Manual &&
        context.NearbyEnemies > std::max(0, context.MaximumNearbyEnemies)) return false;
    return context.Manual || context.Defensive || context.LethalWithoutRealm || context.TargetLowHealth;
}

inline constexpr bool RealmActive(int targetId, int expireTick, int now) {
    return targetId > 0 && expireTick > now;
}

inline constexpr int RealmExpireTick(int castTick) {
    return castTick > 0 ? castTick + kRDurationMs : 0;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Mordekaiser::Geometry
