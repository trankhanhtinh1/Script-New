#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kassadin::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 650.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1400.0f;
inline constexpr float kWRange = 125.0f;
inline constexpr float kERange = 600.0f;
inline constexpr float kEConeAngle = 39.0f;
inline constexpr float kEConeRadius = 350.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRRange = 500.0f;
inline constexpr float kRRadius = 270.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr int kForcePulseRequiredCharges = 6;
inline constexpr int kRiftwalkMaxStacks = 4;
inline constexpr int kRiftwalkStackDurationMs = 15000;
inline constexpr float kNullSphereShieldDurationSeconds = 1.5f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {35.0f, 65.0f, 95.0f, 125.0f, 155.0f}) +
        0.70f * std::max(0.0f, abilityPower);
}
inline constexpr float QMagicShield(int rank, float abilityPower) {
    return RankValue(rank, {50.0f, 80.0f, 110.0f, 140.0f, 170.0f}) +
        0.30f * std::max(0.0f, abilityPower);
}
inline constexpr float WActiveDamage(int rank, float abilityPower) {
    return RankValue(rank, {25.0f, 50.0f, 75.0f, 100.0f, 125.0f}) +
        0.80f * std::max(0.0f, abilityPower);
}
inline constexpr float WOnHitDamage(int rank, float abilityPower) {
    (void)rank;
    return 25.0f + 0.10f * std::max(0.0f, abilityPower);
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {40.0f, 70.0f, 100.0f, 130.0f, 160.0f}) +
        0.70f * std::max(0.0f, abilityPower);
}
inline constexpr float RBaseDamage(int rank, float abilityPower, float maximumMana) {
    return RankValue3(rank, {70.0f, 90.0f, 110.0f}) +
        0.50f * std::max(0.0f, abilityPower) +
        0.02f * std::max(0.0f, maximumMana);
}
inline constexpr float RStackBonus(int rank, float abilityPower, float maximumMana) {
    return RankValue3(rank, {35.0f, 45.0f, 55.0f}) +
        0.07f * std::max(0.0f, abilityPower) +
        0.01f * std::max(0.0f, maximumMana);
}
inline constexpr float RiftwalkDamage(int rank, float abilityPower, float maximumMana,
                                     int stacks) {
    const int safeStacks = std::clamp(stacks, 0, kRiftwalkMaxStacks);
    return RBaseDamage(rank, abilityPower, maximumMana) +
        safeStacks * RStackBonus(rank, abilityPower, maximumMana);
}
inline constexpr float RiftwalkBaseCost(float maximumMana) {
    return 40.0f + 0.02f * std::max(0.0f, maximumMana);
}
inline constexpr float RiftwalkCost(float maximumMana, int stacks) {
    const int safeStacks = std::clamp(stacks, 0, kRiftwalkMaxStacks);
    return RiftwalkBaseCost(maximumMana) +
        safeStacks * 0.01f * std::max(0.0f, maximumMana);
}
inline constexpr bool HasForcePulseCharges(int charges) {
    return charges >= kForcePulseRequiredCharges;
}
inline constexpr int AddForcePulseCast(int charges, int casts = 1) {
    return std::clamp(charges + std::max(0, casts), 0,
                       kForcePulseRequiredCharges);
}
inline constexpr int ConsumeForcePulseCharges(int charges) {
    return HasForcePulseCharges(charges) ? 0 : std::clamp(charges, 0,
                                                           kForcePulseRequiredCharges);
}
inline constexpr bool RiftwalkStacksActive(int stacks, int lastCastTick, int now) {
    return stacks > 0 && lastCastTick > 0 && now <= lastCastTick + kRiftwalkStackDurationMs;
}
inline constexpr int NextRiftwalkStacks(int stacks) {
    return std::clamp(stacks + 1, 0, kRiftwalkMaxStacks);
}
inline float ProjectileTravelSeconds(const Vec3& source, const Vec3& target,
                                     float delay = kQDelay,
                                     float speed = kQSpeed,
                                     float range = kQRange) {
    if (!source.IsValid() || !target.IsValid()) return 0.0f;
    const float distance = std::min(std::max(0.0f, range), source.Distance2D(target));
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}
inline bool ConeHits(const Vec3& origin, const Vec3& directionPoint,
                     const Vec3& target, float range = kERange,
                     float coneAngle = kEConeAngle, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !directionPoint.IsValid() || !target.IsValid()) return false;
    const Vec3 forward = Direction2D(origin, directionPoint);
    const Vec3 toTarget = Direction2D(origin, target);
    if (forward.IsZero() || toTarget.IsZero()) return false;
    const float distance = origin.Distance2D(target);
    if (distance > range + std::max(0.0f, targetRadius)) return false;
    const float dot = forward.x * toTarget.x + forward.z * toTarget.z;
    const float angle = std::acos(std::clamp(dot, -1.0f, 1.0f)) * 57.2957795f;
    return angle <= coneAngle * 0.5f + std::asin(std::clamp(
        std::max(0.0f, targetRadius) / std::max(1.0f, distance), -1.0f, 1.0f)) * 57.2957795f;
}
inline Vec3 ClampBlinkEndpoint(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid() || requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    return direction.IsZero() ? Vec3{} : origin + direction * std::min(kRRange, origin.Distance2D(requested));
}
struct BlinkSafetyContext {
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointTurret = false;
    bool OriginTurret = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    int RiftwalkStacks = 0;
    int ReserveStacks = 0;
};
inline bool SafeBlinkEndpoint(const BlinkSafetyContext& context) {
    if (!context.EndpointValid || context.EndpointWall) return false;
    if (context.EndpointTurret && !context.OriginTurret &&
        !context.Lethal && !context.Defensive && !context.Manual) return false;
    if (!context.Lethal && !context.Defensive && !context.Manual &&
        context.ReserveStacks > 0 &&
        context.RiftwalkStacks >= kRiftwalkMaxStacks - context.ReserveStacks + 1)
        return false;
    return context.Lethal || context.Defensive || context.Manual ||
        context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}
inline constexpr bool SafeRiftwalkResource(float currentMana, float maximumMana, int stacks,
                                           int reserveStacks = 1,
                                           float reserveManaPercent = 20.0f) {
    const int safeStacks = std::clamp(stacks, 0, kRiftwalkMaxStacks);
    if (reserveStacks > 0 && safeStacks >= kRiftwalkMaxStacks) return false;
    const float reserve = std::max(0.0f, maximumMana) *
        std::clamp(reserveManaPercent, 0.0f, 100.0f) / 100.0f;
    return currentMana + 0.5f >= RiftwalkCost(maximumMana, safeStacks) + reserve;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kassadin::Geometry
