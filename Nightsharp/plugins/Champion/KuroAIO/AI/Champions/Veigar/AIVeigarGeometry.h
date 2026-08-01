#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Veigar::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values.
inline constexpr float kQRange = 1000.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 2200.0f;
inline constexpr float kWRange = 950.0f;
inline constexpr float kWMeteorRadius = 225.0f;
inline constexpr float kWImpactDelay = 1.20f;
inline constexpr float kERange = 700.0f;
inline constexpr float kECageRadius = 390.0f;
inline constexpr float kECageEdgeThickness = 80.0f;
inline constexpr float kECageDelay = 0.50f;
inline constexpr float kRRange = 650.0f;
inline constexpr float kRDelay = 0.25f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)];
}

inline constexpr int QMinionStack(bool largeMinion) { return largeMinion ? 3 : 1; }
inline constexpr int ChampionTakedownStacks() { return 5; }
inline constexpr float PassiveAbilityAp(int stacks) {
    return static_cast<float>(std::max(0, stacks));
}
inline constexpr float WCooldownMultiplier(int passiveStacks) {
    return 1.0f - 0.10f * static_cast<float>(std::max(0, passiveStacks) / 50);
}

inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {80.0f, 120.0f, 160.0f, 200.0f, 240.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {85.0f, 140.0f, 195.0f, 250.0f, 305.0f}) +
           RankValue(rank, {0.70f, 0.80f, 0.90f, 1.00f, 1.10f}) *
               std::max(0.0f, abilityPower);
}
inline constexpr float ERawDamage(int, float) { return 0.0f; }
inline constexpr float RBaseDamage(int rank) {
    return RankValue3(rank, {175.0f, 250.0f, 325.0f});
}
inline constexpr float RApRatio(int rank) {
    return RankValue3(rank, {0.65f, 0.70f, 0.75f});
}
inline constexpr float RExecuteMultiplier(float targetHealthPercent) {
    const float missing = 100.0f - std::clamp(targetHealthPercent, 0.0f, 100.0f);
    return 1.0f + missing / 100.0f;
}
inline constexpr float RRawDamage(int rank, float abilityPower,
                                  float targetHealthPercent) {
    return (RBaseDamage(rank) + RApRatio(rank) * std::max(0.0f, abilityPower)) *
           RExecuteMultiplier(targetHealthPercent);
}

inline bool QLineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float targetRadius = 0.0f) {
    if (origin.IsZero() || aim.IsZero() || target.IsZero()) return false;
    if (origin.Distance2D(aim) > kQRange + 0.01f) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * origin.Distance2D(aim);
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kQWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline bool WImpactHits(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    return !center.IsZero() && !target.IsZero() &&
           center.Distance2D(target) <= kWMeteorRadius + std::max(0.0f, targetRadius);
}

inline bool WChargeReady(int nowTick, int lastImpactTick, float cooldownSeconds,
                         float minimumDelaySeconds = kWImpactDelay) {
    if (nowTick < 0 || !std::isfinite(cooldownSeconds)) return false;
    if (lastImpactTick < 0) return true;
    const float elapsed = static_cast<float>(nowTick - lastImpactTick) / 1000.0f;
    return elapsed >= std::max(minimumDelaySeconds, cooldownSeconds);
}

inline bool CageEdgeHit(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    if (center.IsZero() || target.IsZero()) return false;
    const float distance = center.Distance2D(target);
    const float radius = std::max(0.0f, targetRadius);
    return distance + radius >= kECageRadius - kECageEdgeThickness &&
           distance - radius <= kECageRadius + kECageEdgeThickness;
}

inline bool CageContains(const Vec3& center, const Vec3& point,
                         float pointRadius = 0.0f) {
    return !center.IsZero() && !point.IsZero() &&
           center.Distance2D(point) <= kECageRadius + std::max(0.0f, pointRadius);
}

inline bool CageSafePlacement(const Vec3& center, const Vec3& player,
                              int nearbyEnemies, int maximumEnemies,
                              bool underEnemyTurret, bool lethal) {
    if (center.IsZero() || player.IsZero()) return false;
    if (player.Distance2D(center) > kERange + kECageRadius) return false;
    if (underEnemyTurret && !lethal) return false;
    return nearbyEnemies <= std::max(0, maximumEnemies) || lethal;
}

struct ExecuteContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool ProjectileBlocked = false;
    bool Lethal = false;
    bool CageStunned = false;
    bool AttackWindingUp = false;
    bool UnderEnemyTurret = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};
inline bool ShouldCastExecute(const ExecuteContext& c) {
    if (!c.Ready || !c.TargetValid || !c.PredictionHits || c.ProjectileBlocked) return false;
    if (c.AttackWindingUp && !c.Lethal && !c.CageStunned) return false;
    if (c.UnderEnemyTurret && !c.Lethal) return false;
    return c.Lethal || c.CageStunned || c.NearbyEnemies <= std::max(0, c.MaximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Veigar::Geometry
