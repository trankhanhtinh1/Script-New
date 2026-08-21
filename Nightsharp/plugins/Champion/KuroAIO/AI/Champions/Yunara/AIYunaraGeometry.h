#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yunara::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr int kQMaximumResource = 8;
inline constexpr int kQChampionResource = 2;
inline constexpr int kQNonChampionResource = 1;
inline constexpr int kQDurationMs = 5000;
inline constexpr float kQSpreadRadius = 300.0f;
inline constexpr float kWRange = 1150.0f;
inline constexpr float kWProjectileWidth = 60.0f;
inline constexpr float kWProjectileSpeed = 2150.0f;
inline constexpr float kW2Width = 90.0f;
inline constexpr float kE2Range = 450.0f;
inline constexpr int kRDurationMs = 15000;

inline int ClampQResource(int observed) {
    return std::clamp(observed, 0, kQMaximumResource);
}

inline int AddQResource(int current, bool championAttack) {
    return ClampQResource(current +
        (championAttack ? kQChampionResource : kQNonChampionResource));
}

inline float QOnHitRaw(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{
        0.0f, 5.0f, 10.0f, 15.0f, 20.0f, 25.0f,
    };
    return RankValue(base, rank) + 0.20f * std::max(0.0f, abilityPower);
}

inline float QSpreadRaw(float totalAttackDamage) {
    return 0.30f * std::max(0.0f, totalAttackDamage);
}

inline float ArcOfRuinRaw(int ultimateRank,
                          float bonusAttackDamage,
                          float abilityPower) {
    static constexpr std::array<float, 4> base{
        0.0f, 160.0f, 320.0f, 480.0f,
    };
    return RankValue(base, ultimateRank) +
           1.20f * std::max(0.0f, bonusAttackDamage) +
           0.75f * std::max(0.0f, abilityPower);
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Target = false;
    bool Valid = true;
};

struct ProjectileLine {
    bool TargetHit = false;
    bool TargetFirst = false;
    int FirstBodyId = 0;
    int TargetIndex = -1;
    std::vector<int> OrderedIds = {};
};

inline ProjectileLine EvaluateProjectileLine(
    const Vec3& origin,
    const Vec3& endpoint,
    const std::vector<Body>& bodies,
    int targetId,
    float width = kWProjectileWidth) {
    ProjectileLine result{};
    std::vector<std::pair<float, const Body*>> contacts;
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(
            body.Position, origin, endpoint);
        if (projection.Distance <= width * 0.5f +
                                   std::max(0.0f, body.Radius)) {
            contacts.emplace_back(projection.T, &body);
        }
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    for (std::size_t index = 0; index < contacts.size(); ++index) {
        const Body& body = *contacts[index].second;
        if (index == 0) result.FirstBodyId = body.Id;
        result.OrderedIds.push_back(body.Id);
        if (!result.TargetHit && (body.Target || body.Id == targetId)) {
            result.TargetHit = true;
            result.TargetIndex = static_cast<int>(index);
        }
    }
    result.TargetFirst = result.TargetHit && result.TargetIndex == 0;
    return result;
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(
        kE2Range, origin.Distance2D(requested));
}

struct DashContext {
    bool Empowered = false;
    bool EndpointValid = false;
    bool Walkable = false;
    bool TurretSafe = false;
    bool LockdownSafe = false;
    bool DirectionUseful = false;
    bool Flee = false;
    bool LethalReposition = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 1;
};

inline bool MayDash(const DashContext& context) {
    if (!context.Empowered || !context.EndpointValid || !context.Walkable ||
        !context.TurretSafe || !context.LockdownSafe ||
        !context.DirectionUseful ||
        context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies)) {
        return false;
    }
    return context.Flee || context.LethalReposition;
}

struct WContext {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool TargetFirst = false;
    bool Empowered = false;
    bool ProjectileWall = false;
    bool AttackReadySoon = false;
    bool AfterAttack = false;
    bool Lethal = false;
    bool Peel = false;
};

inline bool MayCastW(const WContext& context) {
    if (!context.Ready || !context.PredictionAccepted) return false;
    if (!context.Empowered &&
        (!context.TargetFirst || context.ProjectileWall)) return false;
    if (context.Peel || context.Lethal) return true;
    return context.AfterAttack || !context.AttackReadySoon;
}

struct UltimateContext {
    bool Ready = false;
    bool AlreadyActive = false;
    bool TargetReachable = false;
    bool PlayerSafe = false;
    bool CommittedCombat = false;
    bool EmpoweredWLethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool MayTranscend(const UltimateContext& context) {
    if (!context.Ready || context.AlreadyActive ||
        !context.TargetReachable || !context.PlayerSafe ||
        context.NearbyEnemies > std::max(0, context.MaximumEnemies)) {
        return false;
    }
    return context.CommittedCombat || context.EmpoweredWLethal;
}

inline bool MayActivateQ(bool ready,
                         bool active,
                         bool realAttack,
                         bool projectileBlocked) {
    return ready && !active && realAttack && !projectileBlocked;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Yunara::Geometry
