#pragma once

// Brand-only geometry and deterministic damage/state rules. Runtime target
// validity, prediction and casts remain in AIBrandController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Brand::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kSearRange = 1050.0f;
inline constexpr float kSearWidth = 80.0f;
inline constexpr float kSearSpeed = 1600.0f;
inline constexpr float kCastDelay = 0.25f;
inline constexpr float kPillarRange = 900.0f;
inline constexpr float kPillarRadius = 240.0f;
inline constexpr float kConflagrationRange = 625.0f;
inline constexpr float kConflagrationRadius = 315.0f;
inline constexpr float kPyroclasmRange = 750.0f;
inline constexpr float kPyroclasmBounceRadius = 600.0f;
inline constexpr int kPyroclasmBounces = 5;
inline constexpr int kAblazeStacksForDetonation = 3;
inline constexpr int kAblazeDurationMs = 4000;

inline float AlongRay(const Vec3& origin, const Vec3& direction,
                      const Vec3& point) {
    Vec3 relative = point - origin;
    relative.y = 0.0f;
    return relative.Dot(direction);
}

inline float PerpendicularToRay(const Vec3& origin, const Vec3& direction,
                                const Vec3& point) {
    const float along = AlongRay(origin, direction, point);
    const Vec3 closest = origin + direction * along;
    return point.Distance2D(closest);
}

inline bool SearLineHits(const Vec3& origin, const Vec3& aim,
                         const Vec3& target, float targetRadius = 0.0f) {
    if (!origin.IsValid() || !aim.IsValid() || !target.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const float along = AlongRay(origin, direction, target);
    const float radius = kSearWidth * 0.5f + std::clamp(targetRadius, 0.0f, 200.0f);
    return along >= -radius && along <= kSearRange + radius &&
           PerpendicularToRay(origin, direction, target) <= radius;
}

inline bool SearCollisionFree(const Vec3& origin, const Vec3& aim,
                              const std::vector<Vec3>& blockers,
                              float blockerRadius = 35.0f) {
    if (!origin.IsValid() || !aim.IsValid()) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const float end = std::min(kSearRange, origin.Distance2D(aim));
    for (const Vec3& blocker : blockers) {
        if (!blocker.IsValid()) continue;
        const float along = AlongRay(origin, direction, blocker);
        if (along > 0.0f && along < end &&
            PerpendicularToRay(origin, direction, blocker) <=
                kSearWidth * 0.5f + std::max(0.0f, blockerRadius)) return false;
    }
    return true;
}

inline float SearImpactSeconds(float distance) {
    return kCastDelay + std::clamp(distance, 0.0f, kSearRange) / kSearSpeed;
}

inline bool PillarHits(const Vec3& center, const Vec3& target,
                       float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= kPillarRadius +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool ConflagrationHits(const Vec3& center, const Vec3& target,
                              float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= kConflagrationRadius +
               std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool InRealReach(const Vec3& origin, const Vec3& target, float range,
                        float casterRadius = 0.0f, float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
           origin.Distance2D(target) <= range + std::max(0.0f, casterRadius) +
               std::max(0.0f, targetRadius);
}

inline bool QStunAllowed(int confirmedStacks, bool ablazeBeforeImpact,
                         int safetyMs = 35) {
    (void)safetyMs;
    return confirmedStacks >= 1 || ablazeBeforeImpact;
}

inline bool AblazeDetonationReady(int stacks, bool pendingAblaze = false) {
    return stacks + (pendingAblaze ? 1 : 0) >= kAblazeStacksForDetonation;
}

inline int NextAblazeStacks(int currentStacks) {
    return std::clamp(currentStacks, 0, kAblazeStacksForDetonation - 1) + 1;
}

inline float BlazeRawDamage(int championLevel, float maxHealth,
                            float abilityPower = 0.0f) {
    const float levelPercent = 6.0f +
        (std::clamp(static_cast<float>(championLevel), 1.0f, 18.0f) - 1.0f) *
            (6.0f / 17.0f);
    const float percent = (levelPercent + std::max(0.0f, abilityPower) * 0.02f) * 0.01f;
    return std::max(0.0f, maxHealth) * percent;
}

inline float SpellRawDamage(int slot, int rank, float abilityPower,
                            bool empowered = false) {
    const int r = std::clamp(rank, 0, 5);
    const float ap = std::max(0.0f, abilityPower);
    static constexpr std::array<float, 6> q{0, 40, 70, 100, 130, 160};
    static constexpr std::array<float, 6> w{0, 75, 120, 165, 210, 255};
    static constexpr std::array<float, 6> e{0, 35, 70, 105, 140, 175};
    static constexpr std::array<float, 6> rr{0, 100, 150, 200, 250, 300};
    switch (slot) {
    case 0: return q[static_cast<std::size_t>(r)] + ap * 0.65f;
    case 1: return (w[static_cast<std::size_t>(r)] + ap * 0.70f) *
                     (empowered ? 1.25f : 1.0f);
    case 2: return e[static_cast<std::size_t>(r)] + ap * 0.55f;
    case 3: return rr[static_cast<std::size_t>(r)] + ap * 0.50f;
    default: return 0.0f;
    }
}

struct BounceCandidate {
    int NetworkId = 0;
    Vec3 Position = {};
    float Radius = 0.0f;
    int AblazeStacks = 0;
    bool IsSelected = false;
    bool IsChampion = true;
};

struct BounceRoute {
    std::array<int, kPyroclasmBounces> NetworkIds{};
    int Count = 0;
    bool Safe = false;
};

inline int ClusterCount(const BounceCandidate& center,
                        const std::vector<BounceCandidate>& candidates,
                        int ignoredId) {
    int count = 0;
    for (const auto& candidate : candidates) {
        if (candidate.NetworkId == ignoredId || !candidate.Position.IsValid()) continue;
        if (center.Position.Distance2D(candidate.Position) <=
            kPyroclasmBounceRadius + center.Radius + candidate.Radius) ++count;
    }
    return count;
}

inline BounceRoute BuildBounceRoute(const BounceCandidate& first,
                                    const std::vector<BounceCandidate>& candidates,
                                    int minimumTargets = 2) {
    BounceRoute route{};
    if (first.NetworkId == 0 || !first.Position.IsValid()) return route;
    BounceCandidate current = first;
    std::array<int, kPyroclasmBounces> used{};
    for (int bounce = 0; bounce < kPyroclasmBounces; ++bounce) {
        route.NetworkIds[static_cast<std::size_t>(bounce)] = current.NetworkId;
        used[static_cast<std::size_t>(bounce)] = current.NetworkId;
        ++route.Count;
        const BounceCandidate* best = nullptr;
        float bestScore = -1.0e30f;
        for (const auto& candidate : candidates) {
            if (!candidate.IsChampion || candidate.NetworkId == 0 ||
                !candidate.Position.IsValid() ||
                std::find(used.begin(), used.end(), candidate.NetworkId) != used.end()) continue;
            if (current.Position.Distance2D(candidate.Position) >
                kPyroclasmBounceRadius + current.Radius + candidate.Radius) continue;
            float score = static_cast<float>(candidate.AblazeStacks) * 1000.0f +
                (candidate.IsSelected ? 180.0f : 0.0f) +
                static_cast<float>(ClusterCount(candidate, candidates, current.NetworkId)) * 85.0f;
            score -= current.Position.Distance2D(candidate.Position) * 0.10f;
            if (score > bestScore) { bestScore = score; best = &candidate; }
        }
        if (!best) break;
        current = *best;
    }
    route.Safe = route.Count >= std::clamp(minimumTargets, 1, kPyroclasmBounces);
    return route;
}

inline bool DetonationClusterSafe(const Vec3& center, int nearbyEnemies,
                                  int maximumEnemies, bool lethal,
                                  bool selectedIncluded) {
    return center.IsValid() && selectedIncluded &&
           (lethal || nearbyEnemies <= std::max(1, maximumEnemies));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Brand::Geometry
