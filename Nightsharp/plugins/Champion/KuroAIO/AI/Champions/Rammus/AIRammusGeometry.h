#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Rammus::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values.
inline constexpr float kQRange = 300.0f;
inline constexpr float kQImpactRadius = 200.0f;
inline constexpr float kQKnockbackDistance = 125.0f;
inline constexpr float kQStunSeconds = 0.35f;
inline constexpr float kQRollDuration = 6.0f;
inline constexpr float kQMinSpeedPercent = 25.0f;
inline constexpr float kQMaxSpeedPercent = 39.1f;
inline constexpr float kQSlowDuration = 1.0f;
inline constexpr float kWRadius = 300.0f;
inline constexpr float kWDuration = 7.0f;
inline constexpr float kERange = 325.0f;
inline constexpr float kEBaseDuration = 1.0f;
inline constexpr float kRBaseRange = 800.0f;
inline constexpr float kRMaxRange = 1700.0f;
inline constexpr float kRImpactRadius = 400.0f;
inline constexpr float kRKnockupRadius = 200.0f;
inline constexpr float kRKnockupSeconds = 0.75f;
inline constexpr float kRPulseDuration = 3.5f;
inline constexpr int kRPulses = 3;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float RankValue3(int rank, const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}

inline constexpr float PassiveBonusAttackDamage(float armor, float magicResist) {
    return 0.10f * std::max(0.0f, armor) + 0.10f * std::max(0.0f, magicResist);
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {40.0f, 80.0f, 120.0f, 160.0f, 200.0f}) +
        std::max(0.0f, abilityPower);
}
inline constexpr float WReturnRawDamage(int rank, float bonusArmor, float bonusMagicResist) {
    (void)rank;
    return 15.0f + 0.10f * std::max(0.0f, bonusArmor) +
        0.10f * std::max(0.0f, bonusMagicResist);
}
inline constexpr float EMonsterRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 80.0f, 100.0f, 120.0f, 140.0f}) +
        0.70f * std::max(0.0f, abilityPower);
}
inline constexpr float RInitialRawDamage(int rank, float abilityPower) {
    return RankValue3(rank, {150.0f, 250.0f, 350.0f}) +
        0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float RPulseRawDamage(int rank, float abilityPower) {
    return RankValue3(rank, {20.0f, 30.0f, 40.0f}) +
        0.10f * std::max(0.0f, abilityPower);
}
inline constexpr float RPowerballBonusRawDamage(int qRank, float abilityPower) {
    return QRawDamage(qRank, abilityPower);
}

inline constexpr float QSpeedPercentForLevel(int level) {
    const float t = static_cast<float>(std::clamp(level, 1, 18) - 1) / 17.0f;
    return kQMinSpeedPercent + (kQMaxSpeedPercent - kQMinSpeedPercent) * t;
}
inline constexpr float SoaringSlamReach(float movementSpeed) {
    return std::clamp(kRBaseRange + 1.5f * std::max(0.0f, movementSpeed),
                      kRBaseRange, kRMaxRange);
}
inline constexpr float SoaringSlamLandingDamageMultiplier(float travelDistance) {
    const float t = std::clamp(travelDistance / 1700.0f, 0.0f, 1.0f);
    return 1.0f + 0.50f * t;
}

inline bool SegmentHits(const Vec3& start, const Vec3& end, const Vec3& target,
                        float radius, float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.Distance2D(end) <= 0.001f) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, start, end).Distance <=
        std::max(0.0f, radius) + std::max(0.0f, targetRadius);
}
inline bool PowerballImpactHits(const Vec3& origin, const Vec3& impact,
                                const Vec3& target, float targetRadius = 0.0f) {
    return origin.IsValid() && impact.IsValid() && target.IsValid() &&
        origin.Distance2D(impact) <= kQRange + std::max(0.0f, targetRadius) &&
        impact.Distance2D(target) <= kQImpactRadius + std::max(0.0f, targetRadius);
}
inline Vec3 ClampPowerballAim(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kQRange, origin.Distance2D(requested));
}
inline bool PowerballCollisionObserved(const Vec3& origin, const Vec3& aim,
                                       bool wallBlocked, float targetRadius = 0.0f) {
    return origin.IsValid() && aim.IsValid() && !wallBlocked &&
        origin.Distance2D(aim) <= kQRange + std::max(0.0f, targetRadius);
}

inline bool TauntReachable(const Vec3& player, const Vec3& target,
                           float targetRadius = 0.0f) {
    return player.IsValid() && target.IsValid() &&
        player.Distance2D(target) <= kERange + std::max(0.0f, targetRadius);
}
inline constexpr float TauntDuration(int rank) {
    return RankValue(rank, {1.0f, 1.2f, 1.4f, 1.6f, 1.8f});
}

struct PowerballState {
    bool Active = false;
    bool Impacted = false;
    Vec3 Origin{};
    Vec3 Aim{};
    int StartTick = 0;
    int ImpactTick = 0;
};
inline bool PowerballChargeValid(const PowerballState& state, int now) {
    return state.Active && !state.Impacted && now >= state.StartTick &&
        now <= state.StartTick + static_cast<int>(kQRollDuration * 1000.0f);
}
inline bool PowerballCancelSafe(const PowerballState& state, const Vec3& player,
                                bool impactConfirmed) {
    return state.Active && player.IsValid() && !impactConfirmed &&
        player.Distance2D(state.Origin) > 90.0f;
}

struct ArmorPosture {
    bool CurlActive = false;
    bool AttackThreat = false;
    bool HardCrowdControlThreat = false;
    bool LowHealth = false;
};
inline bool ShouldCurl(const ArmorPosture& posture) {
    return posture.CurlActive == false &&
        (posture.AttackThreat || posture.HardCrowdControlThreat || posture.LowHealth);
}

struct LandingContext {
    bool Ready = false;
    bool PredictedLanding = false;
    bool WallBlocked = false;
    bool LandingPointValid = false;
    bool UnderNewTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    bool PowerballCenter = false;
    int PredictedEnemies = 0;
    int MinimumEnemies = 2;
};
inline bool ShouldLandSoaringSlam(const LandingContext& context) {
    if (!context.Ready || !context.PredictedLanding || !context.LandingPointValid ||
        context.WallBlocked || context.UnderNewTurret) return false;
    if (context.PowerballCenter) return true;
    return context.Defensive || context.Lethal ||
        context.PredictedEnemies >= std::max(1, context.MinimumEnemies);
}
inline bool LandingHits(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kRImpactRadius + std::max(0.0f, targetRadius);
}
inline bool CenterKnockupHits(const Vec3& center, const Vec3& target,
                              float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
        center.Distance2D(target) <= kRKnockupRadius + std::max(0.0f, targetRadius);
}
inline bool AftershockActive(int landingTick, int now) {
    return landingTick > 0 && now >= landingTick &&
        now <= landingTick + static_cast<int>(kRPulseDuration * 1000.0f);
}
inline bool SafeLanding(const Vec3& point, bool wall, bool turret,
                        int enemies, int maximumEnemies, bool defensive) {
    return point.IsValid() && !point.IsZero() && !wall &&
        (defensive || !turret) && enemies <= std::max(0, maximumEnemies);
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
        (context.Defensive || context.AntiGapcloser || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Rammus::Geometry
