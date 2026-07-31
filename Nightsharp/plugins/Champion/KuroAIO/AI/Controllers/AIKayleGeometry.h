#pragma once

// Kayle-only deterministic mechanics. Runtime prediction, target validity,
// collision, resource and event reconciliation remain in AIKayleController.h.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Kayle::Geometry {

using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using Vec3 = ::Vec3;

inline constexpr int kPassiveStackCap = 5;
inline constexpr int kRangedLevel = 6;
inline constexpr int kWaveLevel = 11;
inline constexpr int kPermanentWaveLevel = 16;
inline constexpr float kMeleeAttackRange = 175.0f;
inline constexpr float kRangedAttackRange = 525.0f;
inline constexpr float kQRange = 900.0f;
inline constexpr float kQWidth = 80.0f;
inline constexpr float kWRange = 900.0f;
inline constexpr float kERange = 550.0f;
inline constexpr float kRRange = 900.0f;
inline constexpr float kRRadius = 675.0f;
inline constexpr int kQShredDurationMs = 4000;
inline constexpr int kEExecuteWindowMs = 1200;
inline constexpr int kRInvulnerabilityMs = 2500;

// The passive gains one Zealous stack per attack and reaches Exalted at five;
// these helpers keep malformed runtime levels/stacks bounded and deterministic.
inline int ClampLevel(int level) { return std::clamp(level, 1, 18); }
inline int ClampPassiveStacks(int stacks) {
    return std::clamp(stacks, 0, kPassiveStackCap);
}
inline bool IsRangedLevel(int level) { return ClampLevel(level) >= kRangedLevel; }
inline bool HasWavesAtLevel(int level) { return ClampLevel(level) >= kWaveLevel; }
inline bool HasPermanentWavesAtLevel(int level) {
    return ClampLevel(level) >= kPermanentWaveLevel;
}
inline float AttackRangeForLevel(int level) {
    return IsRangedLevel(level) ? kRangedAttackRange : kMeleeAttackRange;
}
inline float PassiveMoveSpeedPercent(int stacks) {
    return 6.0f * static_cast<float>(ClampPassiveStacks(stacks));
}
inline float PassiveAttackSpeedPercent(int stacks) {
    return 6.0f * static_cast<float>(ClampPassiveStacks(stacks));
}

enum class Form : std::uint8_t { Melee, Ranged, Waves, Transcendent };
inline Form FormForLevel(int level) {
    const int safeLevel = ClampLevel(level);
    if (safeLevel >= kPermanentWaveLevel) return Form::Transcendent;
    if (safeLevel >= kWaveLevel) return Form::Waves;
    if (safeLevel >= kRangedLevel) return Form::Ranged;
    return Form::Melee;
}
inline bool UsesRangedAttacks(Form form) { return form != Form::Melee; }
inline bool UsesWaveAttacks(Form form) {
    return form == Form::Waves || form == Form::Transcendent;
}

inline float MissingHealthExecuteRatio(int rank, float abilityPower) {
    // Live tooltip scales from 8% to 10% missing health plus AP scaling.
    const float base = RankValue(std::array<float, 6>{0.0f, 8.0f, 8.5f, 9.0f, 9.5f, 10.0f}, rank);
    return std::max(0.0f, base + 0.02f * std::max(0.0f, abilityPower));
}
inline float EExecuteDamage(int rank, float abilityPower, float targetHealth,
                            float targetMissingHealth) {
    if (!std::isfinite(targetHealth) || !std::isfinite(targetMissingHealth)) return 0.0f;
    const float missing = std::clamp(targetMissingHealth, 0.0f, std::max(0.0f, targetHealth));
    const float ratio = MissingHealthExecuteRatio(rank, abilityPower) * 0.01f;
    return missing * ratio;
}

inline bool QLineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T > 0.0f && projection.T < 1.001f &&
           projection.Distance <= kQWidth * 0.5f + std::max(0.0f, targetRadius);
}
inline bool QEndpointValid(const Vec3& origin, const Vec3& endpoint) {
    return origin.IsValid() && endpoint.IsValid() && !origin.IsZero() &&
           !endpoint.IsZero() && origin.Distance2D(endpoint) <= kQRange + 0.5f;
}
inline bool RAreaHits(const Vec3& center, const Vec3& target,
                      float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}

struct EExecuteContext {
    bool Ready = false;
    bool TargetValid = false;
    bool InRange = false;
    bool NextAttackAvailable = false;
    bool PreserveAttack = true;
    bool TargetLow = false;
    bool Lethal = false;
};
inline constexpr bool ShouldCastExecute(const EExecuteContext& context) {
    return context.Ready && context.TargetValid && context.InRange &&
           context.NextAttackAvailable && !context.PreserveAttack &&
           (context.TargetLow || context.Lethal);
}

struct UltimateContext {
    bool Ready = false;
    bool AllyValid = false;
    bool AllyLow = false;
    bool AllyThreatened = false;
    bool PlayerIsAlly = false;
    bool IncomingHardCC = false;
    bool Manual = false;
    int EnemiesAtAlly = 0;
    int MaximumEnemies = 3;
};
inline constexpr bool ShouldCastUltimate(const UltimateContext& context) {
    if (!context.Ready || !context.AllyValid) return false;
    if (context.Manual || context.IncomingHardCC || context.AllyThreatened) return true;
    if (context.AllyLow) return context.EnemiesAtAlly <= std::max(0, context.MaximumEnemies);
    return context.PlayerIsAlly && context.EnemiesAtAlly >= 2;
}

struct QCollisionContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool CollisionFree = false;
    bool ProjectileWallClear = false;
    bool TargetInReach = false;
};
inline constexpr bool QCastAllowed(const QCollisionContext& context) {
    return context.Ready && context.EndpointValid && context.CollisionFree &&
           context.ProjectileWallClear && context.TargetInReach;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kayle::Geometry
