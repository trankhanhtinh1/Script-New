#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Elise::Geometry {

using ::Plugins::KuroAIO::AI::SharedGeometry::ProjectPointToSegment2D;
using ::Plugins::KuroAIO::AI::SharedGeometry::Direction2D;

enum class Form { Human, Spider, Unknown };
enum class RappelPhase { Ready, Rising, Descending };

inline constexpr float kNeurotoxinRange = 625.0f;
inline constexpr float kVolatileSpiderlingRange = 950.0f;
inline constexpr float kCocoonRange = 1075.0f;
inline constexpr float kCocoonWidth = 55.0f;
inline constexpr float kCocoonDelay = 0.25f;
inline constexpr float kCocoonSpeed = 1300.0f;
inline constexpr float kSpiderBiteRange = 400.0f;
inline constexpr float kRappelRange = 700.0f;
inline constexpr float kRappelLandingRadius = 180.0f;
inline constexpr int kRappelWindowMs = 2200;
inline constexpr int kCocoonMarkMs = 1800;

inline constexpr float RankValue(int rank, float r1, float r2, float r3,
                                 float r4, float r5) {
    switch (std::clamp(rank, 1, 5)) {
    case 1: return r1;
    case 2: return r2;
    case 3: return r3;
    case 4: return r4;
    default: return r5;
    }
}

inline constexpr float NeurotoxinBaseDamage(int rank) {
    return RankValue(rank, 40.0f, 70.0f, 100.0f, 130.0f, 160.0f);
}
inline constexpr float VenomousBiteBaseDamage(int rank) {
    return RankValue(rank, 50.0f, 80.0f, 110.0f, 140.0f, 180.0f);
}
inline constexpr float NeurotoxinRawDamage(int rank, float abilityPower,
                                           float currentHealth) {
    const float health = std::max(0.0f, currentHealth);
    return NeurotoxinBaseDamage(rank) + health *
        (0.04f + 0.03f * std::max(0.0f, abilityPower) / 100.0f);
}
inline constexpr float VenomousBiteRawDamage(int rank, float abilityPower,
                                              float missingHealth) {
    const float health = std::max(0.0f, missingHealth);
    return VenomousBiteBaseDamage(rank) + health *
        (0.08f + 0.03f * std::max(0.0f, abilityPower) / 100.0f);
}
inline constexpr float CocoonStunSeconds(int rank) {
    return RankValue(rank, 1.6f, 1.7f, 1.8f, 1.9f, 2.0f);
}
inline constexpr float VolatileSpiderlingRawDamage(int rank, float abilityPower) {
    return RankValue(rank, 30.0f, 55.0f, 80.0f, 105.0f, 130.0f) +
        0.08f * std::max(0.0f, abilityPower);
}
inline constexpr float SpiderlingFrenzyAttackSpeed(int rank) {
    return RankValue(rank, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f);
}

inline bool SegmentHits(Vec2 start, Vec2 end, Vec2 target, float radius) {
    const Vec2 segment = end - start;
    const float lengthSquared = segment.LengthSqr();
    const float t = lengthSquared <= 0.0001f ? 0.0f :
        std::clamp((target - start).Dot(segment) / lengthSquared, 0.0f, 1.0f);
    return (target - (start + segment * t)).Length() <= std::max(0.0f, radius);
}

inline bool CocoonHits(Vec2 origin, Vec2 aim, Vec2 target,
                       float targetRadius = 0.0f) {
    const Vec2 direction = (aim - origin).Normalized();
    if (direction.IsZero()) return false;
    return SegmentHits(origin, origin + direction * kCocoonRange, target,
                       kCocoonWidth * 0.5f + std::max(0.0f, targetRadius));
}

struct ProjectileCollision {
    int NetworkId = 0;
    Vec2 Position{};
    float Radius = 0.0f;
};
inline bool HasCocoonCollision(Vec2 origin, Vec2 aim,
                               const std::array<ProjectileCollision, 8>& blockers,
                               int ignoredNetworkId = 0) {
    const Vec2 direction = (aim - origin).Normalized();
    if (direction.IsZero()) return true;
    for (const auto& blocker : blockers) {
        if (blocker.NetworkId != 0 && blocker.NetworkId == ignoredNetworkId) continue;
        if (blocker.NetworkId != 0 && SegmentHits(
                origin, origin + direction * kCocoonRange, blocker.Position,
                kCocoonWidth * 0.5f + std::max(0.0f, blocker.Radius))) return true;
    }
    return false;
}

inline Vec2 ClampRappelEndpoint(Vec2 origin, Vec2 requested) {
    const Vec2 delta = requested - origin;
    const float distance = delta.Length();
    if (distance <= 0.0001f) return {};
    return distance <= kRappelRange ? requested :
        origin + delta * (kRappelRange / distance);
}

struct LandingSafety {
    bool Ready = false;
    bool EndpointValid = false;
    bool PathBlocked = false;
    bool UnderEnemyTurret = false;
    int EnemiesAtLanding = 0;
    int MaximumEnemies = 1;
    bool Defensive = false;
};
inline bool ShouldDescend(const LandingSafety& context) {
    if (!context.Ready || !context.EndpointValid || context.PathBlocked ||
        context.UnderEnemyTurret) return false;
    return context.Defensive || context.EnemiesAtLanding <= context.MaximumEnemies;
}

inline bool TargetIsInBiteRange(Vec2 origin, Vec2 target, float targetRadius = 0.0f) {
    return (target - origin).Length() <= kSpiderBiteRange +
        std::max(0.0f, targetRadius);
}

inline bool ShouldTransform(Form current, Form desired, bool targetInRange,
                            bool escape, bool manualOwnership) {
    if (manualOwnership || current == Form::Unknown || current == desired) return false;
    return targetInRange || escape;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Elise::Geometry
