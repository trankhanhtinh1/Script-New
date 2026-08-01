#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Galio::Geometry {

using SharedGeometry::Direction2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 825.0f;
inline constexpr float kQWidth = 120.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1300.0f;
inline constexpr float kWRadius = 350.0f;
inline constexpr float kWMinCharge = 0.25f;
inline constexpr float kWMaxCharge = 2.0f;
inline constexpr float kWMaxDamageMultiplier = 2.0f;
inline constexpr float kERange = 650.0f;
inline constexpr float kEWidth = 160.0f;
inline constexpr float kESpeed = 1200.0f;
inline constexpr float kEKnockupRadius = 160.0f;
inline constexpr float kRRange = 4000.0f;
inline constexpr float kRRadius = 650.0f;
inline constexpr float kRDelay = 1.25f;
inline constexpr float kRSpeed = 1800.0f;
inline constexpr float kPassiveCooldown = 5.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 105.0f, 140.0f, 175.0f, 210.0f}) +
           0.75f * std::max(0.0f, abilityPower);
}
inline constexpr float PassiveRawDamage(int level, float attackDamage, float abilityPower) {
    const float levelBase = 15.0f + 10.0f * static_cast<float>(std::clamp(level, 1, 18) - 1);
    return levelBase + std::max(0.0f, attackDamage) + 0.50f * std::max(0.0f, abilityPower);
}
inline constexpr float WRawDamage(int rank, float chargePercent) {
    const float base = RankValue(rank, {20.0f, 35.0f, 50.0f, 65.0f, 80.0f});
    return base * (1.0f + std::clamp(chargePercent, 0.0f, 1.0f));
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {90.0f, 130.0f, 170.0f, 210.0f, 250.0f}) +
           0.90f * std::max(0.0f, abilityPower);
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {150.0f, 225.0f, 300.0f}) +
           0.60f * std::max(0.0f, abilityPower);
}

inline bool LineHits(const Vec3& start, const Vec3& end, const Vec3& target,
                     float width, float targetRadius = 0.0f) {
    const auto projection = SharedGeometry::ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, width) * 0.5f +
               std::max(0.0f, targetRadius);
}
inline bool CircleHits(const Vec3& center, const Vec3& target, float radius,
                       float targetRadius = 0.0f) {
    return center.Distance2D(target) <= std::max(0.0f, radius) +
               std::max(0.0f, targetRadius);
}
inline Vec3 ClampRange(const Vec3& origin, const Vec3& requested, float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}
inline bool SafeEEndpoint(const Vec3& endpoint, bool underNewTurret,
                          bool wall, int enemies, int allies,
                          int maximumEnemies, bool defensive, bool lethal) {
    if (endpoint.IsZero() || wall ||
        (underNewTurret && !defensive && !lethal)) return false;
    if (defensive || lethal) return true;
    return enemies <= std::max(0, maximumEnemies) && allies + 1 >= enemies;
}
inline bool PassiveReady(int nowTick, int cooldownUntilTick) {
    return nowTick >= cooldownUntilTick;
}
inline int PassiveResetTick(int castTick) {
    return castTick + static_cast<int>(kPassiveCooldown * 1000.0f);
}

struct WContext {
    bool Ready = false;
    bool Channeling = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool AttackWindingUp = false;
    bool Defensive = false;
    bool Interrupt = false;
    bool Lethal = false;
    bool Manual = false;
    float ChargePercent = 0.0f;
};
inline bool ShouldReleaseW(const WContext& context) {
    if (!context.Ready || !context.Channeling || !context.TargetValid ||
        !context.PredictionHits) return false;
    if (context.AttackWindingUp && !context.Defensive && !context.Interrupt &&
        !context.Lethal && !context.Manual) return false;
    return context.Defensive || context.Interrupt || context.Lethal || context.Manual ||
           context.ChargePercent >= 0.30f;
}

struct RLandingContext {
    bool Ready = false;
    bool AllyValid = false;
    bool AllyThreatened = false;
    bool PredictionValid = false;
    bool LandingWalkable = false;
    bool UnderNewTurret = false;
    bool Manual = false;
    bool Defensive = false;
    bool Lethal = false;
    int EnemiesAtLanding = 0;
    int AlliesAtLanding = 0;
    int MaximumEnemies = 3;
};
inline bool SafeRLanding(const RLandingContext& context) {
    if (!context.Ready || !context.AllyValid || !context.PredictionValid ||
        !context.LandingWalkable || context.UnderNewTurret) return false;
    if (context.Defensive || context.Lethal || context.Manual) return true;
    return context.AlliesAtLanding >= 1 &&
           context.EnemiesAtLanding <= std::max(0, context.MaximumEnemies);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
    bool ManualOwnership = false;
};
inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Galio::Geometry
