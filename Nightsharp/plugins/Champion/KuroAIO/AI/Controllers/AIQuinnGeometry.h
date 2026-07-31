#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Quinn::Geometry {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kQRange = 1025.0f;
inline constexpr float kQHalfWidth = 55.0f;
inline constexpr float kVaultRange = 675.0f;
inline constexpr float kVaultLandingDistance = 525.0f;
inline constexpr float kRevealRadius = 2100.0f;
inline constexpr int kRevealDurationMs = 2000;
inline constexpr int kRRecastSafetyTailMs = 350;
inline constexpr int kRMinimumFlightMs = 650;

inline bool ValidPoint(const Vec2& point) {
    return std::isfinite(point.x) && std::isfinite(point.y);
}

inline float Length(const Vec2& value) {
    return std::sqrt(value.x * value.x + value.y * value.y);
}

inline Vec2 Subtract(const Vec2& left, const Vec2& right) {
    return {left.x - right.x, left.y - right.y};
}

inline Vec2 Add(const Vec2& left, const Vec2& right) {
    return {left.x + right.x, left.y + right.y};
}

inline Vec2 Scale(const Vec2& value, float scale) {
    return {value.x * scale, value.y * scale};
}

inline Vec2 Normalize(const Vec2& value) {
    const float length = Length(value);
    return length > 0.001f ? Scale(value, 1.0f / length) : Vec2{};
}

inline float Dot(const Vec2& left, const Vec2& right) {
    return left.x * right.x + left.y * right.y;
}

inline Vec2 HarrierMarkVector(Vec2 source, Vec2 target) {
    return Normalize(Subtract(target, source));
}

inline int ClampHarrierStacks(int stacks) {
    return std::clamp(stacks, 0, 1);
}

inline bool HarrierReady(int observedStacks, bool markedTarget) {
    return markedTarget || ClampHarrierStacks(observedStacks) > 0;
}

inline int ConsumeHarrier(int observedStacks, bool markedTarget) {
    return HarrierReady(observedStacks, markedTarget) ? 0 : ClampHarrierStacks(observedStacks);
}

inline float HarrierBonusDamage(int level, float bonusAttackDamage) {
    const int clampedLevel = std::clamp(level, 1, 18);
    const float base = 10.0f + static_cast<float>(clampedLevel - 1) * 5.0f;
    return base + std::max(0.0f, bonusAttackDamage) * 0.40f;
}

struct LineHit {
    bool Hits = false;
    bool FirstTarget = false;
    bool Blocked = false;
    float Distance = 0.0f;
};

inline LineHit BlindLineHit(const Vec2& source, const Vec2& direction,
                            float targetDistance, float targetRadius,
                            float blockerDistance = -1.0f,
                            float blockerRadius = 0.0f) {
    const Vec2 ray = Normalize(direction);
    const float target = std::max(0.0f, targetDistance);
    if (Length(ray) <= 0.001f || target > kQRange + std::max(0.0f, targetRadius)) {
        return {};
    }
    if (blockerDistance >= 0.0f && blockerDistance + std::max(0.0f, blockerRadius) <
            target - std::max(0.0f, targetRadius)) {
        return {false, false, true, blockerDistance};
    }
    (void)source;
    return {true, true, false, target};
}

inline bool ProjectileCanReachFirstTarget(float sourceDistance,
                                          float blockerDistance,
                                          float targetRadius,
                                          float blockerRadius) {
    if (sourceDistance < 0.0f || blockerDistance < 0.0f) return false;
    return blockerDistance + std::max(0.0f, blockerRadius) >=
           sourceDistance - std::max(0.0f, targetRadius);
}

inline bool ScoutingRevealActive(bool observedReveal, int nowTick,
                                 int castTick, int durationMs = kRevealDurationMs) {
    return observedReveal || (castTick > 0 && nowTick - castTick >= 0 &&
                              nowTick - castTick <= std::max(0, durationMs));
}

inline bool ScoutingWorthwhile(bool hiddenEnemy, bool objectiveThreat,
                               int nearbyEnemies, bool playerUnsafe) {
    if (playerUnsafe) return false;
    return hiddenEnemy || objectiveThreat || nearbyEnemies >= 2;
}

inline Vec2 VaultLanding(const Vec2& player, const Vec2& target,
                         float distance = kVaultLandingDistance) {
    const Vec2 away = Normalize(Subtract(target, player));
    if (Length(away) <= 0.001f) return target;
    return Add(target, Scale(away, std::max(0.0f, distance)));
}

inline bool VaultReachable(float distance, float targetRadius = 0.0f) {
    return distance >= 0.0f && distance <= kVaultRange + std::max(0.0f, targetRadius);
}

inline bool TurretLandingAllowed(bool landingUnderTurret,
                                 bool playerUnderTurret,
                                 bool lethalDamage,
                                 bool escapeRoute) {
    if (!landingUnderTurret) return true;
    return playerUnderTurret || lethalDamage || escapeRoute;
}

inline bool UnsafeMobilityAllowed(bool wallLanding,
                                  bool landingUnderTurret,
                                  bool playerUnderTurret,
                                  bool lethalDamage,
                                  bool escapeRoute,
                                  int enemiesAtLanding,
                                  int maximumEnemies,
                                  float playerHealthPercent,
                                  float minimumHealthPercent = 25.0f) {
    if (wallLanding) return false;
    if (!TurretLandingAllowed(landingUnderTurret, playerUnderTurret,
                              lethalDamage, escapeRoute)) return false;
    if (!escapeRoute && enemiesAtLanding > std::max(0, maximumEnemies) &&
        playerHealthPercent < minimumHealthPercent) return false;
    return true;
}

enum class RPhase { Ready, Flying, Recast };

inline RPhase ReconcileRPhase(bool observedBuff, bool runtimeRecast,
                              bool castRecently, int elapsedMs) {
    if (runtimeRecast || (castRecently && elapsedMs >= kRMinimumFlightMs)) {
        return RPhase::Recast;
    }
    if (observedBuff || castRecently) return RPhase::Flying;
    return RPhase::Ready;
}

inline bool RRecastSafe(int elapsedMs, bool underTurret, bool lethal,
                        bool escapeRoute, int nearbyEnemies,
                        int maximumEnemies, float healthPercent) {
    if (elapsedMs < kRMinimumFlightMs || elapsedMs < 0) return false;
    if (underTurret && !lethal && !escapeRoute) return false;
    if (!escapeRoute && nearbyEnemies > std::max(0, maximumEnemies) &&
        healthPercent < 35.0f && !lethal) return false;
    return true;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Quinn::Geometry
