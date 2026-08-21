#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Velkoz::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 1050.0f;
inline constexpr float kQWidth = 50.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1300.0f;
inline constexpr float kQSplitRange = 1100.0f;
inline constexpr float kQSplitWidth = 45.0f;
inline constexpr float kQSplitSpeed = 2100.0f;
inline constexpr float kQExtendedRange = 1485.0f;
inline constexpr float kQMinimumSideAngleRadians = 0.4886921905584123f;
inline constexpr float kQMaximumSideAngleRadians = 0.7853981633974483f;
inline constexpr float kHalfPiRadians = 1.5707963267948966f;
inline constexpr float kWRange = 1050.0f;
inline constexpr float kWWidth = 88.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kWSecondDelay = 0.75f;
inline constexpr float kERange = 800.0f;
inline constexpr float kERadius = 225.0f;
inline constexpr float kEDelay = 0.75f;
inline constexpr float kRRange = 1550.0f;
inline constexpr float kRWidth = 90.0f;
inline constexpr float kRChannelSeconds = 2.50f;
inline constexpr int kOrganicStackLimit = 3;
inline constexpr int kOrganicStackDurationMs = 7000;

inline float PointSegmentDistance(const Vec3& point, const Vec3& start,
                                  const Vec3& end) {
    return ProjectPointToSegment2D(point, start, end).Distance;
}

inline bool LineHits(const Vec3& start, const Vec3& end, const Vec3& target,
                     float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    return PointSegmentDistance(target, start, end) <=
        std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

inline bool QLineHits(const Vec3& start, const Vec3& end, const Vec3& target,
                      float targetRadius = 0.0f) {
    return LineHits(start, end, target, kQWidth, targetRadius);
}

inline bool QCollisionFree(float targetDistance, float firstCollisionDistance,
                           float targetRadius = 0.0f) {
    if (targetDistance < 0.0f || firstCollisionDistance < 0.0f) return true;
    return targetDistance + std::max(0.0f, targetRadius) < firstCollisionDistance;
}

struct QSideCast {
    Vec3 Aim{};
    Vec3 SplitOrigin{};
    bool Left = false;
    bool Valid = false;
};

inline QSideCast BuildQSideCast(const Vec3& origin, const Vec3& target,
                                bool left) {
    QSideCast result{};
    if (!origin.IsValid() || !target.IsValid()) return result;
    const Vec3 targetDirection = Direction2D(origin, target);
    const float distance = origin.Distance2D(target);
    if (targetDirection.IsZero() || distance <= 0.0f ||
        distance > kQExtendedRange) {
        return result;
    }

    float angle = kQMinimumSideAngleRadians;
    if (distance * std::cos(angle) > kQRange) {
        angle = std::acos(std::clamp(kQRange / distance, 0.0f, 1.0f));
    }
    angle = std::clamp(angle, kQMinimumSideAngleRadians,
                       kQMaximumSideAngleRadians);
    const float projectedForward = distance * std::cos(angle);
    const float forward = std::min(projectedForward, kQRange);
    const float lateral = distance * std::sin(angle);
    if (projectedForward > kQRange + 1.0f ||
        lateral > kQSplitRange + 0.01f) {
        return result;
    }

    const Vec3 castDirection = SharedGeometry::Rotate2D(
        targetDirection, left ? angle : -angle);
    if (castDirection.IsZero()) return result;
    result.Aim = origin + castDirection * kQRange;
    result.SplitOrigin = origin + castDirection * forward;
    result.Left = left;
    result.Valid = result.Aim.IsValid() && result.SplitOrigin.IsValid();
    return result;
}

inline Vec3 QSplitDirection(const Vec3& missileDirection, bool left) {
    if (missileDirection.IsZero()) return {};
    return SharedGeometry::Rotate2D(
        missileDirection, left ? kHalfPiRadians : -kHalfPiRadians);
}

inline Vec3 QSplitEndpoint(const Vec3& splitOrigin,
                           const Vec3& missileDirection,
                           bool left,
                           float range = kQSplitRange) {
    const Vec3 direction = QSplitDirection(missileDirection, left);
    return direction.IsZero()
        ? Vec3{}
        : splitOrigin + direction * std::max(0.0f, range);
}

inline bool QSplitLineHits(const Vec3& splitOrigin,
                           const Vec3& missileDirection,
                           const Vec3& target,
                           float targetRadius = 0.0f) {
    const Vec3 left = QSplitEndpoint(splitOrigin, missileDirection, true);
    const Vec3 right = QSplitEndpoint(splitOrigin, missileDirection, false);
    return LineHits(splitOrigin, left, target, kQSplitWidth, targetRadius) ||
           LineHits(splitOrigin, right, target, kQSplitWidth, targetRadius);
}

inline bool ZoneContains(const Vec3& center, const Vec3& target,
                         float radius, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= std::max(0.0f, radius) +
            std::max(0.0f, targetRadius);
}

inline bool WLineHits(const Vec3& start, const Vec3& end, const Vec3& target,
                      float targetRadius = 0.0f) {
    return LineHits(start, end, target, kWWidth, targetRadius);
}

struct DeconstructionState {
    int stacks = 0;
    int expiresAtMs = 0;
    bool markedByRay = false;
};

inline int OrganicStacks(const DeconstructionState& state, int nowMs) {
    if (state.expiresAtMs > 0 && nowMs >= state.expiresAtMs) return 0;
    return std::clamp(state.stacks, 0, kOrganicStackLimit);
}

inline bool OrganicDetonationReady(const DeconstructionState& state, int nowMs) {
    return OrganicStacks(state, nowMs) >= kOrganicStackLimit;
}

inline bool AddOrganicStack(DeconstructionState& state, int nowMs) {
    const int current = OrganicStacks(state, nowMs);
    state.stacks = std::min(kOrganicStackLimit, current + 1);
    state.expiresAtMs = nowMs + kOrganicStackDurationMs;
    if (state.stacks < kOrganicStackLimit) return false;
    state.stacks = 0;
    state.expiresAtMs = 0;
    return true;
}

inline void ApplyRayMark(DeconstructionState& state, int nowMs) {
    if (state.expiresAtMs > 0 && nowMs >= state.expiresAtMs)
        state.stacks = 0;
    state.markedByRay = true;
    state.expiresAtMs = std::max(state.expiresAtMs, nowMs + kOrganicStackDurationMs);
}

inline void ClearRayMark(DeconstructionState& state) {
    state.markedByRay = false;
}

inline bool RLineHits(const Vec3& origin, const Vec3& target,
                      const Vec3& victim, float victimRadius = 0.0f) {
    return LineHits(origin, target, victim, kRWidth, victimRadius);
}

inline bool RChannelInterrupted(bool hardCrowdControl, bool silenced,
                                bool dead, bool channelExpired) {
    return hardCrowdControl || silenced || dead || channelExpired;
}

inline bool RWallSafe(const Vec3& origin, const Vec3& endpoint,
                      bool wallBetween) {
    return origin.IsValid() && endpoint.IsValid() && !wallBetween &&
        origin.Distance2D(endpoint) <= kRRange;
}

inline bool RCommitSafe(const Vec3& position, bool wall, bool underTurret,
                        int nearbyEnemies, int maxEnemies, bool lethal) {
    if (!position.IsValid() || position.IsZero() || wall || underTurret) return false;
    if (lethal) return true;
    return nearbyEnemies >= 0 && nearbyEnemies <= std::max(1, maxEnemies);
}
inline float PassiveTrueDamage(int level, float abilityPower) {
    const float clampedLevel = static_cast<float>(std::clamp(level, 1, 18));
    const float base = 35.0f + (clampedLevel - 1.0f) * (145.0f / 17.0f);
    return base + std::max(0.0f, abilityPower) * 0.60f;
}

inline bool RUsesTrueDamage(bool researched) {
    return researched;
}

inline float RTrueDamage(int rank, float abilityPower, bool marked) {
    const std::array<float, 4> base{0.0f, 450.0f, 625.0f, 800.0f};
    const float raw = SharedGeometry::RankValue(base, std::clamp(rank, 0, 3)) +
        std::max(0.0f, abilityPower) * 1.25f;
    (void)marked;
    return raw;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Velkoz::Geometry
