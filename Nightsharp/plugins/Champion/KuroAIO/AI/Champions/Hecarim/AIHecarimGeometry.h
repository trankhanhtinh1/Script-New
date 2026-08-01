#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Hecarim::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

enum class QState : std::uint8_t { Ready, Active };
enum class WState : std::uint8_t { Ready, ZoneActive };
enum class EState : std::uint8_t { Ready, Charging, RamPending };
enum class RState : std::uint8_t { Ready, CastPending, FearActive };

inline constexpr float kQRange = 350.0f;
inline constexpr float kQWidth = 375.0f;
inline constexpr int kQMaxStacks = 3;
inline constexpr int kQStackDurationMs = 8000;
inline constexpr float kQStackBonusPercent = 9.0f;
inline constexpr float kWRadius = 525.0f;
inline constexpr int kWDurationMs = 4000;
inline constexpr int kWTickMs = 500;
inline constexpr float kWHealPercent = 25.0f;
inline constexpr float kWAllyHealPercent = 12.5f;
inline constexpr int kEChargeDurationMs = 3000;
inline constexpr int kEMaxSpeedMs = 2500;
inline constexpr float kEMinSpeedBonus = 25.0f;
inline constexpr float kEMaxSpeedBonus = 65.0f;
inline constexpr float kERange = 1350.0f;
inline constexpr float kEWidth = 120.0f;
inline constexpr float kRMaxRange = 1000.0f;
inline constexpr float kRWidth = 200.0f;
inline constexpr float kRSpeed = 1200.0f;
inline constexpr int kRFearMinMs = 750;
inline constexpr int kRFearMaxMs = 1500;
inline constexpr int kRFearDurationMs = 1500;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline int QStacksAfterHit(int currentStacks, int now, int lastHitTick) {
    const int carried = (lastHitTick > 0 && now - lastHitTick <= kQStackDurationMs)
        ? std::clamp(currentStacks, 0, kQMaxStacks) : 0;
    return std::min(kQMaxStacks, carried + 1);
}

inline bool QStacksActive(int stacks, int now, int lastHitTick) {
    return stacks > 0 && lastHitTick > 0 && now - lastHitTick <= kQStackDurationMs;
}

inline float QRawDamage(int rank, float bonusAttackDamage, int stacks) {
    const float base = RankValue(rank, {30.0f, 60.0f, 90.0f, 120.0f, 150.0f});
    const float scaled = base + 0.90f * std::max(0.0f, bonusAttackDamage);
    const float multiplier = 1.0f + kQStackBonusPercent / 100.0f *
        static_cast<float>(std::clamp(stacks, 0, kQMaxStacks));
    return std::max(0.0f, scaled * multiplier);
}

inline bool QReachable(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || origin.IsZero() || target.IsZero()) return false;
    return origin.Distance2D(target) <= kQRange + std::max(0.0f, targetRadius);
}

inline bool WZoneUseful(const Vec3& center, const Vec3& player, int nearbyEnemies,
                        float playerHealthPercent, float minimumHealthPercent = 80.0f) {
    if (!center.IsValid() || center.IsZero() || !player.IsValid() || player.IsZero()) return false;
    if (player.Distance2D(center) > kWRadius) return false;
    return nearbyEnemies > 0 || playerHealthPercent <= minimumHealthPercent;
}

inline float WRawHeal(int rank, float damageTakenByEnemies, float bonusAttackDamage, float abilityPower,
                      bool allyDamage) {
    (void)rank;
    (void)bonusAttackDamage;
    (void)abilityPower;
    const float ratio = allyDamage ? kWAllyHealPercent : kWHealPercent;
    return std::max(0.0f, damageTakenByEnemies) * ratio / 100.0f;
}

inline float ESpeedBonus(int elapsedMs) {
    if (elapsedMs <= 0) return kEMinSpeedBonus;
    const float progress = std::clamp(static_cast<float>(elapsedMs) /
        static_cast<float>(kEMaxSpeedMs), 0.0f, 1.0f);
    return kEMinSpeedBonus + (kEMaxSpeedBonus - kEMinSpeedBonus) * progress;
}

inline float ERamDamage(int rank, float bonusAttackDamage, float distanceTravelled) {
    const float minBase = RankValue(rank, {15.0f, 30.0f, 45.0f, 60.0f, 75.0f});
    const float maxBase = RankValue(rank, {30.0f, 60.0f, 90.0f, 120.0f, 150.0f});
    const float progress = std::clamp(std::max(0.0f, distanceTravelled) / 1200.0f, 0.0f, 1.0f);
    return minBase + (maxBase - minBase) * progress + 0.50f * std::max(0.0f, bonusAttackDamage);
}

inline bool EChargeAllowed(EState state, int elapsedMs, bool targetValid, bool endpointWall,
                           bool endpointTurret, bool unsafeEnemies, bool fleeing) {
    if (state != EState::Charging || elapsedMs < 250 || (!targetValid && !fleeing)) return false;
    if (endpointWall || endpointTurret) return false;
    return fleeing || !unsafeEnemies;
}

inline float RFearDuration(float distance) {
    const float progress = std::clamp(std::max(0.0f, distance) / 950.0f, 0.0f, 1.0f);
    return static_cast<float>(kRFearMinMs) +
           static_cast<float>(kRFearMaxMs - kRFearMinMs) * progress;
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() || end.IsZero() || target.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}

struct RCollisionTarget {
    int NetworkId = 0;
    Vec3 Position{};
    float Radius = 0.0f;
    bool Valid = false;
};

inline int FirstRCollision(const Vec3& start, const Vec3& end,
                           const std::array<RCollisionTarget, 8>& targets) {
    if (!start.IsValid() || !end.IsValid() || start.IsZero() || end.IsZero()) return 0;
    int result = 0;
    float closest = 2.0f;
    for (const auto& candidate : targets) {
        if (!candidate.Valid || candidate.NetworkId == 0 ||
            !SegmentHits(start, end, candidate.Position, kRWidth * 0.5f, candidate.Radius)) continue;
        const auto projection = SharedGeometry::ProjectPointToSegment2D(candidate.Position, start, end);
        if (projection.T < closest) {
            closest = projection.T;
            result = candidate.NetworkId;
        }
    }
    return result;
}

inline bool RCommitAllowed(bool targetValid, bool targetProtected, bool endpointWall,
                           bool endpointTurret, int enemiesAtEndpoint, int maximumEnemies,
                           bool lethal, bool fleeing) {
    if (!targetValid || targetProtected || endpointWall || endpointTurret) return false;
    if (lethal || fleeing) return true;
    return enemiesAtEndpoint <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Hecarim::Geometry
