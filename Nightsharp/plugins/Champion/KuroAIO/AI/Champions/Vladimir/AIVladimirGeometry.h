#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Vladimir::Geometry {

using Vec3 = ::Vec3;

inline constexpr float kQRange = 600.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1400.0f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWDurationSeconds = 2.0f;
inline constexpr float kERange = 600.0f;
inline constexpr float kERadius = 550.0f;
inline constexpr float kEChargeSeconds = 1.0f;
inline constexpr float kRRange = 625.0f;
inline constexpr float kRRadius = 375.0f;
inline constexpr float kRMarkSeconds = 4.0f;
inline constexpr float kQEmpoweredSeconds = 2.5f;
inline constexpr float kMinimumHealthAfterCastPercent = 12.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr float QDamage(int rank, float abilityPower, bool empowered) {
    const float base = RankValue(rank, {80.0f, 100.0f, 120.0f, 140.0f, 160.0f}) +
        std::max(0.0f, abilityPower) * 0.60f;
    return empowered ? base * 1.85f : base;
}

inline constexpr float QHeal(int rank, float abilityPower, bool empowered,
                             float missingHealthPercent = 0.0f) {
    const float base = RankValue(rank, {20.0f, 25.0f, 30.0f, 35.0f, 40.0f}) +
        std::max(0.0f, abilityPower) * 0.35f;
    if (!empowered) return base;
    const float bonus = 5.0f + std::max(0.0f, abilityPower) * 0.04f;
    return base + bonus + std::clamp(missingHealthPercent, 0.0f, 100.0f) * 0.01f *
        (30.0f + std::max(0.0f, abilityPower) * 0.12f);
}

inline constexpr float EDamage(int rank, float abilityPower, float chargeSeconds) {
    const float t = std::clamp(chargeSeconds / kEChargeSeconds, 0.0f, 1.0f);
    const float minimum = RankValue(rank, {30.0f, 60.0f, 90.0f, 120.0f, 150.0f}) +
        std::max(0.0f, abilityPower) * 0.35f;
    const float maximum = RankValue(rank, {60.0f, 120.0f, 180.0f, 240.0f, 300.0f}) +
        std::max(0.0f, abilityPower) * 0.80f;
    return minimum + (maximum - minimum) * t;
}

inline constexpr float EHealthCostPercent(float chargeSeconds) {
    return 2.5f + 5.5f * std::clamp(chargeSeconds / kEChargeSeconds, 0.0f, 1.0f);
}

inline constexpr float RInitialDamage(int rank, float abilityPower) {
    return RankValue(rank, {150.0f, 250.0f, 350.0f, 350.0f, 350.0f}) +
        std::max(0.0f, abilityPower) * 0.70f;
}

inline constexpr float RAmplifiedDamage(float damage, bool marked) {
    return std::max(0.0f, damage) * (marked ? 1.10f : 1.0f);
}

inline constexpr float RHeal(int rank, float abilityPower, int markedTargets) {
    return (RInitialDamage(rank, abilityPower) * 0.40f) *
        static_cast<float>(std::max(0, markedTargets));
}

inline constexpr bool HealthTradeSafe(float currentHealth, float maximumHealth,
                                      float costPercent,
                                      float minimumHealthPercent =
                                          kMinimumHealthAfterCastPercent) {
    if (maximumHealth <= 0.0f || currentHealth <= 0.0f) return false;
    const float after = currentHealth - maximumHealth *
        std::clamp(costPercent, 0.0f, 100.0f) * 0.01f;
    return after >= maximumHealth * std::clamp(minimumHealthPercent, 0.0f, 100.0f) *
        0.01f;
}

inline constexpr bool CanUsePool(float playerHealthPercent,
                                 bool incomingHardCrowdControl,
                                 bool lethalThreat,
                                 bool flee) {
    return incomingHardCrowdControl || lethalThreat || flee ||
        playerHealthPercent <= 26.0f;
}

inline bool CircleHits(const Vec3& center, const Vec3& target,
                       float radius, float targetRadius = 0.0f) {
    if (!center.IsValid() || !target.IsValid() || center.IsZero() || target.IsZero()) {
        return false;
    }
    const float reach = std::max(0.0f, radius) +
        std::clamp(targetRadius, 0.0f, 250.0f);
    return center.DistanceSqr2D(target) <= reach * reach;
}

inline bool QReachable(const Vec3& origin, const Vec3& target,
                       float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) {
        return false;
    }
    return origin.Distance2D(target) <= kQRange + std::max(0.0f, targetRadius);
}

inline bool EReleaseHits(const Vec3& center, const Vec3& target,
                         float chargeSeconds,
                         float targetRadius = 0.0f) {
    return chargeSeconds >= 0.0f && CircleHits(center, target, kERadius, targetRadius);
}

inline bool RPlacementHits(const Vec3& center, const Vec3& target,
                           float targetRadius = 0.0f) {
    return CircleHits(center, target, kRRadius, targetRadius);
}

inline constexpr int MarkedCount(int first, int second, int third, int fourth) {
    return (first > 0 ? 1 : 0) + (second > 0 ? 1 : 0) +
        (third > 0 ? 1 : 0) + (fourth > 0 ? 1 : 0);
}

struct VladimirUltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetInCircle = false;
    bool TargetLow = false;
    bool MultiTarget = false;
    bool PlayerSafe = false;
};

inline constexpr bool ShouldCastHemoplague(
    const VladimirUltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.TargetInCircle ||
        !context.PlayerSafe) return false;
    return context.TargetLow || context.MultiTarget;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Vladimir::Geometry
