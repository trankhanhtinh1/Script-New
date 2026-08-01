#pragma once

// Deterministic Camille mechanics. NavMesh wall discovery, target prediction,
// damage mitigation and spell/buff events remain in AICamilleController.h.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Camille::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 325.0f;
inline constexpr float kQBonusAttackRange = 50.0f;
inline constexpr int kQPrimeMilliseconds = 1500;
inline constexpr int kQRecastMilliseconds = 3500;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWOuterMinimumRange = 325.0f;
inline constexpr float kWHalfAngleRadians = 35.0f * SharedGeometry::kPi / 180.0f;
inline constexpr float kWDelay = 0.75f;
inline constexpr float kEWallRange = 800.0f;
inline constexpr float kE2ShortRange = 400.0f;
inline constexpr float kE2ChampionRange = 800.0f;
inline constexpr float kECollisionRadius = 130.0f;
inline constexpr float kRRange = 475.0f;
inline constexpr float kRArenaRadius = 425.0f;

inline float PassiveShield(float maximumHealth) {
    return 0.20f * std::max(0.0f, maximumHealth);
}

inline float PassiveCooldownSeconds(int level) {
    if (level >= 13) return 10.0f;
    if (level >= 7) return 14.0f;
    return 18.0f;
}

inline float QBonusRatio(int rank) {
    static constexpr std::array<float, 6> values{
        0.0f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f
    };
    return RankValue(values, rank);
}

inline float Q1BonusRawDamage(int rank, float totalAttackDamage) {
    return QBonusRatio(rank) * std::max(0.0f, totalAttackDamage);
}

inline float Q2BonusRawDamage(int rank, float totalAttackDamage) {
    return 2.0f * Q1BonusRawDamage(rank, totalAttackDamage);
}

inline float Q2TrueDamageFraction(int level) {
    return std::clamp(0.40f + 0.04f * static_cast<float>(std::clamp(level, 1, 16) - 1),
                      0.40f, 1.0f);
}

struct Q2DamageSplit {
    float Physical = 0.0f;
    float True = 0.0f;
};

inline Q2DamageSplit Q2BonusDamageSplit(int rank,
                                        int level,
                                        float totalAttackDamage,
                                        bool fullyPrimed) {
    const float raw = Q2BonusRawDamage(rank, totalAttackDamage);
    const float conversion = fullyPrimed ? Q2TrueDamageFraction(level) : 0.0f;
    return { raw * (1.0f - conversion), raw * conversion };
}

enum class QStage : std::uint8_t {
    Idle,
    FirstAttackArmed,
    SecondWindow,
    SecondAttackArmed,
};

struct QResetContext {
    QStage Stage = QStage::Idle;
    bool AfterAttack = false;
    bool FullyPrimed = false;
    bool RecastExpiring = false;
    bool LethalWithoutPrime = false;
};

inline bool MayActivateQ(const QResetContext& context) {
    if (!context.AfterAttack) return false;
    if (context.Stage == QStage::Idle) return true;
    if (context.Stage != QStage::SecondWindow) return false;
    return context.FullyPrimed || context.RecastExpiring || context.LethalWithoutPrime;
}

inline float WBaseRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> values{
        0.0f, 60.0f, 85.0f, 110.0f, 135.0f, 160.0f
    };
    return RankValue(values, rank) + 0.60f * std::max(0.0f, bonusAttackDamage);
}

inline float WOuterMaximumHealthRatio(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> values{
        0.0f, 0.060f, 0.065f, 0.070f, 0.075f, 0.080f
    };
    return RankValue(values, rank) + 0.00025f * std::max(0.0f, bonusAttackDamage);
}

inline float WRawDamage(int rank,
                        float bonusAttackDamage,
                        float targetMaximumHealth,
                        bool outerCone) {
    const float base = WBaseRawDamage(rank, bonusAttackDamage);
    return outerCone
        ? base + WOuterMaximumHealthRatio(rank, bonusAttackDamage) *
                     std::max(0.0f, targetMaximumHealth)
        : base;
}

inline bool WContains(const Vec3& origin,
                      const Vec3& aim,
                      const Vec3& point,
                      float targetRadius,
                      bool requireOuter) {
    const Vec3 direction = Direction2D(origin, aim);
    const Vec3 toPoint = Direction2D(origin, point);
    if (direction.IsZero() || toPoint.IsZero()) return false;
    const float distance = origin.Distance2D(point);
    const float radius = std::max(0.0f, targetRadius);
    if (distance - radius > kWRange ||
        (requireOuter && distance + radius < kWOuterMinimumRange)) return false;
    return direction.Dot(toPoint) >= std::cos(kWHalfAngleRadians);
}

inline float ERawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> values{
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f
    };
    return RankValue(values, rank) + 0.75f * std::max(0.0f, bonusAttackDamage);
}

inline Vec3 ClampHookshotLanding(const Vec3& wallAnchor,
                                 const Vec3& requested,
                                 bool towardChampion) {
    const Vec3 direction = Direction2D(wallAnchor, requested);
    if (direction.IsZero()) return {};
    const float maximum = towardChampion ? kE2ChampionRange : kE2ShortRange;
    return wallAnchor + direction * std::min(wallAnchor.Distance2D(requested), maximum);
}

struct HookshotContext {
    bool AnchorValid = false;
    bool LandingValid = false;
    bool LandingWalkable = false;
    bool NewTurretDive = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool TargetReachable = false;
    bool Lethal = false;
    bool Fleeing = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool HookshotSafe(const HookshotContext& context) {
    if (!context.AnchorValid || !context.LandingValid || !context.LandingWalkable) {
        return false;
    }
    if (!context.Fleeing && !context.TargetReachable) return false;
    if (context.NewTurretDive && !context.Lethal) return false;
    if (!context.Fleeing && (context.PointClickThreat || context.DashHazard) &&
        !context.Lethal) return false;
    if (!context.Fleeing && context.NearbyEnemies > std::max(0, context.MaximumEnemies) &&
        !context.Lethal) return false;
    return true;
}

struct WallCandidate {
    Vec3 Anchor = {};
    Vec3 Landing = {};
    HookshotContext Safety = {};
    float TargetDistance = FLT_MAX;
    float CursorDistance = FLT_MAX;
};

inline WallCandidate SelectWallCandidate(const std::vector<WallCandidate>& candidates,
                                         bool fleeing) {
    WallCandidate best{};
    float bestScore = -FLT_MAX;
    for (const WallCandidate& candidate : candidates) {
        if (candidate.Anchor.IsZero() || candidate.Landing.IsZero() ||
            !HookshotSafe(candidate.Safety)) continue;
        float score = -candidate.TargetDistance * (fleeing ? 0.05f : 0.65f);
        score -= candidate.CursorDistance * (fleeing ? 0.85f : 0.10f);
        score -= static_cast<float>(candidate.Safety.NearbyEnemies) * 180.0f;
        if (candidate.Safety.Lethal) score += 320.0f;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

inline float RDurationSeconds(int rank) {
    static constexpr std::array<float, 4> values{ 0.0f, 2.50f, 3.25f, 4.00f };
    return RankValue(values, rank);
}

inline float ROnHitCurrentHealthRatio(int rank) {
    static constexpr std::array<float, 4> values{ 0.0f, 0.04f, 0.06f, 0.08f };
    return RankValue(values, rank);
}

inline float ROnHitRawDamage(int rank, float targetCurrentHealth) {
    return ROnHitCurrentHealthRatio(rank) * std::max(0.0f, targetCurrentHealth);
}

struct ArenaContext {
    bool TargetValid = false;
    bool TargetDamageable = false;
    bool TargetInRange = false;
    bool NewTurretDive = false;
    bool IncomingHardCrowdControl = false;
    bool TargetIsolated = false;
    bool LethalFollowup = false;
    bool AllySupport = false;
    int EnemiesNearArena = 0;
    int MaximumEnemies = 2;
};

inline bool ArenaSafe(const ArenaContext& context) {
    if (!context.TargetValid || !context.TargetDamageable || !context.TargetInRange) {
        return false;
    }
    if (context.NewTurretDive && !context.LethalFollowup) return false;
    if (context.EnemiesNearArena > std::max(1, context.MaximumEnemies) &&
        !context.LethalFollowup && !context.IncomingHardCrowdControl) return false;
    return context.LethalFollowup || context.IncomingHardCrowdControl ||
           context.TargetIsolated || context.AllySupport;
}

struct AutomaticContext {
    bool DefensiveDodge = false;
    bool KillSecure = false;
    bool UnsolicitedEngage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.UnsolicitedEngage &&
           (context.DefensiveDodge || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Camille::Geometry
