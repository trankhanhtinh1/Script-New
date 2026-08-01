#pragma once

// Deterministic Lee Sin mechanics. Runtime prediction, collision queries,
// NavMesh safety and spell/buff reconciliation stay in AILeeSinController.h.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::LeeSin::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kMaximumEnergy = 200.0f;
inline constexpr float kQ1Range = 1200.0f;
inline constexpr float kQ1Width = 60.0f;
inline constexpr float kQ1Speed = 1800.0f;
inline constexpr float kQ2Range = 1300.0f;
inline constexpr float kWRange = 700.0f;
inline constexpr float kERadius = 450.0f;
inline constexpr float kE2Radius = 500.0f;
inline constexpr float kRRange = 375.0f;
inline constexpr float kRKnockbackDistance = 1200.0f;
inline constexpr float kRSecondaryHalfWidth = 100.0f;
inline constexpr int kRecastWindowMs = 3000;

inline float ClampFinite(float value, float minimum, float maximum) {
    return std::isfinite(value) ? std::clamp(value, minimum, maximum) : minimum;
}

inline bool HasEnergy(float current, float cost, float reserve = 0.0f) {
    if (!std::isfinite(current) || !std::isfinite(cost) || !std::isfinite(reserve)) {
        return false;
    }
    return current + 0.5f >= std::max(0.0f, cost) + std::max(0.0f, reserve);
}

inline float FlurryEnergyRestore(int attackIndex) {
    return attackIndex <= 0 ? 20.0f : attackIndex == 1 ? 10.0f : 0.0f;
}

inline float QBaseDamage(int rank) {
    static constexpr std::array<float, 6> values{
        0.0f, 55.0f, 80.0f, 105.0f, 130.0f, 155.0f
    };
    return RankValue(values, rank);
}

inline float Q1RawDamage(int rank, float bonusAttackDamage) {
    return QBaseDamage(rank) + 1.15f * std::max(0.0f, bonusAttackDamage);
}

inline float Q2MissingHealthMultiplier(float targetHealth,
                                       float targetMaximumHealth) {
    const float maximum = std::max(1.0f, targetMaximumHealth);
    const float missingFraction = ClampFinite(
        (maximum - std::max(0.0f, targetHealth)) / maximum, 0.0f, 1.0f);
    return 1.0f + missingFraction;
}

inline float Q2RawDamage(int rank,
                         float bonusAttackDamage,
                         float targetHealth,
                         float targetMaximumHealth) {
    return Q1RawDamage(rank, bonusAttackDamage) *
           Q2MissingHealthMultiplier(targetHealth, targetMaximumHealth);
}

inline float WShield(int rank, float abilityPower) {
    static constexpr std::array<float, 6> values{
        0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f
    };
    return RankValue(values, rank) + 0.80f * std::max(0.0f, abilityPower);
}

inline float ERawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> values{
        0.0f, 35.0f, 60.0f, 85.0f, 110.0f, 135.0f
    };
    return RankValue(values, rank) + std::max(0.0f, bonusAttackDamage);
}

inline float ESlowPercent(int rank) {
    static constexpr std::array<float, 6> values{
        0.0f, 20.0f, 30.0f, 40.0f, 50.0f, 60.0f
    };
    return RankValue(values, rank);
}

inline float RRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> values{
        0.0f, 175.0f, 400.0f, 625.0f
    };
    return RankValue(values, rank) + 2.0f * std::max(0.0f, bonusAttackDamage);
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Primary = false;
};

struct SonicCollision {
    bool TargetHit = false;
    bool TargetFirst = false;
    int FirstBodyId = 0;
    int TargetBodyIndex = -1;
};

inline SonicCollision EvaluateSonicCollision(const Vec3& source,
                                             const Vec3& end,
                                             const std::vector<Body>& bodies,
                                             int targetId) {
    SonicCollision result{};
    struct Contact { float T; int Id; bool Target; };
    std::vector<Contact> contacts;
    contacts.reserve(bodies.size());
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, source, end);
        if (projection.T < 0.0f || projection.T > 1.0f ||
            projection.Distance > kQ1Width * 0.5f +
                std::clamp(body.Radius, 0.0f, 150.0f)) {
            continue;
        }
        contacts.push_back({ projection.T, body.Id,
                             body.Primary || body.Id == targetId });
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const Contact& left, const Contact& right) { return left.T < right.T; });
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        if (i == 0) result.FirstBodyId = contacts[i].Id;
        if (contacts[i].Target && !result.TargetHit) {
            result.TargetHit = true;
            result.TargetBodyIndex = static_cast<int>(i);
        }
    }
    result.TargetFirst = result.TargetHit && result.TargetBodyIndex == 0;
    return result;
}

struct EndpointSafety {
    bool Valid = false;
    bool Walkable = false;
    bool UnderEnemyTurret = false;
    bool StartingUnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    int NearbyEnemies = 0;
    int NearbyAllies = 0;
    int MaximumEnemies = 2;
    bool Fleeing = false;
    bool Lethal = false;
};

inline bool EndpointSafe(const EndpointSafety& safety) {
    if (!safety.Valid || !safety.Walkable) return false;
    if (safety.UnderEnemyTurret && !safety.StartingUnderEnemyTurret &&
        !(safety.Lethal && safety.NearbyAllies > 0)) return false;
    if ((safety.PointClickThreat || safety.DashHazard) && !safety.Lethal) return false;
    if (safety.NearbyEnemies > std::max(0, safety.MaximumEnemies) &&
        !(safety.Fleeing && safety.NearbyEnemies < 4) && !safety.Lethal) return false;
    return safety.Fleeing || safety.Lethal || safety.NearbyEnemies <= safety.NearbyAllies + 1;
}

struct SafeguardCandidate {
    Vec3 Position = {};
    int NetworkId = 0;
    bool TargetValid = false;
    bool TargetVisible = false;
    bool Targetable = false;
    bool IsWard = false;
    bool IsChampion = false;
    float CursorDistance = FLT_MAX;
    float RouteGain = -FLT_MAX;
    EndpointSafety Safety = {};
};

inline bool MaySafeguard(const SafeguardCandidate& candidate,
                         float distance,
                         bool firstCast,
                         bool alreadyDashing,
                         bool allowWardHop,
                         bool fleeOrExplicitCombo) {
    if (!firstCast || alreadyDashing || !candidate.TargetValid ||
        !candidate.TargetVisible || !candidate.Targetable ||
        candidate.NetworkId == 0 || candidate.Position.IsZero() ||
        distance > kWRange + 15.0f || !EndpointSafe(candidate.Safety)) {
        return false;
    }
    if (candidate.IsWard && (!allowWardHop || !fleeOrExplicitCombo)) return false;
    return candidate.IsWard || candidate.IsChampion || fleeOrExplicitCombo;
}

inline SafeguardCandidate SelectSafeguard(
    const std::vector<SafeguardCandidate>& candidates,
    bool preferCursor,
    bool requirePositiveGain) {
    SafeguardCandidate best{};
    float bestScore = -FLT_MAX;
    for (const auto& candidate : candidates) {
        if (!candidate.TargetValid || !EndpointSafe(candidate.Safety) ||
            (requirePositiveGain && candidate.RouteGain <= 0.0f)) continue;
        const float score = candidate.RouteGain * 3.0f -
            (preferCursor ? candidate.CursorDistance * 0.02f : 0.0f) +
            (candidate.IsChampion ? 28.0f : 0.0f) -
            (candidate.IsWard ? 8.0f : 0.0f);
        if (score > bestScore) {
            best = candidate;
            bestScore = score;
        }
    }
    return best;
}

inline bool TempestHits(const Vec3& source,
                        const Vec3& target,
                        float targetRadius) {
    return !source.IsZero() && !target.IsZero() &&
           source.Distance2D(target) <= kERadius +
               std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool MayCripple(bool secondCast,
                       bool markConfirmed,
                       int markedEnemies,
                       bool targetEscaping,
                       bool defensive) {
    return secondCast && markConfirmed && markedEnemies > 0 &&
           (targetEscaping || defensive || markedEnemies >= 2);
}

inline Vec3 KickEndpoint(const Vec3& source,
                         const Vec3& primaryPosition,
                         float distance = kRKnockbackDistance) {
    const Vec3 direction = Direction2D(source, primaryPosition);
    return direction.IsZero() ? Vec3{} :
        primaryPosition + direction * std::max(0.0f, distance);
}

inline Vec3 BehindPosition(const Vec3& source,
                           const Vec3& primaryPosition,
                           const Vec3& desiredKickDestination,
                           float standDistance = 190.0f) {
    const Vec3 kickDirection = Direction2D(primaryPosition, desiredKickDestination);
    if (kickDirection.IsZero()) return {};
    Vec3 result = primaryPosition - kickDirection * std::max(0.0f, standDistance);
    result.y = source.y;
    return result;
}

struct KickHit {
    bool Hits = false;
    float Along = 0.0f;
    float Lateral = FLT_MAX;
};

inline KickHit EvaluateKickHit(const Vec3& primaryPosition,
                               const Vec3& kickEndpoint,
                               float primaryRadius,
                               const Vec3& secondaryPosition,
                               float secondaryRadius) {
    if (primaryPosition.IsZero() || kickEndpoint.IsZero() ||
        secondaryPosition.IsZero()) return {};
    const auto projection = ProjectPointToSegment2D(
        secondaryPosition, primaryPosition, kickEndpoint);
    const float width = kRSecondaryHalfWidth +
        std::clamp(primaryRadius, 0.0f, 150.0f) * 0.35f +
        std::clamp(secondaryRadius, 0.0f, 150.0f);
    return { projection.T >= 0.0f && projection.T <= 1.0f &&
             projection.Distance <= width,
             projection.T * primaryPosition.Distance2D(kickEndpoint),
             projection.Distance };
}

struct KickPlan {
    Vec3 Endpoint = {};
    Vec3 Behind = {};
    int SecondaryHits = 0;
    int PriorityHits = 0;
    float TeamwardGain = 0.0f;
    float Score = -FLT_MAX;
};

inline float TowardPointGain(const Vec3& before,
                             const Vec3& after,
                             const Vec3& desiredPoint) {
    if (before.IsZero() || after.IsZero() || desiredPoint.IsZero()) return 0.0f;
    return before.Distance2D(desiredPoint) - after.Distance2D(desiredPoint);
}

inline KickPlan EvaluateKickPlan(const Vec3& source,
                                 const Body& primary,
                                 const std::vector<Body>& enemies,
                                 const Vec3& desiredDestination) {
    KickPlan plan{};
    if (!primary.Valid || primary.Position.IsZero()) return plan;
    plan.Endpoint = KickEndpoint(source, primary.Position);
    plan.Behind = BehindPosition(source, primary.Position, desiredDestination);
    if (plan.Endpoint.IsZero()) return plan;
    for (const Body& enemy : enemies) {
        if (!enemy.Valid || enemy.Id == primary.Id || enemy.Position.IsZero()) continue;
        const KickHit hit = EvaluateKickHit(primary.Position, plan.Endpoint,
                                            primary.Radius, enemy.Position,
                                            enemy.Radius);
        if (!hit.Hits) continue;
        ++plan.SecondaryHits;
        if (enemy.Primary) ++plan.PriorityHits;
    }
    plan.TeamwardGain = TowardPointGain(primary.Position, plan.Endpoint,
                                        desiredDestination);
    plan.Score = static_cast<float>(plan.SecondaryHits) * 120.0f +
                 static_cast<float>(plan.PriorityHits) * 180.0f +
                 plan.TeamwardGain * 0.35f;
    return plan;
}

inline bool MayTakeResonatingStrike(bool secondCast,
                                    bool markConfirmed,
                                    bool targetInRange,
                                    bool endpointSafe,
                                    bool lethal,
                                    bool exitAvailable,
                                    int nearbyEnemies,
                                    int maximumEnemies) {
    if (!secondCast || !markConfirmed || !targetInRange || !endpointSafe) return false;
    if (nearbyEnemies > std::max(0, maximumEnemies) && !lethal) return false;
    return lethal || exitAvailable || nearbyEnemies <= 1;
}

} // namespace Plugins::KuroAIO::AI::Controllers::LeeSin::Geometry
