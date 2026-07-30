#pragma once

// Pure Naafiri 26.15 mechanics. Runtime prediction, event reconciliation,
// NavMesh queries and player-input ownership stay in AINaafiriController.h.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Naafiri::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQWidth = 150.0f;
inline constexpr float kQSpeed = 1700.0f;
inline constexpr float kQCastDelay = 0.25f;
inline constexpr int kQRecastLockoutMs = 750;
inline constexpr int kQRecastWindowMs = 4000;
inline constexpr int kQBleedDurationMs = 5000;
inline constexpr float kWRange = 900.0f;
inline constexpr float kWChannelSeconds = 0.75f;
inline constexpr float kWDashSpeed = 1800.0f;
inline constexpr float kWDashHalfWidth = 70.0f;
inline constexpr int kWTakedownWindowMs = 7000;
inline constexpr int kWRecastWindowMs = 12000;
inline constexpr int kWVisionDurationMs = 4000;
inline constexpr int kWShieldDurationMs = 3000;
inline constexpr float kEVirtualRange = 450.0f;
inline constexpr float kEMinDash = 250.0f;
inline constexpr float kEDashSpeed = 900.0f;
inline constexpr float kEPathHalfWidth = 50.0f;
inline constexpr float kESecondRadius = 230.0f;
inline constexpr int kRDurationMs = 5000;
inline constexpr int kRUntargetableMs = 1000;
inline constexpr int kRAdditionalPackmates = 2;

inline float Clamp01(float value) {
    return std::isfinite(value) ? std::clamp(value, 0.0f, 1.0f) : 0.0f;
}

inline int PackmateCap(int level) {
    const int clamped = std::clamp(level, 1, 18);
    return 2 + (clamped >= 9 ? 1 : 0) + (clamped >= 12 ? 1 : 0) +
           (clamped >= 15 ? 1 : 0);
}

inline float PackmateSpawnCooldownSeconds(int level) {
    const int clamped = std::clamp(level, 1, 18);
    if (clamped >= 15) return 10.0f;
    if (clamped >= 12) return 15.0f;
    if (clamped >= 9) return 20.0f;
    if (clamped >= 6) return 25.0f;
    return 30.0f;
}

inline float ReducePackmateCooldown(float remainingSeconds,
                                    int abilityHits,
                                    int kills) {
    const float reduction = 4.0f * static_cast<float>(std::max(0, abilityHits)) +
                            static_cast<float>(std::max(0, kills));
    return std::max(0.0f, std::max(0.0f, remainingSeconds) - reduction);
}

inline int PackmatesAfterR(int currentPackmates, int level) {
    return std::min(PackmateCap(level) + kRAdditionalPackmates,
                    std::max(0, currentPackmates) + kRAdditionalPackmates);
}

inline float PackmateRawDamage(int level,
                               float bonusAttackDamage,
                               bool frenzy = false,
                               bool nonChampion = false,
                               bool areaDamage = false) {
    const int clamped = std::clamp(level, 1, 18);
    float damage = 10.0f + 10.0f * static_cast<float>(clamped - 1) / 17.0f +
                   0.04f * std::max(0.0f, bonusAttackDamage);
    if (frenzy) damage *= 1.30f;
    if (nonChampion) damage *= 0.50f;
    if (areaDamage) {
        const int aoeLevel = std::min(clamped, 16);
        damage *= 0.76f - 0.015f * static_cast<float>(aoeLevel - 1);
    }
    return damage;
}

inline float QFirstRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f,
    };
    return RankValue(base, rank) + 0.20f * std::max(0.0f, bonusAttackDamage);
}

inline float QFullBleedRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 35.0f, 60.0f, 85.0f, 110.0f, 135.0f,
    };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, bonusAttackDamage);
}

inline float QBleedRemaining(float fullBleedDamage,
                             int remainingMs,
                             int durationMs = kQBleedDurationMs) {
    if (durationMs <= 0) return 0.0f;
    const float fraction = Clamp01(
        static_cast<float>(remainingMs) / static_cast<float>(durationMs));
    // The server ticks every 500 ms. Rounding upward is conservative for
    // recast damage because an in-flight tick may still be owed.
    const float ticks = std::ceil(fraction * 10.0f);
    return std::max(0.0f, fullBleedDamage) * ticks / 10.0f;
}

inline float QSecondBonusRawDamage(int rank,
                                   float bonusAttackDamage,
                                   float missingHealthFraction) {
    static constexpr std::array<float, 6> base{
        0.0f, 30.0f, 42.5f, 55.0f, 67.5f, 80.0f,
    };
    const float missing = Clamp01(missingHealthFraction);
    const float baseDamage = RankValue(base, rank) * (1.0f + missing);
    const float ratio = 0.40f + missing;
    return baseDamage + ratio * std::max(0.0f, bonusAttackDamage);
}

inline float QSecondRawDamage(int rank,
                              float bonusAttackDamage,
                              float missingHealthFraction,
                              float remainingBleedDamage) {
    return std::max(0.0f, remainingBleedDamage) +
           QSecondBonusRawDamage(rank, bonusAttackDamage,
                                 missingHealthFraction);
}

inline float QSecondHeal(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 45.0f, 60.0f, 75.0f, 90.0f, 105.0f,
    };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, bonusAttackDamage);
}

inline bool QRecastAvailable(int firstCastTick, int nowTick) {
    const int elapsed = nowTick - firstCastTick;
    return firstCastTick > 0 && elapsed >= kQRecastLockoutMs &&
           elapsed <= kQRecastWindowMs;
}

struct LineBody {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Champion = false;
    bool LargeMonster = false;
    bool IntendedTarget = false;
};

struct QPathResult {
    bool IntendedTargetHit = false;
    int IntendedBodyIndex = -1;
    int FirstPackmateTargetId = 0;
    std::vector<int> OrderedHitIds = {};
};

inline QPathResult EvaluateQPath(const Vec3& start,
                                 const Vec3& end,
                                 const std::vector<LineBody>& bodies,
                                 int intendedTargetId) {
    QPathResult result{};
    struct Contact {
        float T = FLT_MAX;
        int Id = 0;
        bool ChampionOrLarge = false;
        bool Intended = false;
    };
    std::vector<Contact> contacts;
    contacts.reserve(bodies.size());
    for (const LineBody& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, start, end);
        if (projection.Distance > kQWidth * 0.5f + std::max(0.0f, body.Radius)) {
            continue;
        }
        contacts.push_back({ projection.T, body.Id,
                             body.Champion || body.LargeMonster,
                             body.Id == intendedTargetId || body.IntendedTarget });
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const Contact& left, const Contact& right) { return left.T < right.T; });
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        const Contact& contact = contacts[i];
        result.OrderedHitIds.push_back(contact.Id);
        if (result.FirstPackmateTargetId == 0 && contact.ChampionOrLarge) {
            result.FirstPackmateTargetId = contact.Id;
        }
        if (contact.Intended && !result.IntendedTargetHit) {
            result.IntendedTargetHit = true;
            result.IntendedBodyIndex = static_cast<int>(i);
        }
    }
    return result;
}

struct PursuitPathResult {
    bool ReachesTarget = false;
    int FirstChampionId = 0;
    float FirstContactT = 1.0f;
    std::vector<int> OrderedChampionIds = {};
};

inline PursuitPathResult EvaluatePursuitPath(
    const Vec3& start,
    const Vec3& targetPosition,
    const std::vector<LineBody>& champions,
    int targetId) {
    PursuitPathResult result{};
    struct Contact { float T; int Id; };
    std::vector<Contact> contacts;
    contacts.reserve(champions.size());
    for (const LineBody& body : champions) {
        if (!body.Valid || !body.Champion || body.Id == 0 ||
            body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(
            body.Position, start, targetPosition);
        if (projection.Distance <= kWDashHalfWidth +
                                   std::max(0.0f, body.Radius)) {
            contacts.push_back({ projection.T, body.Id });
        }
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const Contact& left, const Contact& right) { return left.T < right.T; });
    for (const Contact& contact : contacts) {
        result.OrderedChampionIds.push_back(contact.Id);
    }
    if (!contacts.empty()) {
        result.FirstChampionId = contacts.front().Id;
        result.FirstContactT = contacts.front().T;
        result.ReachesTarget = contacts.front().Id == targetId;
    }
    return result;
}

inline float WArrivalSeconds(float distance) {
    return kWChannelSeconds + std::max(0.0f, distance) / kWDashSpeed;
}

inline float WRawDamage(int rank, float bonusAttackDamage, int packmateCount) {
    static constexpr std::array<float, 4> base{
        0.0f, 125.0f, 200.0f, 275.0f,
    };
    const float own = RankValue(base, rank) +
                      std::max(0.0f, bonusAttackDamage);
    return own * (1.0f + 0.10f * static_cast<float>(
        std::max(0, packmateCount)));
}

inline float WShield(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> base{
        0.0f, 100.0f, 150.0f, 200.0f,
    };
    return RankValue(base, rank) + 1.50f *
           std::max(0.0f, bonusAttackDamage);
}

inline float WArmorShredPercent(int championLevel) {
    return 6.0f + 2.0f * static_cast<float>(
        std::clamp(championLevel, 1, 18) - 1);
}

struct PursuitContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetVisible = false;
    bool TargetSpellShielded = false;
    bool PathReachesTarget = false;
    bool ChannelInterruptThreat = false;
    bool LandingWalkable = false;
    bool LandingUnderEnemyTurret = false;
    bool StartedUnderEnemyTurret = false;
    bool Lethal = false;
    bool HasExit = false;
    int EnemiesAtLanding = 0;
    int MaximumEnemies = 2;
};

inline bool PursuitSafe(const PursuitContext& context) {
    if (!context.Ready || !context.TargetValid || !context.TargetVisible ||
        context.TargetSpellShielded || !context.PathReachesTarget ||
        !context.LandingWalkable) return false;
    if (context.ChannelInterruptThreat && !context.Lethal) return false;
    if (context.LandingUnderEnemyTurret && !context.StartedUnderEnemyTurret &&
        !(context.Lethal && context.HasExit)) return false;
    if (context.EnemiesAtLanding > std::max(0, context.MaximumEnemies) &&
        !context.Lethal) return false;
    return context.HasExit || context.Lethal;
}

inline Vec3 EEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    const float requestedDistance = origin.Distance2D(requested);
    const float distance = std::clamp(requestedDistance,
                                      kEMinDash, kEVirtualRange);
    return origin + direction * distance;
}

struct EHitResult {
    bool FirstSlash = false;
    bool SecondSlash = false;
    bool FullHit = false;
};

inline EHitResult EvaluateEHit(const Vec3& origin,
                               const Vec3& endpoint,
                               const Vec3& target,
                               float targetRadius = 0.0f) {
    EHitResult result{};
    if (origin.IsZero() || endpoint.IsZero() || target.IsZero()) return result;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    result.FirstSlash = projection.Distance <= kEPathHalfWidth +
                                              std::max(0.0f, targetRadius);
    result.SecondSlash = endpoint.Distance2D(target) <= kESecondRadius +
                                                       std::max(0.0f, targetRadius);
    result.FullHit = result.FirstSlash && result.SecondSlash;
    return result;
}

inline float EFirstRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 15.0f, 25.0f, 35.0f, 45.0f, 55.0f,
    };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, bonusAttackDamage);
}

inline float ESecondRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 60.0f, 85.0f, 110.0f, 135.0f, 160.0f,
    };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, bonusAttackDamage);
}

inline float ERawDamage(int rank,
                        float bonusAttackDamage,
                        const EHitResult& hit) {
    return (hit.FirstSlash ? EFirstRawDamage(rank, bonusAttackDamage) : 0.0f) +
           (hit.SecondSlash ? ESecondRawDamage(rank, bonusAttackDamage) : 0.0f);
}

struct DashContext {
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool EndpointUnderEnemyTurret = false;
    bool StartingUnderEnemyTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool Lethal = false;
    bool Fleeing = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool DashSafe(const DashContext& context) {
    if (!context.EndpointValid || !context.EndpointWalkable) return false;
    if (context.EndpointUnderEnemyTurret && !context.StartingUnderEnemyTurret &&
        !context.Lethal) return false;
    if ((context.PointClickThreat || context.DashHazard) &&
        !(context.Lethal || context.Fleeing)) return false;
    if (context.NearbyEnemies > std::max(0, context.MaximumEnemies) &&
        !(context.Lethal || context.Fleeing)) return false;
    return true;
}

inline float RMoveSpeedPercent(int rank) {
    static constexpr std::array<float, 6> values{
        0.0f, 20.0f, 22.5f, 25.0f, 27.5f, 30.0f,
    };
    return RankValue(values, rank);
}

inline float RBonusAttackDamage(float bonusAttackDamage) {
    return 0.20f * std::max(0.0f, bonusAttackDamage);
}

struct RContext {
    bool Ready = false;
    bool AlreadyActive = false;
    bool IncomingTargetedThreat = false;
    bool NeedsPackReset = false;
    bool NeedsPackmates = false;
    bool NeedsChase = false;
    bool AllIn = false;
    bool Fleeing = false;
    bool LethalFollowup = false;
    int CurrentPackmates = 0;
    int OrdinaryPackCap = 2;
};

inline bool ShouldCastR(const RContext& context) {
    if (!context.Ready || context.AlreadyActive) return false;
    if (context.IncomingTargetedThreat || context.Fleeing) return true;
    if (context.LethalFollowup) return true;
    if (!context.AllIn) return false;
    return context.NeedsPackReset || context.NeedsPackmates ||
           context.NeedsChase ||
           context.CurrentPackmates < context.OrdinaryPackCap;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Naafiri::Geometry
