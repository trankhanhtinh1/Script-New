#pragma once

// Pure Smolder mechanics.  Runtime target selection, prediction, terrain and
// threat telemetry stay in AISmolderController.h; this header owns only
// deterministic stack, damage, reach and endpoint safety decisions.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Smolder::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr int kQFirstEvolutionStacks = 25;
inline constexpr int kQSecondEvolutionStacks = 125;
inline constexpr int kQFinalEvolutionStacks = 225;
inline constexpr float kQRange = 925.0f;
inline constexpr float kQWidth = 100.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1300.0f;
inline constexpr float kQExplosionSmallRadius = 225.0f;
inline constexpr float kQExplosionLargeRadius = 300.0f;
inline constexpr float kWRange = 1100.0f;
inline constexpr float kWWidth = 100.0f;
inline constexpr float kWDelay = 0.25f;
inline constexpr float kWSpeed = 1200.0f;
inline constexpr float kWExplosionRadius = 125.0f;
inline constexpr float kERange = 1200.0f;
inline constexpr float kEFlightSeconds = 1.25f;
inline constexpr float kEMinEndpointDistance = 75.0f;
inline constexpr float kRRange = 1200.0f;
inline constexpr float kRDelay = 1.0f;
inline constexpr float kRWidth = 350.0f;
inline constexpr float kRCenterMultiplier = 1.50f;

inline int ClampStacks(int stacks) { return std::clamp(stacks, 0, 9999); }

enum class QEvolution {
    Base,
    Explosion,
    LargeExplosion,
    TripleMissile,
};

inline constexpr QEvolution QEvolutionFor(int stacks) {
    const int value = stacks < 0 ? 0 : stacks;
    if (value >= kQFinalEvolutionStacks) return QEvolution::TripleMissile;
    if (value >= kQSecondEvolutionStacks) return QEvolution::LargeExplosion;
    if (value >= kQFirstEvolutionStacks) return QEvolution::Explosion;
    return QEvolution::Base;
}
inline constexpr bool QHasExplosion(int stacks) {
    return QEvolutionFor(stacks) != QEvolution::Base;
}
inline constexpr bool QHasLargeExplosion(int stacks) {
    return QEvolutionFor(stacks) == QEvolution::LargeExplosion ||
           QEvolutionFor(stacks) == QEvolution::TripleMissile;
}
inline constexpr int QMissileCount(int stacks) {
    return QEvolutionFor(stacks) == QEvolution::TripleMissile ? 3 : 1;
}
inline constexpr float QExplosionRadius(int stacks) {
    return QHasLargeExplosion(stacks) ? kQExplosionLargeRadius
                                      : kQExplosionSmallRadius;
}
inline constexpr int StackGainForHit(bool champion, bool largeMinion,
                                     bool epicMonster) {
    // Champion spells and attacks use one Dragon Practice stack.  Large
    // minions/epic monsters also award one; ordinary units are deliberately
    // rejected here so a runtime caller cannot fabricate passive progress.
    return (champion || largeMinion || epicMonster) ? 1 : 0;
}

inline float QRawDamage(int rank, float totalAttackDamage, int stacks,
                        float targetMaximumHealth = 0.0f) {
    static constexpr std::array<float, 6> base =
        {0.0f, 15.0f, 25.0f, 35.0f, 45.0f, 55.0f};
    const int r = std::clamp(rank, 0, 5);
    const float stackHealth = std::max(0.0f, targetMaximumHealth) *
        static_cast<float>(ClampStacks(stacks)) * 0.003f;
    return base[static_cast<std::size_t>(r)] +
           std::max(0.0f, totalAttackDamage) + stackHealth;
}
inline float QExplosionRawDamage(int rank, float totalAttackDamage, int stacks,
                                 float targetMaximumHealth = 0.0f) {
    if (!QHasExplosion(stacks)) return 0.0f;
    return QRawDamage(rank, totalAttackDamage, stacks,
                      targetMaximumHealth) *
           (QHasLargeExplosion(stacks) ? 0.60f : 0.35f);
}
inline float WRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base =
        {0.0f, 45.0f, 65.0f, 85.0f, 105.0f, 125.0f};
    static constexpr std::array<float, 6> ratio =
        {0.0f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] +
           ratio[static_cast<std::size_t>(r)] * std::max(0.0f, bonusAttackDamage);
}
inline float WExplosionRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base =
        {0.0f, 25.0f, 40.0f, 55.0f, 70.0f, 85.0f};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] +
           0.30f * std::max(0.0f, bonusAttackDamage);
}
inline float ERawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base =
        {0.0f, 75.0f, 100.0f, 125.0f, 150.0f, 175.0f};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] +
           0.80f * std::max(0.0f, bonusAttackDamage);
}
inline float RRawDamage(int rank, float bonusAttackDamage, bool center) {
    static constexpr std::array<float, 4> base =
        {0.0f, 200.0f, 300.0f, 400.0f};
    const int r = std::clamp(rank, 0, 3);
    const float raw = base[static_cast<std::size_t>(r)] +
        1.10f * std::max(0.0f, bonusAttackDamage);
    return center ? raw * kRCenterMultiplier : raw;
}
inline bool IsExecute(int rank, float bonusAttackDamage, bool center,
                      float targetHealth, float targetShield = 0.0f) {
    return RRawDamage(rank, bonusAttackDamage, center) >=
           std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float range, float width, float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || !origin.IsValid() || !target.IsValid()) return false;
    const Vec3 end = origin + direction * std::max(0.0f, range);
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, width) * 0.5f +
               std::max(0.0f, targetRadius);
}
inline bool AreaContains(const Vec3& center, const Vec3& target, float radius,
                         float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() && !center.IsZero() &&
           center.Distance2D(target) <= std::max(0.0f, radius) +
               std::max(0.0f, targetRadius);
}
inline Vec3 ClampFlightEndpoint(const Vec3& origin, const Vec3& requested,
                                float range = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

struct FlightContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointUnderTurret = false;
    bool EndpointThreatened = false;
    bool CursorAgrees = false;
    bool Defensive = false;
    bool Lethal = false;
    bool Manual = false;
    float Distance = 0.0f;
    float MaximumRange = kERange;
};
inline bool ShouldTakeFlight(const FlightContext& context) {
    if (!context.Ready || !context.EndpointValid || context.EndpointWall ||
        context.EndpointUnderTurret || context.EndpointThreatened ||
        !context.CursorAgrees || context.Distance < kEMinEndpointDistance ||
        context.Distance > context.MaximumRange + 0.01f) return false;
    return context.Defensive || context.Lethal || context.Manual;
}

struct RContext {
    bool Ready = false;
    bool PredictionHits = false;
    bool ProjectileBlocked = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int PredictedTargets = 0;
    int MinimumTargets = 2;
};
inline bool ShouldCastR(const RContext& context) {
    if (!context.Ready || !context.PredictionHits || context.ProjectileBlocked) return false;
    return context.Manual || context.Defensive || context.Lethal ||
           context.PredictedTargets >= std::max(1, context.MinimumTargets);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Smolder::Geometry
