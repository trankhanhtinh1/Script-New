#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nami::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 875.0f;
inline constexpr float kQRadius = 200.0f;
inline constexpr float kQDelaySeconds = 0.25f;
inline constexpr float kQMissileSpeed = 1750.0f;
inline constexpr float kWRange = 725.0f;
inline constexpr float kWRadius = 210.0f;
inline constexpr float kERange = 800.0f;
inline constexpr float kRRange = 2550.0f;
inline constexpr float kRWidth = 325.0f;
inline constexpr float kRRadius = 210.0f;
inline constexpr float kRMissileSpeed = 1200.0f;

inline bool WithinRange(const Vec3& origin, const Vec3& point, float range,
                        float targetRadius = 0.0f) {
    return origin.IsValid() && point.IsValid() &&
           origin.Distance2D(point) <= std::max(0.0f, range) +
               std::max(0.0f, targetRadius);
}

inline Vec3 QPredictedCenter(const Vec3& origin, const Vec3& target,
                            const Vec3& velocity, float castDelay = kQDelaySeconds,
                            float missileSpeed = kQMissileSpeed) {
    if (!origin.IsValid() || !target.IsValid()) return {};
    const float distance = origin.Distance2D(target);
    const float travel = std::max(0.0f, castDelay) +
        distance / std::max(1.0f, missileSpeed);
    Vec3 result = target + velocity * travel;
    result.y = target.y;
    return result;
}

inline bool BubbleHits(const Vec3& center, const Vec3& target,
                       float targetRadius = 0.0f,
                       float bubbleRadius = kQRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, bubbleRadius) +
               std::clamp(targetRadius, 0.0f, 200.0f);
}

inline float BounceMultiplier(int bounceIndex, float abilityPower) {
    const int bounce = std::clamp(bounceIndex, 0, 2);
    const float perBounce = std::clamp(0.80f +
        std::max(0.0f, abilityPower) * 0.0015f, 0.0f, 1.0f);
    return std::pow(perBounce, static_cast<float>(bounce));
}

inline float WRawAmount(int rank, float abilityPower, int bounceIndex,
                        bool heal) {
    static constexpr std::array<float, 6> damage = {0.0f, 90.0f, 145.0f,
        200.0f, 255.0f, 310.0f};
    static constexpr std::array<float, 6> healing = {0.0f, 55.0f, 80.0f,
        105.0f, 130.0f, 155.0f};
    const auto& values = heal ? healing : damage;
    const int safeRank = std::clamp(rank, 0, 5);
    const float ratio = heal ? 0.40f : 0.50f;
    return (values[static_cast<std::size_t>(safeRank)] +
            std::max(0.0f, abilityPower) * ratio) *
           BounceMultiplier(bounceIndex, abilityPower);
}

inline bool WCanBounce(const Vec3& source, const Vec3& candidate,
                       float bounceRange = kWRange) {
    return WithinRange(source, candidate, bounceRange, 0.0f);
}

inline float EEmpoweredDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 20.0f, 30.0f,
        40.0f, 50.0f, 60.0f};
    const int safeRank = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(safeRank)] +
           std::max(0.0f, abilityPower) * 0.20f;
}

inline float ESlowPercent(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 15.0f, 20.0f,
        25.0f, 30.0f, 35.0f};
    const int safeRank = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(safeRank)] +
           std::max(0.0f, abilityPower) * 0.05f;
}

inline Vec3 REndpoint(const Vec3& origin, const Vec3& direction,
                      float distance = kRRange) {
    const Vec3 unit = Direction2D({}, direction);
    if (unit.IsZero() || !origin.IsValid()) return {};
    Vec3 result = origin + unit * std::clamp(distance, 0.0f, kRRange);
    result.y = origin.y;
    return result;
}

inline float RTravelSeconds(float distance) {
    return std::max(0.0f, distance) / kRMissileSpeed + 0.5f;
}

inline float RSlowDuration(float travelDistance) {
    return std::clamp(2.0f + std::max(0.0f, travelDistance) * 0.002f,
                      2.0f, 4.0f);
}

inline bool RLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 0.0f,
                      float lineWidth = kRWidth) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, lineWidth) * 0.5f +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool FirstCollisionOwnsTarget(const Vec3& origin, const Vec3& endpoint,
                                     const Vec3& selected,
                                     const Vec3& nearerEnemy,
                                     float targetRadius = 0.0f) {
    if (!RLineHits(origin, endpoint, selected, targetRadius)) return false;
    if (!nearerEnemy.IsValid()) return true;
    if (!RLineHits(origin, endpoint, nearerEnemy, 0.0f)) return true;
    return origin.Distance2D(selected) <= origin.Distance2D(nearerEnemy);
}

inline bool SafeAllyEmpower(const Vec3& player, const Vec3& ally,
                            float range = kERange, bool allyThreatened = false,
                            bool underTurret = false) {
    return WithinRange(player, ally, range) && !underTurret &&
           (allyThreatened || ally.Distance2D(player) <= range);
}

inline bool SafePeelEndpoint(const Vec3& endpoint, bool walkable,
                             bool underTurret, int enemiesAtEndpoint,
                             int maximumEnemies, bool fleeing) {
    return endpoint.IsValid() && walkable && !underTurret &&
           (fleeing || enemiesAtEndpoint <= std::max(0, maximumEnemies));
}

inline bool PreserveAaWindup(bool orbwalkerWinding, bool lethalSpell,
                             bool reactivePeel) {
    return !orbwalkerWinding || lethalSpell || reactivePeel;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nami::Geometry
