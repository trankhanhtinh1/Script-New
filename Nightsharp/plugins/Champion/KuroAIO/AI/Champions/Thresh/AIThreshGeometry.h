#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>

namespace Plugins::KuroAIO::AI::Controllers::Thresh::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 1075.0f;
inline constexpr float kQHalfWidth = 38.0f;
inline constexpr float kQMissileSpeed = 1900.0f;
inline constexpr float kQCastSeconds = 0.50f;
inline constexpr float kQRecastSeconds = 1.50f;
inline constexpr float kWRange = 950.0f;
inline constexpr float kWPickupRadius = 150.0f;
inline constexpr float kERange = 500.0f;
inline constexpr float kEHalfWidth = 80.0f;
inline constexpr float kEFlayDistance = 450.0f;
inline constexpr float kRRadius = 450.0f;
inline constexpr float kRWallHalfWidth = 32.0f;
inline constexpr int kRWallCount = 5;

inline float SoulArmor(int souls, float baseArmor = 0.0f) {
    return std::max(0.0f, baseArmor) + static_cast<float>(std::max(0, souls));
}

inline float SoulShield(int souls, int rank, float abilityPower = 0.0f) {
    const float soulTerm = static_cast<float>(std::max(0, souls));
    const float rankTerm = static_cast<float>(std::clamp(rank, 0, 5));
    return soulTerm * 2.0f + rankTerm * 35.0f + std::max(0.0f, abilityPower) * 0.40f;
}

struct HookPlan {
    Vec3 Origin{};
    Vec3 Aim{};
    float Distance = 0.0f;
    float TravelSeconds = 0.0f;
    bool Valid = false;
};

inline HookPlan BuildHookPlan(const Vec3& origin, const Vec3& target,
                              float targetRadius = 60.0f) {
    HookPlan plan{};
    plan.Origin = origin;
    plan.Aim = target;
    if (!origin.IsValid() || !target.IsValid()) return plan;
    plan.Distance = origin.Distance2D(target);
    if (!std::isfinite(plan.Distance) || plan.Distance > kQRange + std::max(0.0f, targetRadius)) return plan;
    plan.TravelSeconds = kQCastSeconds + plan.Distance / kQMissileSpeed;
    plan.Valid = !Direction2D(origin, target).IsZero();
    return plan;
}

inline bool HookHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const auto direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin,
                                                     origin + direction * kQRange);
    return projection.T < 1.0001f &&
           projection.Distance <= kQHalfWidth + std::max(0.0f, targetRadius);
}

inline bool HookRecastSafe(const Vec3& player, const Vec3& hooked,
                           bool wall, bool enemyTurret, int enemies,
                           int allies, int maximumEnemies,
                           bool lethal = false) {
    if (!player.IsValid() || !hooked.IsValid() || wall || enemyTurret) return false;
    if (!lethal && enemies > std::max(0, maximumEnemies)) return false;
    if (!lethal && allies <= 0) return false;
    return player.Distance2D(hooked) <= kQRange + 75.0f;
}

struct LanternPlan {
    Vec3 Position{};
    float Distance = 0.0f;
    float Shield = 0.0f;
    bool Valid = false;
};

inline LanternPlan BuildLanternPlan(const Vec3& origin, const Vec3& ally,
                                    int souls, int rank, float abilityPower = 0.0f) {
    LanternPlan plan{};
    plan.Position = ally;
    if (!origin.IsValid() || !ally.IsValid()) return plan;
    plan.Distance = origin.Distance2D(ally);
    if (!std::isfinite(plan.Distance) || plan.Distance > kWRange + kWPickupRadius) return plan;
    plan.Shield = SoulShield(souls, rank, abilityPower);
    plan.Valid = true;
    return plan;
}

inline bool LanternRescueSafe(const Vec3& landing, int enemies, int allies,
                              bool enemyTurret,
                              int maximumEnemies = 2) {
    if (!landing.IsValid() || enemyTurret) return false;
    return enemies <= std::max(0, maximumEnemies) && allies + 1 >= enemies;
}

inline Vec3 FlayDirection(const Vec3& player, const Vec3& target,
                          bool towardTarget) {
    if (!player.IsValid() || !target.IsValid()) return {};
    const Vec3 outward = Direction2D(player, target);
    return towardTarget ? outward : outward * -1.0f;
}

inline Vec3 FlayEndpoint(const Vec3& player, const Vec3& target,
                         bool towardTarget, float distance = kEFlayDistance) {
    const Vec3 direction = FlayDirection(player, target, towardTarget);
    if (direction.IsZero()) return {};
    return target + direction * std::max(0.0f, distance);
}

inline bool FlayHits(const Vec3& player, const Vec3& direction,
                     const Vec3& target, float targetRadius = 0.0f) {
    if (!player.IsValid() || !direction.IsValid() || direction.IsZero() || !target.IsValid()) return false;
    const Vec3 unit = Direction2D(Vec3{}, direction);
    const auto projection = ProjectPointToSegment2D(target, player,
        player + unit * kERange);
    return projection.T < 1.0001f &&
           projection.Distance <= kEHalfWidth + std::max(0.0f, targetRadius);
}

inline bool FlayCommitSafe(const Vec3& endpoint, bool wall, bool enemyTurret,
                           int enemies, int allies, int maximumEnemies,
                           bool defensive, bool lethal) {
    if (!endpoint.IsValid() || wall || (enemyTurret && !defensive)) return false;
    return defensive || lethal ||
           (enemies <= std::max(0, maximumEnemies) && allies >= enemies);
}

inline Vec3 BoxWallPoint(const Vec3& center, int wallIndex) {
    if (!center.IsValid() || wallIndex < 0 || wallIndex >= kRWallCount) return {};
    const float radians = 2.0f * SharedGeometry::kPi *
        static_cast<float>(wallIndex) / static_cast<float>(kRWallCount);
    return center + Vec3{std::cos(radians) * kRRadius, 0.0f,
                         std::sin(radians) * kRRadius};
}

inline bool InsideBox(const Vec3& center, const Vec3& point,
                     float radius = 0.0f) {
    return center.IsValid() && point.IsValid() &&
           center.Distance2D(point) <= kRRadius + std::max(0.0f, radius);
}

inline bool BoxWallBlocks(const Vec3& center, const Vec3& from,
                          const Vec3& to, float radius = 0.0f) {
    if (!InsideBox(center, from, radius) || !InsideBox(center, to, radius)) return false;
    const float innerWall = std::max(0.0f, kRRadius - kRWallHalfWidth -
        std::max(0.0f, radius));
    const float fromDistance = center.Distance2D(from);
    const float toDistance = center.Distance2D(to);
    return (fromDistance < innerWall && toDistance >= innerWall) ||
           (toDistance < innerWall && fromDistance >= innerWall);
}
inline bool BoxSafe(const Vec3& center, const Vec3& target,
                   const Vec3& ally, int enemiesInside,
                   int alliesInside, bool enemyTurret,
                   int minimumAllies = 1) {
    if (!center.IsValid() || enemyTurret || enemiesInside <= 0) return false;
    if (!target.IsZero() && !InsideBox(center, target, 55.0f)) return false;
    if (ally.IsValid() && !ally.IsZero() && !InsideBox(center, ally, 55.0f)) return false;
    return alliesInside >= std::max(0, minimumAllies);
}

inline float EDamage(int rank, float abilityPower, float souls) {
    static constexpr std::array<float, 6> base{0, 6, 18, 30, 42, 54};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] +
           std::max(0.0f, abilityPower) * 0.40f + std::max(0.0f, souls);
}

inline float RDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0, 250, 400, 550, 700, 850};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] + std::max(0.0f, abilityPower) * 1.0f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Thresh::Geometry
