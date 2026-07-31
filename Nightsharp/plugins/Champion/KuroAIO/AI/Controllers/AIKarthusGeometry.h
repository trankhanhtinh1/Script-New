#pragma once

// Karthus-specific deterministic mechanics.  Runtime target selection,
// prediction and event ownership remain in AIKarthusController.h; this header
// owns Lay Waste isolation, Wall of Pain's finite wall/debuff, Defile's
// sustain economy and Requiem/death-passive timing so those boundaries remain
// independently testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Karthus::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 875.0f;
inline constexpr float kQRadius = 160.0f;
inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kWRange = 1000.0f;
inline constexpr float kWWallHalfWidth = 100.0f;
inline constexpr float kWCastSeconds = 0.25f;
inline constexpr float kWWallDurationSeconds = 5.0f;
inline constexpr float kWDebuffDurationSeconds = 5.0f;
inline constexpr float kERadius = 550.0f;
inline constexpr float kRChannelSeconds = 3.0f;
inline constexpr float kDeathPassiveSeconds = 7.0f;

inline float QRawDamage(int rank, float abilityPower, bool isolated) {
    static constexpr std::array<float, 6> base =
        {0.0f, 40.0f, 59.0f, 78.0f, 97.0f, 116.0f};
    const int index = std::clamp(rank, 0, 5);
    const float raw = base[static_cast<std::size_t>(index)] +
        std::max(0.0f, abilityPower) * 0.35f;
    return raw * (isolated ? 2.0f : 1.0f);
}

inline float QManaCost(int rank) {
    static constexpr std::array<float, 6> cost =
        {0.0f, 20.0f, 25.0f, 30.0f, 35.0f, 40.0f};
    return cost[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline bool QCenterHits(const Vec3& castCenter, const Vec3& target,
                        float targetRadius = 0.0f) {
    if (!castCenter.IsValid() || !target.IsValid()) return false;
    return castCenter.Distance2D(target) <=
        kQRadius + std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool QIsIsolated(const Vec3& castCenter, const Vec3& target,
                        float targetRadius,
                        int otherUnitsInsideArea) {
    return QCenterHits(castCenter, target, targetRadius) &&
           std::max(0, otherUnitsInsideArea) == 0;
}

inline float QImpactSeconds(float distance) {
    // Lay Waste is a ground-targeted spell: there is no missile travel,
    // collision or wall blocking; only the 0.25-second cast delay applies.
    (void)distance;
    return kQCastSeconds;
}

inline Vec3 ClampCastPoint(const Vec3& origin, const Vec3& desired,
                           float range = kQRange) {
    if (!origin.IsValid() || !desired.IsValid()) return {};
    const float distance = origin.Distance2D(desired);
    if (distance <= std::max(0.0f, range)) return desired;
    const Vec3 direction = Direction2D(origin, desired);
    return direction.IsZero() ? origin : origin + direction * std::max(0.0f, range);
}

struct WallPlan {
    Vec3 Center = {};
    Vec3 Start = {};
    Vec3 End = {};
    Vec3 Direction = {};
    float Length = 0.0f;
    float SlowPercent = 0.0f;
    float MagicResistShredPercent = 0.0f;
    bool Valid = false;
};

inline float WallLength(int rank) {
    static constexpr std::array<float, 6> values =
        {0.0f, 800.0f, 900.0f, 1000.0f, 1100.0f, 1200.0f};
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline float WallSlowPercent(int rank) {
    static constexpr std::array<float, 6> values =
        {0.0f, 40.0f, 50.0f, 60.0f, 70.0f, 80.0f};
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline WallPlan BuildWall(const Vec3& center, const Vec3& facing,
                          int rank) {
    WallPlan plan{};
    if (!center.IsValid() || facing.IsZero()) return plan;
    plan.Center = center;
    plan.Direction = Direction2D(center, center + facing);
    if (plan.Direction.IsZero()) return plan;
    plan.Length = WallLength(rank);
    plan.Start = center - plan.Direction * (plan.Length * 0.5f);
    plan.End = center + plan.Direction * (plan.Length * 0.5f);
    plan.SlowPercent = WallSlowPercent(rank);
    plan.MagicResistShredPercent = 25.0f;
    plan.Valid = plan.Length > 0.0f;
    return plan;
}

inline bool WallIntersects(const WallPlan& wall, const Vec3& point,
                           float unitRadius = 0.0f) {
    if (!wall.Valid || !point.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(point, wall.Start, wall.End);
    return projection.Distance <= kWWallHalfWidth +
        std::clamp(unitRadius, 0.0f, 250.0f);
}

inline float DefileDamagePerSecond(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base =
        {0.0f, 30.0f, 50.0f, 70.0f, 90.0f, 110.0f};
    return base[static_cast<std::size_t>(std::clamp(rank, 0, 5))] +
        std::max(0.0f, abilityPower) * 0.20f;
}

inline float DefileManaPerSecond(int rank) {
    static constexpr std::array<float, 6> values =
        {0.0f, 30.0f, 54.0f, 66.0f, 72.0f, 78.0f};
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 5))];
}

inline float DefileManaAfter(float currentMana, int rank,
                             float seconds, bool championKill = false) {
    const float spent = DefileManaPerSecond(rank) * std::max(0.0f, seconds);
    const float refund = championKill
        ? static_cast<float>(std::clamp(rank, 0, 5) * 10)
        : 0.0f;
    return std::max(0.0f, currentMana - spent + refund);
}

inline bool ShouldEnableDefile(float currentMana, int rank, float seconds,
                               float reserveMana, bool championInRange,
                               bool lethalWindow = false) {
    if (!championInRange && !lethalWindow) return false;
    const float reserve = std::max(0.0f, reserveMana);
    return currentMana + 0.01f >=
        DefileManaPerSecond(rank) * std::max(0.0f, seconds) + reserve;
}

inline float RequiemRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base =
        {0.0f, 200.0f, 350.0f, 500.0f};
    return base[static_cast<std::size_t>(std::clamp(rank, 0, 3))] +
        std::max(0.0f, abilityPower) * 0.70f;
}

inline float RequiemImpactSeconds(float nowSeconds) {
    return std::max(0.0f, nowSeconds) + kRChannelSeconds;
}

inline bool RequiemCanStart(float currentMana, float manaCost,
                            bool ready, bool playerDead,
                            bool passiveWindow, bool interrupted) {
    if (!ready || interrupted) return false;
    if (playerDead && passiveWindow) return true;
    if (playerDead) return false;
    return currentMana + 0.01f >= std::max(0.0f, manaCost);
}

inline bool RequiemShouldCommit(float rawDamage, float targetHealth,
                                float targetShield, bool targetProtected,
                                bool multiTarget, bool interruptOnly) {
    if (targetProtected || interruptOnly) return false;
    if (multiTarget) return rawDamage > 0.0f;
    return rawDamage + 0.01f >= std::max(0.0f, targetHealth) +
        std::max(0.0f, targetShield);
}

inline bool DeathPassiveActive(int nowTick, int deathTick,
                               float durationSeconds = kDeathPassiveSeconds) {
    return deathTick > 0 && nowTick >= deathTick &&
           nowTick < deathTick + static_cast<int>(
               std::max(0.0f, durationSeconds) * 1000.0f);
}

inline int DeathPassiveRemainingMs(int nowTick, int deathTick,
                                   float durationSeconds = kDeathPassiveSeconds) {
    if (!DeathPassiveActive(nowTick, deathTick, durationSeconds)) return 0;
    return std::max(0, deathTick + static_cast<int>(
        std::max(0.0f, durationSeconds) * 1000.0f) - nowTick);
}

inline bool DeathPassiveAllowsSpell(int slot, bool active, int nowTick,
                                    int deathTick) {
    return active && DeathPassiveActive(nowTick, deathTick) &&
           slot >= 0 && slot < 4;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Karthus::Geometry
