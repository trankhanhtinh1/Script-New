#pragma once

// Pure Gwen mechanics for the Riot 26.15 / CommunityDragon 16.15 contract.
// Runtime prediction, buff polling, NavMesh checks and casts live in
// AIGwenController; this file makes stack, sweet-spot, mist, dash and recast
// decisions deterministic and independently testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Gwen::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr int kMaximumSnipStacks = 4;
inline constexpr int kSnipStackDurationMs = 6000;
inline constexpr float kQGameplayRange = 450.0f;
inline constexpr float kQCastSeconds = 0.50f;
inline constexpr float kQOuterHalfWidth = 105.0f;
inline constexpr float kQCenterHalfWidth = 30.0f;
inline constexpr float kQCenterTrueFraction = 0.50f;
inline constexpr float kQPassiveEffectiveness = 0.75f;
inline constexpr float kMistRadius = 370.0f;
inline constexpr float kMistDurationSeconds = 4.0f;
inline constexpr float kMistRecastDelaySeconds = 0.50f;
inline constexpr float kMistPullOffset = 75.0f;
inline constexpr float kEDashRange = 350.0f;
inline constexpr float kEDashSpeed = 800.0f;
inline constexpr float kEBuffSeconds = 4.0f;
inline constexpr float kRGameplayRange = 1200.0f;
inline constexpr float kRHalfWidth = 60.0f;
inline constexpr float kRMissileSpeed = 1800.0f;
inline constexpr float kRFirstCastSeconds = 0.25f;
inline constexpr float kRRecastSeconds = 0.50f;
inline constexpr float kRRecastLockSeconds = 1.0f;
inline constexpr float kRRecastWindowSeconds = 6.0f;

inline int ClampSnipStacks(int stacks) {
    return std::clamp(stacks, 0, kMaximumSnipStacks);
}

inline int QMiniSnips(int stacks) {
    return 1 + ClampSnipStacks(stacks);
}

inline int QTotalSnips(int stacks) {
    return QMiniSnips(stacks) + 1;
}

struct SnipStackState {
    int Stacks = 0;
    int ExpireTick = 0;

    void Observe(int stacks, int expireTick) {
        Stacks = ClampSnipStacks(stacks);
        ExpireTick = Stacks > 0 ? std::max(0, expireTick) : 0;
    }

    void AddAttack(int now) {
        Stacks = std::min(kMaximumSnipStacks, Stacks + 1);
        ExpireTick = now + kSnipStackDurationMs;
    }

    void Spend() {
        Stacks = 0;
        ExpireTick = 0;
    }

    void Poll(int now, int observedStacks, bool buffPresent,
              int observedExpireTick = 0) {
        if (buffPresent) {
            Observe(observedStacks,
                    observedExpireTick > now ? observedExpireTick
                                              : now + kSnipStackDurationMs);
        } else if (ExpireTick > 0 && now >= ExpireTick) {
            Spend();
        }
    }
};

inline float PassivePercentOfMaximumHealth(float abilityPower) {
    // CDragon PercentHealth1000Cuts: (1 + 0.006 AP) * 0.01.
    return 0.01f + std::max(0.0f, abilityPower) * 0.00006f;
}

inline float PassiveRawDamage(float targetMaximumHealth,
                              float abilityPower,
                              bool monster = false) {
    const float raw = std::max(0.0f, targetMaximumHealth) *
                      PassivePercentOfMaximumHealth(abilityPower);
    if (!monster) return raw;
    return std::min(raw, 3.0f + std::max(0.0f, abilityPower) * 0.05f);
}

inline float QMiniSnipRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 10.0f, 14.0f, 18.0f, 22.0f, 26.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.05f;
}

inline float QFinalSnipRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 85.0f, 110.0f, 135.0f, 160.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.35f;
}

inline float QRawDamage(int rank, int stacks, float abilityPower) {
    return QMiniSnipRawDamage(rank, abilityPower) *
               static_cast<float>(QMiniSnips(stacks)) +
           QFinalSnipRawDamage(rank, abilityPower);
}

inline float QPassiveRawDamage(int stacks,
                               float targetMaximumHealth,
                               float abilityPower,
                               bool monster = false) {
    return PassiveRawDamage(targetMaximumHealth, abilityPower, monster) *
           static_cast<float>(QTotalSnips(stacks)) *
           kQPassiveEffectiveness;
}

struct QDamageSplit {
    float Magical = 0.0f;
    float True = 0.0f;
    float PassiveMagical = 0.0f;

    float TotalRaw() const { return Magical + True + PassiveMagical; }
};

inline QDamageSplit QDamage(int rank,
                            int stacks,
                            float abilityPower,
                            float targetMaximumHealth,
                            bool center,
                            bool monster = false) {
    const float spell = QRawDamage(rank, stacks, abilityPower);
    const float converted = center ? spell * kQCenterTrueFraction : 0.0f;
    return {
        spell - converted,
        converted,
        QPassiveRawDamage(stacks, targetMaximumHealth,
                          abilityPower, monster),
    };
}

struct QHit {
    bool Outer = false;
    bool Center = false;
    float Along = 0.0f;
    float Perpendicular = 0.0f;
};

inline QHit EvaluateQHit(const Vec3& origin,
                         const Vec3& aim,
                         const Vec3& target,
                         float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return {};
    const Vec3 endpoint = origin + direction * kQGameplayRange;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    const float radius = std::max(0.0f, targetRadius);
    const bool inLength = projection.T >= 0.0f && projection.T <= 1.0f &&
        target.Distance2D(origin) <= kQGameplayRange + radius;
    return {
        inLength && projection.Distance <= kQOuterHalfWidth + radius,
        inLength && projection.Distance <= kQCenterHalfWidth + radius,
        projection.T * kQGameplayRange,
        projection.Distance,
    };
}

inline bool BetterQAim(const QHit& candidate,
                       int candidateStacks,
                       const QHit& current,
                       int currentStacks) {
    if (candidate.Center != current.Center) return candidate.Center;
    if (candidate.Outer != current.Outer) return candidate.Outer;
    if (candidateStacks != currentStacks) return candidateStacks > currentStacks;
    return candidate.Perpendicular < current.Perpendicular;
}

struct MistState {
    bool Active = false;
    bool RecastAvailable = false;
    Vec3 Center = {};
    int StartTick = 0;
    int ExpireTick = 0;
    int Repositions = 0;

    void Begin(const Vec3& center, int now) {
        Active = true;
        RecastAvailable = false;
        Center = center;
        StartTick = now;
        ExpireTick = now + static_cast<int>(kMistDurationSeconds * 1000.0f);
        Repositions = 0;
    }

    void Reposition(const Vec3& center, int now) {
        if (!Active || Repositions >= 1) return;
        Center = center;
        RecastAvailable = false;
        ++Repositions;
        ExpireTick = std::max(ExpireTick, now + 250);
    }

    void Poll(int now, bool buffPresent) {
        if (Active && now - StartTick >=
                static_cast<int>(kMistRecastDelaySeconds * 1000.0f) &&
            Repositions == 0) {
            RecastAvailable = true;
        }
        if (!buffPresent && ExpireTick > 0 && now >= ExpireTick) Clear();
    }

    void Clear() { *this = {}; }
};

inline bool InsideMist(const Vec3& point,
                       const Vec3& center,
                       float bodyRadius = 0.0f,
                       float margin = 0.0f) {
    return point.Distance2D(center) <=
        kMistRadius + std::max(0.0f, bodyRadius) + margin;
}

inline float DistanceToMistBoundary(const Vec3& point,
                                    const Vec3& center) {
    return kMistRadius - point.Distance2D(center);
}

inline bool ThreatBlockedByMist(const Vec3& source,
                                const Vec3& player,
                                const Vec3& center,
                                float sourceRadius = 0.0f) {
    return InsideMist(player, center) &&
           !InsideMist(source, center, sourceRadius);
}

inline bool ShouldRecenterMist(const Vec3& player,
                               const Vec3& center,
                               const Vec3& desired,
                               bool recastAvailable,
                               bool incomingOutsideThreat,
                               float boundaryReserve = 55.0f) {
    if (!recastAvailable) return false;
    const float remaining = DistanceToMistBoundary(player, center);
    if (remaining <= std::max(0.0f, boundaryReserve) &&
        player.Distance2D(desired) + 5.0f < center.Distance2D(desired)) {
        return true;
    }
    return incomingOutsideThreat && remaining <= 90.0f;
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& desired) {
    const float distance = origin.Distance2D(desired);
    if (distance <= 0.001f || !std::isfinite(distance)) return origin;
    if (distance <= kEDashRange) return desired;
    return origin + Direction2D(origin, desired) * kEDashRange;
}

struct DashSafety {
    bool EndpointValid = false;
    bool TerrainBlocked = false;
    bool NewEnemyTurret = false;
    bool DashHazard = false;
    bool PointClickThreat = false;
    bool Lethal = false;
    bool Fleeing = false;
    bool KeepsTargetInAttackRange = false;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline bool SafeDash(const DashSafety& context) {
    if (!context.EndpointValid || context.TerrainBlocked) return false;
    if (context.NewEnemyTurret && !context.Lethal) return false;
    if ((context.DashHazard || context.PointClickThreat) &&
        !context.Fleeing && !context.Lethal) return false;
    if (!context.Fleeing && !context.Lethal &&
        context.EnemiesAtEndpoint >
            std::max(context.MaximumEnemies, context.AlliesAtEndpoint + 1)) {
        return false;
    }
    return context.Fleeing || context.Lethal ||
           context.KeepsTargetInAttackRange;
}

inline float EAttackRangeBonus(int rank) {
    (void)rank;
    return 75.0f;
}

inline float EAttackSpeedPercent(int rank) {
    static constexpr std::array<float, 6> value = {
        0.0f, 30.0f, 42.5f, 55.0f, 67.5f, 80.0f,
    };
    return RankValue(value, rank);
}

inline float ECooldownRefundFraction(int rank) {
    static constexpr std::array<float, 6> value = {
        0.0f, 0.25f, 0.35f, 0.45f, 0.55f, 0.65f,
    };
    return RankValue(value, rank);
}

inline float EOnHitRawDamage(float abilityPower) {
    return 15.0f + std::max(0.0f, abilityPower) * 0.20f;
}

inline int RNeedleCount(int castNumber) {
    static constexpr std::array<int, 4> needles = { 0, 1, 3, 5 };
    return RankValue(needles, castNumber);
}

inline float RNeedleRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 30.0f, 50.0f, 70.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.10f;
}

inline float RCastRawDamage(int rank, int castNumber, float abilityPower) {
    return RNeedleRawDamage(rank, abilityPower) *
           static_cast<float>(RNeedleCount(castNumber));
}

inline float RCastPassiveRawDamage(int castNumber,
                                   float targetMaximumHealth,
                                   float abilityPower,
                                   bool monster = false) {
    return PassiveRawDamage(targetMaximumHealth, abilityPower, monster) *
           static_cast<float>(RNeedleCount(castNumber));
}

struct LineBody {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Champion = true;
};

struct NeedleLine {
    bool HitsPrimary = false;
    int ChampionHits = 0;
    std::vector<int> HitIds = {};
};

inline NeedleLine EvaluateNeedleLine(const Vec3& origin,
                                     const Vec3& aim,
                                     const std::vector<LineBody>& bodies,
                                     int primaryId) {
    NeedleLine result{};
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return result;
    const Vec3 endpoint = origin + direction * kRGameplayRange;
    for (const auto& body : bodies) {
        const auto projection = ProjectPointToSegment2D(
            body.Position, origin, endpoint);
        if (body.Position.Distance2D(origin) >
                kRGameplayRange + std::max(0.0f, body.Radius) ||
            projection.Distance > kRHalfWidth + std::max(0.0f, body.Radius)) {
            continue;
        }
        result.HitIds.push_back(body.Id);
        if (body.Champion) ++result.ChampionHits;
        if (body.Id == primaryId) result.HitsPrimary = true;
    }
    return result;
}

struct RecastState {
    int CastNumber = 0;
    int UnlockTick = 0;
    int ExpireTick = 0;

    void Begin(int now) {
        CastNumber = 1;
        UnlockTick = now + static_cast<int>(kRRecastLockSeconds * 1000.0f);
        ExpireTick = now + static_cast<int>(kRRecastWindowSeconds * 1000.0f);
    }

    void Advance(int now) {
        if (CastNumber <= 0 || CastNumber >= 3) {
            Clear();
            return;
        }
        ++CastNumber;
        UnlockTick = now + static_cast<int>(kRRecastLockSeconds * 1000.0f);
    }

    void Poll(int now, bool recastBuffPresent) {
        if ((ExpireTick > 0 && now >= ExpireTick) ||
            (!recastBuffPresent && CastNumber >= 3 && now >= UnlockTick)) {
            Clear();
        }
    }

    bool CanCast(int now) const {
        return CastNumber > 0 && CastNumber < 3 &&
               now >= UnlockTick && now < ExpireTick;
    }

    int NextCastNumber() const {
        return CastNumber <= 0 ? 1 : std::min(3, CastNumber + 1);
    }

    void Clear() { *this = {}; }
};

struct RPolicy {
    bool Ready = false;
    bool HitsPrimary = false;
    bool HitchanceGood = false;
    bool TargetProtected = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool Defensive = false;
    bool RecastExpiring = false;
    int CastNumber = 1;
    int ChampionHits = 0;
    int MinimumFirstCastHits = 2;
};

inline bool MayCastNeedles(const RPolicy& context) {
    if (!context.Ready || !context.HitsPrimary || !context.HitchanceGood ||
        context.TargetProtected) return false;
    if (context.AttackWindingUp && !context.Lethal &&
        !context.Defensive && !context.RecastExpiring) return false;
    if (context.CastNumber <= 1) {
        return context.Lethal || context.Defensive ||
               context.ChampionHits >= context.MinimumFirstCastHits;
    }
    // Once R1 has committed the cooldown, preserve R2/R3 unless no target can
    // be predicted; late casts are preferable to silently losing the window.
    return true;
}

struct ManaPlan {
    float Current = 0.0f;
    float Cost = 0.0f;
    float Reserve = 0.0f;
    bool Defensive = false;
    bool Lethal = false;
};

inline bool HasMana(const ManaPlan& plan) {
    if (plan.Current + 0.5f < plan.Cost) return false;
    return plan.Defensive || plan.Lethal ||
           plan.Current + 0.5f >= plan.Cost + std::max(0.0f, plan.Reserve);
}

struct AutomaticPolicy {
    bool IncomingOutsideMistThreat = false;
    bool Gapcloser = false;
    bool Lethal = false;
    bool RecastExpiring = false;
    bool WouldEngage = false;
};

inline bool AutomaticAllowed(const AutomaticPolicy& policy) {
    if (policy.WouldEngage && !policy.Lethal) return false;
    return policy.IncomingOutsideMistThreat || policy.Gapcloser ||
           policy.Lethal || policy.RecastExpiring;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Gwen::Geometry
