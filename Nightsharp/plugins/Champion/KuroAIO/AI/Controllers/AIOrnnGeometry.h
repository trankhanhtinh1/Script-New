#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Ornn::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = SharedGeometry::Vec3;

inline constexpr float kQRange = 800.0f;
inline constexpr float kQWidth = 65.0f;
inline constexpr float kQDelay = 0.30f;
inline constexpr float kPillarDelay = 1.125f;
inline constexpr float kPillarDuration = 4.0f;
inline constexpr float kWLength = 500.0f;
inline constexpr float kWFinalLength = 560.0f;
inline constexpr float kWHalfWidth = 87.5f;
inline constexpr float kWDuration = 0.75f;
inline constexpr float kERange = 650.0f;
inline constexpr float kESpeed = 1600.0f;
inline constexpr float kEDashRadius = 175.0f;
inline constexpr float kEShockwaveRadius = 360.0f;
inline constexpr float kRRange = 2500.0f;
inline constexpr float kRWidth = 120.0f;
inline constexpr float kRMaxSpeed = 1200.0f;

inline constexpr float RankValue(int rank,
                                 const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float QRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {20.0f, 45.0f, 70.0f, 95.0f, 120.0f}) +
           1.10f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float WMaximumHealthPercent(int rank) {
    return RankValue(rank, {0.12f, 0.13f, 0.14f, 0.15f, 0.16f});
}
inline constexpr float WRawDamage(int rank, float targetMaximumHealth) {
    return WMaximumHealthPercent(rank) * std::max(0.0f, targetMaximumHealth);
}
inline constexpr float BrittleMaximumHealthPercent(int level) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return 0.09f + 0.08f * t;
}
inline constexpr float BrittleRawDamage(int level, float targetMaximumHealth) {
    return BrittleMaximumHealthPercent(level) *
           std::max(0.0f, targetMaximumHealth);
}
inline constexpr float ERawDamage(int rank,
                                  float bonusArmor,
                                  float bonusMagicResistance) {
    return RankValue(rank, {80.0f, 125.0f, 170.0f, 215.0f, 260.0f}) +
           0.40f * std::max(0.0f, bonusArmor) +
           0.40f * std::max(0.0f, bonusMagicResistance);
}
inline constexpr float RRawDamagePerPass(int rank, float abilityPower) {
    return RankValue(rank, {125.0f, 175.0f, 225.0f}) +
           0.20f * std::max(0.0f, abilityPower);
}

inline bool SegmentHits(const Vec3& start,
                        const Vec3& end,
                        const Vec3& target,
                        float halfWidth,
                        float targetRadius = 0.0f) {
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
           std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
inline bool BreathHits(const Vec3& origin,
                       const Vec3& aim,
                       const Vec3& target,
                       float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * kWFinalLength;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kWHalfWidth + std::max(0.0f, targetRadius);
}
inline Vec3 ClampDash(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}
inline bool WallImpactCanHit(const Vec3& origin,
                             const Vec3& wallImpact,
                             const Vec3& target,
                             float targetRadius = 0.0f) {
    if (origin.IsZero() || wallImpact.IsZero() || target.IsZero() ||
        origin.Distance2D(wallImpact) > kERange + 0.01f) return false;
    return target.Distance2D(wallImpact) <=
           kEShockwaveRadius + std::max(0.0f, targetRadius);
}

struct PillarState {
    Vec3 Position{};
    int SpawnTick = 0;
    int ExpireTick = 0;
};
inline void RecordPillar(PillarState& pillar,
                         const Vec3& position,
                         int castTick) {
    pillar.Position = position;
    pillar.SpawnTick = castTick + static_cast<int>(kPillarDelay * 1000.0f);
    pillar.ExpireTick = pillar.SpawnTick +
        static_cast<int>(kPillarDuration * 1000.0f);
}
inline bool PillarActive(const PillarState& pillar, int now) {
    return !pillar.Position.IsZero() && now >= pillar.SpawnTick &&
           now <= pillar.ExpireTick;
}

struct DashContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool WallImpact = false;
    bool TargetInShockwave = false;
    bool EndpointUnderNewTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
    bool Lethal = false;
};
inline bool ShouldSearingCharge(const DashContext& context) {
    if (!context.Ready || !context.EndpointValid || !context.WallImpact ||
        !context.TargetInShockwave || context.EndpointUnderNewTurret) return false;
    return context.Defensive || context.Lethal ||
           context.EnemiesAtEndpoint <= context.MaximumEnemies;
}

enum class UltimateStage { Idle, Calling, RecastReady };
struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int PredictedHits = 0;
    int MinimumHits = 2;
};
inline bool ShouldCallRam(const UltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.PredictionHits ||
        context.ProjectileWall) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive &&
        !context.Manual) return false;
    return context.Lethal || context.Defensive || context.Manual ||
           context.PredictedHits >= std::max(1, context.MinimumHits);
}
inline bool ShouldHeadbuttRam(const UltimateContext& context,
                             bool ramObservable,
                             bool headingTowardContact) {
    return ramObservable && headingTowardContact && ShouldCallRam(context);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
    bool ManualOwnership = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ornn::Geometry
