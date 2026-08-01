#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Mel::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 950.0f;
inline constexpr float kQHalfWidth = 140.0f;
inline constexpr float kQDelay = 0.35f;
inline constexpr float kQSpeed = 1500.0f;
inline constexpr float kWRange = 250.0f;
inline constexpr float kWReflectSeconds = 0.75f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kEHalfWidth = 70.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kESpeed = 1100.0f;
inline constexpr float kRRange = 25000.0f;
inline constexpr int kPassiveMaxMarks = 999;
inline constexpr int kPassiveMarkDurationMs = 5000;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {35.0f, 60.0f, 85.0f, 110.0f, 135.0f}) +
        std::max(0.0f, abilityPower) * 0.55f;
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 105.0f, 150.0f, 195.0f, 240.0f}) +
        std::max(0.0f, abilityPower) * 0.70f;
}
inline constexpr float OverwhelmRawDamage(int rank, float abilityPower,
                                          int marks) {
    return RankValue(rank, {50.0f, 60.0f, 70.0f, 80.0f, 90.0f}) +
        std::max(0.0f, abilityPower) * 0.10f +
        std::max(0, marks) *
            (RankValue(rank, {2.0f, 3.0f, 4.0f, 5.0f, 6.0f}) +
             std::max(0.0f, abilityPower) * 0.0075f);
}
inline constexpr float RRawDamage(int rank, float abilityPower, int marks) {
    return RankValue(rank, {50.0f, 125.0f, 200.0f, 275.0f, 350.0f}) +
        std::max(0.0f, abilityPower) * 0.30f +
        std::max(0, marks) *
            (RankValue(rank, {1.0f, 4.0f, 7.0f, 10.0f, 13.0f}) +
             std::max(0.0f, abilityPower) * 0.04f);
}

inline constexpr bool CanExecute(float health, float shields, int rank,
                                 float abilityPower, int marks) {
    return marks > 0 && std::max(0.0f, health) + std::max(0.0f, shields) <=
        OverwhelmRawDamage(rank, abilityPower, marks);
}
inline constexpr int ClampMarks(int marks) {
    return std::clamp(marks, 0, kPassiveMaxMarks);
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float halfWidth, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid() ||
        origin.IsZero() || aim.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, aim);
    return projection.T >= -0.01f && projection.T <= 1.01f &&
        projection.Distance <= std::max(0.0f, halfWidth) +
            std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool QVolleyHits(const Vec3& origin, const Vec3& aim,
                        const Vec3& target, float targetRadius = 0.0f) {
    if (origin.Distance2D(aim) > kQRange + std::max(0.0f, targetRadius)) return false;
    return LineHits(origin, aim, target, kQHalfWidth, targetRadius);
}

inline bool ERootHits(const Vec3& origin, const Vec3& aim,
                      const Vec3& target, float targetRadius = 0.0f) {
    if (origin.Distance2D(aim) > kERange + std::max(0.0f, targetRadius)) return false;
    return LineHits(origin, aim, target, kEHalfWidth, targetRadius);
}

inline float ProjectileImpactSeconds(float distance, float delay, float speed,
                                     float range) {
    return std::max(0.0f, delay) +
        std::min(std::max(0.0f, distance), std::max(0.0f, range)) /
            std::max(1.0f, speed);
}

inline bool ReflectionWindowOpen(int castTick, int nowTick) {
    return castTick > 0 && nowTick >= castTick &&
        nowTick <= castTick + static_cast<int>(kWReflectSeconds * 1000.0f);
}

struct ProjectileContext {
    bool Valid = false;
    bool TargetsPlayer = false;
    bool CrossesPlayer = false;
    bool HardCrowdControl = false;
    bool Lethal = false;
    bool Manual = false;
};
inline bool ShouldReflect(const ProjectileContext& context) {
    return context.Valid && (context.TargetsPlayer || context.CrossesPlayer) &&
        (context.HardCrowdControl || context.Lethal || context.Manual);
}

struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool HasMarks = false;
    bool Execute = false;
    bool Defensive = false;
    bool Manual = false;
    int MarkedTargets = 0;
    int MinimumTargets = 1;
};
inline bool ShouldCastUltimate(const UltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.HasMarks) return false;
    return context.Execute || context.Defensive || context.Manual ||
        context.MarkedTargets >= std::max(1, context.MinimumTargets);
}

struct AutomaticContext {
    bool ManualOwnership = false;
    bool IncomingProjectile = false;
    bool IncomingHardCC = false;
    bool Execute = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership &&
        (context.IncomingProjectile || context.IncomingHardCC || context.Execute);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Mel::Geometry
