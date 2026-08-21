#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::MissFortune::Geometry {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline Vec2 Subtract(const Vec2& left, const Vec2& right) {
    return {left.x - right.x, left.y - right.y};
}

inline float Length(const Vec2& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline float Dot(const Vec2& left, const Vec2& right) {
    return left.x * right.x + left.y * right.y;
}

inline bool BounceConeContains(const Vec2& player,
                               const Vec2& primary,
                               const Vec2& desired,
                               float bounceRange = 500.0f,
                               float halfAngleDegrees = 30.0f) {
    const Vec2 forward = Subtract(primary, player);
    const Vec2 bounce = Subtract(desired, primary);
    const float forwardLength = Length(forward);
    const float bounceLength = Length(bounce);
    if (forwardLength <= 1.0f || bounceLength <= 1.0f ||
        bounceLength > std::max(0.0f, bounceRange)) return false;
    const float cosine = Dot(forward, bounce) /
        (forwardLength * bounceLength);
    constexpr float kPi = 3.14159265358979323846f;
    const float minimum = std::cos(
        std::clamp(halfAngleDegrees, 0.0f, 89.0f) * kPi / 180.0f);
    return cosine >= minimum;
}

inline float BounceCriticalMultiplier(float critDamageMultiplier) {
    return 1.0f + 0.50f *
        std::max(0.0f, critDamageMultiplier - 1.0f);
}

inline bool ShouldUseBounceRoute(bool directAvailable,
                                 bool bounceAvailable,
                                 bool guaranteedCritical) {
    return bounceAvailable && (!directAvailable || guaranteedCritical);
}

inline int BulletTimeWaves(int rank) {
    static constexpr int waves[] = {0, 14, 16, 18};
    return waves[std::clamp(rank, 0, 3)];
}

inline float BulletTimeRawPerWave(int rank,
                                  float totalAttackDamage,
                                  float abilityPower) {
    static constexpr float base[] = {0.0f, 20.0f, 30.0f, 40.0f};
    rank = std::clamp(rank, 0, 3);
    if (rank <= 0) return 0.0f;
    return base[rank] + 0.60f * std::max(0.0f, totalAttackDamage) +
           0.25f * std::max(0.0f, abilityPower);
}

inline float MakeItRainRaw(int rank, float abilityPower) {
    static constexpr float totalBase[] = {
        0.0f, 70.0f, 100.0f, 130.0f, 160.0f, 190.0f};
    rank = std::clamp(rank, 0, 5);
    if (rank <= 0) return 0.0f;
    return totalBase[rank] + 1.20f * std::max(0.0f, abilityPower);
}

inline float MakeItRainSecureFraction(bool targetImmobile) {
    // E is a two-second DoT. Kill-secure must not assume every tick lands;
    // hard control earns most of the duration, while a mobile slowed target
    // is credited with only the first reliable portion.
    return targetImmobile ? 0.85f : 0.60f;
}

inline int ConservativeBulletTimeHits(int rank, bool targetControlled) {
    const int waves = BulletTimeWaves(rank);
    if (waves <= 0) return 0;
    // Do not call a full three-second channel lethal. A slowed/rooted target
    // is credited with roughly two thirds of the waves; a mobile target with
    // less than half. Crits remain upside and are not required for execute.
    const float fraction = targetControlled ? 0.65f : 0.45f;
    return std::max(1, static_cast<int>(
        static_cast<float>(waves) * fraction));
}

inline bool DirectionConeContains(const Vec2& origin,
                                  const Vec2& aim,
                                  const Vec2& point,
                                  float range,
                                  float halfAngleDegrees) {
    const Vec2 forward = Subtract(aim, origin);
    const Vec2 offset = Subtract(point, origin);
    const float forwardLength = Length(forward);
    const float pointDistance = Length(offset);
    if (forwardLength <= 1.0f || pointDistance <= 1.0f ||
        pointDistance > std::max(0.0f, range)) return false;
    const float cosine = Dot(forward, offset) /
        (forwardLength * pointDistance);
    constexpr float kPi = 3.14159265358979323846f;
    const float minimum = std::cos(
        std::clamp(halfAngleDegrees, 0.0f, 89.0f) * kPi / 180.0f);
    return cosine >= minimum;
}

struct DoubleUpContext {
    bool Direct = false;
    bool BounceRoute = false;
    bool AttackAvailable = false;
    bool RecentlyAttacked = false;
    bool Lethal = false;
    bool BounceBlocker = false;
    bool ProjectileWall = false;
};

inline bool ShouldCastDoubleUp(const DoubleUpContext& context) {
    if (context.ProjectileWall ||
        (!context.Direct && (!context.BounceRoute || context.BounceBlocker))) {
        return false;
    }
    return true;
}

inline bool ShouldPrimeBulletTime(bool targetAlreadyControlled,
                                  bool rainReady,
                                  bool enoughManaForBoth) {
    return !targetAlreadyControlled && rainReady && enoughManaForBoth;
}

inline bool ShouldSwapLoveTap(bool currentMarked,
                              bool alternateUnmarked,
                              bool currentKillableSoon,
                              bool alternateReachable) {
    return currentMarked && alternateUnmarked && !currentKillableSoon &&
           alternateReachable;
}

struct RainContext {
    bool PredictionHits = false;
    bool AttackAvailable = false;
    bool Lethal = false;
    bool Escaping = false;
    bool Immobilized = false;
    bool Gapcloser = false;
    bool UltimateSetup = false;
};

inline bool ShouldMakeItRain(const RainContext& context) {
    if (!context.PredictionHits) return false;
    if (context.Lethal || context.Gapcloser || context.UltimateSetup) return true;
    return !context.AttackAvailable &&
           (context.Escaping || context.Immobilized);
}

struct BulletTimeContext {
    bool SafeChannel = false;
    bool ValuableCone = false;
    bool TargetInCone = false;
    bool ProjectileWall = false;
    bool BetterAttack = false;
    bool LethalChannel = false;
    int TargetsInCone = 0;
    int MinimumTargets = 2;
};

inline bool ShouldStartBulletTime(const BulletTimeContext& context) {
    if (!context.SafeChannel || !context.TargetInCone ||
        context.ProjectileWall) return false;
    if (context.LethalChannel) return true;
    if (context.BetterAttack) return false;
    return context.ValuableCone ||
           context.TargetsInCone >= std::max(1, context.MinimumTargets);
}

} // namespace Plugins::KuroAIO::AI::Controllers::MissFortune::Geometry
