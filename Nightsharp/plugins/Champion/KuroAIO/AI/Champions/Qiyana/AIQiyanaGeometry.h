#pragma once

// Deterministic Qiyana mechanics. Runtime prediction, NavMesh terrain
// classification, mitigation and spell events stay in AIQiyanaController.h.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Qiyana::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::Rotate2D;

inline constexpr float kQSlashRange = 525.0f;
inline constexpr float kQMissileRange = 650.0f;
inline constexpr float kQMissileWidth = 100.0f;
inline constexpr float kQMissileSpeed = 1600.0f;
inline constexpr float kWDashRange = 300.0f;
inline constexpr float kWSearchRange = 1100.0f;
inline constexpr float kERange = 650.0f;
inline constexpr float kRRange = 950.0f;
inline constexpr float kRWidth = 220.0f;
inline constexpr float kRSpeed = 1000.0f;
inline constexpr float kRKnockbackDistance = 300.0f;

inline float ClampFinite(float value, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : minimum;
}

enum class Element : std::uint8_t {
    None,
    Rock,
    River,
    Brush,
};

inline bool IsEnchanted(Element element) { return element != Element::None; }

inline float PassiveRawDamage(int level, float bonusAttackDamage, float abilityPower) {
    const int clampedLevel = std::clamp(level, 1, 18);
    return 15.0f + 4.0f * static_cast<float>(clampedLevel - 1) +
           0.25f * std::max(0.0f, bonusAttackDamage) +
           0.30f * std::max(0.0f, abilityPower);
}

struct PassiveRecord {
    int TargetId = 0;
    Element LastElement = Element::None;
    int ExpireTick = 0;
    bool Confirmed = false;
};

inline bool PassiveAvailable(const PassiveRecord& record,
                             int targetId,
                             Element heldElement,
                             int nowTick) {
    if (targetId == 0) return false;
    if (!record.Confirmed || record.TargetId != targetId || nowTick >= record.ExpireTick) {
        return true;
    }
    return IsEnchanted(heldElement) && heldElement != record.LastElement;
}

inline float QBaseDamage(int rank) {
    static constexpr std::array<float, 6> values{ 0.0f, 40.0f, 70.0f, 100.0f, 130.0f, 160.0f };
    return RankValue(values, rank);
}

inline float QRawDamage(int rank, float bonusAttackDamage) {
    return QBaseDamage(rank) + 0.85f * std::max(0.0f, bonusAttackDamage);
}

inline float QRockRawDamage(int rank,
                            float bonusAttackDamage,
                            float targetHealthPercent) {
    const float ordinary = QRawDamage(rank, bonusAttackDamage);
    return targetHealthPercent < 50.0f ? ordinary * 1.60f : ordinary;
}

inline float QCollisionMultiplier(int bodyIndex) {
    return bodyIndex <= 0 ? 1.0f : 0.75f;
}

inline float QDamageAfterCollision(int rank,
                                   float bonusAttackDamage,
                                   Element element,
                                   float targetHealthPercent,
                                   int bodyIndex) {
    const float raw = element == Element::Rock
        ? QRockRawDamage(rank, bonusAttackDamage, targetHealthPercent)
        : QRawDamage(rank, bonusAttackDamage);
    return raw * QCollisionMultiplier(bodyIndex);
}

inline float WAttackSpeedPercent(int rank) {
    static constexpr std::array<float, 6> values{ 0.0f, 15.0f, 20.0f, 25.0f, 30.0f, 35.0f };
    return RankValue(values, rank);
}

inline float WMovementSpeedPercent(int rank) {
    static constexpr std::array<float, 6> values{ 0.0f, 3.0f, 5.0f, 7.0f, 9.0f, 11.0f };
    return RankValue(values, rank);
}

inline float WOnHitRawDamage(int rank,
                             float bonusAttackDamage,
                             float abilityPower) {
    static constexpr std::array<float, 6> base{ 0.0f, 8.0f, 16.0f, 24.0f, 32.0f, 40.0f };
    return RankValue(base, rank) + 0.20f * std::max(0.0f, bonusAttackDamage) +
           0.45f * std::max(0.0f, abilityPower);
}

inline float ERawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{ 0.0f, 50.0f, 90.0f, 130.0f, 170.0f, 210.0f };
    return RankValue(base, rank) + 0.50f * std::max(0.0f, bonusAttackDamage);
}

inline float RRawDamage(int rank,
                        float bonusAttackDamage,
                        float targetMaximumHealth) {
    static constexpr std::array<float, 4> base{ 0.0f, 100.0f, 200.0f, 300.0f };
    return RankValue(base, rank) + 1.25f * std::max(0.0f, bonusAttackDamage) +
           0.10f * std::max(0.0f, targetMaximumHealth);
}

inline float RMonsterCap(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> base{ 0.0f, 500.0f, 750.0f, 1000.0f };
    return RankValue(base, rank) + 1.25f * std::max(0.0f, bonusAttackDamage);
}

inline float RStunDuration(float travelledDistance) {
    const float fraction = ClampFinite(travelledDistance / kRRange, 0.0f, 1.0f);
    return 1.0f - 0.5f * fraction;
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Target = false;
};

inline bool LineContains(const Vec3& start,
                         const Vec3& end,
                         const Vec3& point,
                         float radius) {
    if (start.IsZero() || end.IsZero() || point.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(point, start, end);
    return projection.Distance <= std::max(0.0f, radius);
}

struct CollisionResult {
    bool TargetHit = false;
    int TargetBodyIndex = -1;
    int FirstBodyId = 0;
    std::vector<int> OrderedIds = {};
};

inline CollisionResult EvaluateQCollision(const Vec3& start,
                                          const Vec3& end,
                                          const std::vector<Body>& bodies,
                                          int targetId) {
    CollisionResult result{};
    struct Contact { float T; int Id; bool Target; };
    std::vector<Contact> contacts;
    contacts.reserve(bodies.size());
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, start, end);
        if (projection.Distance > kQMissileWidth * 0.5f + std::max(0.0f, body.Radius)) {
            continue;
        }
        contacts.push_back({ projection.T, body.Id, body.Id == targetId || body.Target });
    }
    std::stable_sort(contacts.begin(), contacts.end(), [](const Contact& left, const Contact& right) {
        return left.T < right.T;
    });
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        if (i == 0) result.FirstBodyId = contacts[i].Id;
        result.OrderedIds.push_back(contacts[i].Id);
        if (contacts[i].Target && !result.TargetHit) {
            result.TargetHit = true;
            result.TargetBodyIndex = static_cast<int>(i);
        }
    }
    return result;
}

inline Vec3 ClampDashToward(const Vec3& origin,
                            const Vec3& requested,
                            float maximumDistance) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    const float distance = std::min(origin.Distance2D(requested), std::max(0.0f, maximumDistance));
    return origin + direction * distance;
}

inline Vec3 EProjectedEndpoint(const Vec3& origin,
                               const Vec3& target,
                               float overshoot = 100.0f) {
    const Vec3 direction = Direction2D(origin, target);
    if (direction.IsZero()) return {};
    const float targetDistance = origin.Distance2D(target);
    const float travel = std::min(kERange, targetDistance + std::max(0.0f, overshoot));
    return origin + direction * travel;
}

struct MobilityContext {
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool EndpointUnderEnemyTurret = false;
    bool StartingUnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool ExitAvailable = false;
    bool Lethal = false;
    bool Fleeing = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool MobilitySafe(const MobilityContext& context) {
    if (!context.EndpointValid || !context.EndpointWalkable) return false;
    if (context.EndpointUnderEnemyTurret && !context.StartingUnderEnemyTurret &&
        !(context.Lethal && context.ExitAvailable)) return false;
    if ((context.PointClickThreat || context.DashHazard) && !context.Lethal) return false;
    if (context.NearbyEnemies > std::max(0, context.MaximumEnemies) &&
        !(context.Lethal || context.Fleeing)) return false;
    return context.ExitAvailable || context.Lethal || context.Fleeing;
}

struct TerrainCandidate {
    Vec3 Position = {};
    Element Kind = Element::None;
    MobilityContext Safety = {};
    float CursorDistance = FLT_MAX;
    float TargetDistance = FLT_MAX;
};

inline Element DesiredElement(float targetHealthPercent,
                              bool defensive,
                              bool needsCatch,
                              bool passiveRefresh,
                              Element current) {
    if (defensive) return Element::Brush;
    if (targetHealthPercent < 50.0f) return Element::Rock;
    if (needsCatch) return Element::River;
    if (passiveRefresh) {
        if (current != Element::Brush) return Element::Brush;
        if (current != Element::River) return Element::River;
        return Element::Rock;
    }
    return Element::Brush;
}

inline TerrainCandidate SelectTerrainCandidate(
    const std::vector<TerrainCandidate>& candidates,
    Element desired,
    bool defensive) {
    TerrainCandidate best{};
    float bestScore = -FLT_MAX;
    for (const TerrainCandidate& candidate : candidates) {
        if (candidate.Kind == Element::None || candidate.Position.IsZero() ||
            !MobilitySafe(candidate.Safety)) continue;
        float score = candidate.Kind == desired ? 700.0f : 0.0f;
        score -= candidate.CursorDistance * (defensive ? 0.40f : 0.12f);
        score -= candidate.TargetDistance * (defensive ? 0.03f : 0.10f);
        score -= static_cast<float>(candidate.Safety.NearbyEnemies) * 180.0f;
        if (score > bestScore) {
            bestScore = score;
            best = candidate;
        }
    }
    return best;
}

struct TerrainZone {
    Vec3 Center = {};
    float Radius = 0.0f;
    Element Kind = Element::None;
};

struct REvaluation {
    bool InitialHit = false;
    bool TerrainDetonation = false;
    Element Terrain = Element::None;
    Vec3 KnockbackEnd = {};
    Vec3 TerrainContact = {};
    int TargetHits = 0;
};

inline REvaluation EvaluateRPath(const Vec3& source,
                                 const Vec3& castEnd,
                                 const Body& target,
                                 const std::vector<TerrainZone>& zones) {
    REvaluation result{};
    if (!target.Valid || source.IsZero() || castEnd.IsZero() || target.Position.IsZero()) {
        return result;
    }
    const Vec3 direction = Direction2D(source, castEnd);
    if (direction.IsZero()) return result;
    const Vec3 end = source + direction * kRRange;
    result.InitialHit = LineContains(source, end, target.Position,
                                     kRWidth * 0.5f + target.Radius);
    if (!result.InitialHit) return result;
    result.TargetHits = 1;
    result.KnockbackEnd = target.Position + direction * kRKnockbackDistance;
    float bestT = FLT_MAX;
    for (const TerrainZone& zone : zones) {
        if (zone.Kind == Element::None || zone.Center.IsZero()) continue;
        const auto push = ProjectPointToSegment2D(zone.Center, target.Position,
                                                  result.KnockbackEnd);
        const auto wave = ProjectPointToSegment2D(zone.Center, source, end);
        const bool wallContact = zone.Kind == Element::Rock &&
            push.Distance <= std::max(0.0f, zone.Radius) + target.Radius;
        const bool crossedRiverOrBrush = zone.Kind != Element::Rock &&
            wave.Distance <= std::max(0.0f, zone.Radius) + kRWidth * 0.5f;
        const float t = wallContact ? push.T : wave.T;
        if ((wallContact || crossedRiverOrBrush) && t < bestT) {
            bestT = t;
            result.TerrainDetonation = true;
            result.Terrain = zone.Kind;
            result.TerrainContact = wallContact ? push.Closest : wave.Closest;
        }
    }
    return result;
}

struct QCastContext {
    bool Ready = false;
    bool TargetValid = false;
    bool Reachable = false;
    bool PredictionAccepted = false;
    bool ProjectileWall = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool DuringSupportedEQ = false;
    bool GrassStealthValuable = false;
    bool Defensive = false;
};

inline bool MayCastQ(const QCastContext& context) {
    if (!context.Ready || !context.TargetValid || !context.Reachable ||
        !context.PredictionAccepted || context.ProjectileWall) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.DuringSupportedEQ) return false;
    if (context.GrassStealthValuable && !context.Lethal && !context.Defensive) return false;
    return true;
}

struct RCastContext {
    bool Ready = false;
    bool TargetValid = false;
    bool InitialHit = false;
    bool TerrainDetonation = false;
    bool ProjectileWall = false;
    bool Lethal = false;
    bool DefensivePeel = false;
    bool ObjectiveFight = false;
    int ChampionHits = 0;
    int MinimumHits = 2;
};

inline bool MayCastR(const RCastContext& context) {
    if (!context.Ready || !context.TargetValid || !context.InitialHit ||
        !context.TerrainDetonation || context.ProjectileWall) return false;
    return context.Lethal || context.DefensivePeel || context.ObjectiveFight ||
           context.ChampionHits >= std::max(1, context.MinimumHits);
}

struct ResourceContext {
    float CurrentMana = 0.0f;
    float SequenceCost = 0.0f;
    float ExitReserve = 0.0f;
    bool Lethal = false;
};

inline bool HasSequenceMana(const ResourceContext& context) {
    const float reserve = context.Lethal ? 0.0f : std::max(0.0f, context.ExitReserve);
    return std::max(0.0f, context.CurrentMana) + 0.001f >=
           std::max(0.0f, context.SequenceCost) + reserve;
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Qiyana::Geometry
