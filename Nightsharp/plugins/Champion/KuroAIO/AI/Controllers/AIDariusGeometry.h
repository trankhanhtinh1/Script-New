#pragma once

// Pure Darius mechanics: ring/cone reach, Hemorrhage/Noxian Might state,
// damage and commit boundaries. Runtime target validity and casts live in the controller.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Darius::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kAaRange = 125.0f;
inline constexpr float kQRange = 425.0f;
inline constexpr float kQInnerRadius = 220.0f;
inline constexpr float kQOuterRadius = 425.0f;
inline constexpr float kQDelay = 0.75f;
inline constexpr float kQHealMissingPercent = 0.17f;
inline constexpr float kWRange = 175.0f;
inline constexpr float kWSlowPercent = 0.90f;
inline constexpr float kWSlowDurationSeconds = 1.0f;
inline constexpr float kERange = 535.0f;
inline constexpr float kEConeHalfAngleDegrees = 12.5f;
inline constexpr float kESlowPercent = 0.40f;
inline constexpr float kESlowDurationSeconds = 1.0f;
inline constexpr float kRRange = 460.0f;
inline constexpr int kMaximumHemorrhageStacks = 5;
inline constexpr int kHemorrhageDurationMs = 5000;
inline constexpr int kNoxianMightDurationMs = 5000;
inline constexpr float kRStackMultiplier = 0.20f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}

inline constexpr float QOuterBaseDamage(int rank) {
    return RankValue(rank, {50.0f, 80.0f, 110.0f, 140.0f, 170.0f});
}
inline constexpr float QTotalADRatio(int rank) {
    return RankValue(rank, {1.00f, 1.10f, 1.20f, 1.30f, 1.40f});
}
inline constexpr float QInnerBaseDamage(int rank) { return QOuterBaseDamage(rank) * 0.35f; }
inline constexpr float QOuterRawDamage(int rank, float totalAttackDamage) {
    return QOuterBaseDamage(rank) + QTotalADRatio(rank) * std::max(0.0f, totalAttackDamage);
}
inline constexpr float QInnerRawDamage(int rank, float totalAttackDamage) {
    return QInnerBaseDamage(rank) + QTotalADRatio(rank) * 0.35f *
        std::max(0.0f, totalAttackDamage);
}
inline constexpr float WRawDamage(int rank, float totalAttackDamage) {
    return RankValue(rank, {1.40f, 1.45f, 1.50f, 1.55f, 1.60f}) *
        std::max(0.0f, totalAttackDamage);
}
inline constexpr float EArmorPenetrationPercent(int rank) {
    return RankValue(rank, {20.0f, 25.0f, 30.0f, 35.0f, 40.0f});
}
inline constexpr float ERawDamage(int rank, float bonusAttackDamage) {
    (void)rank;
    return 0.60f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float EffectiveArmor(float armor, int rank) {
    return std::max(0.0f, armor * (1.0f - EArmorPenetrationPercent(rank) * 0.01f));
}
inline constexpr float RBaseDamage(int rank) { return RankValue3(rank, {125.0f, 250.0f, 375.0f}); }
inline constexpr float RRawDamage(int rank, float bonusAttackDamage, int stacks) {
    const int safeStacks = std::clamp(stacks, 0, kMaximumHemorrhageStacks);
    return (RBaseDamage(rank) + 0.75f * std::max(0.0f, bonusAttackDamage)) *
        (1.0f + kRStackMultiplier * static_cast<float>(safeStacks));
}
inline constexpr float NoxianMightBonusAttackDamage(int championLevel) {
    const int level = std::clamp(championLevel, 1, 18);
    if (level <= 10) return 30.0f + 5.0f * static_cast<float>(level - 1);
    if (level <= 13) return 75.0f + 10.0f * static_cast<float>(level - 10);
    return 105.0f + 25.0f * static_cast<float>(level - 13);
}
inline constexpr int NextHemorrhageStacks(int current) {
    return std::clamp(current, 0, kMaximumHemorrhageStacks - 1) + 1;
}
inline constexpr bool NoxianMightReady(int stacks, bool mightBuff) {
    return mightBuff || stacks >= kMaximumHemorrhageStacks;
}
inline constexpr float QHealAmount(float currentHealth, float maxHealth) {
    const float missing = std::max(0.0f, maxHealth - currentHealth);
    return missing * kQHealMissingPercent;
}

inline bool InQOuterEdge(const Vec3& origin, const Vec3& target,
                         float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    const float distance = origin.Distance2D(target);
    const float radius = std::max(0.0f, targetRadius);
    return distance + radius >= kQInnerRadius && distance - radius <= kQOuterRadius;
}
inline bool InQInnerEdge(const Vec3& origin, const Vec3& target,
                         float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) + std::max(0.0f, targetRadius) < kQInnerRadius;
}
inline bool InRange(const Vec3& origin, const Vec3& target, float range,
                    float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() && !target.IsZero() &&
        origin.Distance2D(target) <= range + std::max(0.0f, targetRadius);
}
inline bool InAutoRange(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    return InRange(origin, target, kAaRange, targetRadius);
}
inline bool InECone(const Vec3& origin, const Vec3& aim, const Vec3& target,
                    float targetRadius = 0.0f) {
    if (!InRange(origin, target, kERange, targetRadius) ||
        !origin.IsValid() || !aim.IsValid() || origin.IsZero() || aim.IsZero()) return false;
    const Vec3 forward = Direction2D(origin, aim);
    const Vec3 toTarget = Direction2D(origin, target);
    if (forward.IsZero() || toTarget.IsZero()) return false;
    const float cosine = std::clamp(forward.Dot(toTarget), -1.0f, 1.0f);
    const float halfAngle = kEConeHalfAngleDegrees * 3.14159265358979323846f / 180.0f;
    return std::acos(cosine) <= halfAngle + 0.02f;
}
inline bool RReachable(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    return InRange(origin, target, kRRange, targetRadius);
}
inline bool SafeCommit(bool endpointWall, bool endpointTurret, int enemies,
                       int maximumEnemies, bool lethal, bool defensive, bool fleeing) {
    if (endpointWall || endpointTurret) return lethal || defensive || fleeing;
    if (lethal || defensive || fleeing) return true;
    return enemies <= std::max(0, maximumEnemies);
}
inline bool ResourceAvailable(float resource, float maximum, float reservePercent, float cost) {
    if (!std::isfinite(resource) || !std::isfinite(maximum) || maximum <= 0.0f ||
        !std::isfinite(reservePercent) || !std::isfinite(cost) || cost < 0.0f) return false;
    const float reserve = maximum * std::clamp(reservePercent, 0.0f, 100.0f) * 0.01f;
    return resource >= cost && resource - cost >= reserve;
}
inline bool CooldownAvailable(bool ready, int elapsedMs, int gapMs = 45) {
    return ready && elapsedMs >= std::max(0, gapMs);
}
inline bool RExecute(float damage, float health, float shield) {
    return damage >= std::max(0.0f, health) + std::max(0.0f, shield);
}
inline int TargetPriority(int stacks, bool execute, bool selected, float distance) {
    return (execute ? 10000 : 0) + (selected ? 1000 : 0) +
        std::clamp(stacks, 0, kMaximumHemorrhageStacks) * 250 -
        static_cast<int>(std::max(0.0f, distance));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Darius::Geometry
