#pragma once

// Deterministic Bard mechanics and one-trick decisions. Runtime prediction,
// object ownership and spell casts live in AIBardController; this file owns
// Cosmic Binding's two-stage collision, shrine economy, portal tracing,
// chime/meep breakpoints and Tempered Fate's mixed-team no-grief policy so
// the dangerous parts of the champion remain independently testable.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Bard::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;
using SharedGeometry::kPi;

inline constexpr float kQInitialTargetRange = 850.0f;
inline constexpr float kQMissileResourceRange = 950.0f;
inline constexpr float kQContinuationDistance = 300.0f;
inline constexpr float kQHalfWidth = 60.0f;
inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQMissileSpeed = 1500.0f;
inline constexpr float kQSlowPercent = 0.60f;

inline constexpr float kWCastRange = 800.0f;
inline constexpr float kWShrineRadius = 100.0f;
inline constexpr float kWChargeSeconds = 5.0f;
inline constexpr float kWAmmoRechargeSeconds = 18.0f;
inline constexpr int kWMaximumAmmo = 2;
inline constexpr int kWMaximumGroundShrines = 3;
inline constexpr float kWMoveSpeedSeconds = 1.5f;

inline constexpr float kECastRange = 900.0f;
inline constexpr float kEMaximumTunnelLength = 2600.0f;
inline constexpr float kEPortalSeconds = 10.0f;
inline constexpr float kEEnemyTravelSpeed = 900.0f;
inline constexpr float kEAllyTravelSpeed = 1197.0f;

inline constexpr float kRCastRange = 3400.0f;
inline constexpr float kRRadius = 350.0f;
inline constexpr float kRCastSeconds = 0.50f;
inline constexpr float kRStasisSeconds = 2.50f;
inline constexpr float kRMinimumTravelSeconds = 0.65f;
inline constexpr float kRMaximumTravelSeconds = 1.80f;

inline Vec3 ClampToRange(const Vec3& origin,
                         const Vec3& desired,
                         float range) {
    if (!origin.IsValid() || !desired.IsValid() || desired.IsZero()) return {};
    const float distance = origin.Distance2D(desired);
    if (distance <= std::max(0.0f, range)) return desired;
    const Vec3 direction = Direction2D(origin, desired);
    return direction.IsZero() ? Vec3{} : origin + direction * range;
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 80.0f, 120.0f, 160.0f, 200.0f, 240.0f,
    };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, abilityPower);
}

inline float QDisableSeconds(int rank) {
    static constexpr std::array<float, 6> duration = {
        0.0f, 1.0f, 1.2f, 1.4f, 1.6f, 1.8f,
    };
    return RankValue(duration, rank);
}

inline float WMinimumRawHeal(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 25.0f, 50.0f, 75.0f, 100.0f, 125.0f,
    };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, abilityPower);
}

inline float WMaximumRawHeal(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 50.0f, 87.5f, 125.0f, 162.5f, 200.0f,
    };
    return RankValue(base, rank) + 0.70f * std::max(0.0f, abilityPower);
}

inline float WShrineChargeFraction(float ageSeconds) {
    return std::clamp(ageSeconds / kWChargeSeconds, 0.0f, 1.0f);
}

inline float WShrineRawHeal(int rank,
                            float abilityPower,
                            float ageSeconds) {
    const float minimum = WMinimumRawHeal(rank, abilityPower);
    const float maximum = WMaximumRawHeal(rank, abilityPower);
    return minimum + (maximum - minimum) *
        WShrineChargeFraction(ageSeconds);
}

inline float WMovementSpeedPercent(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 20.0f, 22.5f, 25.0f, 27.5f, 30.0f,
    };
    return RankValue(base, rank) +
           0.06f * std::max(0.0f, abilityPower);
}

struct MeepSnapshot {
    int Chimes = 0;
    int MaximumMeeps = 1;
    float RechargeSeconds = 8.0f;
    float RawMagicDamage = 30.0f;
    float SlowPercent = 0.0f;
    bool HasSlow = false;
    bool HasSplash = false;
    bool HasExpandedSplash = false;
};

inline int MeepMaximumCount(int chimes) {
    const int count = std::max(0, chimes);
    if (count >= 100) return 9;
    if (count >= 95) return 8;
    if (count >= 90) return 7;
    if (count >= 80) return 6;
    if (count >= 65) return 5;
    if (count >= 50) return 4;
    if (count >= 30) return 3;
    if (count >= 10) return 2;
    return 1;
}

inline float MeepRechargeSeconds(int chimes) {
    const int count = std::max(0, chimes);
    if (count >= 70) return 4.0f;
    if (count >= 55) return 5.0f;
    if (count >= 40) return 6.0f;
    if (count >= 20) return 7.0f;
    return 8.0f;
}

inline float MeepSlowPercent(int chimes) {
    const int count = std::max(0, chimes);
    if (count >= 85) return 0.75f;
    if (count >= 75) return 0.65f;
    if (count >= 60) return 0.55f;
    if (count >= 45) return 0.45f;
    if (count >= 25) return 0.35f;
    if (count >= 5) return 0.25f;
    return 0.0f;
}

inline MeepSnapshot MeepState(int chimes, float abilityPower) {
    MeepSnapshot result{};
    result.Chimes = std::max(0, chimes);
    result.MaximumMeeps = MeepMaximumCount(result.Chimes);
    result.RechargeSeconds = MeepRechargeSeconds(result.Chimes);
    result.RawMagicDamage = 30.0f +
        6.0f * static_cast<float>(result.Chimes / 5) +
        0.40f * std::max(0.0f, abilityPower);
    result.SlowPercent = MeepSlowPercent(result.Chimes);
    result.HasSlow = result.SlowPercent > 0.0f;
    result.HasSplash = result.Chimes >= 15;
    result.HasExpandedSplash = result.Chimes >= 35;
    return result;
}

inline float ChimeExperience(float gameMinutes) {
    const int bonus = gameMinutes > 5.0f
        ? static_cast<int>(std::floor(gameMinutes - 5.0f))
        : 0;
    return 20.0f + static_cast<float>(std::max(0, bonus));
}

struct ChimeRouteContext {
    float DirectPathDistance = 0.0f;
    float ChimePathDistance = 0.0f;
    float ExpireSeconds = 600.0f;
    float AllyLaneSafety = 1.0f;
    float ObjectiveUrgency = 0.0f;
    bool CarryCanFarmSafely = true;
    bool OnPrimaryRoute = false;
};

inline float ChimeRouteScore(const ChimeRouteContext& context) {
    const float detour = std::max(
        0.0f, context.ChimePathDistance - context.DirectPathDistance);
    float score = 150.0f - detour * 0.70f;
    score += context.OnPrimaryRoute ? 180.0f : 0.0f;
    score += std::clamp(context.ExpireSeconds, 0.0f, 60.0f) < 20.0f
        ? 70.0f : 0.0f;
    score += std::clamp(context.AllyLaneSafety, 0.0f, 1.0f) * 120.0f;
    score -= std::clamp(context.ObjectiveUrgency, 0.0f, 1.0f) * 320.0f;
    if (!context.CarryCanFarmSafely) score -= 500.0f;
    return score;
}

struct QUnit {
    Vec3 Position = {};
    Vec3 PredictedPosition = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    int Id = 0;
    bool Hostile = true;
    bool Champion = false;
    bool Minion = false;
    bool Monster = false;
    bool Structure = false;
    bool WardOrTrap = false;
    bool SpellShield = false;
    bool HardCrowdControlled = false;
    bool Targetable = true;
    bool Valid = false;
};

struct RayCircleHit {
    int Id = 0;
    float EntryDistance = FLT_MAX;
    float ExitDistance = FLT_MAX;
    Vec3 Contact = {};
    bool Valid = false;
};

inline RayCircleHit RayCircleCollision(const Vec3& origin,
                                       const Vec3& direction,
                                       const QUnit& unit,
                                       float halfWidth) {
    RayCircleHit hit{};
    if (!origin.IsValid() || direction.IsZero() || !unit.Valid ||
        !unit.Targetable || !unit.Hostile || unit.Structure ||
        unit.WardOrTrap) {
        return hit;
    }
    const Vec3 center = unit.PredictedPosition.IsValid() &&
                        !unit.PredictedPosition.IsZero()
        ? unit.PredictedPosition : unit.Position;
    Vec3 relative = center - origin;
    relative.y = 0.0f;
    const float along = relative.Dot(direction);
    const float radius = std::max(0.0f, unit.Radius) +
                         std::max(0.0f, halfWidth);
    const float perpendicularSquared = std::max(
        0.0f, relative.Dot(relative) - along * along);
    if (perpendicularSquared > radius * radius) return hit;
    const float chord = std::sqrt(std::max(
        0.0f, radius * radius - perpendicularSquared));
    const float exit = along + chord;
    if (exit < 0.0f) return hit;
    hit.Id = unit.Id;
    hit.EntryDistance = std::max(0.0f, along - chord);
    hit.ExitDistance = std::max(hit.EntryDistance, exit);
    hit.Contact = origin + direction * hit.EntryDistance;
    hit.Contact.y = center.y;
    hit.Valid = true;
    return hit;
}

struct QEvaluation {
    Vec3 CastPosition = {};
    Vec3 Direction = {};
    Vec3 FirstContact = {};
    Vec3 SecondContact = {};
    Vec3 ContinuationEnd = {};
    int FirstId = 0;
    int SecondId = 0;
    float FirstEntryDistance = FLT_MAX;
    float SecondEntryDistance = FLT_MAX;
    float FirstImpactSeconds = 0.0f;
    float SecondImpactSeconds = 0.0f;
    float Score = -FLT_MAX;
    bool IntendedFirst = false;
    bool WallSecond = false;
    bool FirstDamaged = false;
    bool SecondDamaged = false;
    bool FirstSlowed = false;
    bool FirstStunned = false;
    bool SecondStunned = false;
    bool PerfectStasisExit = false;
    bool Valid = false;
};

inline const QUnit* QUnitById(const std::vector<QUnit>& units, int id) {
    for (const auto& unit : units) {
        if (unit.Valid && unit.Id == id) return &unit;
    }
    return nullptr;
}

inline QEvaluation EvaluateCosmicBinding(
    const Vec3& origin,
    const Vec3& desired,
    const std::vector<QUnit>& units,
    const std::vector<Vec3>& terrainSamples,
    int intendedFirstId = 0) {
    QEvaluation result{};
    const Vec3 direction = Direction2D(origin, desired);
    if (!origin.IsValid() || direction.IsZero()) return result;
    result.Direction = direction;
    result.CastPosition = origin + direction * kQInitialTargetRange;

    RayCircleHit first{};
    for (const auto& unit : units) {
        const RayCircleHit hit = RayCircleCollision(
            origin, direction, unit, kQHalfWidth);
        if (!hit.Valid || hit.EntryDistance > kQInitialTargetRange + 0.01f) {
            continue;
        }
        if (!first.Valid || hit.EntryDistance < first.EntryDistance ||
            (std::abs(hit.EntryDistance - first.EntryDistance) <= 0.01f &&
             hit.Id < first.Id)) {
            first = hit;
        }
    }
    if (!first.Valid) return result;

    result.FirstId = first.Id;
    result.FirstEntryDistance = first.EntryDistance;
    result.FirstContact = first.Contact;
    result.FirstImpactSeconds = kQCastSeconds +
        first.EntryDistance / kQMissileSpeed;
    result.IntendedFirst = intendedFirstId == 0 ||
                           first.Id == intendedFirstId;
    const QUnit* firstUnit = QUnitById(units, first.Id);
    if (!firstUnit) return result;

    const float continuationStart = first.EntryDistance;
    const float continuationEnd = continuationStart +
                                  kQContinuationDistance;
    result.ContinuationEnd = origin + direction * continuationEnd;

    RayCircleHit second{};
    for (const auto& unit : units) {
        if (!unit.Valid || unit.Id == first.Id) continue;
        const RayCircleHit hit = RayCircleCollision(
            origin, direction, unit, kQHalfWidth);
        if (!hit.Valid || hit.ExitDistance <= continuationStart + 0.01f ||
            hit.EntryDistance > continuationEnd + 0.01f) {
            continue;
        }
        const float effectiveEntry = std::max(
            continuationStart, hit.EntryDistance);
        if (!second.Valid || effectiveEntry < second.EntryDistance ||
            (std::abs(effectiveEntry - second.EntryDistance) <= 0.01f &&
             hit.Id < second.Id)) {
            second = hit;
            second.EntryDistance = effectiveEntry;
            second.Contact = origin + direction * effectiveEntry;
        }
    }

    float wallDistance = FLT_MAX;
    Vec3 wallContact{};
    for (const Vec3& sample : terrainSamples) {
        if (!sample.IsValid() || sample.IsZero()) continue;
        Vec3 relative = sample - origin;
        relative.y = 0.0f;
        const float along = relative.Dot(direction);
        if (along <= continuationStart + 0.01f ||
            along > continuationEnd + 0.01f) {
            continue;
        }
        const Vec3 closest = origin + direction * along;
        if (closest.Distance2D(sample) > kQHalfWidth + 8.0f) continue;
        if (along < wallDistance) {
            wallDistance = along;
            wallContact = sample;
        }
    }

    const bool wallWins = wallDistance < FLT_MAX &&
        (!second.Valid || wallDistance <= second.EntryDistance);
    result.WallSecond = wallWins;
    if (wallWins) {
        result.SecondContact = wallContact;
        result.SecondEntryDistance = wallDistance;
        result.SecondImpactSeconds = kQCastSeconds +
            wallDistance / kQMissileSpeed;
    } else if (second.Valid) {
        result.SecondId = second.Id;
        result.SecondContact = second.Contact;
        result.SecondEntryDistance = second.EntryDistance;
        result.SecondImpactSeconds = kQCastSeconds +
            second.EntryDistance / kQMissileSpeed;
    }

    const bool hasTrigger = wallWins || second.Valid;
    const QUnit* secondUnit = second.Valid
        ? QUnitById(units, second.Id) : nullptr;
    result.FirstDamaged = !firstUnit->SpellShield;
    result.FirstSlowed = result.FirstDamaged;
    if (hasTrigger) {
        // A first-target shield blocks initial damage, but terrain can still
        // shackle that unit. Against a second unit, only the unshielded body
        // receives its respective stun/damage.
        result.FirstStunned = wallWins || !firstUnit->SpellShield;
        if (secondUnit && !wallWins) {
            result.SecondDamaged = !secondUnit->SpellShield;
            result.SecondStunned = !secondUnit->SpellShield;
        }
    }

    result.Score = (hasTrigger ? 420.0f : 80.0f) +
        firstUnit->Priority * 110.0f;
    if (result.IntendedFirst) result.Score += 240.0f;
    if (firstUnit->Champion) result.Score += 180.0f;
    if (firstUnit->HardCrowdControlled) result.Score -= 80.0f;
    if (firstUnit->SpellShield && !wallWins) result.Score -= 260.0f;
    if (secondUnit) {
        result.Score += secondUnit->Champion ? 180.0f : 70.0f;
        result.Score += secondUnit->Priority * 55.0f;
        if (secondUnit->SpellShield) result.Score -= 120.0f;
    }
    result.Valid = result.IntendedFirst &&
                   (result.FirstDamaged || result.FirstStunned);
    return result;
}

inline Vec3 PairAlignmentAim(const Vec3& origin,
                             const Vec3& first,
                             const Vec3& second) {
    if (!origin.IsValid() || !first.IsValid() || !second.IsValid()) return {};
    const Vec3 firstDirection = Direction2D(origin, first);
    const Vec3 pairDirection = Direction2D(first, second);
    if (firstDirection.IsZero() || pairDirection.IsZero()) return {};
    Vec3 blended = firstDirection * 0.72f + pairDirection * 0.28f;
    blended.y = 0.0f;
    const float length = blended.Length2D();
    if (length <= 0.001f) return {};
    return origin + blended / length * kQInitialTargetRange;
}

inline bool ShouldWaitForMeepBeforeQ(bool meepAvailable,
                                     bool targetInAttackRange,
                                     bool immediateStun,
                                     bool interrupt,
                                     bool lethal) {
    return meepAvailable && targetInAttackRange && !immediateStun &&
           !interrupt && !lethal;
}

struct Shrine {
    Vec3 Position = {};
    int Id = 0;
    float AgeSeconds = 0.0f;
    bool ConfirmedOwned = false;
    bool Valid = false;
};

inline bool ShrineOverlaps(const Vec3& position,
                           const std::vector<Shrine>& shrines,
                           float minimumSeparation = 145.0f) {
    for (const auto& shrine : shrines) {
        if (shrine.Valid && shrine.Position.Distance2D(position) <
                std::max(0.0f, minimumSeparation)) {
            return true;
        }
    }
    return false;
}

inline bool CanPlaceGroundShrine(const Vec3& bard,
                                 const Vec3& position,
                                 const std::vector<Shrine>& shrines,
                                 int ammo,
                                 bool terrain,
                                 bool reserveLastCharge) {
    if (!bard.IsValid() || !position.IsValid() || position.IsZero() ||
        terrain || ammo <= (reserveLastCharge ? 1 : 0) ||
        bard.Distance2D(position) > kWCastRange) {
        return false;
    }
    int active = 0;
    for (const auto& shrine : shrines) {
        if (shrine.Valid && shrine.ConfirmedOwned) ++active;
    }
    return active < kWMaximumGroundShrines &&
           !ShrineOverlaps(position, shrines);
}

struct EmergencyHealContext {
    float HealthPercent = 100.0f;
    float IncomingDamage = 0.0f;
    float CurrentHealth = 1.0f;
    float Distance = FLT_MAX;
    float Priority = 1.0f;
    bool Targeted = false;
    bool HardCrowdControlled = false;
    bool HasGrievousWounds = false;
};

inline float EmergencyHealScore(const EmergencyHealContext& context,
                                float rawHeal) {
    if (context.Distance > kWCastRange + 80.0f ||
        context.HealthPercent >= 96.0f) {
        return -FLT_MAX;
    }
    const float effectiveHeal = std::max(0.0f, rawHeal) *
        (context.HasGrievousWounds ? 0.60f : 1.0f);
    float score = (100.0f - context.HealthPercent) * 6.0f +
        std::max(0.0f, context.IncomingDamage) * 1.4f +
        std::max(0.5f, context.Priority) * 90.0f;
    if (context.Targeted) score += 190.0f;
    if (context.HardCrowdControlled) score += 140.0f;
    if (context.IncomingDamage >= context.CurrentHealth + effectiveHeal) {
        score += 520.0f;
    }
    return score;
}

struct PortalTrace {
    Vec3 CastPosition = {};
    Vec3 Entrance = {};
    Vec3 Exit = {};
    Vec3 Direction = {};
    float TerrainLength = 0.0f;
    float AllyTravelSeconds = 0.0f;
    float EnemyTravelSeconds = 0.0f;
    bool Valid = false;
};

template <typename IsTerrain>
inline PortalTrace TracePortal(const Vec3& bard,
                               const Vec3& desiredTerrain,
                               IsTerrain&& isTerrain,
                               float sampleStep = 18.0f) {
    PortalTrace result{};
    const Vec3 direction = Direction2D(bard, desiredTerrain);
    if (!bard.IsValid() || direction.IsZero()) return result;
    const float requestedDistance = bard.Distance2D(desiredTerrain);
    const float castDistance = std::min(
        std::max(0.0f, requestedDistance), kECastRange);
    if (castDistance < 20.0f) return result;
    result.CastPosition = bard + direction * castDistance;
    result.Direction = direction;
    if (!isTerrain(result.CastPosition)) return result;

    const float step = std::clamp(sampleStep, 6.0f, 40.0f);
    bool entered = false;
    float entryDistance = 0.0f;
    float exitDistance = 0.0f;
    const float scanEnd = std::min(
        castDistance + kEMaximumTunnelLength,
        kECastRange + kEMaximumTunnelLength);
    for (float distance = step; distance <= scanEnd; distance += step) {
        const Vec3 sample = bard + direction * distance;
        const bool wall = isTerrain(sample);
        if (!entered && wall) {
            entered = true;
            entryDistance = std::max(0.0f, distance - step);
        } else if (entered && !wall) {
            exitDistance = distance;
            break;
        }
    }
    if (!entered || exitDistance <= entryDistance) return result;
    const float thickness = exitDistance - entryDistance;
    if (thickness < 45.0f || thickness > kEMaximumTunnelLength) {
        return result;
    }
    result.Entrance = bard + direction * entryDistance;
    result.Exit = bard + direction * (exitDistance + 24.0f);
    result.TerrainLength = thickness;
    result.AllyTravelSeconds = thickness / kEAllyTravelSpeed;
    result.EnemyTravelSeconds = thickness / kEEnemyTravelSpeed;
    result.Valid = true;
    return result;
}

struct PortalSafetyContext {
    int AlliesAtExit = 0;
    int EnemiesAtExit = 0;
    int EnemiesAtEntrance = 0;
    float CursorDistance = 0.0f;
    float ThreatSeparationGain = 0.0f;
    bool ExitTerrain = false;
    bool ExitUnderEnemyTurret = false;
    bool DashHazardAtExit = false;
    bool AllyRequestedDirection = false;
};

inline float PortalSafetyScore(const PortalTrace& portal,
                               const PortalSafetyContext& context,
                               bool defensive) {
    if (!portal.Valid || context.ExitTerrain ||
        context.DashHazardAtExit ||
        (context.ExitUnderEnemyTurret && defensive)) {
        return -FLT_MAX;
    }
    float score = static_cast<float>(context.AlliesAtExit) * 180.0f -
                  static_cast<float>(context.EnemiesAtExit) * 260.0f;
    score -= std::max(0.0f, context.CursorDistance) * 0.12f;
    score += std::max(0.0f, context.ThreatSeparationGain) *
             (defensive ? 0.85f : 0.25f);
    score += context.AllyRequestedDirection ? 150.0f : 0.0f;
    if (!defensive && context.ExitUnderEnemyTurret) score -= 850.0f;
    if (defensive && context.EnemiesAtEntrance > 0) score += 90.0f;
    return score;
}

inline float RTravelSeconds(float distance) {
    const float t = std::clamp(
        std::max(0.0f, distance) / kRCastRange, 0.0f, 1.0f);
    return kRMinimumTravelSeconds +
           (kRMaximumTravelSeconds - kRMinimumTravelSeconds) * t;
}

inline float RImpactSeconds(float distance) {
    return kRCastSeconds + RTravelSeconds(distance);
}

inline float QArrivalSeconds(float distance) {
    return kQCastSeconds +
           std::max(0.0f, distance) / kQMissileSpeed;
}

inline float QCastDelayForStasisExit(float remainingStasisSeconds,
                                     float qTravelDistance,
                                     float earlyBiasSeconds = 0.04f) {
    return std::max(
        0.0f,
        remainingStasisSeconds - QArrivalSeconds(qTravelDistance) -
            std::max(0.0f, earlyBiasSeconds));
}

enum class TeamRelation : std::uint8_t {
    Neutral,
    Ally,
    Enemy,
};

struct StasisUnit {
    Vec3 Position = {};
    Vec3 PredictedPosition = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    float HealthPercent = 100.0f;
    int Id = 0;
    TeamRelation Team = TeamRelation::Neutral;
    bool Champion = false;
    bool Minion = false;
    bool Monster = false;
    bool EpicMonster = false;
    bool Turret = false;
    bool Plant = false;
    bool CurrentAllyFocus = false;
    bool IncomingLethal = false;
    bool ProtectedAlly = false;
    bool Channeling = false;
    bool HardCrowdControlled = false;
    bool Valid = false;
};

struct StasisContext {
    bool Catch = false;
    bool BacklineIsolation = false;
    bool SaveAlly = false;
    bool DiveTower = false;
    bool ObjectiveDeny = false;
    bool AlliesSecuringObjective = false;
    bool EscapePlantDeny = false;
    int MaximumFriendlyGrief = 0;
    int MinimumEnemyChampions = 1;
};

struct REvaluation {
    Vec3 Center = {};
    std::array<int, 24> HitIds = {};
    int HitCount = 0;
    int EnemyChampions = 0;
    int AllyChampions = 0;
    int SavedAllies = 0;
    int FocusedEnemies = 0;
    int EpicMonsters = 0;
    int Turrets = 0;
    int Plants = 0;
    int FriendlyGrief = 0;
    float Score = -FLT_MAX;
    bool Valid = false;
};

inline REvaluation EvaluateTemperedFate(
    const Vec3& bard,
    const Vec3& center,
    const std::vector<StasisUnit>& units,
    const StasisContext& context) {
    REvaluation result{};
    result.Center = center;
    if (!bard.IsValid() || !center.IsValid() || center.IsZero() ||
        bard.Distance2D(center) > kRCastRange) {
        return result;
    }
    float score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid) continue;
        const Vec3 position = unit.PredictedPosition.IsValid() &&
                              !unit.PredictedPosition.IsZero()
            ? unit.PredictedPosition : unit.Position;
        if (center.Distance2D(position) >
                kRRadius + std::max(0.0f, unit.Radius)) {
            continue;
        }
        if (result.HitCount < static_cast<int>(result.HitIds.size())) {
            result.HitIds[static_cast<std::size_t>(result.HitCount)] = unit.Id;
        }
        ++result.HitCount;

        if (unit.Champion && unit.Team == TeamRelation::Enemy) {
            ++result.EnemyChampions;
            float value = 170.0f + std::max(0.5f, unit.Priority) * 120.0f;
            if (unit.Channeling) value += 180.0f;
            if (unit.HardCrowdControlled) value -= 120.0f;
            if (unit.CurrentAllyFocus) {
                ++result.FocusedEnemies;
                value -= context.BacklineIsolation ? 150.0f : 560.0f;
            }
            if (unit.HealthPercent <= 18.0f && unit.CurrentAllyFocus) {
                value -= 360.0f;
            }
            score += value;
        } else if (unit.Champion && unit.Team == TeamRelation::Ally) {
            ++result.AllyChampions;
            if (unit.IncomingLethal && context.SaveAlly) {
                ++result.SavedAllies;
                score += 780.0f +
                    (unit.ProtectedAlly ? 240.0f : 0.0f);
            } else {
                ++result.FriendlyGrief;
                score -= 300.0f +
                    std::max(0.5f, unit.Priority) * 170.0f;
                if (unit.Channeling) score -= 420.0f;
            }
        } else if (unit.EpicMonster) {
            ++result.EpicMonsters;
            if (context.ObjectiveDeny) score += 560.0f;
            if (context.AlliesSecuringObjective) {
                ++result.FriendlyGrief;
                score -= 720.0f;
            }
        } else if (unit.Turret) {
            ++result.Turrets;
            score += context.DiveTower ? 680.0f : 0.0f;
        } else if (unit.Plant) {
            ++result.Plants;
            score += context.EscapePlantDeny ? 190.0f : 0.0f;
        }
    }

    if (context.Catch) score += result.EnemyChampions * 90.0f;
    if (context.BacklineIsolation && result.EnemyChampions > 0) {
        score += result.EnemyChampions * 130.0f;
    }
    const bool hasPurpose =
        (context.SaveAlly && result.SavedAllies > 0) ||
        (context.DiveTower && result.Turrets > 0) ||
        (context.ObjectiveDeny && result.EpicMonsters > 0) ||
        (context.EscapePlantDeny && result.Plants > 0) ||
        ((context.Catch || context.BacklineIsolation) &&
         result.EnemyChampions >= std::max(1, context.MinimumEnemyChampions));
    result.Score = score;
    result.Valid = hasPurpose &&
        result.FriendlyGrief <= std::max(0, context.MaximumFriendlyGrief) &&
        score > 0.0f;
    return result;
}

inline bool BetterRPlan(const REvaluation& candidate,
                        const REvaluation& current) {
    if (!candidate.Valid) return false;
    if (!current.Valid) return true;
    if (std::abs(candidate.Score - current.Score) > 0.01f) {
        return candidate.Score > current.Score;
    }
    if (candidate.FriendlyGrief != current.FriendlyGrief) {
        return candidate.FriendlyGrief < current.FriendlyGrief;
    }
    if (candidate.EnemyChampions != current.EnemyChampions) {
        return candidate.EnemyChampions > current.EnemyChampions;
    }
    return candidate.Center.x < current.Center.x ||
        (candidate.Center.x == current.Center.x &&
         candidate.Center.z < current.Center.z);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Bard::Geometry
