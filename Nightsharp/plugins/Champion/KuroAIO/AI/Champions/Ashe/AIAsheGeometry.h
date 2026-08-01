#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Ashe::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::Rotate2D;
using SharedGeometry::kPi;

inline constexpr float kAttackRange = 600.0f;
inline constexpr float kVolleyRange = 1200.0f;
inline constexpr float kVolleyCastSeconds = 0.25f;
inline constexpr float kVolleyMissileSpeed = 1500.0f;
inline constexpr float kVolleyMissileRadius = 20.0f;
inline constexpr float kVolleyRayStepRadians = 5.0f * kPi / 180.0f;
inline constexpr float kHawkshotRange = 25000.0f;
inline constexpr float kHawkshotSpeed = 1400.0f;
inline constexpr float kHawkshotPathVisionRadius = 325.0f;
inline constexpr float kHawkshotDestinationVisionRadius = 1000.0f;
inline constexpr float kArrowRange = 25000.0f;
inline constexpr float kArrowCastSeconds = 0.25f;
inline constexpr float kArrowInitialSpeed = 1500.0f;
inline constexpr float kArrowMaximumSpeed = 2100.0f;
inline constexpr float kArrowAcceleration = 200.0f;
inline constexpr float kArrowRadius = 130.0f;
inline constexpr float kArrowExplosionRadius = 400.0f;
inline constexpr float kArrowMaximumStunDistance = 2800.0f;
inline constexpr float kArrowMinimumStunSeconds = 1.0f;
inline constexpr float kArrowMaximumStunSeconds = 3.5f;

inline constexpr int VolleyArrowCount(int rank) {
    return rank <= 0 ? 0 : 6 + std::clamp(rank, 1, 5);
}

inline constexpr float VolleyHalfAngleRadians(int rank) {
    const int count = VolleyArrowCount(rank);
    return count > 0
        ? static_cast<float>(count - 1) * 0.5f * kVolleyRayStepRadians
        : 0.0f;
}

inline Vec3 VolleyRayDirection(const Vec3& aimDirection,
                               int rank,
                               int rayIndex) {
    const int count = VolleyArrowCount(rank);
    if (count <= 0 || rayIndex < 0 || rayIndex >= count ||
        aimDirection.IsZero()) {
        return {};
    }
    const float center = static_cast<float>(count - 1) * 0.5f;
    return Rotate2D(
        aimDirection,
        (static_cast<float>(rayIndex) - center) * kVolleyRayStepRadians);
}

inline float VolleyImpactSeconds(float distance) {
    return kVolleyCastSeconds +
           std::max(0.0f, distance) / kVolleyMissileSpeed;
}

struct VolleyUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Weight = 0.0f;
    float Health = FLT_MAX;
    int Id = 0;
    bool Champion = false;
    bool Minion = false;
    bool Primary = false;
    bool CrowdControlled = false;
    bool Killable = false;
    bool Valid = true;
};

struct VolleyRayHit {
    int RayIndex = -1;
    int UnitId = 0;
    float Along = FLT_MAX;
    Vec3 Position = {};
    bool Champion = false;
    bool Primary = false;
    bool Valid = false;
};

inline VolleyRayHit FirstVolleyRayHit(
    const Vec3& source,
    const Vec3& rayDirection,
    int rayIndex,
    const std::vector<VolleyUnit>& units,
    float range = kVolleyRange,
    float missileRadius = kVolleyMissileRadius) {
    VolleyRayHit result{};
    if (source.IsZero() && rayDirection.IsZero()) return result;
    if (rayDirection.IsZero() || range <= 0.0f) return result;
    const Vec3 end = source + rayDirection * range;
    for (const auto& unit : units) {
        if (!unit.Valid || unit.Id == 0 || unit.Radius < 0.0f) continue;
        const auto projection = ProjectPointToSegment2D(
            unit.Position, source, end);
        if (projection.T <= 0.0001f || projection.T > 1.0f ||
            projection.Distance > unit.Radius + missileRadius) {
            continue;
        }
        const float along = source.Distance2D(projection.Closest);
        if (along >= result.Along) continue;
        result.RayIndex = rayIndex;
        result.UnitId = unit.Id;
        result.Along = along;
        result.Position = unit.Position;
        result.Champion = unit.Champion;
        result.Primary = unit.Primary;
        result.Valid = true;
    }
    return result;
}

struct VolleyEvaluation {
    std::array<VolleyRayHit, 11> Rays = {};
    std::array<int, 11> UniqueHitIds = {};
    int RayCount = 0;
    int BlockedRays = 0;
    int UniqueHits = 0;
    int ChampionHits = 0;
    int MinionHits = 0;
    int PrimaryRay = -1;
    float PrimaryAlong = FLT_MAX;
    float Score = -FLT_MAX;
    bool HitsPrimary = false;
    bool Valid = false;
};

inline const VolleyUnit* FindVolleyUnit(
    const std::vector<VolleyUnit>& units,
    int id) {
    for (const auto& unit : units) {
        if (unit.Valid && unit.Id == id) return &unit;
    }
    return nullptr;
}

inline bool ContainsHitId(const VolleyEvaluation& evaluation, int id) {
    for (int i = 0; i < evaluation.UniqueHits; ++i) {
        if (evaluation.UniqueHitIds[static_cast<std::size_t>(i)] == id) {
            return true;
        }
    }
    return false;
}

inline VolleyEvaluation EvaluateVolley(
    const Vec3& source,
    const Vec3& aimDirection,
    int rank,
    const std::vector<VolleyUnit>& units,
    int primaryId = 0) {
    VolleyEvaluation result{};
    result.RayCount = VolleyArrowCount(rank);
    if (result.RayCount <= 0 || aimDirection.IsZero()) return result;

    float score = 0.0f;
    for (int ray = 0; ray < result.RayCount; ++ray) {
        const Vec3 direction = VolleyRayDirection(aimDirection, rank, ray);
        VolleyRayHit hit = FirstVolleyRayHit(
            source, direction, ray, units);
        result.Rays[static_cast<std::size_t>(ray)] = hit;
        if (!hit.Valid) continue;
        ++result.BlockedRays;

        if (hit.UnitId == primaryId || hit.Primary) {
            result.HitsPrimary = true;
            if (hit.Along < result.PrimaryAlong) {
                result.PrimaryAlong = hit.Along;
                result.PrimaryRay = ray;
            }
        }
        if (ContainsHitId(result, hit.UnitId)) continue;
        if (result.UniqueHits < static_cast<int>(result.UniqueHitIds.size())) {
            result.UniqueHitIds[static_cast<std::size_t>(result.UniqueHits++)] =
                hit.UnitId;
        }
        const VolleyUnit* unit = FindVolleyUnit(units, hit.UnitId);
        if (!unit) continue;
        score += unit->Weight;
        if (unit->Champion) {
            ++result.ChampionHits;
            score += 2.0f;
        }
        if (unit->Minion) ++result.MinionHits;
        if (unit->CrowdControlled) score += 0.65f;
        if (unit->Killable) score += 3.1f;
        if (unit->Primary || unit->Id == primaryId) score += 6.5f;
    }
    if (result.HitsPrimary) score += 1.5f;
    result.Score = score;
    result.Valid = result.UniqueHits > 0;
    return result;
}

inline float QFlurryAttackRatio(int rank) {
    static constexpr std::array<float, 5> values = {
        1.10f, 1.15f, 1.20f, 1.25f, 1.30f,
    };
    return rank > 0 ? values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)]
                    : 0.0f;
}

inline float QBonusAttackSpeedPercent(int rank) {
    static constexpr std::array<float, 5> values = {
        20.0f, 30.0f, 40.0f, 50.0f, 60.0f,
    };
    return rank > 0 ? values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)]
                    : 0.0f;
}

inline float FrostSlowPercent(int championLevel) {
    const float t = static_cast<float>(std::clamp(championLevel, 1, 18) - 1) /
                    17.0f;
    return 20.0f + 10.0f * t;
}

inline float EmpoweredFrostSlowPercent(int championLevel) {
    const float t = static_cast<float>(std::clamp(championLevel, 1, 18) - 1) /
                    17.0f;
    return 40.0f + 20.0f * t;
}

inline float VolleyRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 5> values = {
        60.0f, 95.0f, 130.0f, 165.0f, 200.0f,
    };
    return rank > 0
        ? values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)] +
              std::max(0.0f, bonusAttackDamage)
        : 0.0f;
}

inline float ArrowRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 3> values = {
        200.0f, 400.0f, 600.0f,
    };
    return rank > 0
        ? values[static_cast<std::size_t>(std::clamp(rank, 1, 3) - 1)] +
              1.20f * std::max(0.0f, abilityPower)
        : 0.0f;
}

inline float ArrowTravelSeconds(float distance) {
    const float clamped = std::max(0.0f, distance);
    const float accelerationSeconds =
        (kArrowMaximumSpeed - kArrowInitialSpeed) / kArrowAcceleration;
    const float accelerationDistance =
        kArrowInitialSpeed * accelerationSeconds +
        0.5f * kArrowAcceleration * accelerationSeconds * accelerationSeconds;
    float flight = 0.0f;
    if (clamped <= accelerationDistance) {
        flight = (-kArrowInitialSpeed +
                  std::sqrt(kArrowInitialSpeed * kArrowInitialSpeed +
                            2.0f * kArrowAcceleration * clamped)) /
                 kArrowAcceleration;
    } else {
        flight = accelerationSeconds +
                 (clamped - accelerationDistance) / kArrowMaximumSpeed;
    }
    return kArrowCastSeconds + std::max(0.0f, flight);
}

inline float ArrowStunSeconds(float distance) {
    const float t = std::clamp(
        std::max(0.0f, distance) / kArrowMaximumStunDistance,
        0.0f, 1.0f);
    return kArrowMinimumStunSeconds +
           (kArrowMaximumStunSeconds - kArrowMinimumStunSeconds) * t;
}

struct ArrowUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Weight = 0.0f;
    int Id = 0;
    int AlliedFollowup = 0;
    bool Primary = false;
    bool CrowdControlled = false;
    bool Dashing = false;
    bool Killable = false;
    bool Threat = false;
    bool Valid = true;
};

struct ArrowEvaluation {
    int FirstHitId = 0;
    int ExplosionHits = 0;
    int PriorityExplosionHits = 0;
    int AlliedFollowup = 0;
    float FirstHitDistance = FLT_MAX;
    float StunSeconds = 0.0f;
    float Score = -FLT_MAX;
    Vec3 FirstHitPosition = {};
    bool FirstHitPrimary = false;
    bool FirstHitKillable = false;
    bool FirstHitThreat = false;
    bool Valid = false;
};

inline ArrowEvaluation EvaluateArrowLine(
    const Vec3& source,
    const Vec3& aimDirection,
    const std::vector<ArrowUnit>& units,
    int primaryId = 0,
    float range = kArrowRange) {
    ArrowEvaluation result{};
    if (aimDirection.IsZero() || range <= 0.0f) return result;
    const Vec3 end = source + aimDirection * range;
    const ArrowUnit* first = nullptr;
    for (const auto& unit : units) {
        if (!unit.Valid || unit.Id == 0 || unit.Radius < 0.0f) continue;
        const auto projection = ProjectPointToSegment2D(
            unit.Position, source, end);
        if (projection.T <= 0.0001f || projection.T > 1.0f ||
            projection.Distance > unit.Radius + kArrowRadius) {
            continue;
        }
        const float along = source.Distance2D(projection.Closest);
        if (along >= result.FirstHitDistance) continue;
        result.FirstHitDistance = along;
        first = &unit;
    }
    if (!first) return result;

    result.FirstHitId = first->Id;
    result.FirstHitPosition = first->Position;
    result.FirstHitPrimary = first->Primary || first->Id == primaryId;
    result.FirstHitKillable = first->Killable;
    result.FirstHitThreat = first->Threat;
    result.AlliedFollowup = first->AlliedFollowup;
    result.StunSeconds = ArrowStunSeconds(result.FirstHitDistance);

    float score = first->Weight + result.StunSeconds * 1.6f;
    if (result.FirstHitPrimary) score += 6.0f;
    if (first->Killable) score += 4.0f;
    if (first->Threat) score += 3.0f;
    if (first->CrowdControlled) score += 0.8f;
    if (first->Dashing) score += 0.6f;
    score += static_cast<float>(std::min(first->AlliedFollowup, 3)) * 1.1f;

    for (const auto& unit : units) {
        if (!unit.Valid || unit.Id == 0 ||
            unit.Position.Distance2D(first->Position) >
                kArrowExplosionRadius + std::max(0.0f, unit.Radius)) {
            continue;
        }
        ++result.ExplosionHits;
        if (unit.Primary || unit.Threat || unit.Killable) {
            ++result.PriorityExplosionHits;
        }
        if (unit.Id != first->Id) score += 1.25f + unit.Weight * 0.35f;
    }
    if (result.ExplosionHits >= 3) score += 2.0f;
    result.Score = score;
    result.Valid = true;
    return result;
}

inline float ArrowPathAlignment(const Vec3& arrowDirection,
                                const Vec3& movementDirection) {
    if (arrowDirection.IsZero() || movementDirection.IsZero()) return 0.0f;
    return std::fabs(std::clamp(
        arrowDirection.Dot(movementDirection), -1.0f, 1.0f));
}

struct FocusContext {
    int FocusStacks = 0;
    int ExpectedFollowupAttacks = 0;
    float ManaAfterCast = 0.0f;
    bool CastReady = false;
    bool AlreadyActive = false;
    bool JustAttacked = false;
    bool ChampionTarget = false;
    bool EpicMonster = false;
    bool JungleTarget = false;
    bool WaveTarget = false;
    bool StructureTarget = false;
    bool TargetFrosted = false;
    bool TargetLeavingRange = false;
    bool TargetHasPerHitFlatReduction = false;
    bool LethalWindow = false;
};

inline bool ShouldActivateFocus(const FocusContext& context) {
    if (!context.CastReady || context.AlreadyActive ||
        context.FocusStacks < 4 || !context.JustAttacked ||
        context.ManaAfterCast < -0.5f) {
        return false;
    }
    if (!context.ChampionTarget && !context.EpicMonster &&
        !context.JungleTarget && !context.WaveTarget &&
        !context.StructureTarget) {
        return false;
    }
    if (context.TargetHasPerHitFlatReduction && !context.LethalWindow &&
        !context.EpicMonster && context.ExpectedFollowupAttacks < 4) {
        return false;
    }
    if (context.TargetLeavingRange && !context.TargetFrosted &&
        !context.LethalWindow && context.ExpectedFollowupAttacks < 2) {
        return false;
    }
    if (context.ChampionTarget && context.ExpectedFollowupAttacks < 2 &&
        !context.LethalWindow) {
        return false;
    }
    if (context.JungleTarget && !context.EpicMonster &&
        context.ExpectedFollowupAttacks < 3) {
        return false;
    }
    if (context.WaveTarget && context.ExpectedFollowupAttacks < 4) {
        return false;
    }
    return true;
}

enum class ScoutKind : std::uint8_t {
    Camp,
    River,
    Objective,
    GankRoute,
    LastSeenJungler,
    Brush,
};

struct ScoutLandmark {
    Vec3 Position = {};
    float Weight = 0.0f;
    int Id = 0;
    ScoutKind Kind = ScoutKind::Camp;
    bool RecentlyScouted = false;
    bool Priority = false;
    bool Valid = true;
};

struct ScoutEvaluation {
    int Covered = 0;
    int PriorityCovered = 0;
    int ObjectiveCovered = 0;
    int RecentRepeats = 0;
    float Score = -FLT_MAX;
    bool Valid = false;
};

inline bool HawkshotCoversPoint(const Vec3& source,
                               const Vec3& destination,
                               const Vec3& point,
                               float pathRadius = kHawkshotPathVisionRadius,
                               float destinationRadius =
                                   kHawkshotDestinationVisionRadius) {
    if (destination.IsZero() || point.IsZero()) return false;
    if (point.Distance2D(destination) <= destinationRadius) return true;
    return ProjectPointToSegment2D(point, source, destination).Distance <=
           pathRadius;
}

inline ScoutEvaluation EvaluateHawkshot(
    const Vec3& source,
    const Vec3& destination,
    const std::vector<ScoutLandmark>& landmarks) {
    ScoutEvaluation result{};
    if (destination.IsZero() ||
        source.Distance2D(destination) > kHawkshotRange + 1.0f) {
        return result;
    }
    float score = 0.0f;
    for (const auto& landmark : landmarks) {
        if (!landmark.Valid || landmark.Id == 0 ||
            !HawkshotCoversPoint(source, destination, landmark.Position)) {
            continue;
        }
        ++result.Covered;
        score += landmark.Weight;
        if (landmark.Priority) {
            ++result.PriorityCovered;
            score += 2.0f;
        }
        if (landmark.Kind == ScoutKind::Objective) {
            ++result.ObjectiveCovered;
            score += 1.5f;
        }
        if (landmark.RecentlyScouted) {
            ++result.RecentRepeats;
            score -= 2.5f;
        }
    }
    if (result.Covered >= 3) score += 1.5f;
    if (result.PriorityCovered > 0 && result.Covered >= 2) score += 1.0f;
    result.Score = score;
    result.Valid = result.Covered > 0;
    return result;
}

inline float HawkshotTravelSeconds(float distance) {
    return std::max(0.0f, distance) / kHawkshotSpeed;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Ashe::Geometry
