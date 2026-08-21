#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Briar::Geometry {
using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

enum class FrenzyState : std::uint8_t { Calm, Frenzy, Taunt };
enum class QState : std::uint8_t { Ready, LeapPending, StunPending };
enum class WState : std::uint8_t { Ready, Frenzy, RecastPending };
enum class EState : std::uint8_t { Ready, Charging, ReleasePending };
enum class RState : std::uint8_t { Ready, Traveling, Berserk };

inline constexpr float kAttackReach = 125.0f;
inline constexpr float kQRange = 475.0f;
inline constexpr int kQStunDurationMs = 500;
inline constexpr int kQFollowWindowMs = 850;
inline constexpr float kWRange = 350.0f;
inline constexpr int kWFrenzyDurationMs = 5000;
inline constexpr int kWRecastWindowMs = 5000;
inline constexpr float kERange = 600.0f;
inline constexpr float kEWidth = 220.0f;
inline constexpr int kEChargeMinMs = 250;
inline constexpr int kEChargeMaxMs = 1000;
inline constexpr int kEReleaseTailMs = 200;
inline constexpr float kRGlobalRange = 12000.0f;
inline constexpr float kRWidth = 160.0f;
inline constexpr int kRTravelTimeoutMs = 5000;
inline constexpr int kRBerserkDurationMs = 25000;

inline bool InRange(const Vec3& origin, const Vec3& target, float range, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= std::max(0.0f, range) + std::max(0.0f, targetRadius);
}
inline float ERangeForCharge(int elapsedMs) {
    const float t = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(kEChargeMaxMs), 0.0f, 1.0f);
    return 400.0f + 200.0f * t;
}
inline bool QCastAllowed(QState state, FrenzyState frenzy, bool targetValid,
                         bool targetProtected, bool attacking) {
    return state == QState::Ready && frenzy != FrenzyState::Taunt && targetValid && !targetProtected &&
        (!attacking || frenzy != FrenzyState::Calm);
}
inline bool QStunConfirmed(QState state, int elapsedMs, bool targetStillValid) {
    return state == QState::LeapPending && targetStillValid && elapsedMs >= 0 && elapsedMs <= kQFollowWindowMs;
}
inline bool FrenzyCommitSafe(FrenzyState state, bool targetValid, bool targetProtected,
                             bool underEnemyTurret, int nearbyEnemies, int maximumEnemies,
                             bool lethal, bool fleeing, bool playerLow) {
    if (!targetValid || targetProtected || state == FrenzyState::Taunt) return false;
    if (underEnemyTurret && !lethal && !fleeing && !playerLow) return false;
    if (lethal || fleeing || playerLow) return true;
    return nearbyEnemies <= std::max(0, maximumEnemies);
}
inline bool WRecastAllowed(WState state, int elapsedMs, float playerHealthPercent,
                           bool lethal, bool threatened, bool targetInRange) {
    if (state != WState::Frenzy || !targetInRange || elapsedMs < 0 || elapsedMs > kWRecastWindowMs) return false;
    if (lethal || threatened) return true;
    return std::isfinite(playerHealthPercent) && playerHealthPercent <= 72.0f;
}
inline float EDamageReductionPercent(int) { return 35.0f; }
inline float EHealPercent(float missingHealthPercent, int elapsedMs) {
    if (!std::isfinite(missingHealthPercent) || missingHealthPercent <= 0.0f) return 0.0f;
    const float charge = std::clamp(static_cast<float>(elapsedMs) / static_cast<float>(kEChargeMaxMs), 0.0f, 1.0f);
    return std::min(missingHealthPercent, 8.5f + 9.0f * charge);
}
inline bool EChargeAllowed(EState state, int elapsedMs, bool threatened, bool targetNearby) {
    return state == EState::Charging && elapsedMs >= kEChargeMinMs && elapsedMs <= kEChargeMaxMs &&
        (threatened || targetNearby || elapsedMs >= kEChargeMaxMs - kEReleaseTailMs);
}
inline bool EReleaseSafe(EState state, int elapsedMs, bool endpointWall, bool endpointTurret,
                         int enemiesAtEndpoint, int maximumEnemies, bool defensive, bool fleeing) {
    if (state != EState::Charging || elapsedMs < kEChargeMinMs || endpointWall) return false;
    if (endpointTurret && !defensive && !fleeing) return false;
    if (defensive || fleeing) return true;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}
inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() || start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f && projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
struct RCollisionTarget { int NetworkId = 0; Vec3 Position{}; float Radius = 0.0f; bool Valid = false; };
inline int FirstRHit(const Vec3& start, const Vec3& end, const std::array<RCollisionTarget, 8>& targets) {
    if (!start.IsValid() || !end.IsValid() || start.IsZero() || end.IsZero()) return 0;
    int result = 0; float closest = 2.0f;
    for (const auto& target : targets) {
        if (!target.Valid || target.NetworkId == 0 || !SegmentHits(start, end, target.Position, kRWidth * 0.5f, target.Radius)) continue;
        const auto projection = ProjectPointToSegment2D(target.Position, start, end);
        if (projection.T < closest) { closest = projection.T; result = target.NetworkId; }
    }
    return result;
}
inline bool RCommitAllowed(bool targetValid, bool targetProtected, bool endpointWall, bool endpointTurret,
                           int enemiesAtEndpoint, int maximumEnemies, bool lethal, bool fleeing, bool playerLow) {
    if (!targetValid || targetProtected || endpointWall) return false;
    if (endpointTurret && !lethal && !fleeing && !playerLow) return false;
    if (lethal || fleeing || playerLow) return true;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}
inline bool ResourceAvailable(float resource, float maximum, float reservePercent, float cost) {
    if (!std::isfinite(resource) || !std::isfinite(maximum) || !std::isfinite(reservePercent) || !std::isfinite(cost) || maximum <= 0.0f || cost < 0.0f) return false;
    const float reserve = maximum * std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
    return resource >= cost && resource - cost >= reserve;
}
inline bool CooldownAvailable(bool runtimeReady, int elapsedMs, int minimumGapMs = 45) {
    return runtimeReady && elapsedMs >= std::max(0, minimumGapMs);
}
inline float QRawDamage(int rank, float abilityPower, float bonusAttackDamage) {
    static constexpr std::array<float, 5> base{35.0f, 60.0f, 85.0f, 110.0f, 135.0f};
    return base[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] + 0.6f * std::max(0.0f, abilityPower) + 0.8f * std::max(0.0f, bonusAttackDamage);
}
inline float WRecastHeal(float missingHealth, float damageDealt) {
    if (!std::isfinite(missingHealth) || !std::isfinite(damageDealt)) return 0.0f;
    return std::max(0.0f, std::min(std::max(0.0f, missingHealth), std::max(0.0f, damageDealt) * 0.36f));
}
inline float EReleaseDamage(int rank, float bonusAttackDamage, float abilityPower,
                            bool wallImpact = false) {
    static constexpr std::array<float, 5> base{45.0f, 80.0f, 115.0f, 150.0f, 185.0f};
    static constexpr std::array<float, 5> wallBase{65.0f, 140.0f, 215.0f, 290.0f, 365.0f};
    const auto index = static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1);
    const float adRatio = wallImpact ? 2.4f : 1.0f;
    const float apRatio = wallImpact ? 2.4f : 1.0f;
    return (wallImpact ? wallBase[index] : base[index]) +
        adRatio * std::max(0.0f, bonusAttackDamage) +
        apRatio * std::max(0.0f, abilityPower);
}
inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 3> base{50.0f, 150.0f, 250.0f};
    return base[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)] + 1.3f * std::max(0.0f, abilityPower);
}
} // namespace Plugins::KuroAIO::AI::Controllers::Briar::Geometry
