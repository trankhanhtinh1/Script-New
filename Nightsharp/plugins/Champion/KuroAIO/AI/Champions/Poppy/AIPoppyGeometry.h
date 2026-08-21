#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Poppy::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift geometry.
inline constexpr float kPassiveRange = 475.0f;
inline constexpr float kPassiveShieldDuration = 4.0f;
inline constexpr float kQRange = 430.0f;
inline constexpr float kQWidth = 90.0f;
inline constexpr float kQDelay = 0.35f;
inline constexpr float kQZoneRadius = 100.0f;
inline constexpr float kQZoneDuration = 1.5f;
inline constexpr float kWRadius = 400.0f;
inline constexpr float kWDuration = 2.0f;
inline constexpr float kERange = 475.0f;
inline constexpr float kEWallStunRadius = 80.0f;
inline constexpr float kEDashSpeed = 1600.0f;
inline constexpr float kRRange = 1200.0f;
inline constexpr float kRWidth = 100.0f;
inline constexpr float kRKnockbackDistance = 1200.0f;
inline constexpr float kRChargeSeconds = 4.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}

inline constexpr float PassiveRawDamage(int level, float targetMaximumHealth) {
    const float levelScale = 20.0f +
        (180.0f - 20.0f) * static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return levelScale + 0.04f * std::max(0.0f, targetMaximumHealth);
}
inline constexpr float QRawDamage(int rank, float bonusAttackDamage,
                                  float targetMaximumHealth, bool zoneHit) {
    const float base = RankValue(rank, {40.0f, 60.0f, 80.0f, 100.0f, 120.0f}) +
        0.90f * std::max(0.0f, bonusAttackDamage);
    return base + (zoneHit ? 0.08f * std::max(0.0f, targetMaximumHealth) : 0.0f);
}
inline constexpr float ERawDamage(int rank, float bonusAttackDamage) {
    return RankValue(rank, {60.0f, 80.0f, 100.0f, 120.0f, 140.0f}) +
        0.50f * std::max(0.0f, bonusAttackDamage);
}
inline constexpr float RRawDamage(int rank, float bonusAttackDamage) {
    return RankValue3(rank, {150.0f, 250.0f, 350.0f}) +
        0.90f * std::max(0.0f, bonusAttackDamage);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float halfWidth, float targetRadius = 0.0f) {
    if (start.Distance2D(end) <= 0.001f || !start.IsValid() ||
        !end.IsValid() || !target.IsValid()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
        std::max(0.0f, halfWidth) + std::max(0.0f, targetRadius);
}
inline bool HammerShockHits(const Vec3& origin, const Vec3& aim,
                            const Vec3& target, float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero() || origin.Distance2D(aim) > kQRange + targetRadius) return false;
    const Vec3 end = origin + direction * std::min(kQRange, origin.Distance2D(aim));
    return SegmentHits(origin, end, target, kQWidth * 0.5f, targetRadius) ||
        target.Distance2D(end) <= kQZoneRadius + std::max(0.0f, targetRadius);
}
inline bool QZoneHits(const Vec3& zoneCenter, const Vec3& target,
                      float targetRadius = 0.0f) {
    return zoneCenter.IsValid() && target.IsValid() &&
        zoneCenter.Distance2D(target) <= kQZoneRadius + std::max(0.0f, targetRadius);
}
inline Vec3 ClampHeroicChargeEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}
inline bool WallStunCanHit(const Vec3& origin, const Vec3& target,
                           const Vec3& wallImpact, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !target.IsValid() || !wallImpact.IsValid() ||
        origin.Distance2D(target) > kERange + std::max(0.0f, targetRadius)) return false;
    return target.Distance2D(wallImpact) <=
        kEWallStunRadius + std::max(0.0f, targetRadius);
}
inline bool WallImpactObserved(const Vec3& origin, const Vec3& endpoint,
                               bool navmeshWall, float endpointRadius = 0.0f) {
    return origin.IsValid() && endpoint.IsValid() && navmeshWall &&
        origin.Distance2D(endpoint) <= kERange + std::max(0.0f, endpointRadius);
}

struct BucklerState {
    bool Ready = true;
    bool ShieldOnGround = false;
    Vec3 ShieldPosition{};
    int ThrowTick = 0;
    int PickupExpireTick = 0;
};
inline bool BucklerPickupSafe(const BucklerState& state, const Vec3& player,
                              const Vec3& shield, int now,
                              bool enemyTurret, int nearbyEnemies) {
    return state.ShieldOnGround && player.IsValid() && shield.IsValid() &&
        now <= state.PickupExpireTick && player.Distance2D(shield) <= 225.0f &&
        !enemyTurret && nearbyEnemies <= 2;
}

struct DashInterceptContext {
    bool Ready = false;
    bool TargetDashing = false;
    bool TargetInWRange = false;
    bool ThreateningPlayer = false;
    bool PlayerLow = false;
};
inline bool ShouldInterceptDash(const DashInterceptContext& context) {
    return context.Ready && context.TargetDashing && context.TargetInWRange &&
        context.ThreateningPlayer &&
        (context.PlayerLow || context.ThreateningPlayer);
}

struct WallDashContext {
    bool Ready = false;
    bool TargetValid = false;
    bool EndpointValid = false;
    bool WallImpact = false;
    bool TargetAtImpact = false;
    bool UnderNewTurret = false;
    bool Defensive = false;
    bool Lethal = false;
};
inline bool ShouldHeroicCharge(const WallDashContext& context) {
    if (!context.Ready || !context.TargetValid || !context.EndpointValid ||
        !context.WallImpact || !context.TargetAtImpact || context.UnderNewTurret)
        return false;
    return context.Defensive || context.Lethal;
}

enum class UltimateStage { Idle, Charging, RecastReady };
struct UltimateContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool Charging = false;
    bool KnockbackAway = false;
    bool Defensive = false;
    bool Lethal = false;
    bool PlayerLow = false;
    int PredictedHits = 0;
    int MinimumHits = 2;
};
inline bool ShouldReleaseUltimate(const UltimateContext& context) {
    if (!context.Ready || !context.TargetValid || !context.PredictionHits ||
        context.ProjectileWall) return false;
    if (context.Charging && !context.KnockbackAway) return false;
    if (context.PlayerLow && !context.Defensive && !context.Lethal)
        return false;
    return context.Defensive || context.Lethal ||
        context.PredictedHits >= std::max(1, context.MinimumHits);
}

struct AutomaticContext {
    bool Defensive = false;
    bool AntiGapcloser = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
        (context.Defensive || context.AntiGapcloser || context.Interrupt ||
         context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Poppy::Geometry
