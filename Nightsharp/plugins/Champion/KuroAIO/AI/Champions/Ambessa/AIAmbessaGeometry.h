#pragma once

// Deterministic Ambessa mechanics used by both the live one-trick controller
// and the standalone regression test. The live layer owns prediction,
// NavMesh queries and player-input arbitration; this file owns the parts that
// are easy to get subtly wrong: Q edge/first-target geometry, R's farthest
// target rule, passive energy/dash math and the current 26.14 damage model.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ambessa::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQ1InnerRadius = 275.0f;
inline constexpr float kQ1OuterRadius = 400.0f;
inline constexpr float kQ2Range = 650.0f;
inline constexpr float kQ2HalfWidth = 40.0f;
inline constexpr float kWRadius = 325.0f;
inline constexpr float kERadius = 325.0f;
inline constexpr float kRRange = 1250.0f;
inline constexpr float kRHalfWidth = 65.0f;
inline constexpr float kPassiveDashMinimum = 175.0f;
inline constexpr float kPassiveDashMaximum = 350.0f;
inline constexpr float kPassiveDashTime = 0.30f;
inline constexpr float kBasicSpellEnergyCost = 70.0f;
inline constexpr float kMaximumEnergy = 200.0f;
inline constexpr float kEnergyRegenPerSecond = 10.0f;

enum class Q1Region : int {
    Miss,
    Inner,
    Outer,
};

struct LineTarget {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
};

inline float LevelInterpolation(int championLevel,
                                float levelOne,
                                float levelEighteen) {
    const int level = std::clamp(championLevel, 1, 18);
    return levelOne + (levelEighteen - levelOne) *
        static_cast<float>(level - 1) / 17.0f;
}

inline float PassiveEnergyRefund(int championLevel) {
    if (championLevel >= 13) return 70.0f;
    if (championLevel >= 7) return 55.0f;
    return 40.0f;
}

inline float PassiveBonusRawDamage(int championLevel, float bonusAd) {
    return LevelInterpolation(championLevel, 5.0f, 30.0f) +
        0.25f * std::max(0.0f, bonusAd);
}

inline float PassiveDashSpeed(int championLevel, float movementSpeed) {
    float base = 770.0f;
    if (championLevel >= 16) base += 180.0f;
    else if (championLevel >= 11) base += 120.0f;
    else if (championLevel >= 6) base += 60.0f;
    return base + std::max(0.0f, movementSpeed);
}

// A buffered order inside the minimum travel threshold is modeled as a
// deliberate no-dash/hold branch. A valid movement order farther away is
// clamped to Ambessa's real 350-unit travel; terrain truncation is performed
// by the live controller because this pure layer must not query NavMesh.
inline Vec3 PassiveDashEndpoint(const Vec3& origin,
                                const Vec3& commandPosition,
                                float minimumDistance = kPassiveDashMinimum,
                                float maximumDistance = kPassiveDashMaximum) {
    const Vec3 direction = Direction2D(origin, commandPosition);
    if (direction.IsZero()) return origin;
    const float requested = origin.Distance2D(commandPosition);
    if (requested + 0.001f < std::max(0.0f, minimumDistance)) return origin;
    Vec3 result = origin + direction * std::min(
        requested, std::max(minimumDistance, maximumDistance));
    result.y = origin.y;
    return result;
}

inline float EnergyAfter(float initialEnergy,
                         int basicSpellsCast,
                         int empoweredAutosLanded,
                         int championLevel,
                         float elapsedSeconds = 0.0f) {
    const float spent = std::max(0, basicSpellsCast) *
        kBasicSpellEnergyCost;
    const float refunded = std::max(0, empoweredAutosLanded) *
        PassiveEnergyRefund(championLevel);
    const float regenerated = std::max(0.0f, elapsedSeconds) *
        kEnergyRegenPerSecond;
    return std::clamp(
        std::max(0.0f, initialEnergy) - spent + refunded + regenerated,
        0.0f,
        kMaximumEnergy);
}

inline bool CanPaySequentially(float initialEnergy,
                               int basicSpellsCast,
                               int empoweredAutosBetweenSpells,
                               int championLevel,
                               float safetyReserve = 0.0f) {
    float energy = std::clamp(initialEnergy, 0.0f, kMaximumEnergy);
    const int casts = std::max(0, basicSpellsCast);
    const int autos = std::clamp(
        empoweredAutosBetweenSpells, 0, std::max(0, casts - 1));
    for (int cast = 0; cast < casts; ++cast) {
        if (energy + 0.001f < kBasicSpellEnergyCost) return false;
        energy -= kBasicSpellEnergyCost;
        if (cast < autos) {
            energy = std::min(
                kMaximumEnergy,
                energy + PassiveEnergyRefund(championLevel));
        }
    }
    return energy + 0.001f >= std::max(0.0f, safetyReserve);
}

inline Q1Region ClassifyQ1(const Vec3& origin,
                           const Vec3& direction,
                           const Vec3& targetPosition,
                           float targetRadius = 0.0f) {
    if (!origin.IsValid() || !targetPosition.IsValid() ||
        direction.IsZero()) {
        return Q1Region::Miss;
    }
    Vec3 relative = targetPosition - origin;
    relative.y = 0.0f;
    const float radius = std::clamp(targetRadius, 0.0f, 150.0f);
    const float forward = relative.Dot(direction);
    const float distance = relative.Length2D();
    // 180-degree sweep: a unit whose collision circle crosses the facing
    // plane can still be hit, but a fully-behind unit cannot.
    if (forward < -radius || distance > kQ1OuterRadius + radius) {
        return Q1Region::Miss;
    }
    // Damage region follows the target's nearest edge. This avoids calling a
    // large champion at 300 range an edge hit when its body overlaps Q's inner
    // half-damage area.
    const float nearestEdge = std::max(0.0f, distance - radius);
    return nearestEdge >= kQ1InnerRadius
        ? Q1Region::Outer
        : Q1Region::Inner;
}

inline float Q1SweetspotScore(const Vec3& origin,
                              const Vec3& direction,
                              const Vec3& targetPosition,
                              float targetRadius = 0.0f) {
    if (ClassifyQ1(
            origin, direction, targetPosition, targetRadius) !=
        Q1Region::Outer) {
        return 0.0f;
    }
    const float distance = origin.Distance2D(targetPosition);
    const float ideal = 350.0f +
        std::clamp(targetRadius, 0.0f, 100.0f) * 0.35f;
    const float radial = 1.0f - std::min(
        1.0f, std::fabs(distance - ideal) / 85.0f);
    Vec3 relative = targetPosition - origin;
    relative.y = 0.0f;
    const float facing = relative.Length2D() > 0.001f
        ? std::clamp(relative.Dot(direction) / relative.Length2D(), 0.0f, 1.0f)
        : 0.0f;
    return radial * 0.72f + facing * 0.28f;
}

inline bool LineHits(const Vec3& origin,
                     const Vec3& direction,
                     float range,
                     float halfWidth,
                     const Vec3& targetPosition,
                     float targetRadius = 0.0f) {
    if (!origin.IsValid() || !targetPosition.IsValid() ||
        direction.IsZero() || range <= 0.0f) {
        return false;
    }
    const Vec3 end = origin + direction * range;
    const auto projection = ProjectPointToSegment2D(
        targetPosition, origin, end);
    const float radius = std::clamp(targetRadius, 0.0f, 150.0f);
    return projection.Distance <= std::max(0.0f, halfWidth) + radius &&
           targetPosition.Distance2D(origin) <= range + radius;
}

inline bool Q2Hits(const Vec3& origin,
                   const Vec3& direction,
                   const Vec3& targetPosition,
                   float targetRadius = 0.0f) {
    return LineHits(origin, direction, kQ2Range, kQ2HalfWidth,
                    targetPosition, targetRadius);
}

inline int FirstQ2TargetIndex(const Vec3& origin,
                              const Vec3& direction,
                              const std::vector<LineTarget>& targets) {
    if (direction.IsZero()) return -1;
    const Vec3 end = origin + direction * kQ2Range;
    int best = -1;
    float bestT = FLT_MAX;
    float bestLateral = FLT_MAX;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const auto& target = targets[i];
        if (!target.Valid || !Q2Hits(
                origin, direction, target.Position, target.Radius)) {
            continue;
        }
        const auto projection = ProjectPointToSegment2D(
            target.Position, origin, end);
        if (projection.T < bestT - 0.0001f ||
            (std::fabs(projection.T - bestT) <= 0.0001f &&
             projection.Distance < bestLateral)) {
            best = static_cast<int>(i);
            bestT = projection.T;
            bestLateral = projection.Distance;
        }
    }
    return best;
}

inline int FarthestRTargetIndex(const Vec3& origin,
                                const Vec3& direction,
                                const std::vector<LineTarget>& champions) {
    if (direction.IsZero()) return -1;
    const Vec3 end = origin + direction * kRRange;
    int best = -1;
    float bestT = -FLT_MAX;
    float bestLateral = FLT_MAX;
    for (std::size_t i = 0; i < champions.size(); ++i) {
        const auto& champion = champions[i];
        if (!champion.Valid || !LineHits(
                origin, direction, kRRange, kRHalfWidth,
                champion.Position, champion.Radius)) {
            continue;
        }
        const auto projection = ProjectPointToSegment2D(
            champion.Position, origin, end);
        if (projection.T > bestT + 0.0001f ||
            (std::fabs(projection.T - bestT) <= 0.0001f &&
             projection.Distance < bestLateral)) {
            best = static_cast<int>(i);
            bestT = projection.T;
            bestLateral = projection.Distance;
        }
    }
    return best;
}

inline Vec3 RLandingPoint(const Vec3& castOrigin,
                          const Vec3& targetPosition,
                          float behindDistance = 135.0f) {
    const Vec3 direction = Direction2D(castOrigin, targetPosition);
    if (direction.IsZero()) return targetPosition;
    Vec3 result = targetPosition + direction * std::max(0.0f, behindDistance);
    result.y = targetPosition.y;
    return result;
}

inline float QPercentMaxHealth(int rank,
                               float bonusAd,
                               bool secondCast) {
    static constexpr float basePercent[] = {
        0.0f, 0.040f, 0.045f, 0.050f, 0.055f, 0.060f,
    };
    const int spellRank = std::clamp(rank, 0, 5);
    if (spellRank <= 0) return 0.0f;
    const float perBonusAd = secondCast ? 0.0004f : 0.0003f;
    return basePercent[spellRank] +
        perBonusAd * std::max(0.0f, bonusAd);
}

inline float MonsterPercentHealthDamageCap(int championLevel) {
    return LevelInterpolation(championLevel, 100.0f, 300.0f);
}

inline float Q1RawDamage(int rank,
                         float bonusAd,
                         float targetMaximumHealth,
                         bool outerEdge,
                         bool monster = false,
                         int championLevel = 1) {
    static constexpr float base[] = {
        0.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f,
    };
    const int spellRank = std::clamp(rank, 0, 5);
    if (spellRank <= 0) return 0.0f;
    float healthDamage = std::max(0.0f, targetMaximumHealth) *
        QPercentMaxHealth(spellRank, bonusAd, false);
    if (monster) {
        healthDamage = std::min(
            healthDamage, MonsterPercentHealthDamageCap(championLevel));
    }
    float full = base[spellRank] + 0.60f * std::max(0.0f, bonusAd) +
        healthDamage + (monster ? 75.0f : 0.0f);
    return full * (outerEdge ? 1.0f : 0.5f);
}

inline float Q2RawDamage(int rank,
                         float bonusAd,
                         float targetMaximumHealth,
                         bool firstTarget,
                         bool monster = false,
                         int championLevel = 1) {
    static constexpr float base[] = {
        0.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f,
    };
    const int spellRank = std::clamp(rank, 0, 5);
    if (spellRank <= 0) return 0.0f;
    float healthDamage = std::max(0.0f, targetMaximumHealth) *
        QPercentMaxHealth(spellRank, bonusAd, true);
    if (monster) {
        healthDamage = std::min(
            healthDamage, MonsterPercentHealthDamageCap(championLevel));
    }
    float full = base[spellRank] + 0.90f * std::max(0.0f, bonusAd) +
        healthDamage + (monster ? 75.0f : 0.0f);
    return full * (firstTarget ? 1.0f : 0.5f);
}

inline float WShieldRaw(int championLevel, float bonusAd) {
    return LevelInterpolation(championLevel, 50.0f, 320.0f) +
        1.50f * std::max(0.0f, bonusAd);
}

inline float WRawDamage(int rank, float bonusAd, bool empowered) {
    static constexpr float base[] = {
        0.0f, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f,
    };
    const int spellRank = std::clamp(rank, 0, 5);
    if (spellRank <= 0) return 0.0f;
    const float normal = base[spellRank] +
        0.50f * std::max(0.0f, bonusAd);
    return normal * (empowered ? 1.50f : 1.0f);
}

inline float ERawDamage(int rank, float bonusAd, int hitCount = 1) {
    static constexpr float base[] = {
        0.0f, 40.0f, 60.0f, 80.0f, 100.0f, 120.0f,
    };
    const int spellRank = std::clamp(rank, 0, 5);
    if (spellRank <= 0) return 0.0f;
    return (base[spellRank] + 0.50f * std::max(0.0f, bonusAd)) *
        static_cast<float>(std::clamp(hitCount, 0, 2));
}

inline float RRawDamage(int rank, float bonusAd) {
    static constexpr float base[] = { 0.0f, 150.0f, 250.0f, 350.0f };
    const int spellRank = std::clamp(rank, 0, 3);
    return spellRank > 0
        ? base[spellRank] + 0.80f * std::max(0.0f, bonusAd)
        : 0.0f;
}

inline float RAbilityHealingRate(int rank, float lifeStealPercent = 0.0f) {
    static constexpr float base[] = { 0.0f, 0.15f, 0.175f, 0.20f };
    const int spellRank = std::clamp(rank, 0, 3);
    return spellRank > 0
        ? base[spellRank] + 0.005f * std::max(0.0f, lifeStealPercent)
        : 0.0f;
}

inline float ExpectedAbilityHealing(int rank,
                                    float postMitigationDamage,
                                    float lifeStealPercent = 0.0f,
                                    bool minion = false,
                                    bool monster = false) {
    float effectiveness = 1.0f;
    if (minion || monster) effectiveness = 0.25f;
    return std::max(0.0f, postMitigationDamage) *
        RAbilityHealingRate(rank, lifeStealPercent) * effectiveness;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ambessa::Geometry
