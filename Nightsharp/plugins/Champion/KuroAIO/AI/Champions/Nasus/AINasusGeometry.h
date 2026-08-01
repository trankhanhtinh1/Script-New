#pragma once

// Deterministic Nasus mechanics. Runtime prediction, target selection,
// cooldowns and event reconciliation remain in AINasusController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Nasus::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::RankValue;
using Vec3 = ::Vec3;

inline constexpr float kQCastRange = 255.0f;
inline constexpr float kQBonusRange = 50.0f;
inline constexpr float kQHitRadius = 210.0f;
inline constexpr float kWRange = 700.0f;
inline constexpr float kWDurationSeconds = 5.0f;
inline constexpr float kERange = 650.0f;
inline constexpr float kERadius = 380.0f;
inline constexpr float kEDurationSeconds = 5.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRRadius = 400.0f;
inline constexpr float kRDurationSeconds = 15.0f;
inline constexpr float kStackBasic = 3.0f;
inline constexpr float kStackBig = 12.0f;

inline float ClampStacks(float stacks) {
    return std::max(0.0f, std::isfinite(stacks) ? stacks : 0.0f);
}

inline float QBonusDamage(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 20.0f, 40.0f, 60.0f,
                                           80.0f, 100.0f, 120.0f}, rank);
}

inline float QRawDamage(int rank, float totalAttackDamage, float stacks) {
    return QBonusDamage(rank) + std::max(0.0f, totalAttackDamage) + ClampStacks(stacks);
}

inline float QCooldownSeconds(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 8.5f, 7.5f, 6.5f, 5.5f, 4.5f, 3.5f}, rank);
}

enum class StackTarget : std::uint8_t { SmallMinion, LargeMinion, SiegeMinion,
                                         JungleMonster, EpicMonster, Champion };

inline float StackReward(StackTarget target) {
    switch (target) {
    case StackTarget::LargeMinion:
    case StackTarget::SiegeMinion:
    case StackTarget::EpicMonster:
    case StackTarget::Champion:
        return kStackBig;
    default:
        return kStackBasic;
    }
}

inline bool QLastHitSafe(float qDamage, float health, float shield = 0.0f,
                         float safetyMargin = 1.0f) {
    return std::isfinite(qDamage) && std::isfinite(health) &&
           std::isfinite(shield) && qDamage + 0.001f >=
               std::max(0.0f, health) + std::max(0.0f, shield) +
               std::max(0.0f, safetyMargin);
}

inline float QReach(float attackRange, float targetRadius) {
    return std::max(0.0f, attackRange) + kQBonusRange +
           std::max(0.0f, targetRadius);
}

inline bool WithinQReach(float distance, float attackRange, float targetRadius) {
    return std::isfinite(distance) && distance <= QReach(attackRange, targetRadius);
}

inline float WSlowPercent(int rank, float elapsedSeconds) {
    const float perTick = RankValue(std::array<float, 7>{0.0f, 0.0f, 3.0f,
                                                          6.0f, 9.0f, 12.0f,
                                                          15.0f}, rank);
    return std::clamp(35.0f + perTick * std::clamp(elapsedSeconds, 0.0f,
                                                    kWDurationSeconds),
                      0.0f, 100.0f);
}

inline float WAttackSpeedSlowPercent(int rank, float elapsedSeconds) {
    return WSlowPercent(rank, elapsedSeconds) * 0.75f;
}

inline float EInitialDamage(int rank, float abilityPower) {
    return RankValue(std::array<float, 7>{0.0f, 50.0f, 80.0f, 110.0f,
                                           140.0f, 170.0f, 170.0f}, rank) +
           0.60f * std::max(0.0f, abilityPower);
}

inline float EDamagePerTick(int rank, float abilityPower) {
    return RankValue(std::array<float, 7>{0.0f, 10.0f, 16.0f, 22.0f,
                                           28.0f, 34.0f, 34.0f}, rank) +
           0.12f * std::max(0.0f, abilityPower);
}

inline float EArmorReductionPercent(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 30.0f, 35.0f, 40.0f,
                                           45.0f, 50.0f, 50.0f}, rank);
}

inline bool ZoneContains(const Vec3& center, const Vec3& point,
                         float radius = kERadius, float pointRadius = 0.0f) {
    return center.IsValid() && point.IsValid() && !center.IsZero() &&
           !point.IsZero() && center.Distance2D(point) <=
               std::max(0.0f, radius) + std::max(0.0f, pointRadius);
}

inline Vec3 PredictZoneCenter(const Vec3& origin, const Vec3& predictedTarget,
                             float maxRange = kERange) {
    if (!origin.IsValid() || !predictedTarget.IsValid() || origin.IsZero() ||
        predictedTarget.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, predictedTarget);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(std::max(0.0f, maxRange),
                                          origin.Distance2D(predictedTarget));
}

struct ZonePlacementContext {
    bool Ready = false;
    bool CenterValid = false;
    bool CenterWalkable = false;
    bool ProjectileWall = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool ZonePlacementSafe(const ZonePlacementContext& context) {
    if (!context.Ready || !context.CenterValid || !context.CenterWalkable ||
        context.ProjectileWall) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.Lethal) return false;
    return context.Defensive || context.Lethal ||
           context.NearbyEnemies <= std::max(0, context.MaximumEnemies);
}

inline float RBonusHealth(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 300.0f, 450.0f, 600.0f,
                                           750.0f, 900.0f, 1050.0f}, rank);
}

inline float RBonusResist(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 40.0f, 55.0f, 70.0f,
                                           85.0f, 100.0f, 115.0f}, rank);
}

inline float RSizeIncreasePercent(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 30.0f, 35.0f, 40.0f,
                                           45.0f, 50.0f, 55.0f}, rank);
}

inline float RRadius(int rank) {
    return std::max(kRRadius, kRRadius * (1.0f + RSizeIncreasePercent(rank) / 100.0f));
}

inline float RPercentMaxHealthPerTick(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 3.0f, 4.0f, 5.0f, 6.0f,
                                           7.0f, 8.0f}, rank);
}

struct UltimateContext {
    bool Ready = false;
    bool Defensive = false;
    bool Lethal = false;
    bool TargetLow = false;
    bool IncomingHardCc = false;
    bool UnderTurret = false;
    int NearbyEnemies = 0;
    int MinimumEnemies = 2;
};

inline constexpr bool ShouldCastUltimate(const UltimateContext& context) {
    if (!context.Ready) return false;
    if (context.Defensive || context.Lethal || context.IncomingHardCc) return true;
    if (context.UnderTurret && !context.TargetLow) return false;
    return context.TargetLow || context.NearbyEnemies >= std::max(1, context.MinimumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Nasus::Geometry
