#pragma once

// Deterministic Amumu mechanics.  The live controller supplies prediction,
// target value, NavMesh/turret state and spell events; this file keeps the
// first-collision Q model, Q arrival clock, R coverage/quality, Curse order,
// W ticks and E attack-refund arithmetic independently testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Amumu::Geometry {

using SharedGeometry::Direction2D;

inline constexpr float kBandageRange = 1100.0f;
inline constexpr float kBandageHalfWidth = 80.0f;
inline constexpr float kBandageMissileSpeed = 2000.0f;
inline constexpr float kBandageDashSpeed = 1800.0f;
inline constexpr float kBandageCastSeconds = 0.25f;
inline constexpr float kDespairRadius = 350.0f;
inline constexpr float kTantrumRadius = 350.0f;
inline constexpr float kCurseRadius = 550.0f;

struct LineUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = false;
};

inline float RayLongitudinal(const Vec3& origin,
                             const Vec3& direction,
                             const Vec3& point) {
    Vec3 relative = point - origin;
    relative.y = 0.0f;
    return relative.Dot(direction);
}

inline float RayPerpendicular(const Vec3& origin,
                              const Vec3& direction,
                              const Vec3& point) {
    const float along = RayLongitudinal(origin, direction, point);
    Vec3 closest = origin + direction * along;
    closest.y = origin.y;
    return point.Distance2D(closest);
}

// Distance along the ray at which the projectile's collision capsule first
// touches a unit.  Ordering by center distance is wrong for large monsters or
// offset units, so the controller uses this entry distance instead.
inline float BandageEntryDistance(
    const Vec3& origin,
    const Vec3& direction,
    const LineUnit& unit,
    float range = kBandageRange,
    float halfWidth = kBandageHalfWidth) {
    if (!unit.Valid || direction.IsZero()) return FLT_MAX;
    const float along = RayLongitudinal(origin, direction, unit.Position);
    const float radius = std::max(0.0f, halfWidth) +
                         std::clamp(unit.Radius, 0.0f, 250.0f);
    const float perpendicular = RayPerpendicular(
        origin, direction, unit.Position);
    if (perpendicular > radius) return FLT_MAX;
    const float chord = std::sqrt(std::max(
        0.0f, radius * radius - perpendicular * perpendicular));
    const float entry = along - chord;
    if (along + chord < 0.0f || entry > std::max(0.0f, range)) {
        return FLT_MAX;
    }
    return std::max(0.0f, entry);
}

inline bool BandageHits(const Vec3& origin,
                        const Vec3& direction,
                        const Vec3& target,
                        float targetRadius = 0.0f,
                        float range = kBandageRange,
                        float halfWidth = kBandageHalfWidth) {
    const float entry = BandageEntryDistance(
        origin, direction,
        LineUnit{ target, targetRadius, 0, true }, range, halfWidth);
    return std::isfinite(entry) && entry < FLT_MAX * 0.5f;
}

inline int FirstBandageCollisionIndex(
    const Vec3& origin,
    const Vec3& direction,
    const std::vector<LineUnit>& units,
    float range = kBandageRange,
    float halfWidth = kBandageHalfWidth) {
    int best = -1;
    float bestEntry = FLT_MAX;
    for (std::size_t i = 0; i < units.size(); ++i) {
        const float entry = BandageEntryDistance(
            origin, direction, units[i], range, halfWidth);
        if (entry + 0.001f < bestEntry) {
            bestEntry = entry;
            best = static_cast<int>(i);
        }
    }
    return best;
}

inline float BandageMissileSeconds(float collisionEntryDistance,
                                   float castSeconds = kBandageCastSeconds,
                                   float missileSpeed = kBandageMissileSpeed) {
    return std::max(0.0f, castSeconds) +
           std::max(0.0f, collisionEntryDistance) /
               std::max(1.0f, missileSpeed);
}

inline float BandageArrivalSeconds(float centerDistance,
                                   float collisionEntryDistance,
                                   float amumuRadius = 55.0f,
                                   float targetRadius = 65.0f,
                                   float castSeconds = kBandageCastSeconds,
                                   float missileSpeed = kBandageMissileSpeed,
                                   float dashSpeed = kBandageDashSpeed) {
    const float exposedDash = std::max(
        0.0f, centerDistance - std::max(0.0f, amumuRadius) -
                  std::max(0.0f, targetRadius));
    return BandageMissileSeconds(
               collisionEntryDistance, castSeconds, missileSpeed) +
           exposedDash / std::max(1.0f, dashSpeed);
}

inline bool CircleHits(const Vec3& center,
                       const Vec3& target,
                       float targetRadius,
                       float effectRadius) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, effectRadius) +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool DespairHits(const Vec3& center,
                        const Vec3& target,
                        float targetRadius = 0.0f) {
    return CircleHits(center, target, targetRadius, kDespairRadius);
}

inline bool TantrumHits(const Vec3& center,
                        const Vec3& target,
                        float targetRadius = 0.0f) {
    return CircleHits(center, target, targetRadius, kTantrumRadius);
}

inline bool CurseHits(const Vec3& center,
                      const Vec3& target,
                      float targetRadius = 0.0f) {
    return CircleHits(center, target, targetRadius, kCurseRadius);
}

struct UltimateUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    float ExistingCrowdControlSeconds = 0.0f;
    bool SpellShield = false;
    bool CanCleanse = false;
    bool Dashing = false;
    bool Valid = false;
};

inline float UltimateUnitScore(const Vec3& center,
                               const UltimateUnit& unit) {
    if (!unit.Valid || !CurseHits(
            center, unit.Position, unit.Radius)) {
        return 0.0f;
    }
    float score = std::max(0.1f, unit.Priority);
    if (unit.SpellShield) score *= 0.08f;
    if (unit.CanCleanse) score *= 0.62f;
    if (unit.ExistingCrowdControlSeconds > 0.75f) score *= 0.38f;
    if (unit.Dashing) score += 0.55f;
    return score;
}

inline int UltimateHitCount(const Vec3& center,
                            const std::vector<UltimateUnit>& units,
                            bool countSpellShields = false) {
    int count = 0;
    for (const auto& unit : units) {
        if (!unit.Valid || (!countSpellShields && unit.SpellShield) ||
            !CurseHits(center, unit.Position, unit.Radius)) {
            continue;
        }
        ++count;
    }
    return count;
}

inline float UltimateScore(const Vec3& center,
                           const std::vector<UltimateUnit>& units) {
    float score = 0.0f;
    for (const auto& unit : units) {
        score += UltimateUnitScore(center, unit);
    }
    return score;
}

inline int DespairTickCount(float elapsedSeconds,
                            float cadenceSeconds = 0.5f) {
    if (elapsedSeconds < 0.0f) return 0;
    const float cadence = std::max(0.05f, cadenceSeconds);
    return 1 + static_cast<int>(std::floor(elapsedSeconds / cadence));
}

inline float DespairPercentMaxHealthPerSecond(int rank,
                                              float abilityPower) {
    static constexpr float base[] = {
        0.0f, 0.0100f, 0.0125f, 0.0150f, 0.0175f, 0.0200f,
    };
    const int clamped = std::clamp(rank, 0, 5);
    return base[clamped] + std::max(0.0f, abilityPower) * 0.00005f;
}

inline float DespairRawDamagePerTick(int rank,
                                     float abilityPower,
                                     float targetMaximumHealth) {
    return 5.0f + std::max(0.0f, targetMaximumHealth) *
        DespairPercentMaxHealthPerSecond(rank, abilityPower) * 0.5f;
}

inline float TantrumRawDamage(int rank, float abilityPower) {
    static constexpr float base[] = {
        0.0f, 65.0f, 95.0f, 125.0f, 155.0f, 185.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.50f;
}

inline float TantrumRemainingCooldown(
    float currentRemainingSeconds,
    int incomingBasicAttacks,
    float reductionPerAttack = 0.75f) {
    return std::max(
        0.0f, currentRemainingSeconds -
                  static_cast<float>(std::max(0, incomingBasicAttacks)) *
                      std::max(0.0f, reductionPerAttack));
}

inline float CurseBonusTrueDamage(float preMitigationMagicDamage,
                                  bool cursedBeforeDamage) {
    return cursedBeforeDamage
        ? std::max(0.0f, preMitigationMagicDamage) * 0.10f
        : 0.0f;
}

inline float UltimateRawDamage(int rank, float abilityPower) {
    static constexpr float base[] = {
        0.0f, 200.0f, 300.0f, 400.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
           std::max(0.0f, abilityPower) * 0.80f;
}

inline float UltimatePackageRawDamage(int rank,
                                      float abilityPower,
                                      bool cursedBeforeUltimate) {
    const float magic = UltimateRawDamage(rank, abilityPower);
    return magic + CurseBonusTrueDamage(magic, cursedBeforeUltimate);
}

inline bool ShouldLayerBandage(float crowdControlRemainingSeconds,
                               float bandageImpactSeconds,
                               float overlapAllowanceSeconds = 0.12f) {
    return crowdControlRemainingSeconds <=
        std::max(0.0f, bandageImpactSeconds) +
        std::max(0.0f, overlapAllowanceSeconds);
}

inline float ArrivalSafetyScore(int nearbyEnemies,
                                int nearbyAllies,
                                bool enemyTurret,
                                bool dashHazard,
                                float playerHealthPercent) {
    float score = static_cast<float>(std::max(0, nearbyAllies)) * 190.0f -
                  static_cast<float>(std::max(0, nearbyEnemies)) * 245.0f +
                  std::clamp(playerHealthPercent, 0.0f, 100.0f) * 2.0f;
    if (enemyTurret) score -= 1200.0f;
    if (dashHazard) score -= 900.0f;
    return score;
}

inline bool BridgeImprovesReach(const Vec3& origin,
                                const Vec3& bridge,
                                const Vec3& champion,
                                float secondBandageRange = kBandageRange,
                                float minimumGain = 220.0f) {
    if (!origin.IsValid() || !bridge.IsValid() || !champion.IsValid()) {
        return false;
    }
    const float before = origin.Distance2D(champion);
    const float after = bridge.Distance2D(champion);
    return after <= std::max(0.0f, secondBandageRange) &&
           before - after >= std::max(0.0f, minimumGain);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Amumu::Geometry
