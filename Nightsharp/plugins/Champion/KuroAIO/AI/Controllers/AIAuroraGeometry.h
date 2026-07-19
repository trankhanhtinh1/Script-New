#pragma once

// Deterministic Aurora mechanics and one-trick decisions. The runtime
// controller contributes prediction, terrain, buffs and threat telemetry;
// this file keeps the live passive arithmetic, Q return-bolt alignment,
// W reset commitment, E recoil safety and R portal geometry independently
// testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Aurora::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;
using SharedGeometry::kPi;

inline constexpr float kPassiveMarkSeconds = 4.0f;
inline constexpr float kSpiritSeconds = 4.0f;
inline constexpr int kMaximumSpirits = 4;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQOutboundSpeed = 1600.0f;
inline constexpr float kQReturnSpeed = 2000.0f;
inline constexpr float kQMissileHalfWidth = 45.0f;
inline constexpr float kQMarkSeconds = 3.5f;
inline constexpr float kQMaximumMissingHealthMultiplier = 1.5f;
inline constexpr float kQAdditionalBoltModifier = 0.20f;
inline constexpr float kQ2MinionModifier = 0.40f;

inline constexpr float kWBaseDash = 300.0f;
inline constexpr float kWWallExtension = 450.0f;
inline constexpr float kWResetDamageWindowSeconds = 3.0f;
inline constexpr float kWRealmHopperSeconds = 4.0f;

inline constexpr float kERange = 825.0f;
inline constexpr float kECastSeconds = 0.35f;
inline constexpr float kEHalfWidth = 87.5f;
inline constexpr float kERecoilDistance = 250.0f;
inline constexpr float kESlowSeconds = 1.0f;

inline constexpr float kRCastRange = 700.0f;
inline constexpr float kRLeapMaximum = 250.0f;
inline constexpr float kRWallForgiveness = 450.0f;
inline constexpr float kRArenaRadius = 700.0f;
// The damaging pulse/ring is offset ahead of Aurora. This is intentionally a
// named live geometry constant rather than treating R as a generic 700-radius
// circle centered on the champion.
inline constexpr float kRArenaCenterOffset = 425.0f;
inline constexpr float kRInitialSlowSeconds = 2.0f;
inline constexpr float kRBoundarySlowPercent = 0.50f;
inline constexpr float kRPortalEdgeTolerance = 62.0f;

inline float PassiveMaximumHealthPercent(float abilityPower) {
    return 0.01f + std::max(0.0f, abilityPower) * 0.00027f;
}

inline float PassiveRawDamage(float targetMaximumHealth,
                              float abilityPower,
                              bool monster = false,
                              int championLevel = 1) {
    const float raw = std::max(0.0f, targetMaximumHealth) *
        PassiveMaximumHealthPercent(abilityPower);
    if (!monster) return raw;
    const float cap = 10.0f + 80.0f * static_cast<float>(
        std::clamp(championLevel, 1, 18) - 1) / 17.0f;
    return std::min(raw, cap);
}

inline float SpiritHealPerSecond(int championLevel, float abilityPower) {
    const float base = 3.0f + 17.0f * static_cast<float>(
        std::clamp(championLevel, 1, 18) - 1) / 17.0f;
    return base + std::max(0.0f, abilityPower) * 0.02f;
}

inline float TotalSpiritHealPerSecond(int spirits,
                                      int championLevel,
                                      float abilityPower) {
    return static_cast<float>(std::clamp(spirits, 0, kMaximumSpirits)) *
        SpiritHealPerSecond(championLevel, abilityPower);
}

// Some SDK bridges expose three passive applications as 1/2/3, while older
// bridges expose 2/4/6. The controller chooses the observed encoding; the
// model never guesses from a single sample.
inline int NormalizePassiveStacks(int observedCount,
                                  bool doubledTelemetry) {
    const int applications = doubledTelemetry
        ? std::max(0, observedCount) / 2
        : std::max(0, observedCount);
    return applications % 3;
}

struct PassiveState {
    int Stacks = 0;
    int Procs = 0;
    float RemainingSeconds = 0.0f;
};

inline PassiveState AdvancePassive(PassiveState state,
                                   int applications,
                                   float elapsedSeconds) {
    const float elapsed = std::max(0.0f, elapsedSeconds);
    state.RemainingSeconds = std::max(
        0.0f, state.RemainingSeconds - elapsed);
    if (state.RemainingSeconds <= 0.0f) state.Stacks = 0;
    for (int hit = 0; hit < std::max(0, applications); ++hit) {
        ++state.Stacks;
        state.RemainingSeconds = kPassiveMarkSeconds;
        if (state.Stacks >= 3) {
            state.Stacks = 0;
            ++state.Procs;
        }
    }
    return state;
}

inline float QBaseDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 45.0f, 70.0f, 95.0f, 120.0f, 145.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.40f;
}

inline float Q2MissingHealthMultiplier(float missingHealthFraction) {
    return 1.0f + 0.50f * std::clamp(missingHealthFraction, 0.0f, 1.0f);
}

inline float Q2BoltRawDamage(int rank,
                            float abilityPower,
                            float missingHealthFraction,
                            bool additionalBolt = false,
                            bool minion = false) {
    float damage = QBaseDamage(rank, abilityPower) *
        Q2MissingHealthMultiplier(missingHealthFraction);
    if (additionalBolt) damage *= kQAdditionalBoltModifier;
    if (minion) damage *= kQ2MinionModifier;
    return damage;
}

struct LineUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Champion = false;
    bool Minion = false;
    bool Valid = false;
};

inline bool CapsuleHits(const Vec3& start,
                        const Vec3& end,
                        const Vec3& position,
                        float unitRadius,
                        float missileHalfWidth) {
    if (start.Distance2D(end) <= 0.001f) return false;
    const auto projection = ProjectPointToSegment2D(position, start, end);
    return projection.Distance <= std::max(0.0f, unitRadius) +
        std::max(0.0f, missileHalfWidth);
}

inline bool QLineHits(const Vec3& origin,
                      const Vec3& aim,
                      const LineUnit& unit) {
    return unit.Valid && CapsuleHits(
        origin, aim, unit.Position, unit.Radius, kQMissileHalfWidth);
}

inline std::vector<int> QLineHitIds(const Vec3& origin,
                                    const Vec3& aim,
                                    const std::vector<LineUnit>& units) {
    std::vector<int> result;
    for (const auto& unit : units) {
        if (QLineHits(origin, aim, unit)) result.push_back(unit.Id);
    }
    return result;
}

struct QMark {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    float RemainingSeconds = 0.0f;
    bool Champion = false;
    bool Minion = false;
    bool Valid = false;
};

struct ReturnBoltHit {
    int SourceId = 0;
    float ArrivalDistance = FLT_MAX;
    bool GuaranteedOwnBolt = false;
};

struct QReturnEvaluation {
    int Bolts = 0;
    int CrossingBolts = 0;
    float DamageUnits = 0.0f;
    float Score = -FLT_MAX;
    bool OwnBolt = false;
};

inline float ReturnBoltArrivalDistance(const QMark& mark,
                                       const LineUnit& target,
                                       const Vec3& returnPosition) {
    if (!mark.Valid || !target.Valid) return FLT_MAX;
    if (mark.Id == target.Id) {
        return mark.Position.Distance2D(returnPosition);
    }
    if (!CapsuleHits(mark.Position, returnPosition,
                     target.Position, target.Radius,
                     kQMissileHalfWidth)) return FLT_MAX;
    const auto projection = ProjectPointToSegment2D(
        target.Position, mark.Position, returnPosition);
    return projection.T * mark.Position.Distance2D(returnPosition);
}

inline QReturnEvaluation EvaluateQReturn(
    const LineUnit& target,
    const std::vector<QMark>& marks,
    const Vec3& returnPosition) {
    QReturnEvaluation result{};
    if (!target.Valid || !returnPosition.IsValid()) return result;

    std::vector<ReturnBoltHit> hits;
    hits.reserve(marks.size());
    for (const auto& mark : marks) {
        if (!mark.Valid || mark.RemainingSeconds <= 0.0f) continue;
        const float arrival = ReturnBoltArrivalDistance(
            mark, target, returnPosition);
        if (!std::isfinite(arrival) || arrival == FLT_MAX) continue;
        hits.push_back({ mark.Id, arrival, mark.Id == target.Id });
    }
    std::stable_sort(hits.begin(), hits.end(),
        [](const ReturnBoltHit& left, const ReturnBoltHit& right) {
            return left.ArrivalDistance < right.ArrivalDistance;
        });
    result.Bolts = static_cast<int>(hits.size());
    result.DamageUnits = hits.empty() ? 0.0f : 1.0f;
    for (std::size_t index = 0; index < hits.size(); ++index) {
        if (index > 0) result.DamageUnits += kQAdditionalBoltModifier;
        if (!hits[index].GuaranteedOwnBolt) ++result.CrossingBolts;
        result.OwnBolt = result.OwnBolt || hits[index].GuaranteedOwnBolt;
    }
    result.Score = result.Bolts > 0
        ? result.DamageUnits * 100.0f +
          static_cast<float>(result.CrossingBolts) * 24.0f +
          (result.OwnBolt ? 30.0f : 0.0f)
        : -FLT_MAX;
    return result;
}

struct QRecastContext {
    bool MarkActive = false;
    bool ControllerOwned = false;
    bool AutoAttackWindup = false;
    bool TargetValid = false;
    bool TargetEscaping = false;
    bool LethalNow = false;
    bool PassiveProcNow = false;
    bool EReady = false;
    bool EWouldHit = false;
    bool EWouldKillMarkedWave = false;
    bool WaveSequence = false;
    int CurrentBolts = 0;
    int BetterPositionBolts = 0;
    float RemainingSeconds = 0.0f;
    float ExpectedPreRecastDamage = 0.0f;
};

inline bool ShouldRecastQ(const QRecastContext& context) {
    if (!context.MarkActive || !context.ControllerOwned) return false;
    const bool urgent = context.RemainingSeconds <= 0.16f ||
                        context.LethalNow || context.TargetEscaping;
    if (context.AutoAttackWindup && !urgent) return false;
    if (context.WaveSequence && context.EReady &&
        context.EWouldKillMarkedWave) return true;
    if (context.LethalNow || context.PassiveProcNow) return true;
    if (context.TargetEscaping || context.RemainingSeconds <= 0.16f) {
        return context.TargetValid || context.CurrentBolts > 0;
    }
    if (context.BetterPositionBolts > context.CurrentBolts &&
        context.RemainingSeconds > 0.35f) return false;
    if (context.EReady && context.EWouldHit &&
        !context.WaveSequence && context.RemainingSeconds > 0.48f) {
        return false;
    }
    if (context.ExpectedPreRecastDamage > 0.0f &&
        context.RemainingSeconds > 0.34f) return false;
    return context.RemainingSeconds <= 0.30f;
}

inline bool PreferQ2BeforeE(bool markedWave,
                            int markedMinions,
                            bool eWouldKillMarkedMinions,
                            bool singleChampionOnly) {
    return !singleChampionOnly && markedWave && markedMinions > 0 &&
           eWouldKillMarkedMinions;
}

inline float WInvisibilitySeconds(int rank) {
    static constexpr std::array<float, 6> values = {
        0.0f, 1.0f, 1.15f, 1.30f, 1.45f, 1.60f,
    };
    return RankValue(values, rank);
}

inline float WMovementSpeedPercent(int rank) {
    static constexpr std::array<float, 6> values = {
        0.0f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f,
    };
    return RankValue(values, rank);
}

inline Vec3 WBaseEndpoint(const Vec3& origin, const Vec3& castPosition) {
    const Vec3 direction = Direction2D(origin, castPosition);
    return direction.IsZero() ? Vec3{} : origin + direction * kWBaseDash;
}

inline Vec3 WResolvedEndpoint(const Vec3& origin,
                              const Vec3& castPosition,
                              const Vec3& terrainExit,
                              bool crossesWall) {
    if (crossesWall && terrainExit.IsValid() && !terrainExit.IsZero() &&
        origin.Distance2D(terrainExit) <= kWWallExtension + 1.0f) {
        return terrainExit;
    }
    return WBaseEndpoint(origin, castPosition);
}

struct WRouteContext {
    bool EndpointValid = false;
    bool CursorAgrees = false;
    bool TerrainReachable = true;
    bool EnemyTurret = false;
    bool DashHazard = false;
    bool PointClickLockdown = false;
    bool EscapesThreat = false;
    bool CreatesSpellAngle = false;
    bool ConcealsTurn = false;
    bool TakedownLikely = false;
    bool DamagedChampionRecently = false;
    bool Defensive = false;
    bool PlayerWindingUp = false;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
    float DistanceFromThreat = 0.0f;
};

inline float WRouteScore(const WRouteContext& context) {
    if (!context.EndpointValid || !context.TerrainReachable) return -FLT_MAX;
    float score = context.Defensive ? 180.0f : 0.0f;
    score += context.CursorAgrees ? 85.0f : -190.0f;
    score += context.EscapesThreat ? 310.0f : 0.0f;
    score += context.CreatesSpellAngle ? 170.0f : 0.0f;
    score += context.ConcealsTurn ? 55.0f : 0.0f;
    score += context.AlliesAtEndpoint * 75.0f;
    score -= context.EnemiesAtEndpoint * 150.0f;
    score += std::clamp(context.DistanceFromThreat, 0.0f, 700.0f) * 0.20f;
    if (context.EnemyTurret) score -= 800.0f;
    if (context.DashHazard) score -= 720.0f;
    if (context.PointClickLockdown) score -= 620.0f;
    if (context.PlayerWindingUp && !context.Defensive) score -= 260.0f;
    if (context.TakedownLikely && context.DamagedChampionRecently) {
        score += 230.0f;
    }
    return score;
}

inline bool ShouldSpendW(const WRouteContext& context,
                         float minimumScore = 130.0f) {
    if (context.PlayerWindingUp && !context.Defensive) return false;
    if (context.EnemyTurret || context.DashHazard ||
        context.PointClickLockdown) return false;
    return WRouteScore(context) >= minimumScore &&
        (context.Defensive || context.CreatesSpellAngle ||
         (context.TakedownLikely && context.DamagedChampionRecently));
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f,
    };
    return RankValue(base, rank) + std::max(0.0f, abilityPower) * 0.70f;
}

inline Vec3 ERecoilEndpoint(const Vec3& origin,
                            const Vec3& castPosition,
                            bool groundedOrRooted) {
    if (groundedOrRooted) return origin;
    const Vec3 direction = Direction2D(origin, castPosition);
    return direction.IsZero()
        ? Vec3{}
        : origin - direction * kERecoilDistance;
}

inline bool ELineHits(const Vec3& origin,
                      const Vec3& castPosition,
                      const LineUnit& unit) {
    if (!unit.Valid) return false;
    const Vec3 direction = Direction2D(origin, castPosition);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * kERange;
    return CapsuleHits(origin, end, unit.Position,
                       unit.Radius, kEHalfWidth);
}

struct ECastContext {
    bool TargetHit = false;
    bool RecoilEndpointValid = false;
    bool GroundedOrRooted = false;
    bool EndpointTurret = false;
    bool EndpointTerrainBlocked = false;
    bool EndpointLockdown = false;
    bool IncomingDisplacement = false;
    bool CanBufferDisplacement = false;
    bool AllIn = false;
    bool FinalMobilityResource = false;
    bool TargetCanBeChasedAfter = true;
    bool StandalonePoke = false;
    bool PlayerWindingUp = false;
};

inline bool ShouldCastE(const ECastContext& context) {
    if (!context.TargetHit || context.PlayerWindingUp) return false;
    if (context.IncomingDisplacement && context.CanBufferDisplacement) {
        return !context.EndpointTurret && !context.EndpointLockdown;
    }
    if (!context.GroundedOrRooted &&
        (!context.RecoilEndpointValid || context.EndpointTurret ||
         context.EndpointTerrainBlocked || context.EndpointLockdown)) {
        return false;
    }
    if (context.AllIn && context.FinalMobilityResource &&
        !context.TargetCanBeChasedAfter) return false;
    return context.AllIn || context.StandalonePoke ||
           context.GroundedOrRooted;
}

inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 175.0f, 275.0f, 375.0f,
    };
    return RankValue(base, rank) +
           std::max(0.0f, abilityPower) * 0.70f;
}

inline float RArenaDuration(int rank) {
    static constexpr std::array<float, 4> values = {
        0.0f, 2.50f, 3.25f, 4.00f,
    };
    return RankValue(values, rank);
}

inline float RBoundarySlowSeconds(int rank) {
    static constexpr std::array<float, 4> values = {
        0.0f, 1.50f, 1.75f, 2.00f,
    };
    return RankValue(values, rank);
}

struct RPlacement {
    Vec3 Direction = {};
    Vec3 LeapEndpoint = {};
    Vec3 ArenaCenter = {};
    bool Valid = false;
};

inline RPlacement ResolveRPlacement(const Vec3& origin,
                                    const Vec3& castPosition) {
    RPlacement result{};
    result.Direction = Direction2D(origin, castPosition);
    if (result.Direction.IsZero()) return result;
    const float castDistance = std::min(
        origin.Distance2D(castPosition), kRCastRange);
    const float leap = std::min(castDistance, kRLeapMaximum);
    result.LeapEndpoint = origin + result.Direction * leap;
    result.ArenaCenter = origin + result.Direction *
        std::min(kRArenaCenterOffset, std::max(25.0f, castDistance));
    result.Valid = true;
    return result;
}

inline Vec3 PortalDestination(const Vec3& arenaCenter,
                              const Vec3& boundaryContact) {
    const Vec3 radial = Direction2D(arenaCenter, boundaryContact);
    return radial.IsZero()
        ? Vec3{}
        : arenaCenter - radial * kRArenaRadius;
}

inline Vec3 BoundaryContactForRay(const Vec3& arenaCenter,
                                  const Vec3& from,
                                  const Vec3& toward) {
    const Vec3 direction = Direction2D(from, toward);
    if (direction.IsZero()) return {};
    Vec3 offset = from - arenaCenter;
    offset.y = 0.0f;
    const float b = 2.0f * offset.Dot(direction);
    const float c = offset.Dot(offset) - kRArenaRadius * kRArenaRadius;
    const float discriminant = b * b - 4.0f * c;
    if (discriminant < 0.0f) return {};
    const float root = std::sqrt(discriminant);
    const float first = (-b - root) * 0.5f;
    const float second = (-b + root) * 0.5f;
    const float distance = first >= 0.0f ? first : second;
    return distance >= 0.0f ? from + direction * distance : Vec3{};
}

inline bool NearPortalBoundary(const Vec3& position,
                               const Vec3& arenaCenter,
                               float tolerance = kRPortalEdgeTolerance) {
    const float radialDistance = position.Distance2D(arenaCenter);
    return std::fabs(radialDistance - kRArenaRadius) <=
        std::max(0.0f, tolerance);
}

struct PortalContext {
    bool ArenaActive = false;
    bool PortalReady = false;
    bool NearBoundary = false;
    bool DestinationSafe = false;
    bool IncomingTargetedDamage = false;
    bool IncomingOneInstanceCrowdControl = false;
    bool IncomingSuppressionOrLongCrowdControl = false;
    bool TurretShotPending = false;
    bool ChasingPriorityTarget = false;
    bool WReady = false;
    bool EReady = false;
    bool CombiningMobilityAddsValue = false;
    float RemainingSeconds = 0.0f;
};

inline bool ShouldUsePortal(const PortalContext& context) {
    if (!context.ArenaActive || !context.PortalReady ||
        !context.NearBoundary || !context.DestinationSafe ||
        context.RemainingSeconds <= 0.10f) return false;
    if (context.IncomingSuppressionOrLongCrowdControl) return false;
    if (context.IncomingTargetedDamage ||
        context.IncomingOneInstanceCrowdControl ||
        context.TurretShotPending) return true;
    if (context.ChasingPriorityTarget) return true;
    // W and E are normally preserved as independent mobility resources. A
    // combination is intentional only when the interaction itself adds value.
    return context.CombiningMobilityAddsValue &&
           (context.WReady || context.EReady);
}

struct REarlyEndContext {
    bool ArenaActive = false;
    bool RecastReady = false;
    bool ForcedUnsafePortal = false;
    bool DestinationUnsafe = false;
    bool PlayerWantsExit = false;
    bool TargetEscaped = false;
    bool IncomingThreatCanBePortaled = false;
    float RemainingSeconds = 0.0f;
};

inline bool ShouldEndR(const REarlyEndContext& context) {
    if (!context.ArenaActive || !context.RecastReady) return false;
    if (context.ForcedUnsafePortal ||
        (context.DestinationUnsafe && context.PlayerWantsExit)) return true;
    if (context.TargetEscaped && !context.IncomingThreatCanBePortaled &&
        context.RemainingSeconds > 0.35f) return true;
    return false;
}

struct RUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool Primary = false;
    bool HardCrowdControlled = false;
    bool BlinkReady = false;
    bool SpellShield = false;
    bool Valid = false;
};

struct REvaluation {
    int Hits = 0;
    int PriorityHits = 0;
    float Priority = 0.0f;
    float Score = -FLT_MAX;
    bool PrimaryHit = false;
};

inline REvaluation EvaluateR(const Vec3& center,
                             const std::vector<RUnit>& units,
                             int alliedFollowup,
                             int expectedPassiveProcs,
                             bool terrainFeasible) {
    REvaluation result{};
    if (!terrainFeasible || !center.IsValid() || center.IsZero()) {
        return result;
    }
    result.Score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid || unit.Position.Distance2D(center) >
            kRArenaRadius + std::max(0.0f, unit.Radius)) continue;
        ++result.Hits;
        result.Priority += std::max(0.0f, unit.Priority);
        result.PrimaryHit = result.PrimaryHit || unit.Primary;
        if (unit.Priority >= 1.5f) ++result.PriorityHits;
        result.Score += 115.0f * std::max(0.2f, unit.Priority);
        if (unit.HardCrowdControlled) result.Score += 75.0f;
        if (unit.BlinkReady && !unit.HardCrowdControlled) result.Score -= 95.0f;
        if (unit.SpellShield) result.Score -= 80.0f;
    }
    result.Score += std::max(0, alliedFollowup) * 90.0f;
    result.Score += std::max(0, expectedPassiveProcs) * 125.0f;
    if (result.PrimaryHit) result.Score += 80.0f;
    if (result.Hits == 0) result.Score = -FLT_MAX;
    return result;
}

struct RCastContext {
    bool TerrainFeasible = false;
    bool LeapEndpointSafe = false;
    bool PlayerWindingUp = false;
    bool FollowupReady = false;
    bool DefensiveBuffer = false;
    bool IncomingOneInstanceCrowdControl = false;
    bool IncomingSuppressionOrLongCrowdControl = false;
    int MinimumHits = 2;
    REvaluation Evaluation = {};
};

inline bool ShouldCastR(const RCastContext& context) {
    if (!context.TerrainFeasible || !context.LeapEndpointSafe ||
        context.PlayerWindingUp ||
        context.IncomingSuppressionOrLongCrowdControl) return false;
    if (context.DefensiveBuffer &&
        context.IncomingOneInstanceCrowdControl) return true;
    return context.FollowupReady && context.Evaluation.PrimaryHit &&
        context.Evaluation.Hits >= std::max(1, context.MinimumHits);
}

enum class ComboAction : std::uint8_t {
    Wait,
    Q1,
    AutoAttackWindow,
    Q2,
    E,
    W,
    R,
};

struct ComboContext {
    bool QMarkActive = false;
    bool QReady = false;
    bool EReady = false;
    bool WReady = false;
    bool RReady = false;
    bool AutoAvailable = false;
    bool AutoSafe = false;
    bool WaveHasMarkedMinions = false;
    bool EWouldKillMarkedMinions = false;
    bool AllIn = false;
    bool SafeWAngle = false;
    bool RWindow = false;
    int PassiveStacks = 0;
    int PassiveProcsThisSequence = 0;
    float QRemainingSeconds = 0.0f;
};

inline ComboAction NextComboAction(const ComboContext& context) {
    if (!context.QMarkActive) {
        if (context.AllIn && context.RReady && context.RWindow) {
            return ComboAction::R;
        }
        if (context.QReady) return ComboAction::Q1;
        if (context.AllIn && context.WReady && context.SafeWAngle) {
            return ComboAction::W;
        }
        return ComboAction::Wait;
    }
    if (PreferQ2BeforeE(
            context.WaveHasMarkedMinions, 1,
            context.EWouldKillMarkedMinions, false)) {
        return ComboAction::Q2;
    }
    if (context.AutoAvailable && context.AutoSafe &&
        context.QRemainingSeconds > 0.45f &&
        (context.PassiveStacks < 2 ||
         context.PassiveProcsThisSequence < 2)) {
        return ComboAction::AutoAttackWindow;
    }
    if (context.EReady && context.PassiveStacks == 2 &&
        context.QRemainingSeconds > 0.38f) {
        return ComboAction::E;
    }
    if (context.QRemainingSeconds <= 0.30f) return ComboAction::Q2;
    if (context.EReady && context.PassiveProcsThisSequence == 0) {
        return ComboAction::E;
    }
    return ComboAction::Q2;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Aurora::Geometry
