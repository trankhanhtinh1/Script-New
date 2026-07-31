#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Sett::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQRange = 750.0f;
inline constexpr float kAAResetWindowMs = 4000.0f;
inline constexpr float kWCenterRadius = 125.0f;
inline constexpr float kWOuterRadius = 210.0f;
inline constexpr float kWMaxGritPercent = 50.0f;
inline constexpr float kWDecayPerSecond = 0.30f;
inline constexpr float kERange = 490.0f;
inline constexpr float kERadius = 350.0f;
inline constexpr float kRRange = 400.0f;
inline constexpr float kRCarryDistance = 400.0f;
inline constexpr float kRLandingRadius = 350.0f;
inline constexpr float kRCraterRadius = 600.0f;

enum class QState : std::uint8_t { Ready, Armed, Expired };
enum class WState : std::uint8_t { Ready, Fired, Expired };
enum class EOutcome : std::uint8_t { Miss, Slow, Stun };

inline float ClampFinite(float value, float low, float high) {
    return std::clamp(std::isfinite(value) ? value : low, low, high);
}

inline float GritCap(float maxHealth, float capPercent = kWMaxGritPercent) {
    if (!std::isfinite(maxHealth) || maxHealth <= 0.0f) return 0.0f;
    return maxHealth * ClampFinite(capPercent, 0.0f, 100.0f) / 100.0f;
}

inline float AddGrit(float current, float incomingDamage, float maxHealth) {
    const float cap = GritCap(maxHealth);
    if (cap <= 0.0f) return 0.0f;
    return ClampFinite(std::max(0.0f, current) + std::max(0.0f, incomingDamage), 0.0f, cap);
}

inline float DecayGrit(float current, float elapsedSeconds,
                       float decayPerSecond = kWDecayPerSecond) {
    if (!std::isfinite(current) || current <= 0.0f || !std::isfinite(elapsedSeconds) ||
        elapsedSeconds <= 0.0f) return std::max(0.0f, current);
    return std::max(0.0f, current - current * std::max(0.0f, decayPerSecond) * elapsedSeconds);
}

inline bool QArmAllowed(QState state, bool targetValid, bool targetReachable,
                        bool protectedTarget, bool preservingWindup) {
    return state == QState::Ready && targetValid && targetReachable &&
           !protectedTarget && !preservingWindup;
}

inline float QDamage(int rank, float targetMaxHealth, float bonusAttackDamage,
                     bool monster = false) {
    static constexpr std::array<float, 5> base{10.0f, 20.0f, 30.0f, 40.0f, 50.0f};
    const int clampedRank = std::clamp(rank, 1, 5);
    const float bonusRate = 0.00005f * static_cast<float>(clampedRank);
    const float damage = base[static_cast<std::size_t>(clampedRank - 1)] +
        std::max(0.0f, targetMaxHealth) * (0.01f + bonusRate * std::max(0.0f, bonusAttackDamage));
    return monster ? std::min(400.0f, damage) : damage;
}

inline bool WCastAllowed(float grit, float maxHealth, bool targetValid,
                         bool threatened, bool lethal) {
    return targetValid && grit > 0.0f && GritCap(maxHealth) > 0.0f &&
           (threatened || lethal || grit >= GritCap(maxHealth) * 0.35f);
}

inline float WShield(float grit, float maxHealth) {
    return std::min(std::max(0.0f, grit), GritCap(maxHealth));
}

inline float WDamage(int rank, float bonusAttackDamage, float grit,
                     bool trueCenter) {
    static constexpr std::array<float, 5> base{60.0f, 80.0f, 100.0f, 120.0f, 140.0f};
    const int clampedRank = std::clamp(rank, 1, 5);
    const float convertedGrit = std::max(0.0f, grit) *
        (0.25f + 0.0025f * std::max(0.0f, bonusAttackDamage));
    const float raw = base[static_cast<std::size_t>(clampedRank - 1)] + convertedGrit;
    return trueCenter ? raw : raw * 0.5f;
}

inline bool WCenterHit(const Vec3& source, const Vec3& aim, const Vec3& target,
                       float targetRadius = 0.0f) {
    if (!source.IsValid() || !aim.IsValid() || !target.IsValid() || source.IsZero() ||
        aim.IsZero() || target.IsZero()) return false;
    const Vec3 facing = Direction2D(source, aim);
    if (facing.IsZero()) return false;
    const Vec3 toTarget = target - source;
    const float forward = toTarget.Dot(facing);
    const float lateral = std::fabs(toTarget.x * facing.z - toTarget.z * facing.x);
    return forward >= 0.0f && forward <= kWOuterRadius &&
           lateral <= kWCenterRadius + std::max(0.0f, targetRadius);
}

inline EOutcome FacebreakerOutcome(const Vec3& center, const Vec3& facing,
                                   const Vec3& first, const Vec3& second,
                                   float radius = kERadius) {
    if (!center.IsValid() || !facing.IsValid() || center.IsZero() || facing.IsZero())
        return EOutcome::Miss;
    const Vec3 direction = Direction2D(center, facing);
    if (direction.IsZero()) return EOutcome::Miss;
    const auto side = [&](const Vec3& point) {
        if (!point.IsValid() || point.IsZero() || center.Distance2D(point) > radius) return 0;
        return (point - center).Dot(direction) >= 0.0f ? 1 : -1;
    };
    const int firstSide = side(first);
    const int secondSide = side(second);
    if (firstSide == 0 && secondSide == 0) return EOutcome::Miss;
    if (firstSide != 0 && secondSide != 0 && firstSide != secondSide) return EOutcome::Stun;
    return EOutcome::Slow;
}

inline Vec3 RLandingEndpoint(const Vec3& source, const Vec3& target,
                             float carryDistance = kRCarryDistance) {
    if (!source.IsValid() || !target.IsValid() || source.IsZero() || target.IsZero()) return {};
    const Vec3 direction = Direction2D(source, target);
    return direction.IsZero() ? Vec3{} : target + direction * std::max(0.0f, carryDistance);
}

inline float RDamage(int rank, float targetMaxHealth, float bonusAttackDamage) {
    static constexpr std::array<float, 3> base{200.0f, 300.0f, 400.0f};
    static constexpr std::array<float, 3> healthRate{0.40f, 0.50f, 0.60f};
    const int clampedRank = std::clamp(rank, 1, 3);
    return base[static_cast<std::size_t>(clampedRank - 1)] +
        1.20f * std::max(0.0f, bonusAttackDamage) +
        healthRate[static_cast<std::size_t>(clampedRank - 1)] * std::max(0.0f, targetMaxHealth);
}

inline bool RCommitAllowed(bool targetValid, bool protectedTarget, bool endpointValid,
                           bool endpointWall, bool endpointTurret, int enemiesAtLanding,
                           int maximumEnemies, bool lethal, bool fleeing) {
    if (!targetValid || protectedTarget || !endpointValid || endpointWall) return false;
    if (endpointTurret && !lethal && !fleeing) return false;
    return lethal || fleeing || enemiesAtLanding <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Sett::Geometry
