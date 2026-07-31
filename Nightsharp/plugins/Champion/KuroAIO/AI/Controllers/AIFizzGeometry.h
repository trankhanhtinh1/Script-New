#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Fizz::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 550.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1400.0f;
inline constexpr float kWRange = 125.0f;
inline constexpr float kERange = 400.0f;
inline constexpr float kEImpactRadius = 330.0f;
inline constexpr int kERecastWindowMs = 1250;
inline constexpr int kEUntargetableMs = 750;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kRRange = 1300.0f;
inline constexpr float kRSpeed = 1300.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kRMinRadius = 55.0f;
inline constexpr float kRMaxRadius = 125.0f;
inline constexpr float kRMediumDistance = 455.0f;
inline constexpr float kRLargeDistance = 910.0f;

enum class SharkSize {
    Small,
    Medium,
    Large,
};

inline constexpr float RankValue(int rank, const float* values, int count) {
    return values[std::clamp(rank, 1, count) - 1];
}

inline constexpr float QRawDamage(int rank, float abilityPower, float totalAttackDamage) {
    constexpr float base[] = {10.0f, 25.0f, 40.0f, 55.0f, 70.0f};
    return RankValue(rank, base, 5) + 0.55f * std::max(0.0f, abilityPower) +
        std::max(0.0f, totalAttackDamage);
}

inline constexpr float WPassiveDamage(int rank, float abilityPower) {
    constexpr float base[] = {20.0f, 30.0f, 40.0f, 50.0f, 60.0f};
    return RankValue(rank, base, 5) + 0.40f * std::max(0.0f, abilityPower);
}

inline constexpr float WActiveDamage(int rank, float abilityPower) {
    constexpr float base[] = {10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    return RankValue(rank, base, 5) + 0.70f * std::max(0.0f, abilityPower);
}

inline constexpr float ERawDamage(int rank, float abilityPower) {
    constexpr float base[] = {70.0f, 100.0f, 130.0f, 160.0f, 190.0f};
    return RankValue(rank, base, 5) + 0.75f * std::max(0.0f, abilityPower);
}

inline constexpr float RBaseDamage(int rank, float abilityPower) {
    constexpr float base[] = {150.0f, 225.0f, 300.0f};
    return RankValue(rank, base, 3) + 0.80f * std::max(0.0f, abilityPower);
}

inline constexpr float SharkDamageMultiplier(SharkSize size) {
    switch (size) {
    case SharkSize::Medium: return 1.25f;
    case SharkSize::Large: return 1.50f;
    default: return 1.0f;
    }
}

inline constexpr SharkSize SharkSizeForDistance(float distance) {
    const float safeDistance = std::max(0.0f, distance);
    return safeDistance < kRMediumDistance ? SharkSize::Small :
        safeDistance < kRLargeDistance ? SharkSize::Medium : SharkSize::Large;
}

inline constexpr float SharkDamage(int rank, float abilityPower, float travelDistance) {
    return RBaseDamage(rank, abilityPower) *
        SharkDamageMultiplier(SharkSizeForDistance(travelDistance));
}

inline constexpr float SharkRadius(float travelDistance) {
    const float t = std::clamp(std::max(0.0f, travelDistance) / kRRange, 0.0f, 1.0f);
    return kRMinRadius + (kRMaxRadius - kRMinRadius) * t;
}

inline constexpr float RTravelSeconds(float distance) {
    return kRDelay + std::clamp(std::max(0.0f, distance), 0.0f, kRRange) /
        std::max(1.0f, kRSpeed);
}

inline Vec3 ClampEEndpoint(const Vec3& origin, const Vec3& requested,
                           float range = kERange) {
    if (!origin.IsValid() || !requested.IsValid() || requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline Vec3 EReturnEndpoint(const Vec3& current, const Vec3& requested,
                            float range = kERange) {
    return ClampEEndpoint(current, requested, range);
}

struct EState {
    bool OnPole = false;
    int HopStartTick = 0;
    int RecastExpireTick = 0;
    int UntargetableExpireTick = 0;
};

inline constexpr bool ERecastAvailable(const EState& state, int now) {
    return state.OnPole && now <= state.RecastExpireTick;
}

inline constexpr bool EUntargetable(const EState& state, int now) {
    return state.OnPole && now < state.UntargetableExpireTick;
}

inline constexpr bool EWindowExpired(const EState& state, int now) {
    return !state.OnPole || now > state.RecastExpireTick;
}

struct EEndpointSafety {
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointTurret = false;
    bool OriginTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    bool Manual = false;
    bool IncomingThreat = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool SafeEEndpoint(const EEndpointSafety& context) {
    if (!context.EndpointValid || context.EndpointWall) return false;
    if (context.EndpointTurret && !context.OriginTurret && !context.Defensive &&
        !context.Lethal && !context.Manual) return false;
    if (!context.Defensive && !context.Lethal && !context.Manual && !context.IncomingThreat &&
        context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies)) return false;
    return true;
}

struct RBody {
    Vec3 Position{};
    float Radius = 0.0f;
    int Id = 0;
    bool Champion = true;
    bool Targetable = true;
};

inline float RayEntryDistance(const Vec3& origin, const Vec3& direction,
                              const RBody& body, float range, float projectileRadius) {
    if (!origin.IsValid() || !direction.IsValid() || direction.IsZero() ||
        !body.Position.IsValid() || !body.Targetable || !body.Champion) return INFINITY;
    const Vec3 endpoint = origin + direction * range;
    const auto projection = ProjectPointToSegment2D(body.Position, origin, endpoint);
    const float touch = std::max(0.0f, body.Radius) + std::max(0.0f, projectileRadius);
    if (projection.Distance > touch) return INFINITY;
    const float along = std::max(0.0f, (projection.Closest - origin).Length2D());
    return along;
}

inline int FirstRChampionCollisionIndex(const Vec3& origin, const Vec3& direction,
                                         const std::vector<RBody>& bodies,
                                         float range = kRRange,
                                         float projectileRadius = kRMinRadius) {
    if (!origin.IsValid() || !direction.IsValid() || direction.IsZero()) return -1;
    int first = -1;
    float nearest = INFINITY;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const float entry = RayEntryDistance(origin, direction, bodies[i], range, projectileRadius);
        if (entry < nearest) {
            nearest = entry;
            first = static_cast<int>(i);
        }
    }
    return first;
}

struct EvasionSafety {
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointTurret = false;
    bool OriginTurret = false;
    bool ThreatActive = false;
    bool Defensive = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool SafeEvasion(const EvasionSafety& context) {
    if (!context.EndpointValid || context.EndpointWall || !context.ThreatActive) return false;
    if (context.EndpointTurret && !context.OriginTurret) return false;
    return context.Defensive || context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Fizz::Geometry
