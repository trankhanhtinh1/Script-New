#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Sivir::Geometry {

struct Vec3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

inline constexpr float kQMaximumRange = 1250.0f;
inline constexpr float kQWidth = 100.0f;
inline constexpr float kQReturnGrace = 35.0f;
inline constexpr float kShieldWindowMs = 1500.0f;
inline constexpr float kShieldReactionMs = 80.0f;

inline float DistanceSquared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dz = a.z - b.z;
    return dx * dx + dz * dz;
}

inline float Distance(const Vec3& a, const Vec3& b) {
    return std::sqrt(DistanceSquared(a, b));
}

inline Vec3 Direction2D(const Vec3& from, const Vec3& to) {
    const float distance = Distance(from, to);
    if (distance <= 0.001f) return {};
    return {(to.x - from.x) / distance, 0.0f,
            (to.z - from.z) / distance};
}

inline Vec3 PointAlong(const Vec3& from, const Vec3& to, float distance) {
    const Vec3 direction = Direction2D(from, to);
    return {from.x + direction.x * distance, from.y,
            from.z + direction.z * distance};
}

inline float PointSegmentDistance(const Vec3& point,
                                  const Vec3& start,
                                  const Vec3& end) {
    const float dx = end.x - start.x;
    const float dz = end.z - start.z;
    const float lengthSquared = dx * dx + dz * dz;
    if (lengthSquared <= 0.001f) return Distance(point, start);
    const float t = std::clamp(
        ((point.x - start.x) * dx + (point.z - start.z) * dz) /
            lengthSquared,
        0.0f, 1.0f);
    return Distance(point, {start.x + dx * t, start.y,
                            start.z + dz * t});
}

inline bool SegmentHits(const Vec3& start,
                        const Vec3& end,
                        const Vec3& target,
                        float targetRadius,
                        float spellWidth = kQWidth) {
    return PointSegmentDistance(target, start, end) <=
           std::max(0.0f, targetRadius) + std::max(0.0f, spellWidth) * 0.5f;
}

inline bool OutgoingHits(const Vec3& origin,
                         const Vec3& outboundEnd,
                         const Vec3& target,
                         float targetRadius,
                         float width = kQWidth) {
    return SegmentHits(origin, outboundEnd, target, targetRadius, width);
}

inline bool ReturnHits(const Vec3& origin,
                       const Vec3& outboundEnd,
                       const Vec3& target,
                       float targetRadius,
                       float width = kQWidth) {
    // The return projectile traverses the exact reverse segment. Keeping this
    // separate prevents the common mistake of treating it as an outgoing-only
    // line and allows callers to evaluate the second hit independently.
    return SegmentHits(outboundEnd, origin, target, targetRadius, width);
}

inline bool BoomerangHitsEitherPass(const Vec3& origin,
                                    const Vec3& outboundEnd,
                                    const Vec3& target,
                                    float targetRadius,
                                    float width = kQWidth) {
    return OutgoingHits(origin, outboundEnd, target, targetRadius, width) ||
           ReturnHits(origin, outboundEnd, target, targetRadius, width);
}
inline bool ReturnPathHits(const Vec3& origin,
                           const Vec3& outboundEnd,
                           const Vec3& target,
                           float targetRadius,
                           float width = kQWidth) {
    return ReturnHits(origin, outboundEnd, target, targetRadius, width);
}

inline bool BoomerangPathHits(const Vec3& origin,
                              const Vec3& outboundEnd,
                              const Vec3& target,
                              float targetRadius,
                              float width = kQWidth) {
    return BoomerangHitsEitherPass(
        origin, outboundEnd, target, targetRadius, width);
}

struct BoomerangContext {
    bool TargetValid = false;
    bool OutgoingCollisionFree = false;
    bool ReturnPathAvailable = false;
    bool TargetEscaping = false;
    bool OutsideAttackRange = false;
    bool Lethal = false;
    bool Manual = false;
};

inline bool ShouldThrowBoomerang(const BoomerangContext& context) {
    if (!context.TargetValid || !context.OutgoingCollisionFree) return false;
    return context.Manual || context.Lethal || context.OutsideAttackRange ||
           context.ReturnPathAvailable || context.TargetEscaping;
}

struct RicochetContext {
    bool PrimaryIsOrbwalkerTarget = false;
    bool PrimaryValid = false;
    bool HasNearbySecondary = false;
    bool PrimaryIsChampion = false;
    bool InAttackRange = false;
    bool ManaHealthy = false;
    bool LastHit = false;
};

inline bool ShouldCastRicochet(const RicochetContext& context) {
    if (!context.PrimaryValid || !context.PrimaryIsChampion ||
        !context.PrimaryIsOrbwalkerTarget || !context.InAttackRange ||
        !context.LastHit && !context.ManaHealthy) return false;
    return context.HasNearbySecondary || context.LastHit;
}

inline int SelectRicochetBounces(int availableBounces,
                                int uniqueEnemyTargets,
                                int uniqueMinionTargets) {
    const int targets = std::max(uniqueEnemyTargets, uniqueMinionTargets);
    return std::clamp(std::min(availableBounces, targets), 0,
                      std::max(0, availableBounces));
}

inline bool ShieldTimingAllowed(float impactInMs,
                                float shieldDurationMs = kShieldWindowMs,
                                float reactionBufferMs = kShieldReactionMs,
                                bool shieldAlreadyActive = false,
                                bool canCastNow = true) {
    if (!canCastNow || shieldAlreadyActive || impactInMs < 0.0f) return false;
    return impactInMs <= std::max(0.0f, shieldDurationMs) &&
           impactInMs >= std::max(0.0f, reactionBufferMs);
}

inline bool SpellShieldCovers(float impactInMs,
                              float shieldRemainingMs,
                              float reactionBufferMs = kShieldReactionMs) {
    return impactInMs >= std::max(0.0f, reactionBufferMs) &&
           impactInMs <= std::max(0.0f, shieldRemainingMs);
}

enum class RPosture {
    Hold,
    Engage,
    Disengage,
    Peel,
};

struct RContext {
    RPosture Posture = RPosture::Hold;
    bool Ready = false;
    bool HasMana = false;
    bool PlayerThreatened = false;
    bool AlliesMovingWithPlayer = false;
    bool AlliedFollowup = false;
    bool EnemyCommitment = false;
    bool Manual = false;
    int NearbyAllies = 0;
    int NearbyEnemies = 0;
};

inline bool ShouldCastOnTheHunt(const RContext& context) {
    if (!context.Ready || !context.HasMana) return false;
    if (context.Manual) return true;
    if (context.Posture == RPosture::Disengage ||
        context.Posture == RPosture::Peel) {
        return context.PlayerThreatened &&
               (context.EnemyCommitment || context.NearbyEnemies > 0);
    }
    if (context.Posture != RPosture::Engage ||
        !context.AlliesMovingWithPlayer || !context.AlliedFollowup) {
        return false;
    }
    return context.NearbyAllies >= context.NearbyEnemies &&
           context.NearbyAllies > 0 && context.NearbyEnemies > 0;
}

inline bool ManaReserveSatisfied(float mana,
                                float spellCost,
                                float reserveCost = 0.0f) {
    return mana + 0.5f >= std::max(0.0f, spellCost) +
        std::max(0.0f, reserveCost);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Sivir::Geometry
