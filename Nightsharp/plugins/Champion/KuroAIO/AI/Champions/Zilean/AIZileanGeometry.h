#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace Plugins::KuroAIO::AI::Controllers::Zilean::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

// Riot 26.15 / CommunityDragon 16.15 Summoner's Rift values.
inline constexpr float kQRange = 900.0f;
inline constexpr float kQRadius = 150.0f;
inline constexpr float kQMissileWidth = 120.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 2000.0f;
inline constexpr float kQFuseSeconds = 3.0f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kERange = 550.0f;
inline constexpr float kEDurationSeconds = 2.5f;
inline constexpr float kRRange = 900.0f;
inline constexpr float kRDurationSeconds = 5.0f;
inline constexpr int kWCooldownReductionMs = 10000;

struct CollisionBody {
    Vec3 Position{};
    float Radius = 0.0f;
    int NetworkId = 0;
    bool Intended = false;
};

inline bool CircleContains(const Vec3& center, const Vec3& point,
                           float radius, float pointRadius = 0.0f) {
    return center.IsValid() && !center.IsZero() && point.IsValid() &&
           !point.IsZero() && center.Distance2D(point) <=
               std::max(0.0f, radius) + std::max(0.0f, pointRadius);
}

inline bool BombExplosionHits(const Vec3& center, const Vec3& predictedTarget,
                              float targetRadius = 0.0f) {
    return CircleContains(center, predictedTarget, kQRadius, targetRadius);
}

inline float BombImpactSeconds(const Vec3& origin, const Vec3& destination) {
    if (!origin.IsValid() || !destination.IsValid() || origin.IsZero() ||
        destination.IsZero()) return 0.0f;
    return kQDelay + origin.Distance2D(destination) / kQSpeed;
}

inline float BombDetonationSeconds(const Vec3& origin, const Vec3& destination) {
    return BombImpactSeconds(origin, destination) + kQFuseSeconds;
}

inline bool BombPathCollision(const Vec3& origin, const Vec3& destination,
                              const CollisionBody* bodies, std::size_t count,
                              bool allowIntended) {
    if (!origin.IsValid() || !destination.IsValid() || origin.IsZero() ||
        destination.IsZero()) return true;
    if (origin.Distance2D(destination) > kQRange + 0.01f) return true;
    for (std::size_t i = 0; i < count; ++i) {
        const auto& body = bodies[i];
        if (body.Position.IsZero() || (allowIntended && body.Intended)) continue;
        const auto projected = SharedGeometry::ProjectPointToSegment2D(
            body.Position, origin, destination);
        if (projected.T >= 0.0f && projected.T <= 1.0f &&
            projected.Distance <= kQMissileWidth * 0.5f +
                std::max(0.0f, body.Radius)) return true;
    }
    return false;
}

inline bool ProjectileWallSafe(const Vec3& origin, const Vec3& destination,
                               bool wallBlocks) {
    return origin.IsValid() && destination.IsValid() && !origin.IsZero() &&
           !destination.IsZero() && origin.Distance2D(destination) <=
               kQRange + 0.01f && !wallBlocks;
}

inline Vec3 PredictedPosition(const Vec3& position, const Vec3& velocity,
                              float seconds, float maxDisplacement = 650.0f) {
    if (!position.IsValid() || position.IsZero()) return {};
    const float t = std::max(0.0f, seconds);
    Vec3 result = position + velocity * t;
    result.y = position.y;
    if (maxDisplacement > 0.0f && result.Distance2D(position) > maxDisplacement) {
        result = position + Direction2D(position, result) * maxDisplacement;
        result.y = position.y;
    }
    return result;
}

inline bool DoubleBombWindow(float firstDetonationTick,
                             float secondImpactTick,
                             float nowTick,
                             float fuseMs = kQFuseSeconds * 1000.0f) {
    return firstDetonationTick > nowTick && secondImpactTick >= nowTick &&
           secondImpactTick < firstDetonationTick &&
           firstDetonationTick - nowTick <= fuseMs + 1.0f;
}

inline bool DoubleBombStuns(float firstDetonationTick,
                            float secondImpactTick,
                            float nowTick,
                            float fuseMs = kQFuseSeconds * 1000.0f) {
    return DoubleBombWindow(firstDetonationTick, secondImpactTick, nowTick,
                            fuseMs);
}

inline float StunDurationSeconds(int rank) {
    static constexpr std::array<float, 5> values{1.1f, 1.2f, 1.3f, 1.4f, 1.5f};
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline float SpeedAmountPercent(int rank) {
    static constexpr std::array<float, 5> values{25.0f, 40.0f, 55.0f, 70.0f, 85.0f};
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

struct TimeWarpEffect {
    bool Ally = false;
    float ModifierPercent = 0.0f;
    float DurationSeconds = kEDurationSeconds;
};

inline TimeWarpEffect MakeTimeWarpEffect(bool ally, int rank) {
    const float amount = SpeedAmountPercent(rank);
    return {ally, ally ? amount : -amount, kEDurationSeconds};
}

inline bool ETargetInReach(const Vec3& origin, const Vec3& target,
                           float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() &&
           !target.IsZero() && origin.Distance2D(target) <=
               kERange + std::max(0.0f, targetRadius);
}

inline bool RTargetInReach(const Vec3& origin, const Vec3& target,
                           float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() && !origin.IsZero() &&
           !target.IsZero() && origin.Distance2D(target) <=
               kRRange + std::max(0.0f, targetRadius);
}

struct ResurrectionCandidate {
    int NetworkId = 0;
    float HealthPercent = 100.0f;
    float IncomingDamagePercent = 0.0f;
    float CarryScore = 0.0f;
    int NearbyEnemies = 0;
    bool IsSelf = false;
    bool AlreadyProtected = false;
    bool ManualSave = false;
    bool Valid = false;
};

inline float ChronoTargetScore(const ResurrectionCandidate& candidate) {
    if (!candidate.Valid || candidate.AlreadyProtected) return -1000000.0f;
    float score = candidate.CarryScore;
    score += (100.0f - std::clamp(candidate.HealthPercent, 0.0f, 100.0f)) * 2.4f;
    score += std::clamp(candidate.IncomingDamagePercent, 0.0f, 100.0f) * 4.2f;
    score += static_cast<float>(std::max(0, candidate.NearbyEnemies)) * 115.0f;
    if (candidate.IsSelf) score += 90.0f;
    if (candidate.ManualSave) score += 420.0f;
    return score;
}

inline int ChooseChronoTarget(const ResurrectionCandidate* candidates,
                              std::size_t count) {
    int bestId = 0;
    float bestScore = -1000000.0f;
    for (std::size_t i = 0; i < count; ++i) {
        const float score = ChronoTargetScore(candidates[i]);
        if (score > bestScore) {
            bestScore = score;
            bestId = candidates[i].NetworkId;
        }
    }
    return bestId;
}

inline bool ManualSaveProtected(bool controllerCast, bool recentlyManual,
                                bool alreadyProtected, int nowTick,
                                int manualUntilTick) {
    return alreadyProtected || (!controllerCast && recentlyManual &&
                                nowTick <= manualUntilTick);
}

inline bool LethalAfterShield(float damage, float health, float shield) {
    return damage >= std::max(0.0f, health) + std::max(0.0f, shield);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zilean::Geometry
