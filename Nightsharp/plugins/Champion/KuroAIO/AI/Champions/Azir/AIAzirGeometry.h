#pragma once

// Deterministic Azir mechanics and one-trick decisions. Runtime object
// discovery, prediction and spell casts live in AIAzirController; this file
// owns the soldier formation, late-Q, drift, collision, R wall and Sun Disc
// policies so the difficult parts remain independently testable.

#include "../../AIGeometry.h"
#include "../../../../../Core/OrbwalkerKuro/AzirSoldierRules.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Azir::Geometry {

using SharedGeometry::Cross2D;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;
using SharedGeometry::kPi;
namespace SoldierRules = ::OrbwalkerKuro::AzirSoldierRules;

inline constexpr float kQCastRange = 720.0f;
inline constexpr float kQDisplayRange = 740.0f;
inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQMoveSpeed = 1600.0f;
inline constexpr float kQPathHalfWidth = 35.0f;
inline constexpr float kQSlowSeconds = 1.0f;
inline constexpr float kQSlowPercent = 0.25f;
inline constexpr float kQFormationSpacing = 105.0f;
inline constexpr float kQArrivalOvershoot = 50.0f;

inline constexpr float kWSpawnRange = 500.0f;
inline constexpr float kWCastSeconds = 0.25f;
inline constexpr float kWSoldierLifetimeSeconds = 10.0f;
inline constexpr float kWTurretLifetimeMultiplier = 0.50f;
inline constexpr float kWMinimumCastSeparation = 50.0f;

inline constexpr float kESelectionRange = 1100.0f;
inline constexpr float kEDashSpeed = 1700.0f;
inline constexpr float kEDashHalfWidth = 70.0f;
inline constexpr float kEShieldSeconds = 1.5f;
inline constexpr float kEChampionCollisionPadding = 35.0f;
inline constexpr float kEQMinimumRedirectDistance = 115.0f;
inline constexpr float kEQBufferLeadSeconds = 0.10f;

inline constexpr float kRCastDistance = 250.0f;
inline constexpr float kRCastSeconds = 0.50f;
inline constexpr float kRBehindOrigin = 300.0f;
inline constexpr float kRForwardReach = 600.0f;
inline constexpr float kRPushEndpoint = 650.0f;
inline constexpr float kRWallDurationSeconds = 5.0f;
inline constexpr float kRWallDepth = 190.0f;

inline constexpr float kPassiveCastRange = 700.0f;
inline constexpr float kPassiveChannelRange = 850.0f;
inline constexpr float kPassiveCooldownSeconds = 90.0f;
inline constexpr float kPassiveTowerLifetimeSeconds = 45.0f;

inline Vec3 ClampCast(const Vec3& origin,
                      const Vec3& desired,
                      float range) {
    if (!origin.IsValid() || !desired.IsValid() || desired.IsZero()) return {};
    const float distance = origin.Distance2D(desired);
    if (distance <= std::max(0.0f, range)) return desired;
    const Vec3 direction = Direction2D(origin, desired);
    return direction.IsZero() ? origin : origin + direction * range;
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 80.0f, 100.0f, 120.0f, 140.0f,
    };
    static constexpr std::array<float, 6> ratio = {
        0.0f, 0.35f, 0.40f, 0.45f, 0.50f, 0.55f,
    };
    return RankValue(base, rank) +
           RankValue(ratio, rank) * std::max(0.0f, abilityPower);
}

inline float WRawDamage(int championLevel,
                        int rank,
                        float abilityPower,
                        int attackingSoldiers = 1) {
    return SoldierRules::MultiSoldierRawDamage(
        championLevel, std::clamp(rank, 1, 5), abilityPower,
        std::max(0, attackingSoldiers));
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f,
    };
    return RankValue(base, rank) + 0.60f * std::max(0.0f, abilityPower);
}

inline float EShield(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 70.0f, 110.0f, 150.0f, 190.0f, 230.0f,
    };
    return RankValue(base, rank) + 0.60f * std::max(0.0f, abilityPower);
}

inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 200.0f, 400.0f, 600.0f,
    };
    return RankValue(base, rank) + 0.75f * std::max(0.0f, abilityPower);
}

inline int RSoldierCount(int rank) {
    static constexpr std::array<int, 4> values = { 0, 6, 7, 8 };
    return values[static_cast<std::size_t>(std::clamp(rank, 0, 3))];
}

inline float RWallLength(int rank) {
    static constexpr std::array<float, 4> values = {
        0.0f, 700.0f, 810.0f, 930.0f,
    };
    return RankValue(values, rank);
}

inline float RWallHalfLength(int rank) {
    return RWallLength(rank) * 0.5f;
}

inline float SunDiscRawDamage(int championLevel, float abilityPower) {
    const int level = std::clamp(championLevel, 1, 18);
    const float levelDamage = level >= 7
        ? static_cast<float>(level - 6) * 15.0f
        : 0.0f;
    return 230.0f + levelDamage +
           0.40f * std::max(0.0f, abilityPower);
}

inline float SunDiscBonusResists(int championLevel) {
    const int level = std::clamp(championLevel, 1, 18);
    return 30.0f + (level >= 7
        ? static_cast<float>(level - 6) * 5.0f
        : 0.0f);
}

struct Soldier {
    Vec3 Position = {};
    int Id = 0;
    float Radius = 1.0f;
    float RemainingSeconds = 0.0f;
    bool ConfirmedOwned = false;
    bool Valid = false;
};

struct Unit {
    Vec3 Position = {};
    Vec3 PredictedPosition = {};
    float Radius = 0.0f;
    int Id = 0;
    float Health = 0.0f;
    float Priority = 1.0f;
    bool Champion = false;
    bool Minion = false;
    bool Jungle = false;
    bool Structure = false;
    bool WardOrTrap = false;
    bool HardCrowdControlled = false;
    bool Valid = false;
};

inline SoldierRules::Point2 Point2(const Vec3& value) {
    return { value.x, value.z };
}

inline bool Commandable(const Vec3& azir, const Soldier& soldier) {
    return soldier.Valid && soldier.RemainingSeconds > 0.0f &&
           SoldierRules::IsCommandable(
               Point2(azir), Point2(soldier.Position));
}

inline bool SoldierCanAttack(const Vec3& azir,
                             const Soldier& soldier,
                             const Unit& target) {
    return target.Valid && !target.Structure && !target.WardOrTrap &&
           Commandable(azir, soldier) &&
           SoldierRules::CanReachPrimaryTarget(
               Point2(soldier.Position), Point2(target.Position),
               target.Radius);
}

inline int SoldierAttackCount(const Vec3& azir,
                              const std::vector<Soldier>& soldiers,
                              const Unit& target) {
    int count = 0;
    for (const auto& soldier : soldiers) {
        if (SoldierCanAttack(azir, soldier, target)) ++count;
    }
    return count;
}

inline float SoldierCoverageDistance(const Soldier& soldier,
                                     const Unit& target) {
    if (!soldier.Valid || !target.Valid) return FLT_MAX;
    return soldier.Position.Distance2D(target.Position) - target.Radius;
}

inline const Soldier* NearestCommandableSoldier(
    const Vec3& azir,
    const std::vector<Soldier>& soldiers,
    const Vec3& desired) {
    const Soldier* best = nullptr;
    float bestDistance = FLT_MAX;
    for (const auto& soldier : soldiers) {
        if (!Commandable(azir, soldier)) continue;
        const float distance = soldier.Position.Distance2D(desired);
        if (distance < bestDistance) {
            best = &soldier;
            bestDistance = distance;
        }
    }
    return best;
}

inline bool CapsuleHits(const Vec3& start,
                        const Vec3& end,
                        const Vec3& position,
                        float bodyRadius,
                        float pathHalfWidth) {
    if (!start.IsValid() || !end.IsValid() || start.Distance2D(end) < 0.01f) {
        return false;
    }
    const auto projection = ProjectPointToSegment2D(position, start, end);
    return projection.Distance <= std::max(0.0f, bodyRadius) +
                                  std::max(0.0f, pathHalfWidth);
}

inline std::vector<Vec3> QFormation(const Vec3& azir,
                                    const Vec3& desired,
                                    int soldierCount) {
    std::vector<Vec3> result;
    const int count = std::clamp(soldierCount, 0, 8);
    if (count <= 0) return result;
    const Vec3 clamped = ClampCast(azir, desired, kQCastRange);
    const Vec3 forward = Direction2D(azir, clamped);
    if (forward.IsZero()) return result;
    const Vec3 lateral = Rotate2D(forward, kPi * 0.5f);
    const Vec3 center = clamped + forward * kQArrivalOvershoot;
    result.reserve(static_cast<std::size_t>(count));
    for (int index = 0; index < count; ++index) {
        const float centered = static_cast<float>(index) -
            static_cast<float>(count - 1) * 0.5f;
        result.push_back(center + lateral *
            (centered * kQFormationSpacing));
    }
    return result;
}

inline std::vector<Soldier> SortSoldiersForFormation(
    const Vec3& azir,
    const Vec3& desired,
    std::vector<Soldier> soldiers) {
    const Vec3 forward = Direction2D(azir, desired);
    const Vec3 lateral = Rotate2D(forward, kPi * 0.5f);
    std::stable_sort(soldiers.begin(), soldiers.end(),
        [&](const Soldier& left, const Soldier& right) {
            const float leftOffset = (left.Position - azir).Dot(lateral);
            const float rightOffset = (right.Position - azir).Dot(lateral);
            return leftOffset < rightOffset;
        });
    return soldiers;
}

struct QEvaluation {
    Vec3 CastPosition = {};
    std::vector<Vec3> Endpoints = {};
    std::vector<int> HitIds = {};
    int ChampionHits = 0;
    int FarmHits = 0;
    int CurrentPrimaryAttackers = 0;
    int FuturePrimaryAttackers = 0;
    float Score = -FLT_MAX;
    bool PrimaryHit = false;
    bool PreservesPrimaryCoverage = false;
    bool ExtendsAlongRetreat = false;
    bool Valid = false;
};

inline QEvaluation EvaluateQ(const Vec3& azir,
                             const Vec3& desired,
                             const std::vector<Soldier>& soldiers,
                             const std::vector<Unit>& units,
                             int primaryId,
                             const Vec3& primaryRetreatDirection = {}) {
    QEvaluation result{};
    std::vector<Soldier> active;
    for (const auto& soldier : soldiers) {
        if (Commandable(azir, soldier)) active.push_back(soldier);
    }
    if (active.empty()) return result;
    result.CastPosition = ClampCast(azir, desired, kQCastRange);
    active = SortSoldiersForFormation(azir, result.CastPosition, active);
    result.Endpoints = QFormation(
        azir, result.CastPosition, static_cast<int>(active.size()));
    if (result.Endpoints.size() != active.size()) return result;

    const Unit* primary = nullptr;
    for (const auto& unit : units) {
        if (unit.Id == primaryId && unit.Valid) {
            primary = &unit;
            break;
        }
    }
    if (primary) {
        result.CurrentPrimaryAttackers = SoldierAttackCount(
            azir, active, *primary);
    }

    for (const auto& unit : units) {
        if (!unit.Valid) continue;
        const Vec3 body = unit.PredictedPosition.IsValid() &&
                !unit.PredictedPosition.IsZero()
            ? unit.PredictedPosition : unit.Position;
        bool hit = false;
        for (std::size_t index = 0; index < active.size(); ++index) {
            if (CapsuleHits(active[index].Position, result.Endpoints[index],
                            body, unit.Radius, kQPathHalfWidth)) {
                hit = true;
                break;
            }
        }
        if (hit) {
            result.HitIds.push_back(unit.Id);
            if (unit.Champion) ++result.ChampionHits;
            if (unit.Minion || unit.Jungle) ++result.FarmHits;
            if (unit.Id == primaryId) result.PrimaryHit = true;
        }
    }

    if (primary) {
        Unit future = *primary;
        if (future.PredictedPosition.IsValid() &&
            !future.PredictedPosition.IsZero()) {
            future.Position = future.PredictedPosition;
        }
        std::vector<Soldier> moved = active;
        for (std::size_t index = 0; index < moved.size(); ++index) {
            moved[index].Position = result.Endpoints[index];
        }
        result.FuturePrimaryAttackers = SoldierAttackCount(
            azir, moved, future);
        result.PreservesPrimaryCoverage =
            result.FuturePrimaryAttackers > 0;
        const Vec3 castDirection = Direction2D(
            primary->Position, result.CastPosition);
        result.ExtendsAlongRetreat = primaryRetreatDirection.IsZero() ||
            castDirection.IsZero() ||
            castDirection.Dot(primaryRetreatDirection) >= 0.25f;
    }

    result.Score = static_cast<float>(result.ChampionHits) * 420.0f +
        static_cast<float>(result.FarmHits) * 48.0f +
        static_cast<float>(result.FuturePrimaryAttackers) * 330.0f +
        (result.PrimaryHit ? 260.0f : 0.0f) +
        (result.ExtendsAlongRetreat ? 115.0f : -180.0f) -
        static_cast<float>(std::max(
            0, result.CurrentPrimaryAttackers -
               result.FuturePrimaryAttackers)) * 370.0f;
    result.Valid = true;
    return result;
}

struct LateQContext {
    bool QReady = false;
    bool TargetValid = false;
    bool TargetLeavingCoverage = false;
    bool CurrentCoverage = false;
    bool FutureCoverage = false;
    bool QHitsTarget = false;
    bool QLethal = false;
    bool TargetHardCrowdControlled = false;
    bool PlayerAttackWindingUp = false;
    bool AttackJustCompleted = false;
    bool EnoughMana = false;
    bool DirectionAgrees = false;
    bool EscapeAnchorWouldBeLost = false;
    int CurrentSoldierAttackers = 0;
    int FutureSoldierAttackers = 0;
};

inline bool ShouldCastLateQ(const LateQContext& context) {
    if (!context.QReady || !context.TargetValid || !context.EnoughMana ||
        !context.DirectionAgrees || context.EscapeAnchorWouldBeLost) {
        return false;
    }
    if (context.PlayerAttackWindingUp && !context.QLethal) return false;
    if (!context.QHitsTarget && !context.FutureCoverage) return false;
    if (context.QLethal) return true;
    if (context.CurrentCoverage && !context.TargetLeavingCoverage &&
        !context.AttackJustCompleted) return false;
    if (context.CurrentSoldierAttackers >= 2 &&
        context.FutureSoldierAttackers < context.CurrentSoldierAttackers &&
        !context.TargetLeavingCoverage) return false;
    return context.TargetLeavingCoverage || context.AttackJustCompleted ||
           (!context.CurrentCoverage && context.FutureCoverage) ||
           context.TargetHardCrowdControlled;
}

struct WPlacementContext {
    bool CastPositionValid = false;
    bool Offensive = false;
    bool Defensive = false;
    bool Farm = false;
    bool Flee = false;
    bool CursorAgrees = false;
    bool EnemyTurret = false;
    bool Terrain = false;
    bool PlayerAttackWindingUp = false;
    bool EReady = false;
    bool ExistingEscapeAnchor = false;
    bool CreatesTargetCoverage = false;
    bool CreatesZone = false;
    bool TargetCommitted = false;
    bool TargetCanImmediatelyEscape = false;
    int Charges = 0;
    int MinimumReserve = 0;
    int FarmHits = 0;
};

inline bool ShouldPlaceW(const WPlacementContext& context) {
    if (!context.CastPositionValid || context.Charges <= 0 ||
        context.EnemyTurret || context.Terrain) return false;
    if (context.PlayerAttackWindingUp && !context.Defensive &&
        !context.Flee) return false;
    const int remaining = context.Charges - 1;
    if (remaining < context.MinimumReserve &&
        context.EReady && !context.ExistingEscapeAnchor &&
        !context.Defensive && !context.Flee) return false;
    if (context.Defensive || context.Flee) {
        return context.CursorAgrees || context.ExistingEscapeAnchor;
    }
    if (context.Farm) return context.FarmHits >= 3;
    if (!context.Offensive) return false;
    if (context.TargetCanImmediatelyEscape && !context.TargetCommitted &&
        !context.CreatesZone) return false;
    return context.CreatesTargetCoverage || context.CreatesZone;
}

struct CollisionBody {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Champion = false;
    bool Priority = false;
    bool Valid = false;
};

struct DashResult {
    Vec3 IntendedEndpoint = {};
    Vec3 ActualEndpoint = {};
    int CollisionId = 0;
    float CollisionT = 1.0f;
    bool HitChampion = false;
    bool ReachedSoldier = false;
    bool Valid = false;
};

inline DashResult ResolveDashSegment(
    const Vec3& start,
    const Vec3& endpoint,
    const std::vector<CollisionBody>& champions) {
    DashResult result{};
    result.IntendedEndpoint = endpoint;
    result.ActualEndpoint = endpoint;
    if (!start.IsValid() || !endpoint.IsValid() || endpoint.IsZero() ||
        start.Distance2D(endpoint) < 1.0f) return result;
    float earliest = 1.0f;
    for (const auto& champion : champions) {
        if (!champion.Valid || !champion.Champion) continue;
        const auto projection = ProjectPointToSegment2D(
            champion.Position, start, endpoint);
        if (projection.Distance > champion.Radius +
                kEDashHalfWidth + kEChampionCollisionPadding ||
            projection.T >= earliest) continue;
        earliest = projection.T;
        result.CollisionId = champion.Id;
        result.HitChampion = true;
    }
    result.CollisionT = earliest;
    if (result.HitChampion) {
        result.ActualEndpoint = start + (endpoint - start) * earliest;
        result.ActualEndpoint.y = start.y;
    }
    result.ReachedSoldier = !result.HitChampion || earliest >= 0.985f;
    result.Valid = true;
    return result;
}

struct DriftResult {
    Vec3 Anchor = {};
    Vec3 RedirectEndpoint = {};
    Vec3 FinalEndpoint = {};
    int CollisionId = 0;
    float TravelDistance = 0.0f;
    float TravelSeconds = 0.0f;
    bool QBuffered = false;
    bool HitChampion = false;
    bool ReachedRedirect = false;
    bool Valid = false;
};

inline DriftResult ResolveDrift(
    const Vec3& azir,
    const Soldier& anchor,
    const Vec3& qDesired,
    const std::vector<CollisionBody>& champions) {
    DriftResult result{};
    if (!anchor.Valid || !Commandable(azir, anchor)) return result;
    result.Anchor = anchor.Position;
    const auto first = ResolveDashSegment(
        azir, anchor.Position, champions);
    if (!first.Valid) return result;
    result.TravelDistance = azir.Distance2D(first.ActualEndpoint);
    if (first.HitChampion) {
        result.FinalEndpoint = first.ActualEndpoint;
        result.CollisionId = first.CollisionId;
        result.HitChampion = true;
        result.TravelSeconds = result.TravelDistance / kEDashSpeed;
        result.Valid = true;
        return result;
    }

    result.RedirectEndpoint = QFormation(azir, qDesired, 1).empty()
        ? Vec3{} : QFormation(azir, qDesired, 1).front();
    if (!result.RedirectEndpoint.IsValid() ||
        result.RedirectEndpoint.IsZero() ||
        anchor.Position.Distance2D(result.RedirectEndpoint) <
            kEQMinimumRedirectDistance) {
        result.FinalEndpoint = anchor.Position;
        result.TravelSeconds = result.TravelDistance / kEDashSpeed;
        result.ReachedRedirect = true;
        result.Valid = true;
        return result;
    }
    result.QBuffered = true;
    const auto second = ResolveDashSegment(
        anchor.Position, result.RedirectEndpoint, champions);
    result.TravelDistance += anchor.Position.Distance2D(second.ActualEndpoint);
    result.TravelSeconds = result.TravelDistance / kEDashSpeed;
    result.FinalEndpoint = second.ActualEndpoint;
    result.CollisionId = second.CollisionId;
    result.HitChampion = second.HitChampion;
    result.ReachedRedirect = second.ReachedSoldier;
    result.Valid = second.Valid;
    return result;
}

struct ECommitContext {
    bool EReady = false;
    bool AnchorValid = false;
    bool EndpointNavigable = false;
    bool PathCrossesForbiddenTerrain = false;
    bool EndpointEnemyTurret = false;
    bool EndpointDashHazard = false;
    bool EndpointPointClickThreat = false;
    bool PlayerMobilityLocked = false;
    bool PlayerAttackWindingUp = false;
    bool Defensive = false;
    bool Flee = false;
    bool Shuffle = false;
    bool Killable = false;
    bool TargetCollisionDesired = false;
    bool TargetCollisionConfirmed = false;
    bool QReadyForRedirect = false;
    bool CursorAgrees = false;
    bool HasRExit = false;
    bool HasAlliedFollowup = false;
    int EnemiesAtEndpoint = 0;
    int AlliesAtEndpoint = 0;
};

inline bool ShouldCommitE(const ECommitContext& context) {
    if (!context.EReady || !context.AnchorValid ||
        !context.EndpointNavigable || context.PathCrossesForbiddenTerrain ||
        context.PlayerMobilityLocked) return false;
    if (context.PlayerAttackWindingUp && !context.Defensive &&
        !context.Flee && !context.Killable) return false;
    if (context.EndpointEnemyTurret && !context.Killable) return false;
    if ((context.EndpointDashHazard || context.EndpointPointClickThreat) &&
        !context.Defensive && !context.Flee) return false;
    if (!context.CursorAgrees && !context.Defensive) return false;
    if (context.Defensive || context.Flee) return true;
    if (context.TargetCollisionDesired &&
        !context.TargetCollisionConfirmed) return false;
    if (context.Shuffle) {
        return context.QReadyForRedirect && context.HasRExit &&
               context.HasAlliedFollowup &&
               context.EnemiesAtEndpoint <=
                   context.AlliesAtEndpoint + 1;
    }
    return context.Killable && context.TargetCollisionConfirmed;
}

inline Vec3 RDirection(const Vec3& azir, const Vec3& castPosition) {
    return Direction2D(azir, castPosition);
}

inline bool RHits(const Vec3& azir,
                  const Vec3& direction,
                  int rank,
                  const Unit& target) {
    if (!target.Valid || direction.IsZero() || rank <= 0) return false;
    Vec3 relative = target.Position - azir;
    relative.y = 0.0f;
    const float longitudinal = relative.Dot(direction);
    const float lateral = std::fabs(Cross2D(direction, relative));
    return longitudinal >= -kRBehindOrigin - target.Radius &&
           longitudinal <= kRForwardReach + target.Radius &&
           lateral <= RWallHalfLength(rank) + target.Radius;
}

inline Vec3 RLandingPosition(const Vec3& azir,
                             const Vec3& direction,
                             const Unit& target) {
    if (!target.Valid || direction.IsZero()) return {};
    Vec3 relative = target.Position - azir;
    relative.y = 0.0f;
    const float lateral = Cross2D(direction, relative);
    const Vec3 side = Rotate2D(direction, kPi * 0.5f);
    return azir + direction * kRPushEndpoint + side * lateral;
}

struct REvaluation {
    Vec3 CastPosition = {};
    Vec3 Direction = {};
    Vec3 PrimaryLanding = {};
    std::vector<int> HitIds = {};
    int Hits = 0;
    int PriorityHits = 0;
    int AlliedFollowup = 0;
    float Score = -FLT_MAX;
    bool PrimaryHit = false;
    bool PrimaryLandingSafe = false;
    bool PushesPrimaryTowardAllies = false;
    bool SeparatesFrontFromBack = false;
    bool Valid = false;
};

inline REvaluation EvaluateR(const Vec3& azir,
                             const Vec3& desiredDirection,
                             int rank,
                             const std::vector<Unit>& enemies,
                             int primaryId,
                             const Vec3& alliedCentroid,
                             int alliedFollowup,
                             bool landingSafe) {
    REvaluation result{};
    result.Direction = Direction2D(azir, desiredDirection);
    if (result.Direction.IsZero() || rank <= 0) return result;
    result.CastPosition = azir + result.Direction * kRCastDistance;
    result.AlliedFollowup = std::max(0, alliedFollowup);
    const Unit* primary = nullptr;
    int frontHits = 0;
    int backHits = 0;
    for (const auto& enemy : enemies) {
        if (!RHits(azir, result.Direction, rank, enemy)) continue;
        result.HitIds.push_back(enemy.Id);
        ++result.Hits;
        if (enemy.Priority >= 1.35f) ++result.PriorityHits;
        Vec3 relative = enemy.Position - azir;
        if (relative.Dot(result.Direction) >= 0.0f) ++frontHits;
        else ++backHits;
        if (enemy.Id == primaryId) {
            primary = &enemy;
            result.PrimaryHit = true;
        }
    }
    if (primary) {
        result.PrimaryLanding = RLandingPosition(
            azir, result.Direction, *primary);
        result.PrimaryLandingSafe = landingSafe;
        if (alliedCentroid.IsValid() && !alliedCentroid.IsZero()) {
            result.PushesPrimaryTowardAllies =
                result.PrimaryLanding.Distance2D(alliedCentroid) + 35.0f <
                primary->Position.Distance2D(alliedCentroid);
        }
    }
    result.SeparatesFrontFromBack = frontHits > 0 && backHits == 0;
    result.Score = static_cast<float>(result.Hits) * 470.0f +
        static_cast<float>(result.PriorityHits) * 310.0f +
        static_cast<float>(result.AlliedFollowup) * 145.0f +
        (result.PrimaryHit ? 240.0f : 0.0f) +
        (result.PushesPrimaryTowardAllies ? 300.0f : 0.0f) +
        (result.SeparatesFrontFromBack ? 130.0f : 0.0f) -
        (!landingSafe && result.PrimaryHit ? 900.0f : 0.0f);
    result.Valid = true;
    return result;
}

enum class RPurpose : std::uint8_t {
    None,
    Peel,
    Disengage,
    KillSecure,
    Pick,
    Shuffle,
    Revenant,
    MultiTarget,
};

struct RCastContext {
    REvaluation Evaluation = {};
    RPurpose Purpose = RPurpose::None;
    bool RReady = false;
    bool PlayerAttackWindingUp = false;
    bool TargetSpellShield = false;
    bool TargetFlashReady = false;
    bool TargetDashReady = false;
    bool KeyCrowdControlSpent = false;
    bool PlayerLow = false;
    bool ThreatCommitted = false;
    bool EndpointEnemyTurret = false;
    bool EndpointTerrain = false;
    bool EndpointPointClickThreat = false;
    bool CursorAgrees = false;
    bool HasStasisOrExit = false;
    bool FrontToBackDpsAvailable = false;
    bool Lethal = false;
    int MinimumHits = 2;
};

inline bool ShouldCastR(const RCastContext& context) {
    if (!context.RReady || !context.Evaluation.Valid ||
        context.Evaluation.Hits <= 0 || context.EndpointTerrain) return false;
    const bool defensive = context.Purpose == RPurpose::Peel ||
        context.Purpose == RPurpose::Disengage;
    if (context.PlayerAttackWindingUp && !defensive && !context.Lethal) {
        return false;
    }
    if (context.TargetSpellShield && !defensive &&
        context.Evaluation.Hits == 1) return false;
    if (defensive) {
        return context.ThreatCommitted || context.PlayerLow;
    }
    if (context.Purpose == RPurpose::KillSecure) return context.Lethal;
    if (context.EndpointEnemyTurret || context.EndpointPointClickThreat ||
        !context.CursorAgrees) return false;
    if (context.Purpose == RPurpose::Shuffle ||
        context.Purpose == RPurpose::Revenant) {
        if (context.FrontToBackDpsAvailable &&
            context.Evaluation.Hits < 3) return false;
        if (!context.HasStasisOrExit ||
            context.Evaluation.AlliedFollowup <= 0 ||
            !context.Evaluation.PushesPrimaryTowardAllies) return false;
        if ((context.TargetFlashReady || context.TargetDashReady) &&
            !context.KeyCrowdControlSpent &&
            context.Evaluation.Hits < 3) return false;
    }
    return context.Evaluation.Hits >= std::max(1, context.MinimumHits) ||
           (context.Purpose == RPurpose::Pick &&
            context.Evaluation.PrimaryHit &&
            context.Evaluation.AlliedFollowup > 0);
}

enum class Sequence : std::uint8_t {
    None,
    WAutoQAuto,
    ExtendedSoldierDps,
    DriftEscape,
    DriftEngage,
    ShurimaShuffle,
    RevenantShuffle,
    CollisionRefund,
    DefensivePeel,
    FarmFormation,
    PlayerLed,
};

enum class SequencePhase : std::uint8_t {
    Idle,
    AwaitSoldier,
    AwaitFirstAttack,
    AwaitTargetExit,
    Dashing,
    AwaitQBuffer,
    AwaitRWindow,
    AwaitFollowupSoldier,
    RecoverDps,
};

struct SequenceState {
    Sequence Kind = Sequence::None;
    SequencePhase Phase = SequencePhase::Idle;
    int TargetId = 0;
    int AnchorId = 0;
    int StartedTick = 0;
    int LastTransitionTick = 0;
    int ExpireTick = 0;
    bool ControllerOwned = false;
};

inline bool SequenceExpired(const SequenceState& state, int now) {
    return state.Kind == Sequence::None || state.ExpireTick <= now ||
           state.StartedTick <= 0;
}

inline bool CanAdvanceSequence(const SequenceState& state,
                               int now,
                               int minimumDelayMs) {
    return !SequenceExpired(state, now) &&
           now - state.LastTransitionTick >= std::max(0, minimumDelayMs);
}

struct ShuffleGate {
    bool ManualKey = false;
    bool AutomaticEnabled = false;
    bool TargetValid = false;
    bool WReadyOrAnchor = false;
    bool EReady = false;
    bool QReady = false;
    bool RReady = false;
    bool CursorAgrees = false;
    bool AlliedFollowup = false;
    bool ExitAvailable = false;
    bool TargetFlashReady = false;
    bool TargetDashReady = false;
    bool KeyCrowdControlSpent = false;
    bool FrontToBackDpsAvailable = false;
    bool MultiTargetOpportunity = false;
    bool TurretRisk = false;
    bool TerrainRisk = false;
};

inline bool MayStartShuffle(const ShuffleGate& gate) {
    if (!gate.TargetValid || !gate.WReadyOrAnchor || !gate.EReady ||
        !gate.QReady || !gate.RReady) {
        return false;
    }
    // Manual key overrides tactical economy, never a known lethal endpoint.
    if (gate.ManualKey) {
        return !gate.TurretRisk && !gate.TerrainRisk;
    }
    if (!gate.AutomaticEnabled || !gate.CursorAgrees || !gate.AlliedFollowup ||
        !gate.ExitAvailable || gate.TurretRisk || gate.TerrainRisk) {
        return false;
    }
    if (gate.FrontToBackDpsAvailable && !gate.MultiTargetOpportunity) return false;
    if ((gate.TargetFlashReady || gate.TargetDashReady) &&
        !gate.KeyCrowdControlSpent && !gate.MultiTargetOpportunity) {
        return false;
    }
    return true;
}

struct SunDiscContext {
    bool PassiveReady = false;
    bool RuinInRange = false;
    bool PlayerChannelSafe = false;
    bool ObjectiveSoon = false;
    bool SideLanePressure = false;
    bool DefendingBase = false;
    bool TeamCanUseZone = false;
    bool EnemyCanImmediatelyDestroy = false;
    bool PlayerLeavingArea = false;
    int AlliedFollowup = 0;
    int NearbyEnemies = 0;
};

inline bool ShouldSuggestSunDisc(const SunDiscContext& context) {
    if (!context.PassiveReady || !context.RuinInRange ||
        !context.PlayerChannelSafe || context.EnemyCanImmediatelyDestroy ||
        context.PlayerLeavingArea) return false;
    const bool strategicWindow = context.ObjectiveSoon ||
        context.SideLanePressure || context.DefendingBase;
    if (!strategicWindow || !context.TeamCanUseZone) return false;
    return context.AlliedFollowup > 0 || context.NearbyEnemies == 0;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Azir::Geometry
