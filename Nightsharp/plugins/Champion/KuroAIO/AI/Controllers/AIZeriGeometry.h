#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Zeri::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 825.0f;
inline constexpr float kQWidth = 20.0f;
inline constexpr float kQSpeed = 2600.0f;
inline constexpr float kWRange = 1200.0f;
inline constexpr float kWWidth = 80.0f;
inline constexpr float kWWallWidth = 120.0f;
inline constexpr float kWSpeed = 2500.0f;
inline constexpr float kERange = 300.0f;
inline constexpr float kRRange = 825.0f;
inline constexpr int kMaximumOvercharge = 50;
inline constexpr int kOverchargeDurationMs = 5000;

inline float ClampFinite(float value, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : minimum;
}

inline float ClampReach(float distance, float maximum) {
    return ClampFinite(distance, 0.0f, std::max(0.0f, maximum));
}

inline bool InReach(float distance, float range, float radius = 0.0f) {
    return std::isfinite(distance) && distance >= 0.0f &&
           distance <= std::max(0.0f, range) + std::max(0.0f, radius);
}

struct ShotState {
    bool Charged = false;
    bool Basic = true;
    bool AttackWindup = false;
    bool MovementLocked = false;
};

inline ShotState ReconcileShotState(bool chargedBuff,
                                    bool attackWindup,
                                    bool movementLocked) {
    ShotState result{};
    result.Charged = chargedBuff;
    result.Basic = !chargedBuff;
    result.AttackWindup = attackWindup;
    result.MovementLocked = movementLocked;
    return result;
}

inline bool ShouldPreserveShot(const ShotState& state,
                               bool validTarget,
                               bool castCommitted) {
    if (!validTarget) return false;
    if (state.Charged) return !castCommitted;
    return state.AttackWindup || state.MovementLocked;
}

inline int ClampOvercharge(int stacks) {
    return std::clamp(stacks, 0, kMaximumOvercharge);
}

inline int AddOvercharge(int stacks, int amount = 1) {
    return ClampOvercharge(ClampOvercharge(stacks) + std::max(0, amount));
}

inline bool OverchargeActive(int stacks, int nowTick, int expiryTick) {
    return ClampOvercharge(stacks) > 0 && expiryTick > nowTick;
}

inline int RefreshOverchargeExpiry(int nowTick, int durationMs = kOverchargeDurationMs) {
    return nowTick + std::max(0, durationMs);
}

inline float OverchargeMultiplier(int stacks) {
    return 1.0f + 0.02f * static_cast<float>(ClampOvercharge(stacks));
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Target = false;
    bool Valid = true;
};

struct CollisionResult {
    bool TargetHit = false;
    bool TargetFirst = false;
    int FirstBodyId = 0;
    int TargetBodyIndex = -1;
    std::vector<int> OrderedIds = {};
};

inline CollisionResult EvaluateQCollision(const Vec3& origin,
                                          const Vec3& endpoint,
                                          const std::vector<Body>& bodies,
                                          int targetId,
                                          float width = kQWidth) {
    CollisionResult result{};
    struct Contact { float T; int Id; float Radius; bool Target; };
    std::vector<Contact> contacts;
    contacts.reserve(bodies.size());
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(
            body.Position, origin, endpoint);
        if (projection.Distance <= std::max(0.0f, width) * 0.5f +
                                     std::max(0.0f, body.Radius)) {
            contacts.push_back({projection.T, body.Id, body.Radius,
                                body.Target || body.Id == targetId});
        }
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const Contact& left, const Contact& right) {
            return left.T < right.T;
        });
    for (std::size_t index = 0; index < contacts.size(); ++index) {
        result.OrderedIds.push_back(contacts[index].Id);
        if (index == 0) result.FirstBodyId = contacts[index].Id;
        if (!result.TargetHit && contacts[index].Target) {
            result.TargetHit = true;
            result.TargetBodyIndex = static_cast<int>(index);
        }
    }
    result.TargetFirst = result.TargetHit && result.TargetBodyIndex == 0;
    return result;
}

inline bool LaserHits(const Vec3& origin,
                      const Vec3& endpoint,
                      const Vec3& target,
                      float targetRadius,
                      bool throughWall,
                      float normalWidth = kWWidth) {
    if (origin.Distance2D(endpoint) <= 0.001f) return false;
    const float width = throughWall ? kWWallWidth : normalWidth;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
                                  std::max(0.0f, targetRadius);
}

inline Vec3 ClampDashEndpoint(const Vec3& origin,
                              const Vec3& requested,
                              float maximumRange = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    const float distance = std::min(origin.Distance2D(requested),
                                    std::max(0.0f, maximumRange));
    return origin + direction * distance;
}

struct DashSafety {
    bool EndpointValid = false;
    bool Walkable = false;
    bool TerrainInteraction = false;
    bool TurretSafe = false;
    bool ThreatSafe = false;
    bool DirectionUseful = false;
    bool Flee = false;
    bool Lethal = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 1;
};

inline bool MayDash(const DashSafety& context) {
    if (!context.EndpointValid || !context.Walkable || !context.TurretSafe ||
        !context.ThreatSafe || !context.DirectionUseful ||
        context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies)) {
        return false;
    }
    return context.Flee || context.Lethal || context.TerrainInteraction;
}

inline bool ShouldCastUltimate(bool ready,
                               bool active,
                               bool manual,
                               bool committed,
                               bool lethal,
                               bool safe,
                               int nearbyEnemies,
                               int minimumEnemies) {
    if (!ready || active || !safe) return false;
    if (manual || lethal) return true;
    return committed && nearbyEnemies >= std::max(1, minimumEnemies);
}

inline bool ShouldHoldMovement(bool attackWindup,
                               bool chargedShot,
                               bool castCommitted,
                               bool targetReachable) {
    if (!targetReachable) return false;
    return attackWindup || (chargedShot && !castCommitted);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zeri::Geometry
