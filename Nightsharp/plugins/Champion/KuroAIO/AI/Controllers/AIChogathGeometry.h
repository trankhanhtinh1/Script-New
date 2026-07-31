#pragma once

// Pure Cho'Gath geometry and patch data.  Runtime target selection, spell
// ownership and NavMesh/turret queries stay in AIChogathController.h.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Chogath::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 950.0f;
inline constexpr float kQRuptureRadius = 230.0f;
inline constexpr float kQDelay = 0.65f;
inline constexpr float kQSlowSeconds = 1.5f;
inline constexpr float kWRange = 675.0f;
inline constexpr float kWHalfAngleDegrees = 14.0f;
inline constexpr float kWCastConeRange = 585.0f;
inline constexpr float kECastRange = 40.0f;
inline constexpr float kEAttackRange = 125.0f;
inline constexpr float kEVorpalBonusRange = 50.0f;
inline constexpr int kEMaximumAttacks = 3;
inline constexpr float kRBaseCastRange = 175.0f;
inline constexpr float kRRangePerStack = 2.5f;
inline constexpr float kRMaximumBonusRange = 25.0f;
inline constexpr int kRMinionMaximumStacks = 6;
inline constexpr float kRMonsterRawDamage = 1200.0f;

inline constexpr float RankValue(int rank, const std::array<float, 6>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {25.0f, 80.0f, 135.0f, 190.0f, 245.0f, 300.0f}) +
        std::max(0.0f, abilityPower);
}

inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {80.0f, 80.0f, 130.0f, 180.0f, 230.0f, 280.0f}) +
        0.70f * std::max(0.0f, abilityPower);
}

// Live Vorpal Spikes is an attack reset/toggle.  The percentage is max-health
// magic damage and gains 0.5 percentage points per Feast stack, capped by the
// three empowered attacks in the current E window.
inline constexpr float EPercentMaxHealth(int rank, int feastStacks) {
    const float rankPercent = RankValue(rank, {2.15f, 2.50f, 2.85f, 3.20f, 3.55f, 3.90f});
    return rankPercent + 0.50f * static_cast<float>(std::clamp(feastStacks, 0, 255));
}
inline constexpr float ERawDamage(int rank, int feastStacks, float targetMaxHealth) {
    return std::max(0.0f, targetMaxHealth) * EPercentMaxHealth(rank, feastStacks) * 0.01f;
}

inline constexpr float RChampionRawDamage(int rank, float abilityPower,
                                           float bonusHealth) {
    return RankValue(rank, {125.0f, 300.0f, 475.0f, 650.0f, 825.0f, 1000.0f}) +
        0.50f * std::max(0.0f, abilityPower) +
        0.10f * std::max(0.0f, bonusHealth);
}
inline constexpr float RMonsterRawDamage(float abilityPower, float bonusHealth) {
    return kRMonsterRawDamage + 0.50f * std::max(0.0f, abilityPower) +
        0.10f * std::max(0.0f, bonusHealth);
}
inline constexpr float FeastCastRange(int feastStacks) {
    return kRBaseCastRange + std::min(kRMaximumBonusRange,
        std::max(0, feastStacks) * kRRangePerStack);
}
inline constexpr float FeastAttackRangeBonus(int rRank, int feastStacks) {
    const float perStack = RankValue(rRank, {0.0f, 3.2f, 4.7f, 6.2f, 7.7f, 9.2f});
    return std::min(75.0f, perStack * static_cast<float>(std::max(0, feastStacks)));
}

inline bool RuptureHits(const Vec3& castCenter, const Vec3& target,
                        float targetRadius = 0.0f) {
    return castCenter.IsValid() && target.IsValid() &&
        castCenter.Distance2D(target) <= kQRuptureRadius +
            std::clamp(targetRadius, 0.0f, 250.0f);
}

inline Vec3 RuptureAim(const Vec3& origin, const Vec3& predictedTarget) {
    if (!origin.IsValid() || !predictedTarget.IsValid() ||
        origin.Distance2D(predictedTarget) > kQRange) return {};
    return predictedTarget;
}

struct ConeHit {
    bool Hits = false;
    float Forward = 0.0f;
    float Lateral = 0.0f;
    float Score = 0.0f;
};

inline ConeHit FeralScreamCone(const Vec3& origin, const Vec3& aim,
                               const Vec3& target, float targetRadius = 0.0f) {
    ConeHit result{};
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return result;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return result;
    Vec3 relative = target - origin;
    relative.y = 0.0f;
    const float distance = relative.Length2D();
    if (distance <= 0.001f) {
        result.Hits = true;
        result.Score = 1.0f;
        return result;
    }
    result.Forward = relative.Dot(direction);
    result.Lateral = std::fabs(SharedGeometry::Cross2D(direction, relative));
    const float radius = std::clamp(targetRadius, 0.0f, 250.0f);
    const float angularPadding = std::asin(std::clamp(radius / distance, 0.0f, 1.0f));
    const float halfAngle = kWHalfAngleDegrees * SharedGeometry::kPi / 180.0f + angularPadding;
    result.Hits = result.Forward >= -radius && result.Forward <= kWRange + radius &&
        direction.Dot(relative / distance) >= std::cos(halfAngle);
    result.Score = result.Hits ? std::max(0.0f, 1.0f - result.Lateral /
        std::max(1.0f, result.Forward * std::tan(kWHalfAngleDegrees * SharedGeometry::kPi / 180.0f))) : 0.0f;
    return result;
}

inline bool VorpalReach(const Vec3& player, const Vec3& target, int feastStacks,
                        bool vorpalEnabled, float targetRadius = 0.0f,
                        int rRank = 1) {
    if (!vorpalEnabled || !player.IsValid() || !target.IsValid()) return false;
    const float bonus = kEVorpalBonusRange +
        FeastAttackRangeBonus(std::clamp(rRank, 1, 5), feastStacks);
    return player.Distance2D(target) <= kEAttackRange + bonus +
        std::max(0.0f, targetRadius);
}

inline bool ProjectileWallSafe(const Vec3& origin, const Vec3& endpoint,
                               bool (*isWall)(const Vec3&) = nullptr,
                               float spacing = 35.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() || endpoint.IsZero()) return false;
    if (!isWall) return true;
    const Vec3 direction = Direction2D(origin, endpoint);
    const float distance = origin.Distance2D(endpoint);
    if (direction.IsZero() || !std::isfinite(distance)) return false;
    for (float d = std::max(1.0f, spacing); d < distance; d += std::max(8.0f, spacing))
        if (isWall(origin + direction * d)) return false;
    return true;
}

struct FeastDecision {
    bool InRange = false;
    bool Lethal = false;
    bool EpicMonster = false;
    bool Champion = false;
    bool GrantsStack = false;
    int Priority = 0;
};
inline FeastDecision EvaluateFeast(bool inRange, bool lethal, bool epicMonster,
                                  bool champion, int currentStacks) {
    FeastDecision result{inRange, lethal, epicMonster, champion,
        champion || epicMonster || currentStacks < kRMinionMaximumStacks, 0};
    if (!inRange || !lethal) return result;
    result.Priority = epicMonster ? 100 : champion ? 90 : result.GrantsStack ? 60 : 30;
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Chogath::Geometry
