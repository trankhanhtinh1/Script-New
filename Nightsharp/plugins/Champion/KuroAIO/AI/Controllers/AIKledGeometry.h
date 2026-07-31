#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Kled::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kCourageMax = 100.0f;
inline constexpr float kQRange = 800.0f;
inline constexpr float kQDisplayRange = 750.0f;
inline constexpr float kQWidth = 45.0f;
inline constexpr float kQSpeed = 1600.0f;
inline constexpr float kQTetherRange = 700.0f;
inline constexpr float kQTetherSeconds = 1.75f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kERange = 550.0f;
inline constexpr float kERecastRange = 700.0f;
inline constexpr float kEDashSpeed = 600.0f;
inline constexpr float kEPassthroughDistance = 350.0f;
inline constexpr float kERadius = 100.0f;
inline constexpr float kERecastSeconds = 3.0f;
inline constexpr float kRMaxRange = 6000.0f;
inline constexpr float kRZoneRadius = 500.0f;
inline constexpr float kRMaxChargeSeconds = 3.0f;
inline constexpr int kRChargeGraceMs = 180;
inline constexpr int kWAttackCount = 4;
inline constexpr int kWActiveMs = 4000;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid() && !point.IsZero();
}

inline float ClampCourage(float courage) {
    if (!std::isfinite(courage)) return 0.0f;
    return std::clamp(courage, 0.0f, kCourageMax);
}

inline bool CourageCanRemount(float courage, bool mounted,
                              bool passiveOnCooldown = false) {
    return !mounted && !passiveOnCooldown && ClampCourage(courage) >= kCourageMax;
}

enum class MountState : std::uint8_t {
    Mounted,
    Dismounted,
    Remounting,
};

struct MountObservation {
    bool MountedBuff = false;
    bool DismountedBuff = false;
    bool RemountBuff = false;
    bool PassiveCooldown = false;
    float Courage = 0.0f;
    MountState Previous = MountState::Mounted;
};

inline MountState ResolveMountState(const MountObservation& observation) {
    if (observation.RemountBuff) return MountState::Remounting;
    if (observation.MountedBuff && !observation.DismountedBuff) {
        return MountState::Mounted;
    }
    if (CourageCanRemount(observation.Courage,
                          observation.MountedBuff,
                          observation.PassiveCooldown)) {
        return MountState::Remounting;
    }
    if (observation.DismountedBuff || !observation.MountedBuff) {
        return MountState::Dismounted;
    }
    return observation.Previous;
}

inline bool QLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) ||
        !FinitePoint(target)) return false;
    if (origin.Distance2D(endpoint) > kQRange + std::max(0.0f, targetRadius)) {
        return false;
    }
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kQWidth * 0.5f +
               std::clamp(targetRadius, 0.0f, 180.0f);
}

inline bool QChainCanHold(const Vec3& caster, const Vec3& target,
                          float elapsedSeconds,
                          float leashRange = kQTetherRange) {
    return FinitePoint(caster) && FinitePoint(target) &&
        std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= kQTetherSeconds &&
        caster.Distance2D(target) <= std::max(0.0f, leashRange);
}

inline bool QChainWillPop(const Vec3& caster, const Vec3& target,
                          float elapsedSeconds) {
    return QChainCanHold(caster, target, elapsedSeconds) &&
        elapsedSeconds >= kQTetherSeconds - 0.15f;
}

inline float QTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return kQDelay + distance / kQSpeed;
}

inline int AdvanceWAttack(int stage, bool attackLanded) {
    const int current = std::clamp(stage, 0, kWAttackCount);
    if (!attackLanded) return current;
    return current >= kWAttackCount ? 0 : current + 1;
}

inline bool WFourthHitReady(int stage, bool active) {
    return active && std::clamp(stage, 0, kWAttackCount) == kWAttackCount - 1;
}

inline bool PreserveWCadence(int stage, bool active, bool lethalResponse,
                             bool reactive) {
    return active && stage < kWAttackCount && !lethalResponse && !reactive;
}

inline float QDamageRaw(int rank, float totalAttackDamage, bool mounted,
                        bool tetherPop = false) {
    static constexpr std::array<float, 6> mountedBase{0, 5, 30, 55, 80, 105};
    static constexpr std::array<float, 6> dismountedBase{0, 20, 35, 50, 65, 80};
    const float base = RankValue(mounted ? mountedBase : dismountedBase, rank);
    const float ratio = mounted ? 0.60f : 0.65f;
    return (base + ratio * std::max(0.0f, totalAttackDamage)) *
        (tetherPop ? 2.0f : 1.0f);
}

inline float WFourthHitRaw(int rank, float targetMaximumHealth,
                           float totalAttackDamage) {
    static constexpr std::array<float, 6> flat{0, 10, 20, 30, 40, 50};
    static constexpr std::array<float, 6> maxHealthPercent{
        0.0f, 0.04f, 0.045f, 0.05f, 0.055f, 0.06f};
    return RankValue(flat, rank) +
        RankValue(maxHealthPercent, rank) * std::max(0.0f, targetMaximumHealth) +
        0.02f * std::max(0.0f, totalAttackDamage);
}

inline float EDamageRaw(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> base{0, 10, 35, 60, 85, 110};
    return RankValue(base, rank) + 0.55f * std::max(0.0f, totalAttackDamage);
}

inline float RDamageRaw(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> base{0, 100, 200, 300};
    return RankValue(base, std::clamp(rank, 0, 3)) +
        1.50f * std::max(0.0f, bonusAttackDamage);
}

inline bool DashEndpointValid(const Vec3& origin, const Vec3& endpoint,
                              bool wall, bool enemyTurret, int enemies,
                              int maxEnemies, bool lethal,
                              float maxRange = kERange) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || wall ||
        enemies < 0 || maxEnemies < 0) return false;
    if (origin.Distance2D(endpoint) > std::max(0.0f, maxRange)) return false;
    if (enemyTurret && !lethal) return false;
    return lethal || enemies <= maxEnemies;
}

inline bool CanRecastE(const Vec3& caster, const Vec3& target,
                       bool marked, float elapsedSeconds,
                       float range = kERecastRange) {
    return marked && FinitePoint(caster) && FinitePoint(target) &&
        std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= kERecastSeconds &&
        caster.Distance2D(target) <= std::max(0.0f, range);
}

inline bool SafeChargeEndpoint(const Vec3& endpoint, bool wall, bool turret,
                               int enemies, int allies, int maxEnemies,
                               bool hasTarget, bool lethal) {
    if (!FinitePoint(endpoint) || wall || enemies < 0 || allies < 0 ||
        maxEnemies < 0) return false;
    if (turret && !lethal) return false;
    if (!lethal && enemies > maxEnemies) return false;
    return hasTarget || allies > 0;
}

inline bool ChargeEndpointWithin(const Vec3& origin, const Vec3& endpoint,
                                 float range = kRMaxRange) {
    return FinitePoint(origin) && FinitePoint(endpoint) &&
        origin.Distance2D(endpoint) <= std::max(0.0f, range);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kled::Geometry
