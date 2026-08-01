#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cstddef>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::RenataGlasc::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQSpeed = 1450.0f;
inline constexpr float kQRecastRange = 100.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kEWidth = 100.0f;
inline constexpr float kESpeed = 1500.0f;
inline constexpr float kRRange = 2000.0f;
inline constexpr float kRWidth = 250.0f;
inline constexpr float kRSpeed = 650.0f;
inline constexpr float kWDurationSeconds = 5.0f;
inline constexpr float kWReviveWindowSeconds = 3.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline float LevelFraction(int level) {
    return static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
}

inline float PassiveMarkPercent(int level, float abilityPower) {
    if (!std::isfinite(abilityPower)) return 0.0f;
    return 1.0f + 10.0f * LevelFraction(level) +
        0.02f * std::max(0.0f, abilityPower);
}

inline float PassiveMarkDamageRaw(int level, float targetMaxHealth,
                                  float abilityPower) {
    if (!std::isfinite(targetMaxHealth) || targetMaxHealth <= 0.0f) return 0.0f;
    return targetMaxHealth * PassiveMarkPercent(level, abilityPower) / 100.0f;
}

inline bool MarkCanBeConsumed(bool marked, bool allyAttack,
                              bool targetValid = true) {
    return marked && allyAttack && targetValid;
}

inline float QTravelSeconds(float distance, float delay = 0.25f) {
    if (!std::isfinite(distance) || distance < 0.0f || !std::isfinite(delay)) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, kQSpeed);
}

inline bool QLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= kQWidth * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool QCollisionFree(const Vec3& origin, const Vec3& endpoint,
                           const Vec3& target, float targetRadius,
                           const std::vector<Vec3>& blockers) {
    if (!QLineHits(origin, endpoint, target, targetRadius)) return false;
    const auto targetProjection = ProjectPointToSegment2D(target, origin, endpoint);
    for (const Vec3& blocker : blockers) {
        if (!FinitePoint(blocker)) continue;
        const auto projection = ProjectPointToSegment2D(blocker, origin, endpoint);
        if (projection.T + 0.001f < targetProjection.T &&
            projection.Distance <= kQWidth * 0.5f + 45.0f) return false;
    }
    return true;
}

inline Vec3 QEndpoint(const Vec3& origin, const Vec3& target,
                      float range = kQRange) {
    if (!FinitePoint(origin) || !FinitePoint(target)) return {};
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(target));
}

inline Vec3 QRecastEndpoint(const Vec3& target, const Vec3& desired,
                            float maxDistance = kQRecastRange) {
    if (!FinitePoint(target) || !FinitePoint(desired)) return {};
    const Vec3 direction = Direction2D(target, desired);
    if (direction.IsZero()) return target;
    return target + direction * std::min(std::max(0.0f, maxDistance),
                                         target.Distance2D(desired));
}

inline bool QRecastReachable(const Vec3& target, const Vec3& endpoint,
                             float maxDistance = kQRecastRange) {
    return FinitePoint(target) && FinitePoint(endpoint) &&
        target.Distance2D(endpoint) <= std::max(0.0f, maxDistance);
}

inline float ReviveWindowSeconds(float deathElapsedSeconds) {
    if (!std::isfinite(deathElapsedSeconds)) return 0.0f;
    return std::clamp(kWReviveWindowSeconds - std::max(0.0f, deathElapsedSeconds),
                      0.0f, kWReviveWindowSeconds);
}

inline bool ReviveWindowOpen(float deathElapsedSeconds) {
    return ReviveWindowSeconds(deathElapsedSeconds) > 0.0f;
}

inline float EShieldRaw(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 50.0f, 70.0f,
                                                   90.0f, 110.0f, 130.0f};
    if (!std::isfinite(abilityPower)) return 0.0f;
    return base[static_cast<std::size_t>(std::clamp(rank, 0, 5))] +
        0.40f * std::max(0.0f, abilityPower);
}

inline bool ELineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& unit, float unitRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(unit)) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin, endpoint);
    return projection.Distance <= kEWidth * 0.5f +
        std::clamp(unitRadius, 0.0f, 150.0f);
}

inline float RTravelSeconds(float distance, float delay = 0.75f) {
    if (!std::isfinite(distance) || distance < 0.0f || !std::isfinite(delay)) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, kRSpeed);
}

inline bool RLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& unit, float unitRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(unit)) return false;
    const auto projection = ProjectPointToSegment2D(unit, origin, endpoint);
    return projection.Distance <= kRWidth * 0.5f +
        std::clamp(unitRadius, 0.0f, 150.0f);
}

inline bool RHostileTargetAllowed(bool enemy, bool valid, bool invulnerable,
                                  bool spellShielded) {
    return enemy && valid && !invulnerable && !spellShielded;
}

inline bool RCommitSafe(int enemies, int allies, bool underTurret,
                        bool wall, int maximumEnemies = 3) {
    return enemies >= 1 && allies >= 1 && !wall &&
        (!underTurret || allies >= 2) &&
        enemies <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::RenataGlasc::Geometry
