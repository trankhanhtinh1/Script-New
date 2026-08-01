#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Braum::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 1050.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kQSpeed = 1700.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWResistRadius = 350.0f;
inline constexpr float kERange = 350.0f;
inline constexpr float kEShieldSeconds = 3.0f;
inline constexpr float kRRange = 1200.0f;
inline constexpr float kRWidth = 115.0f;
inline constexpr float kRKnockupRadius = 210.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr int kPassiveStackCap = 4;
inline constexpr float kPassiveDuration = 4.0f;

inline int ClampStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveStackCap);
}

inline bool PassiveStunReady(int stacks) {
    return ClampStacks(stacks) >= kPassiveStackCap;
}

inline int NextPassiveStacks(int stacks) {
    return PassiveStunReady(stacks) ? 0 : ClampStacks(stacks) + 1;
}

inline float QTravelSeconds(float distance) {
    if (!std::isfinite(distance) || distance < 0.0f) return -1.0f;
    return kQDelay + distance / kQSpeed;
}

struct QPlan {
    Vec3 Origin{};
    Vec3 Aim{};
    float Distance = 0.0f;
    float TravelSeconds = 0.0f;
    bool Valid = false;
};

inline QPlan BuildQPlan(const Vec3& origin, const Vec3& predicted,
                        float targetRadius = 0.0f) {
    QPlan plan{};
    plan.Origin = origin;
    plan.Aim = predicted;
    if (!origin.IsValid() || !predicted.IsValid()) return plan;
    plan.Distance = origin.Distance2D(predicted);
    if (!std::isfinite(plan.Distance) || plan.Distance > kQRange +
        std::max(0.0f, targetRadius)) return plan;
    plan.TravelSeconds = QTravelSeconds(plan.Distance);
    plan.Valid = !Direction2D(origin, predicted).IsZero();
    return plan;
}

inline bool QHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                  float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin,
                                                     origin + direction * kQRange);
    return projection.T < 1.0001f && projection.Distance <=
        kQWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline float QDamage(int rank, float abilityPower, float targetMaxHealth) {
    static constexpr std::array<float, 6> base{0, 30, 75, 120, 165, 210};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] + std::max(0.0f, abilityPower) * 0.60f +
        std::max(0.0f, targetMaxHealth) * 0.025f;
}

struct WPlan {
    Vec3 Destination{};
    float Distance = 0.0f;
    bool Valid = false;
};

inline WPlan BuildWPlan(const Vec3& origin, const Vec3& ally) {
    WPlan plan{};
    plan.Destination = ally;
    if (!origin.IsValid() || !ally.IsValid()) return plan;
    plan.Distance = origin.Distance2D(ally);
    plan.Valid = std::isfinite(plan.Distance) && plan.Distance <= kWRange;
    return plan;
}

inline bool WResistSafe(const Vec3& destination, int enemies, int allies,
                        bool enemyTurret, bool defensive,
                        int maximumEnemies = 3) {
    if (!destination.IsValid() || enemyTurret) return false;
    if (defensive) return enemies <= std::max(0, maximumEnemies) + 1;
    return enemies <= std::max(0, maximumEnemies) && allies + 1 >= enemies;
}

inline bool WAllyWorthwhile(float healthPercent, int enemies,
                            bool selected, bool hardThreat) {
    return selected || hardThreat || (healthPercent < 85.0f && enemies > 0);
}

inline bool ShieldCoversLine(const Vec3& shieldOrigin, const Vec3& threatOrigin,
                             const Vec3& threatEnd, const Vec3& protectedPoint,
                             float halfWidth = kRWidth * 0.5f) {
    if (!shieldOrigin.IsValid() || !threatOrigin.IsValid() ||
        !threatEnd.IsValid() || !protectedPoint.IsValid()) return false;
    const auto threat = ProjectPointToSegment2D(shieldOrigin, threatOrigin, threatEnd);
    const auto protectedProjection = ProjectPointToSegment2D(protectedPoint,
                                                               threatOrigin, threatEnd);
    return threat.T > 0.0f && threat.T < 1.0001f &&
           protectedProjection.Distance <= halfWidth + 45.0f;
}

inline bool EInterceptionWorthwhile(bool incoming, bool hardThreat,
                                    float playerHealthPercent, int enemies) {
    return incoming && (hardThreat || playerHealthPercent < 72.0f || enemies > 0);
}

struct FissurePlan {
    Vec3 Origin{};
    Vec3 Aim{};
    float Distance = 0.0f;
    float TravelSeconds = 0.0f;
    bool Valid = false;
};

inline FissurePlan BuildFissurePlan(const Vec3& origin, const Vec3& predicted,
                                    float targetRadius = 60.0f) {
    FissurePlan plan{};
    plan.Origin = origin;
    plan.Aim = predicted;
    if (!origin.IsValid() || !predicted.IsValid()) return plan;
    plan.Distance = origin.Distance2D(predicted);
    if (!std::isfinite(plan.Distance) || plan.Distance > kRRange +
        std::max(0.0f, targetRadius)) return plan;
    plan.TravelSeconds = kRDelay + plan.Distance / 1400.0f;
    plan.Valid = !Direction2D(origin, predicted).IsZero();
    return plan;
}

inline bool FissureHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                        float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin,
                                                     origin + direction * kRRange);
    return projection.T < 1.0001f && projection.Distance <=
        kRWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline bool FissureSafe(const Vec3& center, int enemies, int allies,
                        bool enemyTurret, bool wall, bool defensive,
                        int maximumEnemies = 3) {
    if (!center.IsValid() || wall || (enemyTurret && !defensive)) return false;
    return defensive ? enemies <= std::max(0, maximumEnemies) + 1 :
        enemies <= std::max(0, maximumEnemies) && allies >= enemies;
}

inline bool AllySafety(float healthPercent, int nearbyEnemies, int nearbyAllies,
                       bool hardThreat, bool underTurret) {
    if (underTurret && !hardThreat) return false;
    return hardThreat || healthPercent <= 62.0f ||
        (nearbyEnemies > nearbyAllies && nearbyEnemies > 0);
}

inline float RDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{0, 150, 250, 350, 450, 550};
    const int r = std::clamp(rank, 0, 5);
    return base[static_cast<std::size_t>(r)] + std::max(0.0f, abilityPower) * 0.60f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Braum::Geometry
