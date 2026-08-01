#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Taric::Geometry {

using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 325.0f;
inline constexpr float kQRadius = 325.0f;
inline constexpr float kWRange = 800.0f;
inline constexpr float kWLinkRange = 1300.0f;
inline constexpr float kERange = 610.0f;
inline constexpr float kEWidth = 100.0f;
inline constexpr float kESpeed = 1750.0f;
inline constexpr float kEChargeSeconds = 1.25f;
inline constexpr float kRRadius = 400.0f;
inline constexpr float kRDelaySeconds = 2.5f;
inline constexpr float kRInvulnerabilitySeconds = 2.5f;
inline constexpr float kBravadoWindowSeconds = 5.0f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid() && std::isfinite(point.x) &&
        std::isfinite(point.y) && std::isfinite(point.z);
}

inline int QMaxCharges(int rank) {
    return std::clamp(rank, 1, 5);
}

inline float QHealPerCharge(int rank, float abilityPower, float maxHealth) {
    (void)rank;
    if (!std::isfinite(abilityPower) || !std::isfinite(maxHealth)) return 0.0f;
    return std::max(0.0f, 25.0f + std::max(0.0f, abilityPower) * 0.15f +
        std::max(0.0f, maxHealth) * 0.01f);
}

inline float ERawDamage(int rank, float abilityPower, float bonusArmor) {
    if (!std::isfinite(abilityPower) || !std::isfinite(bonusArmor)) return 0.0f;
    const int clampedRank = std::clamp(rank, 1, 5);
    return std::max(0.0f, 50.0f + static_cast<float>(clampedRank - 1) * 40.0f +
        std::max(0.0f, abilityPower) * 0.50f +
        std::max(0.0f, bonusArmor) * 0.50f);
}

inline float QHealValue(int rank, int charges, float abilityPower,
                        float maxHealth) {
    const int usable = std::clamp(charges, 0, QMaxCharges(rank));
    return static_cast<float>(usable) *
        QHealPerCharge(rank, abilityPower, maxHealth);
}

inline float BravadoRawDamage(int championLevel, float bonusArmor) {
    if (!std::isfinite(bonusArmor)) return 0.0f;
    const int level = std::clamp(championLevel, 1, 18);
    const float base = 25.0f + (static_cast<float>(level - 1) * 68.0f / 17.0f);
    return std::max(0.0f, base + std::max(0.0f, bonusArmor) * 0.15f);
}

inline bool BravadoWindowOpen(int nowTick, int expireTick) {
    return expireTick > 0 && nowTick >= 0 && nowTick <= expireTick;
}

inline bool QAreaCovers(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    return FinitePoint(center) && FinitePoint(target) &&
        center.Distance2D(target) <= kQRadius + std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool ELineHits(const Vec3& origin, const Vec3& endpoint,
                      const Vec3& target, float targetRadius = 0.0f,
                      float width = kEWidth) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline float ETravelSeconds(float distance, float delay = 0.25f,
                            float speed = kESpeed) {
    if (!std::isfinite(distance) || !std::isfinite(delay) ||
        !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline float EChargeProgress(float elapsedSeconds) {
    if (!std::isfinite(elapsedSeconds)) return 0.0f;
    return std::clamp(elapsedSeconds / kEChargeSeconds, 0.0f, 1.0f);
}

inline bool EChargeComplete(float elapsedSeconds) {
    return std::isfinite(elapsedSeconds) && elapsedSeconds >= kEChargeSeconds;
}

inline bool LinkInRange(const Vec3& caster, const Vec3& ally,
                        float range = kWLinkRange) {
    return FinitePoint(caster) && FinitePoint(ally) &&
        caster.Distance2D(ally) <= std::max(0.0f, range);
}

inline bool AllyLinkValid(int linkId, int nowTick, int expiryTick) {
    return linkId != 0 && expiryTick > nowTick;
}

inline int RInvulnerabilityStartTick(int castTick) {
    return castTick + static_cast<int>(kRDelaySeconds * 1000.0f);
}

inline int RInvulnerabilityEndTick(int castTick) {
    return RInvulnerabilityStartTick(castTick) +
        static_cast<int>(kRInvulnerabilitySeconds * 1000.0f);
}

inline bool RPending(int nowTick, int castTick) {
    return castTick > 0 && nowTick >= castTick &&
        nowTick < RInvulnerabilityStartTick(castTick);
}

inline bool RInvulnerable(int nowTick, int castTick) {
    return castTick > 0 && nowTick >= RInvulnerabilityStartTick(castTick) &&
        nowTick < RInvulnerabilityEndTick(castTick);
}

inline bool RAreaCovers(const Vec3& center, const Vec3& target,
                        float targetRadius = 0.0f) {
    return FinitePoint(center) && FinitePoint(target) &&
        center.Distance2D(target) <= kRRadius + std::clamp(targetRadius, 0.0f, 150.0f);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Taric::Geometry
