#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Udyr::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

enum class Stance : std::uint8_t { None, WildingClaw, IronMantle, BlazingStampede, WingborneStorm };
enum class RecastState : std::uint8_t { Ready, RecastWindow, RecastPending };

inline constexpr float kAttackReach = 125.0f;
inline constexpr float kStanceReach = 370.0f;
inline constexpr float kStormRadius = 370.0f;
inline constexpr float kLightningReach = 450.0f;
inline constexpr int kRecastWindowMs = 4000;
inline constexpr int kRecastSafetyTailMs = 250;
inline constexpr int kStunImmunityMs = 1500;
inline constexpr int kStormDurationMs = 4000;
inline constexpr float kStormSlowBase = 0.30f;
inline constexpr float kStormAwakenedSlowBonus = 0.05f;

inline bool InRecastWindow(RecastState state, Stance stance, int elapsedMs) {
    return stance != Stance::None && state == RecastState::RecastWindow &&
        elapsedMs >= 0 && elapsedMs <= kRecastWindowMs;
}

inline bool RecastSafeTail(int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs + kRecastSafetyTailMs < kRecastWindowMs;
}
inline bool StampedeImmunityActive(int elapsedMs) {
    return elapsedMs >= 0 && elapsedMs < kStunImmunityMs;
}

inline bool MantleRecastValuable(float playerHealthPercent, bool incomingThreat) {
    return incomingThreat || (std::isfinite(playerHealthPercent) && playerHealthPercent <= 68.0f);
}

inline float StormSlow(bool awakened) {
    return kStormSlowBase + (awakened ? kStormAwakenedSlowBonus : 0.0f);
}

inline float ReachFor(Stance stance, float targetRadius = 0.0f) {
    const float radius = std::max(0.0f, targetRadius);
    return (stance == Stance::WingborneStorm ? kStanceReach : kAttackReach) + radius;
}

inline bool InStanceReach(const Vec3& origin, const Vec3& target, Stance stance,
                          float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= ReachFor(stance, targetRadius);
}

inline bool StormCovers(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    if (!center.IsValid() || !target.IsValid() || center.IsZero() || target.IsZero()) return false;
    return center.Distance2D(target) <= kStormRadius + std::max(0.0f, targetRadius);
}

inline bool LightningReachable(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kLightningReach + std::max(0.0f, targetRadius);
}

inline bool SegmentCollision(const Vec3& start, const Vec3& end, const Vec3& target,
                             float width, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() || start.IsZero() || end.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) + std::max(0.0f, targetRadius);
}

inline bool UnsafeCommitBoundary(bool endpointWall, bool endpointTurret, int enemiesAtEndpoint,
                                 int maximumEnemies, bool lethal, bool defensive, bool fleeing) {
    if (lethal || defensive || fleeing) return false;
    if (endpointWall || endpointTurret) return true;
    return enemiesAtEndpoint > std::max(0, maximumEnemies);
}

inline bool MobilitySafe(bool endpointWall, bool endpointTurret, int enemiesAtEndpoint,
                         int maximumEnemies, bool defensive, bool fleeing) {
    if (fleeing) return false;
    return !UnsafeCommitBoundary(endpointWall, endpointTurret, enemiesAtEndpoint,
                                 maximumEnemies, false, defensive, false);
}

inline bool ResourceAvailable(float resource, float maximum, float reservePercent, float cost) {
    if (!std::isfinite(resource) || !std::isfinite(maximum) || !std::isfinite(reservePercent) ||
        !std::isfinite(cost) || maximum <= 0.0f || cost < 0.0f) return false;
    const float reserve = maximum * std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
    return resource >= cost && resource - cost >= reserve;
}

inline bool CooldownAvailable(bool runtimeReady, int elapsedMs, int minimumGapMs = 45) {
    return runtimeReady && elapsedMs >= std::max(0, minimumGapMs);
}

inline bool StormTargetValid(const Vec3& center, const Vec3& predictedTarget,
                             float targetRadius, bool projectileWallBlocked) {
    return !projectileWallBlocked && StormCovers(center, predictedTarget, targetRadius);
}

inline float QMaxHealthDamage(int rank, float targetMaxHealth, float bonusAttackDamage) {
    static constexpr std::array<float, 5> ratio{0.00035f, 0.00035f, 0.00035f, 0.00035f, 0.00035f};
    const int index = std::clamp(rank - 1, 0, 4);
    return std::max(0.0f, targetMaxHealth) * ratio[static_cast<std::size_t>(index)] *
        std::max(0.0f, bonusAttackDamage);
}

inline float StormPulseDamage(int rank, float abilityPower, float totalAttackDamage) {
    static constexpr std::array<float, 5> base{20.0f, 36.0f, 52.0f, 68.0f, 84.0f};
    const int index = std::clamp(rank - 1, 0, 4);
    return base[static_cast<std::size_t>(index)] + 0.35f * std::max(0.0f, abilityPower) +
        0.35f * std::max(0.0f, totalAttackDamage);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Udyr::Geometry
