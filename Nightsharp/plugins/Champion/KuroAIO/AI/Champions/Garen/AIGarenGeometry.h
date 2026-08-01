#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Garen::Geometry {

using Vec3 = ::Vec3;

enum class SpinState : std::uint8_t { Ready, Spinning, Expired };
enum class CourageState : std::uint8_t { Ready, Active, Expired };

enum class DecisiveStrikeState : std::uint8_t { Ready, Armed, Consumed, Expired };

inline constexpr float kAttackReach = 125.0f;
inline constexpr float kJudgmentRadius = 325.0f;
inline constexpr float kDecisiveStrikeRange = 175.0f;
inline constexpr float kDemacianJusticeRange = 400.0f;
inline constexpr int kJudgmentDurationMs = 3000;
inline constexpr int kJudgmentBodyLifetimeMs = 3250;
inline constexpr int kDecisiveStrikeWindowMs = 4000;
inline constexpr int kDecisiveStrikeSilenceMs = 1500;
inline constexpr int kCourageDurationMs = 2000;
inline constexpr float kCourageDamageReduction = 0.30f;
inline constexpr float kCourageBaseShield[] = {80.0f, 110.0f, 140.0f, 170.0f, 200.0f};

inline bool SpinActive(SpinState state, int elapsedMs) {
    return state == SpinState::Spinning && elapsedMs >= 0 && elapsedMs < kJudgmentDurationMs;
}

inline bool SpinBodyPresent(SpinState state, int elapsedMs, bool bodyVisible) {
    return SpinActive(state, elapsedMs) && bodyVisible && elapsedMs < kJudgmentBodyLifetimeMs;
}

inline bool SpinTargetTrackable(SpinState state, int elapsedMs, int trackedTargetId,
                                int candidateTargetId, const Vec3& body,
                                const Vec3& candidate, float candidateRadius) {
    if (!SpinBodyPresent(state, elapsedMs, body.IsValid()) || trackedTargetId == 0 ||
        candidateTargetId == 0 || trackedTargetId != candidateTargetId ||
        !candidate.IsValid() || candidate.IsZero()) return false;
    return body.Distance2D(candidate) <= kJudgmentRadius + std::max(0.0f, candidateRadius);
}

inline bool InJudgmentReach(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kJudgmentRadius + std::max(0.0f, targetRadius);
}

inline bool InAttackReach(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kAttackReach + std::max(0.0f, targetRadius);
}

inline bool DecisiveStrikeWindow(DecisiveStrikeState state, int elapsedMs) {
    return state == DecisiveStrikeState::Armed && elapsedMs >= 0 &&
        elapsedMs <= kDecisiveStrikeWindowMs;
}

inline bool DecisiveStrikeCanArm(DecisiveStrikeState state, bool targetInAttackRange,
                                 bool attackReady, bool preservingWindup) {
    return state == DecisiveStrikeState::Ready && targetInAttackRange && attackReady &&
        !preservingWindup;
}
inline bool SilenceActive(int silenceUntilTick, int nowTick) {
    return silenceUntilTick > nowTick;
}
inline bool CourageActive(CourageState state, int elapsedMs) {
    return state == CourageState::Active && elapsedMs >= 0 && elapsedMs < kCourageDurationMs;
}

inline float CourageShield(int rank, float bonusHealth) {
    const int index = std::clamp(rank - 1, 0, 4);
    return kCourageBaseShield[index] + 0.20f * std::max(0.0f, bonusHealth);
}

inline float JudgmentTickDamage(int rank, float totalAttackDamage, float bonusAttackDamage,
                                bool isolatedTarget) {
    static constexpr std::array<float, 5> base{4.0f, 7.0f, 10.0f, 13.0f, 16.0f};
    static constexpr std::array<float, 5> totalAdRatio{0.36f, 0.37f, 0.38f, 0.39f, 0.40f};
    const int index = std::clamp(rank - 1, 0, 4);
    const float raw = base[static_cast<std::size_t>(index)] +
        totalAdRatio[static_cast<std::size_t>(index)] * std::max(0.0f, totalAttackDamage) +
        (isolatedTarget ? 0.25f * std::max(0.0f, bonusAttackDamage) : 0.0f);
    return std::max(0.0f, raw);
}

inline float DemacianJusticeDamage(int rank, float missingHealth) {
    static constexpr std::array<float, 5> base{150.0f, 150.0f, 300.0f, 450.0f, 450.0f};
    static constexpr std::array<float, 5> missingHealthRatio{0.25f, 0.25f, 0.30f, 0.35f, 0.35f};
    const int index = std::clamp(rank, 1, 3);
    return base[static_cast<std::size_t>(index)] +
        missingHealthRatio[static_cast<std::size_t>(index)] * std::max(0.0f, missingHealth);
}

inline bool ExecuteLethal(float targetHealth, float targetShield, float trueDamage) {
    if (!std::isfinite(targetHealth) || !std::isfinite(targetShield) ||
        !std::isfinite(trueDamage)) return false;
    return trueDamage >= std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

inline bool SafeJudgmentCommit(bool endpointWall, bool endpointTurret, int enemiesAtBody,
                               int maximumEnemies, bool lethal, bool defensive) {
    if (endpointWall || endpointTurret) return false;
    if (lethal || defensive) return true;
    return enemiesAtBody <= std::max(0, maximumEnemies);
}

inline bool ResourceFreeFarmPolicy(float healthPercent, bool passiveReady,
                                   bool nearbyEnemyThreat, bool jungleMode) {
    if (!std::isfinite(healthPercent) || healthPercent <= 0.0f) return false;
    if (nearbyEnemyThreat) return healthPercent >= 30.0f;
    if (jungleMode) return healthPercent >= 18.0f || passiveReady;
    return healthPercent >= 12.0f || passiveReady;
}

inline bool CooldownAvailable(bool runtimeReady, int elapsedMs, int minimumGapMs = 45) {
    return runtimeReady && elapsedMs >= std::max(0, minimumGapMs);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Garen::Geometry
