#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Locke::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 950.0f;
inline constexpr float kQWidth = 64.0f;
inline constexpr float kQHalfWidth = 32.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1800.0f;
inline constexpr float kWRadius = 250.0f;
inline constexpr float kWDuration = 4.0f;
inline constexpr float kERange = 425.0f;
inline constexpr float kEDashRange = 625.0f;
inline constexpr float kEArrivalRadius = 250.0f;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRRadius = 150.0f;
inline constexpr float kRDelay = 0.50f;
inline constexpr float kRSpeed = 1200.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

// CDragon 16.15 exposes tooltip placeholders for Locke's rank values. These
// conservative formulas keep lethal checks below the unknown server values.
inline constexpr float NailMissileDamage(int rank, float abilityPower) {
    return RankValue(rank, {35.0f, 55.0f, 75.0f, 95.0f, 115.0f}) +
           0.45f * std::max(0.0f, abilityPower);
}
inline constexpr float NailOnHitDamage(int rank, float abilityPower,
                                       int stacks, float missingHealthPercent) {
    const float perStack = RankValue(rank, {18.0f, 27.0f, 36.0f, 45.0f, 54.0f}) +
                           0.20f * std::max(0.0f, abilityPower);
    const float multiplier = stacks >= 3 ? 1.40f : (stacks == 2 ? 1.20f : 1.0f);
    return perStack * std::clamp(stacks, 0, 3) * multiplier *
           (1.0f + 0.50f * std::clamp(missingHealthPercent, 0.0f, 1.0f));
}
inline constexpr float RBaseDamage(int rank, float abilityPower) {
    return RankValue(rank, {100.0f, 165.0f, 230.0f, 295.0f, 360.0f}) +
           0.75f * std::max(0.0f, abilityPower);
}
inline constexpr float RExecuteThreshold(int sealedChampions) {
    return std::clamp(0.08f + 0.04f * std::max(0, sealedChampions), 0.08f, 0.40f);
}

inline bool SegmentHits(const Vec3& source, const Vec3& endpoint,
                        const Vec3& point, float radius) {
    if (!source.IsValid() || !endpoint.IsValid() || !point.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(point, source, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, radius);
}

struct Blocker {
    Vec3 Position{};
    float Radius = 35.0f;
    int NetworkId = 0;
};
struct FirstCollision {
    bool Blocked = false;
    int NetworkId = 0;
    float TravelFraction = 1.0f;
    Vec3 Position{};
};
inline FirstCollision FirstQCollision(const Vec3& source, const Vec3& endpoint,
                                      float targetRadius,
                                      const std::vector<Blocker>& blockers) {
    FirstCollision result{};
    if (!source.IsValid() || !endpoint.IsValid() || source.Distance2D(endpoint) <= 1.0f)
        return result;
    const float targetEntry = std::clamp(1.0f - std::max(0.0f, targetRadius) /
        source.Distance2D(endpoint), 0.0f, 1.0f);
    for (const auto& blocker : blockers) {
        if (!blocker.Position.IsValid()) continue;
        const auto projection = ProjectPointToSegment2D(blocker.Position, source, endpoint);
        if (projection.T <= 0.001f || projection.T >= targetEntry ||
            projection.Distance > kQHalfWidth + std::max(0.0f, blocker.Radius)) continue;
        if (!result.Blocked || projection.T < result.TravelFraction) {
            result.Blocked = true;
            result.NetworkId = blocker.NetworkId;
            result.TravelFraction = projection.T;
            result.Position = projection.Closest;
        }
    }
    return result;
}

inline Vec3 ClampEEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}
inline bool SafeBlink(const Vec3& endpoint, bool wall, bool turret,
                      int enemiesAtEndpoint, int maxEnemies) {
    return endpoint.IsValid() && !endpoint.IsZero() && !wall &&
           !turret && enemiesAtEndpoint <= std::max(0, maxEnemies);
}
inline bool DashLineHits(const Vec3& from, const Vec3& to, const Vec3& target,
                         float targetRadius) {
    return SegmentHits(from, to, target, 42.0f + std::max(0.0f, targetRadius));
}

struct WContext {
    bool Ready = false;
    bool Active = false;
    bool Manual = false;
    bool IncomingThreat = false;
    bool EnoughHealth = false;
    bool EnoughMana = false;
    bool NearEnemy = false;
};
inline bool ShouldIgnite(const WContext& c) {
    return c.Ready && !c.Active && c.EnoughHealth && c.EnoughMana &&
           (c.Manual || c.IncomingThreat || c.NearEnemy);
}
inline bool ShouldRecastW(const WContext& c) {
    return c.Active && (c.Manual || !c.EnoughHealth || !c.NearEnemy);
}

struct RContext {
    bool Ready = false;
    bool PredictedHit = false;
    bool Blocked = false;
    bool Lethal = false;
    bool ExecuteWindow = false;
    bool Manual = false;
    bool UnderEnemyTurret = false;
    int EnemiesAtImpact = 0;
};
inline bool ShouldCastPurgatory(const RContext& c, int minimumAoe = 1) {
    if (!c.Ready || !c.PredictedHit || c.Blocked || c.UnderEnemyTurret) return false;
    return c.Manual || c.Lethal || c.ExecuteWindow || c.EnemiesAtImpact >= std::max(1, minimumAoe);
}
inline bool ExecuteEligible(float healthPercent, int sealedChampions) {
    return healthPercent <= RExecuteThreshold(sealedChampions);
}

struct ThreatRoute {
    Vec3 endpoint{};
    bool Valid = false;
    bool IncreasesSeparation = false;
    bool UnderTurret = false;
    int Enemies = 0;
};
inline bool SafeFleeRoute(const ThreatRoute& route, int maxEnemies) {
    return route.Valid && !route.endpoint.IsZero() && route.IncreasesSeparation &&
           !route.UnderTurret && route.Enemies <= std::max(0, maxEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Locke::Geometry
