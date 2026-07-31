#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Xerath::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values.
inline constexpr float kQMinRange = 750.0f;
inline constexpr float kQMaxRange = 1125.0f;
inline constexpr float kQWidth = 145.0f;
inline constexpr float kQChargeSeconds = 4.0f;
inline constexpr float kWRange = 1000.0f;
inline constexpr float kWOuterRadius = 250.0f;
inline constexpr float kWCenterRadius = 100.0f;
inline constexpr float kWDelay = 0.50f;
inline constexpr float kERange = 1050.0f;
inline constexpr float kELineRange = 1125.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kEMissileSpeed = 1600.0f;
inline constexpr float kRRange = 5000.0f;
inline constexpr float kRChannelSeconds = 10.0f;
inline constexpr float kRTrajectorySeconds = 0.60f;
inline constexpr float kRShotInterval = 0.60f;
inline constexpr float kRRadius = 100.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float RankValue(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}

inline float QChargeFraction(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds)) return 0.0f;
    return std::clamp(elapsedSeconds / kQChargeSeconds, 0.0f, 1.0f);
}
inline float QRangeForCharge(float elapsedSeconds) {
    return kQMinRange + (kQMaxRange - kQMinRange) * QChargeFraction(elapsedSeconds);
}
inline bool QCanRelease(bool charging, float elapsedSeconds) {
    return charging && std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.10f;
}

inline bool LineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                     float width, float targetRadius = 0.0f) {
    if (endpoint.IsZero() || target.IsZero() ||
        !std::isfinite(width) || width < 0.0f) return false;
    Vec3 segment = endpoint - origin;
    segment.y = 0.0f;
    Vec3 relative = target - origin;
    relative.y = 0.0f;
    const float lengthSquared = segment.Dot(segment);
    if (lengthSquared <= 0.001f || !std::isfinite(lengthSquared)) return false;
    const float rawT = relative.Dot(segment) / lengthSquared;
    const float reachAllowance = std::max(0.0f, targetRadius) /
        std::sqrt(lengthSquared);
    if (rawT < -reachAllowance || rawT > 1.0f + reachAllowance) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= width * 0.5f + std::max(0.0f, targetRadius);
}
inline bool QLineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 0.0f) {
    const float length = endpoint.IsZero() ? 0.0f : origin.Distance2D(endpoint);
    return length <= kQMaxRange + 0.01f &&
           LineHits(origin, endpoint, target, kQWidth, targetRadius);
}

inline float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 5>{75.0f, 115.0f, 155.0f, 195.0f, 235.0f}) +
           0.85f * std::max(0.0f, abilityPower);
}
inline float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 5>{50.0f, 85.0f, 120.0f, 155.0f, 190.0f}) +
           0.65f * std::max(0.0f, abilityPower);
}
inline float WCenterDamage(int rank, float abilityPower) {
    return WRawDamage(rank, abilityPower) * 1.667f;
}
inline bool WOuterHits(const Vec3& center, const Vec3& target,
                       float targetRadius = 0.0f) {
    return !center.IsZero() && !target.IsZero() &&
           center.Distance2D(target) <= kWOuterRadius + std::max(0.0f, targetRadius);
}
inline bool WCenterHits(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    return !center.IsZero() && !target.IsZero() &&
           center.Distance2D(target) <= kWCenterRadius + std::max(0.0f, targetRadius);
}

inline float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 5>{70.0f, 100.0f, 130.0f, 160.0f, 190.0f}) +
           0.45f * std::max(0.0f, abilityPower);
}
inline float EStunDuration(float distance) {
    if (!std::isfinite(distance)) return 0.75f;
    return std::clamp(0.75f + std::max(0.0f, distance) * 0.17f / 100.0f,
                      0.75f, 2.25f);
}
inline bool ELineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 0.0f) {
    const float length = endpoint.IsZero() ? 0.0f : origin.Distance2D(endpoint);
    return length <= kELineRange + 0.01f &&
           LineHits(origin, endpoint, target, kEWidth, targetRadius);
}

inline constexpr int RShotsForRank(int rank) {
    return static_cast<int>(RankValue(rank, std::array<float, 3>{4.0f, 5.0f, 6.0f}));
}
inline constexpr float RBaseDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 3>{170.0f, 220.0f, 270.0f}) + 0.45f * std::max(0.0f, abilityPower);
}
inline constexpr float RRampDamage(int rank, float abilityPower) {
    return RankValue(rank, std::array<float, 3>{20.0f, 25.0f, 30.0f}) + 0.05f * std::max(0.0f, abilityPower);
}
inline constexpr float RShotDamage(int rank, float abilityPower, int priorHits) {
    return RBaseDamage(rank, abilityPower) +
           RRampDamage(rank, abilityPower) * std::max(0, priorHits);
}

struct RAmmoState {
    int Remaining = 0;
    int Maximum = 0;
    int PriorHits = 0;
    bool Channeling = false;
};
inline constexpr RAmmoState BeginR(int rank) {
    const int shots = RShotsForRank(rank);
    return {shots, shots, 0, true};
}
inline constexpr bool RCanFire(const RAmmoState& state, int nowTick, int lastShotTick) {
    return state.Channeling && state.Remaining > 0 && nowTick >= 0 &&
           (lastShotTick < 0 || nowTick - lastShotTick >=
                                  static_cast<int>(kRShotInterval * 1000.0f));
}
inline constexpr RAmmoState ConsumeRShot(RAmmoState state) {
    if (state.Remaining > 0) {
        --state.Remaining;
        ++state.PriorHits;
    }
    return state;
}
inline constexpr bool RChannelSafe(bool owned, bool interrupted, bool mobilityLocked,
                                   bool underEnemyTurret, int nearbyEnemies,
                                   int maximumEnemies) {
    return owned && !interrupted && !mobilityLocked &&
           (!underEnemyTurret || nearbyEnemies <= std::max(0, maximumEnemies));
}
inline constexpr bool RShouldStop(const RAmmoState& state, int nowTick, int startTick,
                                  bool interrupted, bool mobilityLocked) {
    return !state.Channeling || state.Remaining <= 0 || interrupted || mobilityLocked ||
           nowTick - startTick >= static_cast<int>(kRChannelSeconds * 1000.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Xerath::Geometry
