#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Plugins::KuroAIO::AI::Controllers::Shaco::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kDeceiveRange = 400.0f;
inline constexpr float kBoxRange = 500.0f;
inline constexpr float kBoxTriggerRadius = 300.0f;
inline constexpr float kShivRange = 625.0f;
inline constexpr float kCloneExplosionRadius = 400.0f;
inline constexpr int kBoxArmMs = 2000;
inline constexpr int kBoxLifetimeMs = 6000;
inline constexpr int kStealthMs = 3000;
inline constexpr int kCloneLifetimeMs = 18000;

inline Vec3 ClampBlinkEndpoint(const Vec3& origin, const Vec3& requested,
                               float maxRange = kDeceiveRange) {
    if (!origin.IsValid() || origin.IsZero() || !requested.IsValid() ||
        requested.IsZero() || !std::isfinite(maxRange) || maxRange <= 0.0f) {
        return {};
    }
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return origin;
    const float distance = origin.Distance2D(requested);
    if (distance <= maxRange) return requested;
    return origin + direction * maxRange;
}

inline bool BlinkEndpointSafe(const Vec3& origin, const Vec3& endpoint,
                              float maxRange = kDeceiveRange,
                              bool wall = false, bool underTurret = false,
                              int nearbyEnemies = 0,
                              int maximumEnemies = 2) {
    if (!origin.IsValid() || !endpoint.IsValid() || origin.IsZero() ||
        endpoint.IsZero() || wall || underTurret || nearbyEnemies < 0 ||
        maximumEnemies < 0 || nearbyEnemies > maximumEnemies) return false;
    return origin.Distance2D(endpoint) <= maxRange + 1.0f;
}

inline Vec3 BehindTargetEndpoint(const Vec3& player, const Vec3& target,
                                 float offset = 65.0f) {
    if (!player.IsValid() || !target.IsValid() || player.IsZero() ||
        target.IsZero() || !std::isfinite(offset) || offset < 0.0f) return {};
    const Vec3 awayFromPlayer = Direction2D(player, target);
    if (awayFromPlayer.IsZero()) return {};
    return target + awayFromPlayer * offset;
}

inline bool BoxPlacementValid(const Vec3& player, const Vec3& position,
                              float maxRange = kBoxRange, bool wall = false,
                              bool underTurret = false) {
    if (!player.IsValid() || !position.IsValid() || player.IsZero() ||
        position.IsZero() || wall || underTurret || !std::isfinite(maxRange) ||
        maxRange <= 0.0f) return false;
    return player.Distance2D(position) <= maxRange + 1.0f;
}

inline bool BoxCanFear(int castTick, int nowTick,
                       int armMs = kBoxArmMs,
                       int lifetimeMs = kBoxLifetimeMs) {
    if (castTick <= 0 || nowTick < castTick || armMs < 0 || lifetimeMs <= armMs) {
        return false;
    }
    const int age = nowTick - castTick;
    return age >= armMs && age <= lifetimeMs;
}

inline bool ShivExecuteLethal(float baseDamage, float executeBonus,
                             float targetHealth, float targetShield,
                             float targetHealthPercent,
                             float executeThresholdPercent = 30.0f) {
    if (!std::isfinite(baseDamage) || !std::isfinite(executeBonus) ||
        !std::isfinite(targetHealth) || !std::isfinite(targetShield) ||
        !std::isfinite(targetHealthPercent) ||
        !std::isfinite(executeThresholdPercent)) return false;
    const float health = std::max(0.0f, targetHealth);
    const float shield = std::max(0.0f, targetShield);
    const float threshold = std::clamp(executeThresholdPercent, 0.0f, 100.0f);
    const float damage = std::max(0.0f, baseDamage) +
                         (targetHealthPercent <= threshold
                              ? std::max(0.0f, executeBonus) : 0.0f);
    return damage >= health + shield;
}

inline bool CloneExplosionWorthwhile(const Vec3& clonePosition,
                                     const Vec3& targetPosition,
                                     float targetRadius,
                                     int enemiesAtExplosion,
                                     bool cloneConfirmed,
                                     float radius = kCloneExplosionRadius) {
    if (!cloneConfirmed || !clonePosition.IsValid() || !targetPosition.IsValid() ||
        clonePosition.IsZero() || targetPosition.IsZero() ||
        !std::isfinite(targetRadius) || !std::isfinite(radius) ||
        radius <= 0.0f || enemiesAtExplosion < 0) return false;
    return clonePosition.Distance2D(targetPosition) <=
               radius + std::max(0.0f, targetRadius) &&
           enemiesAtExplosion >= 1;
}

inline bool IsCloneIdentity(const char* name, const char* characterName) {
    const auto contains = [](const char* value, const char* token) {
        if (!value || !token || !*value || !*token) return false;
        const std::size_t valueLength = std::strlen(value);
        const std::size_t tokenLength = std::strlen(token);
        if (tokenLength > valueLength) return false;
        for (std::size_t i = 0; i + tokenLength <= valueLength; ++i) {
            std::size_t j = 0;
            for (; j < tokenLength; ++j) {
                const char left = value[i + j] >= 'A' && value[i + j] <= 'Z'
                    ? static_cast<char>(value[i + j] - 'A' + 'a') : value[i + j];
                const char right = token[j] >= 'A' && token[j] <= 'Z'
                    ? static_cast<char>(token[j] - 'A' + 'a') : token[j];
                if (left != right) break;
            }
            if (j == tokenLength) return true;
        }
        return false;
    };
    return contains(name, "shacoclone") || contains(name, "hallucinate") ||
           contains(name, "shacor") || contains(characterName, "shacoclone") ||
           contains(characterName, "hallucinate") ||
           contains(characterName, "shacor");
}

inline bool CloneCommitSafe(float playerHealthPercent, int enemiesAtClone,
                            bool escapeEndpointSafe, bool cloneConfirmed) {
    return cloneConfirmed && escapeEndpointSafe && enemiesAtClone <= 2 &&
           playerHealthPercent >= 22.0f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Shaco::Geometry
