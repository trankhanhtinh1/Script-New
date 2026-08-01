#pragma once

// Pure Graves shell, projectile and safety mechanics. Runtime objects, buffs,
// prediction, and event ownership remain in AIGravesController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Graves::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 925.0f;
inline constexpr float kQCastRange = 800.0f;
inline constexpr float kQWidth = 100.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 902.0f;
inline constexpr float kQExplosionRadius = 100.0f;
inline constexpr float kWRange = 950.0f;
inline constexpr float kWSmokeRadius = 225.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kWSpeed = 1650.0f;
inline constexpr float kWSmokeDurationSeconds = 4.0f;
inline constexpr float kEDashRange = 425.0f;
inline constexpr float kERadius = 20.0f;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRWidth = 100.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kRSpeed = 1400.0f;
inline constexpr float kRRecoilDistance = 400.0f;

inline float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline float QInitialDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {25.0f, 50.0f, 75.0f, 100.0f, 125.0f}) +
        0.65f * std::max(0.0f, bonusAttackDamage);
}

inline float QExplosionDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {35.0f, 80.0f, 125.0f, 170.0f, 215.0f}) +
        RankValue(rank, {0.40f, 0.55f, 0.70f, 0.85f, 1.0f}) *
            std::max(0.0f, bonusAttackDamage);
}

inline float QTotalDamage(int rank, float bonusAttackDamage, bool explosion) {
    const float initial = QInitialDamage(rank, bonusAttackDamage);
    return initial + (explosion ? QExplosionDamage(rank, bonusAttackDamage) : 0.0f);
}

inline float WImpactDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 110.0f, 160.0f, 210.0f, 260.0f}) +
        0.60f * std::max(0.0f, abilityPower);
}

inline float EArmorPerStack(int rank) {
    return RankValue(rank, {7.0f, 10.0f, 13.0f, 16.0f, 19.0f});
}

inline float EMagicResistancePerStack(int rank) {
    return 0.5f * EArmorPerStack(rank);
}

inline float RPrimaryDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {275.0f, 425.0f, 575.0f}) +
        1.50f * std::max(0.0f, bonusAttackDamage);
}

inline float RSecondaryDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {200.0f, 320.0f, 440.0f}) +
        1.20f * std::max(0.0f, bonusAttackDamage);
}

inline Vec3 ClampEndpoint(const Vec3& origin, const Vec3& requested, float range) {
    if (!origin.IsValid() || !requested.IsValid() || requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline Vec3 QExplosionEndpoint(const Vec3& origin, const Vec3& requested) {
    return ClampEndpoint(origin, requested, kQRange);
}

inline bool InSmoke(const Vec3& center, const Vec3& target, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kWSmokeRadius + std::max(0.0f, targetRadius);
}

inline bool IsInsideLine(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                         float targetRadius = 0.0f, float width = kQWidth) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= -0.001f && projection.T <= 1.001f &&
        projection.Distance <= width * 0.5f + std::max(0.0f, targetRadius);
}

inline bool IsFirstCollision(const Vec3& origin, const Vec3& endpoint,
                             const Vec3& target, const std::array<Vec3, 8>& blockers,
                             int blockerCount, float targetRadius = 0.0f,
                             float blockerRadius = 55.0f) {
    if (!IsInsideLine(origin, endpoint, target, targetRadius, kRWidth)) return false;
    const float targetT = origin.Distance2D(target);
    const int count = std::clamp(blockerCount, 0, static_cast<int>(blockers.size()));
    for (int i = 0; i < count; ++i) {
        if (!blockers[static_cast<std::size_t>(i)].IsValid()) continue;
        const auto projection = ProjectPointToSegment2D(blockers[static_cast<std::size_t>(i)], origin, endpoint);
        if (projection.T > 0.001f && origin.Distance2D(projection.Closest) + blockerRadius < targetT &&
            projection.Distance <= kRWidth * 0.5f + blockerRadius) return false;
    }
    return true;
}

inline Vec3 RecoilEndpoint(const Vec3& origin, const Vec3& aim, float distance = kRRecoilDistance) {
    if (!origin.IsValid() || !aim.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return {};
    return origin - direction * std::max(0.0f, distance);
}

struct AmmoState {
    int Shells = 2;
    bool Reloading = false;
    bool AttackWindingUp = false;
    bool AttackAvailable = true;
};

inline bool CanFire(const AmmoState& state, bool lethal = false) {
    return state.Shells > 0 && !state.Reloading &&
        (!state.AttackWindingUp || lethal);
}

inline bool NeedsReload(const AmmoState& state) {
    return state.Shells <= 0 || state.Reloading;
}

inline int ReconcileShells(bool ammoOneBuff, bool ammoTwoBuff, bool attackWindingUp) {
    if (ammoTwoBuff) return 2;
    if (ammoOneBuff) return 1;
    // No ammo buff is the reload state, unless a cast is already winding up.
    return attackWindingUp ? 1 : 0;
}

struct QGate {
    bool Ready = false;
    bool AmmoReady = false;
    bool PredictionAccepted = false;
    bool WallBlocked = false;
    bool TargetInLine = false;
    bool ExplosionReachable = false;
    bool Lethal = false;
    bool AttackWindingUp = false;
};

inline bool CanCastQ(const QGate& gate) {
    return gate.Ready && gate.AmmoReady && gate.PredictionAccepted &&
        !gate.WallBlocked && gate.TargetInLine && gate.ExplosionReachable &&
        (!gate.AttackWindingUp || gate.Lethal);
}

struct SmokeGate {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool WallBlocked = false;
    bool TargetInRange = false;
    bool AlreadySmoked = false;
    bool VisionRequired = false;
    bool VisionSafe = false;
    bool Committed = false;
};

inline bool CanCastSmoke(const SmokeGate& gate) {
    return gate.Ready && gate.PredictionAccepted && !gate.WallBlocked &&
        gate.TargetInRange && !gate.AlreadySmoked &&
        (!gate.VisionRequired || gate.VisionSafe || gate.Committed);
}

struct DashGate {
    bool Ready = false;
    bool DirectionValid = false;
    bool WallBlocked = false;
    bool TurretRisk = false;
    bool Hazard = false;
    bool Unsafe = false;
    bool AfterAttack = false;
    bool Reloading = false;
    bool Defensive = false;
};

inline bool CanDash(const DashGate& gate) {
    if (!gate.Ready || !gate.DirectionValid || gate.WallBlocked || gate.Hazard || gate.Unsafe) return false;
    if (gate.TurretRisk && !gate.Defensive) return false;
    return gate.Defensive || gate.AfterAttack || gate.Reloading;
}

struct RecoilGate {
    bool Ready = false;
    bool FirstCollision = false;
    bool PredictionAccepted = false;
    bool WallBlocked = false;
    bool TurretRisk = false;
    bool Unsafe = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool CanCastRecoil(const RecoilGate& gate) {
    if (!gate.Ready || !gate.FirstCollision || !gate.PredictionAccepted ||
        gate.WallBlocked || gate.TurretRisk || gate.Unsafe) return false;
    return gate.Lethal || gate.NearbyEnemies <= std::max(0, gate.MaximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Graves::Geometry
