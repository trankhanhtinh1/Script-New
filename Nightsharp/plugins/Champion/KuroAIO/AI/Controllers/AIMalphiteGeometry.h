#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Malphite::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 625.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1200.0f;
inline constexpr float kQMissileRadius = 45.0f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWResetWindowSeconds = 0.45f;
inline constexpr float kERadius = 400.0f;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRRadius = 270.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kRSpeed = 700.0f;
inline constexpr float kRMissileRadius = 80.0f;
inline constexpr float kPassiveShieldPercent = 0.10f;
inline constexpr int kPassiveBaseCooldownMs = 8000;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}

inline constexpr float QBaseDamage(int rank) {
    return RankValue(rank, {70.0f, 120.0f, 170.0f, 220.0f, 270.0f});
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return QBaseDamage(rank) + 0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float QSpeedStealPercent(int rank) {
    return RankValue(rank, {20.0f, 25.0f, 30.0f, 35.0f, 40.0f});
}
inline constexpr float QSlowDurationSeconds() { return 3.0f; }

inline constexpr float WBaseDamage(int rank) {
    return RankValue(rank, {30.0f, 40.0f, 50.0f, 60.0f, 70.0f});
}
inline constexpr float WSplashDamage(int rank) {
    return RankValue(rank, {15.0f, 25.0f, 35.0f, 45.0f, 55.0f});
}
inline constexpr float WRawDamage(int rank, float abilityPower, float armor,
                                  bool primaryHit = true) {
    const float base = primaryHit ? WBaseDamage(rank) : WSplashDamage(rank);
    return base + 0.20f * std::max(0.0f, abilityPower) +
        0.15f * std::max(0.0f, armor);
}
inline constexpr float WBonusArmorPercent(int rank) {
    return RankValue(rank, {10.0f, 15.0f, 20.0f, 25.0f, 30.0f}) * 0.01f;
}
inline constexpr float EffectiveArmor(float baseArmor, int rank,
                                      bool graniteShieldActive) {
    const float bonus = std::max(0.0f, baseArmor) * WBonusArmorPercent(rank);
    return std::max(0.0f, baseArmor) +
        (graniteShieldActive ? bonus * 3.0f : bonus);
}

inline constexpr float EBaseDamage(int rank) {
    return RankValue(rank, {60.0f, 95.0f, 130.0f, 165.0f, 200.0f});
}
inline constexpr float ERawDamage(int rank, float abilityPower, float armor) {
    return EBaseDamage(rank) + 0.40f * std::max(0.0f, abilityPower) +
        0.60f * std::max(0.0f, armor);
}
inline constexpr float EAttackSpeedReductionPercent(int rank) {
    return RankValue(rank, {30.0f, 35.0f, 40.0f, 45.0f, 50.0f});
}
inline constexpr float ECrippleDurationSeconds() { return 3.0f; }

inline constexpr float RBaseDamage(int rank) {
    return RankValue3(rank, {200.0f, 300.0f, 400.0f});
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RBaseDamage(rank) + 0.90f * std::max(0.0f, abilityPower);
}
inline constexpr float RKnockupDurationSeconds() { return 1.5f; }

inline constexpr float PassiveShield(float maximumHealth) {
    return std::max(0.0f, maximumHealth) * kPassiveShieldPercent;
}
inline constexpr int PassiveCooldownMs(int championLevel) {
    if (championLevel >= 13) return 6000;
    if (championLevel >= 7) return 7000;
    return kPassiveBaseCooldownMs;
}
inline constexpr bool PassiveReady(int now, int lastBrokenTick,
                                   int championLevel = 1) {
    return now >= lastBrokenTick + PassiveCooldownMs(championLevel);
}

inline Vec3 ClampQAim(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool QReachable(const Vec3& origin, const Vec3& target,
                       float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() &&
        !target.IsZero() && origin.Distance2D(target) <=
            kQRange + std::max(0.0f, targetRadius);
}
inline bool QProjectileHits(const Vec3& origin, const Vec3& target,
                            float targetRadius = 0.0f) {
    return QReachable(origin, target, targetRadius);
}

inline Vec3 ClampRAim(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    return origin + direction * std::min(kRRange, origin.Distance2D(requested));
}
inline bool RImpactContains(const Vec3& impact, const Vec3& target,
                            float targetRadius = 0.0f) {
    return impact.IsValid() && target.IsValid() && !impact.IsZero() &&
        impact.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}
inline bool RProjectileClear(const Vec3& origin, const Vec3& impact) {
    return origin.IsValid() && impact.IsValid() && !origin.IsZero() &&
        !impact.IsZero() && origin.Distance2D(impact) <= kRRange;
}
inline int CountRHits(const Vec3& impact, const std::array<Vec3, 5>& targets,
                      int targetCount, float targetRadius = 0.0f) {
    int hits = 0;
    const int count = std::clamp(targetCount, 0, static_cast<int>(targets.size()));
    for (int i = 0; i < count; ++i)
        if (RImpactContains(impact, targets[static_cast<std::size_t>(i)], targetRadius)) ++hits;
    return hits;
}
inline bool RCommitAllowed(bool lethal, bool defensive, int predictedHits,
                           int minimumTargets, int enemiesAtImpact,
                           int maximumEnemies, bool impactTurret,
                           bool currentlyUnderTurret, bool unsafeMobility,
                           bool projectileBlocked) {
    if (projectileBlocked || unsafeMobility ||
        impactTurret && !currentlyUnderTurret && !lethal && !defensive)
        return false;
    if (enemiesAtImpact > std::max(0, maximumEnemies) && !lethal && !defensive)
        return false;
    return lethal || defensive || predictedHits >= std::max(1, minimumTargets);
}

inline bool SafeWReset(bool inAttackRange, bool shieldActive,
                       bool playerMobilityLocked) {
    return inAttackRange && !playerMobilityLocked &&
        (shieldActive || inAttackRange);
}
inline bool SafeEPosition(const Vec3& origin, const Vec3& target,
                          float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() &&
        origin.Distance2D(target) <= kERadius + std::max(0.0f, targetRadius);
}
inline float ImpactTravelSeconds(const Vec3& origin, const Vec3& impact) {
    if (!RProjectileClear(origin, impact)) return 0.0f;
    return kRDelay + origin.Distance2D(impact) / kRSpeed;
}

struct MalphiteState {
    int LastShieldBreakTick = 0;
    int QStealTargetId = 0;
    int QStealExpireTick = 0;
    int WResetExpireTick = 0;
    int ECrippleExpireTick = 0;
    int RImpactTick = 0;
    bool ShieldActive = false;
};
inline void ReconcilePassive(MalphiteState& state, bool buffPresent, int now) {
    state.ShieldActive = buffPresent;
    if (!buffPresent && state.LastShieldBreakTick == 0) state.LastShieldBreakTick = now;
}
inline void ExpireState(MalphiteState& state, int now) {
    if (state.QStealExpireTick <= now) state.QStealTargetId = state.QStealExpireTick = 0;
    if (state.WResetExpireTick <= now) state.WResetExpireTick = 0;
    if (state.ECrippleExpireTick <= now) state.ECrippleExpireTick = 0;
    if (state.RImpactTick <= now) state.RImpactTick = 0;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Malphite::Geometry
