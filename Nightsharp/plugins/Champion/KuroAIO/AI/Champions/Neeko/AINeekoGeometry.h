#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Neeko::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 800.0f;
inline constexpr float kQRadius = 225.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQBurstDelay = 1.50f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr float kESpeed = 1300.0f;
inline constexpr float kERootRadius = 42.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWStealthSeconds = 0.50f;
inline constexpr float kWCloneSeconds = 3.0f;
inline constexpr float kRRadius = 600.0f;
inline constexpr float kRChannelSeconds = 1.25f;
inline constexpr float kRLandingDelay = 0.15f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RRankValue(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {35.0f, 55.0f, 75.0f, 95.0f, 115.0f}) +
           0.50f * std::max(0.0f, abilityPower);
}
inline constexpr float QBloomRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 105.0f, 140.0f, 175.0f, 210.0f}) +
           0.65f * std::max(0.0f, abilityPower);
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {80.0f, 115.0f, 150.0f, 185.0f, 220.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RRankValue(rank, {150.0f, 350.0f, 550.0f}) +
           1.30f * std::max(0.0f, abilityPower);
}

inline bool CircleHits(const Vec3& center, const Vec3& target,
                       float radius, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, radius) +
               std::max(0.0f, targetRadius);
}

inline bool BloomHits(const Vec3& origin, const Vec3& target,
                      float targetRadius = 0.0f) {
    return CircleHits(origin, target, kQRadius, targetRadius);
}

inline bool RootLineHits(const Vec3& origin, const Vec3& endpoint,
                         const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kEWidth * 0.5f + kERootRadius +
               std::max(0.0f, targetRadius);
}

struct CollisionResult {
    bool ValidPath = false;
    bool TargetHit = false;
    bool BlockedByChampion = false;
    int CollisionCount = 0;
};
inline bool ECollisionAcceptable(const CollisionResult& result) {
    return result.ValidPath && result.TargetHit && !result.BlockedByChampion;
}

inline Vec3 ClampLineEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}

inline float TravelSeconds(const Vec3& origin, const Vec3& endpoint,
                           float speed = kESpeed, float delay = kQDelay) {
    if (!origin.IsValid() || !endpoint.IsValid()) return 0.0f;
    return std::max(0.0f, delay) + origin.Distance2D(endpoint) /
        std::max(1.0f, speed);
}

struct DisguiseState {
    bool PassiveReady = true;
    bool Disguised = false;
    bool CloneActive = false;
    int StartedTick = 0;
    int ExpireTick = 0;
};
inline bool DisguiseVisible(const DisguiseState& state, int now) {
    return state.Disguised && (state.ExpireTick <= 0 || now < state.ExpireTick);
}
inline bool CloneVisible(const DisguiseState& state, int now) {
    return state.CloneActive && (state.ExpireTick <= 0 || now < state.ExpireTick);
}

enum class UltimateStage : std::uint8_t { Idle, Channeling, Landing };
struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool Lethal = false;
    bool Defensive = false;
    bool AttackWindingUp = false;
    bool UnderEnemyTurret = false;
    int PredictedHits = 0;
    int MinimumHits = 2;
};
inline bool ShouldStartUltimate(const UltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.PredictionHits ||
        context.UnderEnemyTurret) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive)
        return false;
    return context.Lethal || context.Defensive ||
           context.PredictedHits >= std::max(1, context.MinimumHits);
}
inline bool ShouldAbortUltimate(bool channeling, bool playerDead,
                               bool hardCrowdControl) {
    return channeling && (playerDead || hardCrowdControl);
}
inline bool ShouldLandUltimate(UltimateStage stage, bool channelObserved,
                               bool landingPositionValid, int predictedHits,
                               int minimumHits, bool lethal, bool defensive) {
    if (stage != UltimateStage::Channeling || !channelObserved ||
        !landingPositionValid) return false;
    return lethal || defensive || predictedHits >= std::max(1, minimumHits);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Neeko::Geometry
