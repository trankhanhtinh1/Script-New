#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::MonkeyKing::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 300.0f;
inline constexpr float kQWidth = 65.0f;
inline constexpr float kWRange = 275.0f;
inline constexpr float kCloneDurationMs = 2500.0f;
inline constexpr float kERange = 625.0f;
inline constexpr float kEDashRadius = 110.0f;
inline constexpr float kRRadius = 315.0f;
inline constexpr float kRDurationMs = 2000.0f;
inline constexpr float kRTickMs = 500.0f;
inline constexpr int kRMaximumTicks = 4;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float QRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {20.0f, 45.0f, 70.0f, 95.0f, 120.0f}) +
           0.40f * std::max(0.0f, bonusAttackDamage);
}

inline constexpr float QArmorReductionPercent(int rank) {
    return RankValue(rank, {10.0f, 15.0f, 20.0f, 25.0f, 30.0f});
}

inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {80.0f, 110.0f, 140.0f, 170.0f, 200.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}

inline constexpr float RRawDamagePerTick(int rank, float totalAttackDamage,
                                          float targetMaximumHealth) {
    return 1.10f * std::max(0.0f, totalAttackDamage) +
           RankValue(rank, {1.0f, 1.5f, 2.0f, 2.5f, 3.0f}) *
               std::max(0.0f, targetMaximumHealth) / 100.0f;
}

inline constexpr float RTotalRawDamage(int rank, float totalAttackDamage,
                                       float targetMaximumHealth, int ticks) {
    return RRawDamagePerTick(rank, totalAttackDamage, targetMaximumHealth) *
           static_cast<float>(std::clamp(ticks, 0, kRMaximumTicks));
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (start.IsZero() || end.IsZero() || target.IsZero()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
           std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

inline bool SpinHits(const Vec3& center, const Vec3& target,
                     float targetRadius = 0.0f) {
    if (center.IsZero() || target.IsZero()) return false;
    return center.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}

struct CloneState {
    int NetworkId = 0;
    Vec3 Position{};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Confirmed = false;
};

inline void RecordClone(CloneState& clone, int networkId, const Vec3& position,
                        int spawnTick, int durationMs = static_cast<int>(kCloneDurationMs)) {
    clone.NetworkId = networkId;
    clone.Position = position;
    clone.SpawnTick = spawnTick;
    clone.ExpireTick = spawnTick + std::max(1, durationMs);
    clone.Confirmed = true;
}

inline bool CloneActive(const CloneState& clone, int now) {
    return clone.Confirmed && clone.NetworkId != 0 && !clone.Position.IsZero() &&
           now >= clone.SpawnTick && now <= clone.ExpireTick;
}

inline bool CloneMatches(int networkId, const CloneState& clone) {
    return networkId != 0 && networkId == clone.NetworkId;
}

struct DashContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool TargetValid = false;
    bool UnderEnemyTurret = false;
    bool WallUnknown = false;
    bool Defensive = false;
    bool Lethal = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline bool ShouldNimbusStrike(const DashContext& context) {
    if (!context.Ready || !context.EndpointValid || !context.TargetValid ||
        context.UnderEnemyTurret || context.WallUnknown) return false;
    if (context.Defensive || context.Lethal) return true;
    return context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

enum class SpinPosture { Idle, Channeling, RecastReady };

struct SpinContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetInRadius = false;
    bool Interrupt = false;
    bool Defensive = false;
    bool Lethal = false;
    bool AttackWindingUp = false;
    bool UnderEnemyTurret = false;
    int PredictedTargets = 0;
    int MinimumTargets = 2;
};

inline bool ShouldStartSpin(const SpinContext& context) {
    if (!context.Ready || !context.TargetValid || !context.TargetInRadius ||
        context.UnderEnemyTurret) return false;
    if (context.AttackWindingUp && !context.Interrupt && !context.Defensive &&
        !context.Lethal) return false;
    return context.Interrupt || context.Defensive || context.Lethal ||
           context.PredictedTargets >= std::max(1, context.MinimumTargets);
}

inline bool ShouldContinueSpin(const SpinContext& context, SpinPosture posture,
                               int elapsedMs) {
    if (posture == SpinPosture::Idle || elapsedMs < 0 || elapsedMs >
        static_cast<int>(kRDurationMs)) return false;
    if (context.Interrupt || context.Defensive || context.Lethal)
        return true;
    return context.TargetValid && context.TargetInRadius;
}

inline bool ShouldRecastSpin(SpinPosture posture, int elapsedMs,
                             bool targetLost, bool interrupt, bool lethal) {
    if (posture != SpinPosture::Channeling || elapsedMs < static_cast<int>(kRTickMs))
        return false;
    return targetLost || interrupt || lethal || elapsedMs >= static_cast<int>(kRDurationMs);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::MonkeyKing::Geometry
