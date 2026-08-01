#pragma once

// Deterministic Rell geometry and policy. Runtime state reconciliation, target
// discovery and spell casts remain in AIRellController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Rell::Geometry {

using ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

enum class MountState : std::uint8_t { Unknown, Mounted, Dismounted };
enum class WAction : std::uint8_t { None, CrashDown, MountUp };

inline constexpr float kQRange = 700.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQSpeed = 1450.0f;
inline constexpr float kQDelaySeconds = 0.40f;
inline constexpr float kWCrashRange = 400.0f;
inline constexpr float kWCrashRadius = 200.0f;
inline constexpr float kWMountRange = 200.0f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr float kERadius = 1000.0f;
inline constexpr float kRRadius = 450.0f;
inline constexpr float kRChannelSeconds = 2.0f;

inline float QRawDamage(int rank, float abilityPower) {
    return RankValue(std::array<float, 6>{0.0f, 70.0f, 100.0f, 130.0f,
                                          160.0f, 190.0f}, rank) +
           std::max(0.0f, abilityPower) * 0.50f;
}

inline float QHeal(int rank, float missingHealth) {
    return std::min(std::max(0.0f, missingHealth),
                    RankValue(std::array<float, 6>{0.0f, 5.0f, 10.0f,
                                                   15.0f, 20.0f, 25.0f}, rank));
}

inline float PassiveShred(int rank) {
    return RankValue(std::array<float, 6>{0.0f, 10.0f, 12.5f, 15.0f,
                                          17.5f, 20.0f}, rank);
}

inline float QTravelSeconds(float distance) {
    return kQDelaySeconds + std::max(0.0f, distance) / kQSpeed;
}

inline bool QLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 65.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    return ProjectPointToSegment2D(target, origin, endpoint).Distance <=
           kQWidth * 0.5f + std::max(0.0f, targetRadius);
}

struct QContext {
    bool Ready = false;
    bool TargetValid = false;
    bool CollisionFree = false;
    bool ProjectileWall = false;
    bool TargetSpellShielded = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool QSafe(const QContext& context) {
    if (!context.Ready || !context.TargetValid || !context.CollisionFree ||
        context.ProjectileWall || context.TargetSpellShielded) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.Lethal)
        return false;
    return context.Defensive || context.Lethal ||
           context.NearbyEnemies <= std::max(0, context.MaximumEnemies);
}

inline Vec3 CrashEndpoint(const Vec3& origin, const Vec3& aim,
                          float range = kWCrashRange) {
    if (!origin.IsValid() || !aim.IsValid()) return {};
    const Vec3 direction = SharedGeometry::Direction2D(origin, aim);
    if (direction.IsZero()) return {};
    const float distance = std::min(std::max(0.0f, origin.Distance2D(aim)),
                                    std::max(0.0f, range));
    return origin + direction * distance;
}

inline bool CrashEndpointSafe(const Vec3& endpoint, bool ready,
                              bool underEnemyTurret, bool defensive,
                              bool lethal, bool projectileWall,
                              bool dashHazard, int nearbyEnemies,
                              int maximumEnemies) {
    if (!ready || !endpoint.IsValid() || projectileWall ||
        dashHazard) return false;
    if (underEnemyTurret && !defensive && !lethal) return false;
    return defensive || lethal || nearbyEnemies <= std::max(0, maximumEnemies);
}

inline bool MountUpReachable(const Vec3& origin, const Vec3& ally,
                            bool ready, bool mounted, bool allySafe) {
    return ready && !mounted && allySafe && origin.IsValid() && ally.IsValid() &&
           origin.Distance2D(ally) <= kWMountRange;
}

inline float TetherDistance(const Vec3& player, const Vec3& ally) {
    if (!player.IsValid() || !ally.IsValid()) return INFINITY;
    return player.Distance2D(ally);
}

inline bool TetherValid(const Vec3& player, const Vec3& ally, bool ready,
                        bool allySafe) {
    return ready && allySafe && TetherDistance(player, ally) <= kERange;
}

inline bool TetherStunHits(const Vec3& player, const Vec3& ally,
                           const Vec3& enemy, float enemyRadius = 65.0f) {
    if (!player.IsValid() || !ally.IsValid() || !enemy.IsValid()) return false;
    return ProjectPointToSegment2D(enemy, player, ally).Distance <=
           kEWidth * 0.5f + std::max(0.0f, enemyRadius);
}

struct EContext {
    bool Ready = false;
    bool AllyValid = false;
    bool AllySafe = false;
    bool EnemyLineHit = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
};

inline constexpr bool ESafe(const EContext& context) {
    if (!context.Ready || !context.AllyValid || !context.AllySafe ||
        !context.EnemyLineHit) return false;
    return !context.UnderEnemyTurret || context.Defensive || context.Lethal;
}

inline bool MagneticPullHits(const Vec3& center, const Vec3& enemy,
                             float enemyRadius = 65.0f) {
    if (!center.IsValid() || !enemy.IsValid()) return false;
    return center.Distance2D(enemy) <= kRRadius + std::max(0.0f, enemyRadius);
}

struct RContext {
    bool Ready = false;
    bool ChannelActive = false;
    bool UnderEnemyTurret = false;
    bool AllySafetyOverride = false;
    bool Defensive = false;
    bool Lethal = false;
    int EnemiesInside = 0;
    int AlliesInside = 0;
    int MinimumEnemies = 2;
    int MaximumEnemyAdvantage = 1;
};

inline constexpr bool MagneticPullSafe(const RContext& context) {
    if (!context.Ready || context.ChannelActive) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.Lethal)
        return false;
    if (context.EnemiesInside < std::max(1, context.MinimumEnemies) &&
        !context.Defensive && !context.Lethal) return false;
    if (!context.AllySafetyOverride &&
        context.EnemiesInside > context.AlliesInside +
            std::max(0, context.MaximumEnemyAdvantage) &&
        !context.Defensive && !context.Lethal) return false;
    return true;
}

inline float RRawDamage(int rank, float abilityPower, int ticks) {
    const float base = RankValue(std::array<float, 6>{0.0f, 100.0f, 160.0f,
                                                      220.0f, 280.0f, 340.0f}, rank);
    return std::max(0, ticks) * (base + std::max(0.0f, abilityPower) * 0.35f) /
           8.0f;
}

inline bool AllyChannelSafe(int enemiesInside, int alliesInside,
                            bool underEnemyTurret, bool urgentPeel) {
    if (underEnemyTurret && !urgentPeel) return false;
    return urgentPeel || enemiesInside <= alliesInside + 1;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Rell::Geometry
