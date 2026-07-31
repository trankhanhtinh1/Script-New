#pragma once

// Pure Zed mechanics.  This header deliberately has no game, menu or SDK
// dependency so collision, shadow and death-mark safety can be tested in
// isolation from live-object telemetry.
#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Zed::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::SegmentProjection;

inline constexpr float kQRange = 925.0f;
inline constexpr float kQHalfWidth = 45.0f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWLifetimeSeconds = 5.0f;
inline constexpr float kERadius = 290.0f;
inline constexpr float kRRange = 625.0f;
inline constexpr float kRMarkSeconds = 3.0f;
inline constexpr float kEnergyReserve = 20.0f;

inline bool InReach(float distance, float range, float radius = 0.0f) {
    return std::isfinite(distance) && distance >= 0.0f &&
           distance <= std::max(0.0f, range) + std::max(0.0f, radius);
}

struct CollisionBody {
    Vec3 Position = {};
    float Radius = 0.0f;
    int NetworkId = 0;
    bool Target = false;
    bool Blocking = true;
};

struct QCollisionResult {
    bool TargetHit = false;
    bool TargetFirst = false;
    int FirstBodyId = 0;
    int TargetBodyId = 0;
    float FirstDistance = 0.0f;
    float TargetDistance = 0.0f;
    float TargetDamageMultiplier = 0.0f;
};

inline QCollisionResult EvaluateQFirstCollision(
    const Vec3& source,
    const Vec3& castEndpoint,
    const std::vector<CollisionBody>& bodies,
    int targetId,
    float halfWidth = kQHalfWidth) {
    QCollisionResult result{};
    if (!source.IsValid() || !castEndpoint.IsValid() ||
        source.Distance2D(castEndpoint) <= 0.01f) return result;
    float firstT = 2.0f;
    float targetT = 2.0f;
    for (const CollisionBody& body : bodies) {
        if (!body.Blocking || !body.Position.IsValid()) continue;
        const SegmentProjection projection = ProjectPointToSegment2D(
            body.Position, source, castEndpoint);
        const float hitRadius = std::max(1.0f, halfWidth) +
            std::clamp(body.Radius, 0.0f, 120.0f);
        if (projection.T < -0.001f || projection.T > 1.001f ||
            projection.Distance > hitRadius) continue;
        if (projection.T < firstT) {
            firstT = projection.T;
            result.FirstBodyId = body.NetworkId;
            result.FirstDistance = source.Distance2D(body.Position);
        }
        if (body.NetworkId == targetId || body.Target) {
            result.TargetHit = true;
            result.TargetBodyId = body.NetworkId;
            targetT = std::min(targetT, projection.T);
            result.TargetDistance = source.Distance2D(body.Position);
        }
    }
    if (result.FirstBodyId != 0) {
        result.TargetFirst = result.TargetHit &&
            std::fabs(firstT - targetT) <= 0.0005f;
        result.TargetDamageMultiplier = result.TargetFirst ? 1.0f : 0.6f;
    }
    return result;
}

inline float ShurikenDamageMultiplier(bool firstCollision) {
    return firstCollision ? 1.0f : 0.6f;
}

inline bool ShurikenHits(const Vec3& source,
                         const Vec3& endpoint,
                         const Vec3& target,
                         float targetRadius,
                         float halfWidth = kQHalfWidth) {
    if (!source.IsValid() || !endpoint.IsValid() || !target.IsValid()) return false;
    const SegmentProjection projection = ProjectPointToSegment2D(
        target, source, endpoint);
    return projection.T >= -0.001f && projection.T <= 1.001f &&
           projection.Distance <= std::max(1.0f, halfWidth) +
               std::clamp(targetRadius, 0.0f, 120.0f);
}

enum class ShadowKind : int { None, W, R };
enum class ShadowAction : int { None, Hold, Swap, Expire };

struct ShadowPair {
    ShadowKind Kind = ShadowKind::None;
    int NetworkId = 0;
    Vec3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Observed = false;
};

struct ShadowPairState {
    ShadowPair W = {};
    ShadowPair R = {};
    bool Swapped = false;
};

inline bool ShadowAlive(const ShadowPair& shadow, int now) {
    return shadow.Kind != ShadowKind::None && shadow.Position.IsValid() &&
           shadow.ExpireTick > now;
}

inline ShadowPair ReconcileShadow(const ShadowPair& prior,
                                  ShadowKind kind,
                                  int networkId,
                                  const Vec3& position,
                                  int now,
                                  int lifetimeMs,
                                  bool observed) {
    ShadowPair next = prior;
    next.Kind = kind;
    if (networkId != 0) next.NetworkId = networkId;
    if (position.IsValid() && !position.IsZero()) next.Position = position;
    if (next.SpawnTick <= 0) next.SpawnTick = now;
    next.ExpireTick = std::max(next.ExpireTick, now + std::max(1, lifetimeMs));
    next.Observed = next.Observed || observed;
    return next;
}

inline ShadowPairState ReconcileShadowPair(const ShadowPairState& prior,
                                           bool wObserved,
                                           const Vec3& wPosition,
                                           int wId,
                                           bool rObserved,
                                           const Vec3& rPosition,
                                           int rId,
                                           int now) {
    ShadowPairState next = prior;
    if (wObserved) next.W = ReconcileShadow(
        next.W, ShadowKind::W, wId, wPosition, now, 5000, true);
    if (rObserved) next.R = ReconcileShadow(
        next.R, ShadowKind::R, rId, rPosition, now, 4000, true);
    if (!ShadowAlive(next.W, now)) next.W = {};
    if (!ShadowAlive(next.R, now)) next.R = {};
    return next;
}

inline bool CanSwapToShadow(const ShadowPair& shadow,
                            int now,
                            bool turretSafe,
                            int enemiesAtDestination,
                            int maximumEnemies,
                            bool lethal,
                            bool emergency) {
    if (!ShadowAlive(shadow, now) || !shadow.Position.IsValid() ||
        shadow.Position.IsZero()) return false;
    if (!turretSafe && !lethal && !emergency) return false;
    return lethal || emergency ||
           enemiesAtDestination <= std::max(0, maximumEnemies);
}

struct ShadowMark {
    int TargetId = 0;
    int ShadowId = 0;
    int AppliedTick = 0;
    int ExpireTick = 0;
    bool FromW = false;
    bool FromE = false;
};

inline ShadowMark ApplyShadowMark(const ShadowMark& prior,
                                  int targetId,
                                  int shadowId,
                                  int now,
                                  int durationMs,
                                  bool fromW,
                                  bool fromE) {
    ShadowMark next = prior;
    next.TargetId = targetId;
    next.ShadowId = shadowId;
    next.AppliedTick = now;
    next.ExpireTick = now + std::max(1, durationMs);
    next.FromW = prior.FromW || fromW;
    next.FromE = prior.FromE || fromE;
    return next;
}

inline bool MarkActive(const ShadowMark& mark, int targetId, int now) {
    return mark.TargetId == targetId && targetId != 0 && mark.ExpireTick > now &&
           (mark.FromW || mark.FromE);
}

inline ShadowMark ReconcileShadowMark(const ShadowMark& mark,
                                      bool buffPresent,
                                      int targetId,
                                      int now,
                                      int fallbackDurationMs) {
    ShadowMark next = mark;
    if (buffPresent && targetId != 0) {
        next.TargetId = targetId;
        next.ExpireTick = std::max(next.ExpireTick,
            now + std::max(1, fallbackDurationMs));
    }
    if ((!buffPresent && next.ExpireTick <= now) || next.ExpireTick <= now) next = {};
    return next;
}

enum class DeathMarkAction : int { None, Apply, RecastReturn, Hold };

struct DeathMark {
    bool Active = false;
    int TargetId = 0;
    Vec3 ReturnPosition = {};
    int AppliedTick = 0;
    int ExpireTick = 0;
    float StoredDamage = 0.0f;
};

inline float DeathMarkDamage(float storedDamage, int rank) {
    const float coefficient = rank <= 1 ? 0.25f : rank == 2 ? 0.40f : 0.55f;
    return std::max(0.0f, storedDamage) * coefficient;
}

inline bool DeathMarkReady(const DeathMark& mark,
                          int targetId,
                          int now,
                          float targetHealth,
                          float targetShield,
                          int rank) {
    return mark.Active && mark.TargetId == targetId && mark.ExpireTick > now &&
           DeathMarkDamage(mark.StoredDamage, rank) >=
               std::max(0.0f, targetHealth) + std::max(0.0f, targetShield);
}

inline bool SafeReturn(const Vec3& returnPosition,
                       bool walkable,
                       bool turretSafe,
                       int enemiesAtReturn,
                       int maximumEnemies,
                       bool lethal,
                       bool emergency) {
    if (!returnPosition.IsValid() || returnPosition.IsZero() || !walkable) return false;
    if (!turretSafe && !lethal && !emergency) return false;
    return lethal || emergency ||
           enemiesAtReturn <= std::max(0, maximumEnemies);
}

struct EnergyGate {
    float Current = 0.0f;
    float Cost = 0.0f;
    float Reserve = 0.0f;
};

inline bool HasEnergy(const EnergyGate& gate) {
    return std::isfinite(gate.Current) &&
           gate.Current + 0.01f >= std::max(0.0f, gate.Cost) +
               std::max(0.0f, gate.Reserve);
}

inline bool CommitSafe(bool turretSafe,
                       int enemies,
                       int maximumEnemies,
                       float healthPercent,
                       float minimumHealthPercent,
                       bool lethal,
                       bool emergency) {
    if (!turretSafe && !lethal && !emergency) return false;
    return lethal || emergency ||
           (enemies <= std::max(0, maximumEnemies) &&
            healthPercent >= minimumHealthPercent);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zed::Geometry
