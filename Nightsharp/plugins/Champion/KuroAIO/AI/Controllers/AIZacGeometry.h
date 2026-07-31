#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Zac::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

enum class PassiveState : std::uint8_t { Ready, Split, Cooldown };
enum class QState : std::uint8_t { Ready, FirstHit, SecondCast };
enum class EState : std::uint8_t { Ready, Charging, LaunchPending };
enum class RState : std::uint8_t { Ready, Bouncing, Carrying };

inline constexpr float kQRange = 800.0f;
inline constexpr float kQWidth = 120.0f;
inline constexpr float kQPairRadius = 240.0f;
inline constexpr int kQPairWindowMs = 4000;
inline constexpr int kQSlowDurationMs = 2000;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWHealthCostPercent = 4.0f;
inline constexpr float kWTargetMaxHealthPercent = 6.0f;
inline constexpr float kERangeMin = 400.0f;
inline constexpr float kERangeMax = 1800.0f;
inline constexpr float kEWidth = 275.0f;
inline constexpr int kEChargeMinMs = 0;
inline constexpr int kEChargeMaxMs = 1500;
inline constexpr float kEKnockupRadius = 275.0f;
inline constexpr int kEKnockupDurationMs = 1000;
inline constexpr float kRRadius = 300.0f;
inline constexpr int kRBounces = 4;
inline constexpr int kRBounceIntervalMs = 500;
inline constexpr int kRCarryDurationMs = 1000;
inline constexpr float kRDamagePercent = 5.0f;

inline float ClampPercent(float value) {
    return std::clamp(std::isfinite(value) ? value : 0.0f, 0.0f, 100.0f);
}

inline bool PassiveReady(PassiveState state, int now, int cooldownEnd) {
    return state == PassiveState::Ready && now >= cooldownEnd;
}

inline int PassiveBlobsAfterDeath(int blobs, bool passiveReady) {
    if (!passiveReady) return 0;
    return std::clamp(blobs, 0, 4);
}

inline float BlobHeal(int rank, float maxHealth, bool collected) {
    static constexpr std::array<float, 5> values{4.0f, 4.5f, 5.0f, 5.5f, 6.0f};
    if (!collected || !std::isfinite(maxHealth) || maxHealth <= 0.0f) return 0.0f;
    return maxHealth * values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] / 100.0f;
}

inline bool QFirstHitAllowed(QState state, bool targetValid, bool protectedTarget,
                             bool attacking) {
    return state == QState::Ready && targetValid && !protectedTarget && !attacking;
}

inline bool QSecondCastAllowed(QState state, int elapsedMs, bool firstTargetValid,
                               bool secondTargetValid, bool protectedTarget) {
    return state == QState::FirstHit && elapsedMs >= 0 && elapsedMs <= kQPairWindowMs &&
           firstTargetValid && secondTargetValid && !protectedTarget;
}

inline bool QPairHits(const Vec3& first, const Vec3& second, float radius = kQPairRadius) {
    if (!first.IsValid() || !second.IsValid() || first.IsZero() || second.IsZero()) return false;
    return first.Distance2D(second) <= std::max(0.0f, radius);
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 5> base{40.0f, 55.0f, 70.0f, 85.0f, 100.0f};
    return base[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] +
           0.30f * std::max(0.0f, abilityPower);
}

inline float WRawDamage(int rank, float targetMaxHealth, float abilityPower) {
    static constexpr std::array<float, 5> base{40.0f, 55.0f, 70.0f, 85.0f, 100.0f};
    const float health = std::max(0.0f, targetMaxHealth);
    return base[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] +
           0.02f * health + 0.02f * health * std::max(0.0f, abilityPower) / 100.0f;
}

inline bool WCastAllowed(float healthPercent, int enemies, bool lethal, bool threatened) {
    if (!std::isfinite(healthPercent) || healthPercent <= kWHealthCostPercent) return false;
    return enemies > 0 || lethal || threatened;
}

inline float ERangeForCharge(int elapsedMs) {
    const float progress = std::clamp(static_cast<float>(elapsedMs) /
        static_cast<float>(kEChargeMaxMs), 0.0f, 1.0f);
    return kERangeMin + (kERangeMax - kERangeMin) * progress;
}

inline bool EChargeAllowed(EState state, int elapsedMs, bool targetValid, bool fleeing) {
    return state == EState::Charging && elapsedMs >= kEChargeMinMs &&
           elapsedMs <= kEChargeMaxMs && (targetValid || fleeing);
}

inline bool EReleaseSafe(EState state, int elapsedMs, bool endpointWall, bool endpointTurret,
                         int enemiesAtEndpoint, int maximumEnemies, bool lethal, bool fleeing) {
    if (state != EState::Charging || elapsedMs < kEChargeMinMs) return false;
    if (endpointWall || (endpointTurret && !lethal && !fleeing)) return false;
    return lethal || fleeing || enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

inline float EImpactDamage(int rank, float abilityPower, int elapsedMs) {
    static constexpr std::array<float, 5> base{60.0f, 90.0f, 120.0f, 150.0f, 180.0f};
    const float progress = std::clamp(static_cast<float>(std::max(0, elapsedMs)) /
        static_cast<float>(kEChargeMaxMs), 0.0f, 1.0f);
    return base[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] *
        (0.75f + 0.25f * progress) + 0.90f * std::max(0.0f, abilityPower);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

struct RCollisionTarget {
    int NetworkId = 0;
    Vec3 Position{};
    float Radius = 0.0f;
    bool Valid = false;
};

inline int FirstRHit(const Vec3& center, const std::array<RCollisionTarget, 8>& targets) {
    if (!center.IsValid() || center.IsZero()) return 0;
    float closest = std::numeric_limits<float>::max();
    int result = 0;
    for (const auto& target : targets) {
        if (!target.Valid || target.NetworkId == 0 || !target.Position.IsValid() ||
            target.Position.IsZero() || center.Distance2D(target.Position) >
            kRRadius + std::max(0.0f, target.Radius)) continue;
        const float distance = center.Distance2D(target.Position);
        if (distance < closest) { closest = distance; result = target.NetworkId; }
    }
    return result;
}

inline bool RCommitAllowed(RState state, bool targetValid, bool targetProtected,
                           bool endpointWall, bool endpointTurret, int enemiesAtLanding,
                           int maximumEnemies, bool lethal, bool fleeing) {
    if (state != RState::Ready || !targetValid || targetProtected || endpointWall) return false;
    if (endpointTurret && !lethal && !fleeing) return false;
    return lethal || fleeing || enemiesAtLanding <= std::max(0, maximumEnemies);
}

inline bool RCanCarry(RState state, bool targetValid, bool targetProtected,
                      int elapsedMs, bool wallAtLanding) {
    return state == RState::Bouncing && targetValid && !targetProtected &&
           elapsedMs >= 0 && elapsedMs <= kRCarryDurationMs && !wallAtLanding;
}

inline int NextBounce(int currentBounce) {
    return std::clamp(currentBounce + 1, 0, kRBounces);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zac::Geometry
