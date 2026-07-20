#pragma once

// Deterministic Anivia mechanics.  The live controller owns prediction,
// target value, buff/object tracking and input arbitration; this file keeps
// Flash Frost's two hit regions, Frostbite's impact race, Crystallize's real
// occupied segment and Glacial Storm's growth/ticks independently testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Anivia::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kFlashFrostRange = 1075.0f;
inline constexpr float kFlashFrostHalfWidth = 110.0f;
inline constexpr float kFlashFrostExplosionRadius = 225.0f;
inline constexpr float kFlashFrostSpeed = 950.0f;
inline constexpr float kFlashFrostCastSeconds = 0.25f;
inline constexpr float kCrystallizeRange = 1000.0f;
inline constexpr float kCrystallizeSegmentRadius = 100.0f;
inline constexpr float kCrystallizeChampionDisplacement = 120.0f;
inline constexpr float kFrostbiteRange = 650.0f;
inline constexpr float kFrostbiteSpeed = 1600.0f;
inline constexpr float kFrostbiteCastSeconds = 0.25f;
inline constexpr float kStormCastRange = 750.0f;
inline constexpr float kStormInitialRadius = 200.0f;
inline constexpr float kStormFullRadius = 400.0f;
inline constexpr float kStormGrowSeconds = 1.5f;
inline constexpr float kStormTickSeconds = 0.5f;

inline float AlongRay(const Vec3& origin,
                      const Vec3& direction,
                      const Vec3& point) {
    Vec3 relative = point - origin;
    relative.y = 0.0f;
    return relative.Dot(direction);
}

inline float PerpendicularToRay(const Vec3& origin,
                                const Vec3& direction,
                                const Vec3& point) {
    const float along = AlongRay(origin, direction, point);
    Vec3 closest = origin + direction * along;
    closest.y = origin.y;
    return point.Distance2D(closest);
}

inline bool FlashFrostPassHits(const Vec3& origin,
                               const Vec3& direction,
                               float travelled,
                               const Vec3& target,
                               float targetRadius = 0.0f) {
    if (!origin.IsValid() || direction.IsZero() || !target.IsValid()) {
        return false;
    }
    const float along = AlongRay(origin, direction, target);
    const float radius = kFlashFrostHalfWidth +
        std::clamp(targetRadius, 0.0f, 250.0f);
    return along >= -radius &&
           along <= std::max(0.0f, travelled) + radius &&
           PerpendicularToRay(origin, direction, target) <= radius;
}

inline bool FlashFrostExplosionHits(const Vec3& missilePosition,
                                    const Vec3& target,
                                    float targetRadius = 0.0f) {
    return missilePosition.IsValid() && target.IsValid() &&
           missilePosition.Distance2D(target) <=
               kFlashFrostExplosionRadius +
                   std::clamp(targetRadius, 0.0f, 250.0f);
}

inline float FlashFrostTravelSeconds(float distance) {
    return kFlashFrostCastSeconds +
           std::clamp(distance, 0.0f, kFlashFrostRange) /
               kFlashFrostSpeed;
}

inline Vec3 FlashFrostPosition(const Vec3& origin,
                               const Vec3& direction,
                               float elapsedAfterReleaseSeconds) {
    if (!origin.IsValid() || direction.IsZero()) return origin;
    return origin + direction * std::clamp(
        std::max(0.0f, elapsedAfterReleaseSeconds) * kFlashFrostSpeed,
        0.0f, kFlashFrostRange);
}

// Positive means the missile center has passed the target center.  Anivia
// normally wants a small positive overshoot while the 225-radius explosion
// still covers the target, securing both Q damage instances.
inline float FlashFrostOvershoot(const Vec3& origin,
                                 const Vec3& direction,
                                 const Vec3& missilePosition,
                                 const Vec3& target) {
    return AlongRay(origin, direction, missilePosition) -
           AlongRay(origin, direction, target);
}

inline bool DoubleHitDetonationWindow(const Vec3& origin,
                                      const Vec3& direction,
                                      const Vec3& missilePosition,
                                      const Vec3& target,
                                      float targetRadius,
                                      float minimumOvershoot = 8.0f) {
    if (!FlashFrostExplosionHits(
            missilePosition, target, targetRadius)) {
        return false;
    }
    const float perpendicular = PerpendicularToRay(
        origin, direction, target);
    return perpendicular <= kFlashFrostHalfWidth +
               std::clamp(targetRadius, 0.0f, 250.0f) &&
           FlashFrostOvershoot(
               origin, direction, missilePosition, target) >=
               minimumOvershoot;
}

inline bool DetonationStillRecoverable(const Vec3& origin,
                                       const Vec3& direction,
                                       const Vec3& missilePosition,
                                       const Vec3& target,
                                       const Vec3& targetVelocity,
                                       float targetRadius,
                                       float reactionSeconds = 0.06f) {
    const Vec3 futureTarget = target + targetVelocity *
        std::max(0.0f, reactionSeconds);
    const Vec3 futureMissile = FlashFrostPosition(
        origin, direction,
        std::max(0.0f, AlongRay(origin, direction, missilePosition) /
            kFlashFrostSpeed) + std::max(0.0f, reactionSeconds));
    return FlashFrostExplosionHits(
        futureMissile, futureTarget, targetRadius);
}

inline float FrostbiteImpactSeconds(float distance) {
    return kFrostbiteCastSeconds +
           std::max(0.0f, distance) / kFrostbiteSpeed;
}

inline bool FrostbiteWillBeEmpowered(int nowTick,
                                     int impactTick,
                                     int chillExpireTick,
                                     int scheduledChillTick = 0,
                                     int safetyMs = 35) {
    const int safeImpact = impactTick + std::max(0, safetyMs);
    const bool alreadyChilled = chillExpireTick >= safeImpact;
    const bool chillBeforeImpact = scheduledChillTick > 0 &&
        scheduledChillTick <= impactTick - std::max(0, safetyMs);
    return alreadyChilled || chillBeforeImpact;
}

inline float FlashFrostPassRawDamage(int rank, float abilityPower) {
    static constexpr float base[] = {
        0.0f, 50.0f, 70.0f, 90.0f, 110.0f, 130.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.25f;
}

inline float FlashFrostExplosionRawDamage(int rank, float abilityPower) {
    static constexpr float base[] = {
        0.0f, 60.0f, 95.0f, 130.0f, 165.0f, 200.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.45f;
}

inline float FlashFrostRawDamage(int rank,
                                 float abilityPower,
                                 bool passHit,
                                 bool explosionHit) {
    return (passHit ? FlashFrostPassRawDamage(rank, abilityPower) : 0.0f) +
           (explosionHit
                ? FlashFrostExplosionRawDamage(rank, abilityPower)
                : 0.0f);
}

inline float FrostbiteRawDamage(int rank,
                                float abilityPower,
                                bool empowered) {
    static constexpr float base[] = {
        0.0f, 55.0f, 80.0f, 105.0f, 130.0f, 155.0f,
    };
    const float ordinary = base[std::clamp(rank, 0, 5)] +
                           std::max(0.0f, abilityPower) * 0.55f;
    return ordinary * (empowered ? 2.0f : 1.0f);
}

inline float CrystallizeOuterSegmentDistance(int rank) {
    static constexpr float distance[] = {
        0.0f, 400.0f, 500.0f, 600.0f, 700.0f, 800.0f,
    };
    return distance[std::clamp(rank, 0, 5)];
}

inline int CrystallizeSegments(int rank) {
    static constexpr int segments[] = { 0, 4, 5, 6, 7, 8 };
    return segments[std::clamp(rank, 0, 5)];
}

inline float CrystallizeOccupiedWidth(int rank) {
    if (rank <= 0) return 0.0f;
    return CrystallizeOuterSegmentDistance(rank) +
           2.0f * kCrystallizeSegmentRadius;
}

struct WallSegment {
    Vec3 Center = {};
    Vec3 Normal = {};
    Vec3 Tangent = {};
    Vec3 Start = {};
    Vec3 End = {};
    float Radius = kCrystallizeSegmentRadius;
    bool Valid = false;
};

inline WallSegment BuildWallSegment(const Vec3& caster,
                                    const Vec3& center,
                                    int rank) {
    const Vec3 normal = Direction2D(caster, center);
    if (normal.IsZero() || rank <= 0) return {};
    const Vec3 tangent{ -normal.z, 0.0f, normal.x };
    const float halfCore = CrystallizeOuterSegmentDistance(rank) * 0.5f;
    return WallSegment{
        center,
        normal,
        tangent,
        center - tangent * halfCore,
        center + tangent * halfCore,
        kCrystallizeSegmentRadius,
        true,
    };
}

inline bool WallContains(const WallSegment& wall,
                         const Vec3& point,
                         float unitRadius = 0.0f) {
    if (!wall.Valid || !point.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(
        point, wall.Start, wall.End);
    return projection.Distance <= wall.Radius +
        std::clamp(unitRadius, 0.0f, 250.0f);
}

inline float WallSignedSide(const WallSegment& wall,
                            const Vec3& point) {
    if (!wall.Valid) return 0.0f;
    Vec3 relative = point - wall.Center;
    relative.y = 0.0f;
    return relative.Dot(wall.Normal);
}

inline Vec3 WallDisplacementDestination(
    const WallSegment& wall,
    const Vec3& target,
    float targetRadius = 0.0f,
    float championDisplacement = kCrystallizeChampionDisplacement) {
    if (!wall.Valid || !target.IsValid()) return target;
    float side = WallSignedSide(wall, target);
    // Exactly centered units are displaced away from the caster.  A live
    // plan avoids relying on this tie by offsetting the wall a few units.
    const float sign = side < -0.5f ? -1.0f : 1.0f;
    const float fromPlane = std::fabs(side);
    const float clearance = wall.Radius +
        std::clamp(targetRadius, 0.0f, 250.0f) +
        std::max(0.0f, championDisplacement);
    return target + wall.Normal * sign *
        std::max(0.0f, clearance - fromPlane);
}

inline float Orientation2D(const Vec3& a,
                           const Vec3& b,
                           const Vec3& c) {
    return SharedGeometry::Cross2D(b - a, c - a);
}

inline bool SegmentsIntersect2D(const Vec3& a,
                                const Vec3& b,
                                const Vec3& c,
                                const Vec3& d) {
    const float abC = Orientation2D(a, b, c);
    const float abD = Orientation2D(a, b, d);
    const float cdA = Orientation2D(c, d, a);
    const float cdB = Orientation2D(c, d, b);
    return ((abC <= 0.0f && abD >= 0.0f) ||
            (abC >= 0.0f && abD <= 0.0f)) &&
           ((cdA <= 0.0f && cdB >= 0.0f) ||
            (cdA >= 0.0f && cdB <= 0.0f));
}

inline float SegmentDistance2D(const Vec3& a,
                               const Vec3& b,
                               const Vec3& c,
                               const Vec3& d) {
    if (SegmentsIntersect2D(a, b, c, d)) return 0.0f;
    return std::min({
        ProjectPointToSegment2D(a, c, d).Distance,
        ProjectPointToSegment2D(b, c, d).Distance,
        ProjectPointToSegment2D(c, a, b).Distance,
        ProjectPointToSegment2D(d, a, b).Distance,
    });
}

inline bool WallBlocksPath(const WallSegment& wall,
                           const Vec3& pathStart,
                           const Vec3& pathEnd,
                           float pathRadius = 0.0f) {
    if (!wall.Valid || !pathStart.IsValid() || !pathEnd.IsValid()) {
        return false;
    }
    return SegmentDistance2D(
               wall.Start, wall.End, pathStart, pathEnd) <=
           wall.Radius + std::max(0.0f, pathRadius);
}

inline float StormRadius(float elapsedSeconds) {
    const float progress = std::clamp(
        elapsedSeconds / kStormGrowSeconds, 0.0f, 1.0f);
    return kStormInitialRadius +
           (kStormFullRadius - kStormInitialRadius) * progress;
}

inline bool StormIsFull(float elapsedSeconds,
                        float toleranceSeconds = 0.02f) {
    return elapsedSeconds + std::max(0.0f, toleranceSeconds) >=
           kStormGrowSeconds;
}

inline bool StormHits(const Vec3& center,
                      const Vec3& target,
                      float elapsedSeconds,
                      float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= StormRadius(elapsedSeconds) +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline int StormTickCount(float elapsedSeconds,
                          bool includeActivationTick = true) {
    if (elapsedSeconds < 0.0f) return 0;
    const int elapsedTicks = static_cast<int>(
        std::floor(elapsedSeconds / kStormTickSeconds + 0.0001f));
    return elapsedTicks + (includeActivationTick ? 1 : 0);
}

inline float StormManaPerSecond(int rank) {
    static constexpr float drain[] = {
        0.0f, 35.0f, 45.0f, 55.0f,
    };
    return drain[std::clamp(rank, 0, 3)];
}

inline float StormManaAfter(float currentMana,
                            int rank,
                            float futureSeconds) {
    return std::max(0.0f, currentMana -
        StormManaPerSecond(rank) * std::max(0.0f, futureSeconds));
}

inline float StormRawDamagePerTick(int rank,
                                   float abilityPower,
                                   bool fullyFormed) {
    static constexpr float base[] = {
        0.0f, 15.0f, 22.5f, 30.0f,
    };
    float damage = base[std::clamp(rank, 0, 3)] +
                   std::max(0.0f, abilityPower) * 0.0625f;
    return damage * (fullyFormed ? 3.0f : 1.0f);
}

inline Vec3 LeadStormCenter(const Vec3& target,
                            const Vec3& targetVelocity,
                            float leadSeconds,
                            float maximumLead = 190.0f) {
    Vec3 lead = targetVelocity * std::max(0.0f, leadSeconds);
    lead.y = 0.0f;
    const float length = lead.Length2D();
    if (length > std::max(0.0f, maximumLead) && length > 0.001f) {
        lead = lead / length * std::max(0.0f, maximumLead);
    }
    return target + lead;
}

struct StormUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool HardCrowdControlled = false;
    bool Dashing = false;
    bool Valid = false;
};

inline float StormUnitScore(const Vec3& center,
                            float elapsedSeconds,
                            const StormUnit& unit) {
    if (!unit.Valid || !StormHits(
            center, unit.Position, elapsedSeconds, unit.Radius)) {
        return 0.0f;
    }
    float score = std::max(0.1f, unit.Priority);
    if (unit.HardCrowdControlled) score += 0.35f;
    if (unit.Dashing) score += 0.50f;
    const float edge = center.Distance2D(unit.Position) /
        std::max(1.0f, StormRadius(elapsedSeconds) + unit.Radius);
    score *= 1.12f - std::clamp(edge, 0.0f, 1.0f) * 0.22f;
    return score;
}

inline float StormScore(const Vec3& center,
                        float elapsedSeconds,
                        const std::vector<StormUnit>& units) {
    float score = 0.0f;
    for (const auto& unit : units) {
        score += StormUnitScore(center, elapsedSeconds, unit);
    }
    return score;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Anivia::Geometry
