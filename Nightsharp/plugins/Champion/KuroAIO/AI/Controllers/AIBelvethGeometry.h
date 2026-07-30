#pragma once

// Deterministic Bel'Veth mechanics and one-trick policy. Runtime prediction,
// NavMesh tracing, HUD-buff observation and casts live in AIBelvethController;
// this file owns the four independent Q directions, AA-reset economics, W
// direction refresh, E's forced lowest-health target, R coral/global-form
// decision and live 26.15 damage data so each dangerous edge is testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Belveth::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQOpenDashDistance = 400.0f;
inline constexpr float kQTrueFormWallDistance = 625.0f;
inline constexpr float kQHalfWidth = 100.0f;
inline constexpr float kWGameplayRange = 660.0f;
inline constexpr float kWHalfWidth = 100.0f;
inline constexpr float kWCastSeconds = 0.50f;
inline constexpr float kERadius = 500.0f;
inline constexpr float kEDurationSeconds = 1.50f;
inline constexpr float kEEarlyCancelSeconds = 0.75f;
inline constexpr float kRRadius = 500.0f;
inline constexpr float kRCoralSeconds = 15.0f;
inline constexpr float kRShortFormSeconds = 45.0f;
inline constexpr float kRLongFormSeconds = 90.0f;
inline constexpr float kRChannelSeconds = 1.0f;
inline constexpr float kRPostLockSeconds = 0.25f;
inline constexpr float kPassiveSheenDurationSeconds = 3.0f;
inline constexpr float kRPassiveStackDurationSeconds = 5.0f;
inline constexpr int kRMaximumEpicProcs = 8;

// Q arrows are map-fixed diagonal quadrants. A cast is still aimed precisely
// inside its 90-degree sector; aiming near a sector boundary is how an OTP can
// spend two different arrows in almost the same chase/escape direction.
enum class Quadrant : int {
    NorthEast = 0,
    NorthWest = 1,
    SouthWest = 2,
    SouthEast = 3,
    Invalid = -1,
};

inline int QuadrantIndex(Quadrant quadrant) {
    const int index = static_cast<int>(quadrant);
    return index >= 0 && index < 4 ? index : -1;
}

inline std::uint8_t QuadrantMask(Quadrant quadrant) {
    const int index = QuadrantIndex(quadrant);
    return index >= 0 ? static_cast<std::uint8_t>(1u << index) : 0u;
}

inline Quadrant QuadrantForDirection(const Vec3& direction,
                                     bool positiveBoundary = true) {
    if (direction.IsZero()) return Quadrant::Invalid;
    const bool east = direction.x > 0.0f ||
        (std::fabs(direction.x) <= 0.0001f && positiveBoundary);
    const bool north = direction.z > 0.0f ||
        (std::fabs(direction.z) <= 0.0001f && positiveBoundary);
    if (east && north) return Quadrant::NorthEast;
    if (!east && north) return Quadrant::NorthWest;
    if (!east && !north) return Quadrant::SouthWest;
    return Quadrant::SouthEast;
}

inline Quadrant QuadrantForPoints(const Vec3& origin,
                                  const Vec3& destination,
                                  bool positiveBoundary = true) {
    return QuadrantForDirection(
        Direction2D(origin, destination), positiveBoundary);
}

inline Vec3 QuadrantCenter(Quadrant quadrant) {
    constexpr float diagonal = 0.7071067811865475f;
    switch (quadrant) {
    case Quadrant::NorthEast: return { diagonal, 0.0f, diagonal };
    case Quadrant::NorthWest: return { -diagonal, 0.0f, diagonal };
    case Quadrant::SouthWest: return { -diagonal, 0.0f, -diagonal };
    case Quadrant::SouthEast: return { diagonal, 0.0f, -diagonal };
    default: return {};
    }
}

inline bool DirectionInsideQuadrant(const Vec3& direction,
                                    Quadrant quadrant,
                                    float boundaryTolerance = 0.001f) {
    if (direction.IsZero() || QuadrantIndex(quadrant) < 0) return false;
    const float x = direction.x;
    const float z = direction.z;
    switch (quadrant) {
    case Quadrant::NorthEast:
        return x >= -boundaryTolerance && z >= -boundaryTolerance;
    case Quadrant::NorthWest:
        return x <= boundaryTolerance && z >= -boundaryTolerance;
    case Quadrant::SouthWest:
        return x <= boundaryTolerance && z <= boundaryTolerance;
    case Quadrant::SouthEast:
        return x >= -boundaryTolerance && z <= boundaryTolerance;
    default:
        return false;
    }
}

inline Vec3 BoundaryBiasedDirection(const Vec3& desired,
                                    Quadrant quadrant,
                                    float boundaryNudge = 0.015f) {
    if (desired.IsZero() || QuadrantIndex(quadrant) < 0) return {};
    if (DirectionInsideQuadrant(desired, quadrant, 0.0f)) return desired;

    // Project the requested aim onto the nearest axis boundary of the chosen
    // quadrant. Keep a tiny component inside the sector so the game consumes
    // the intended arrow instead of resolving a floating-point tie itself.
    Vec3 candidate = desired;
    const float nudge = std::clamp(boundaryNudge, 0.001f, 0.15f);
    switch (quadrant) {
    case Quadrant::NorthEast:
        candidate.x = std::max(candidate.x, nudge);
        candidate.z = std::max(candidate.z, nudge);
        break;
    case Quadrant::NorthWest:
        candidate.x = std::min(candidate.x, -nudge);
        candidate.z = std::max(candidate.z, nudge);
        break;
    case Quadrant::SouthWest:
        candidate.x = std::min(candidate.x, -nudge);
        candidate.z = std::min(candidate.z, -nudge);
        break;
    case Quadrant::SouthEast:
        candidate.x = std::max(candidate.x, nudge);
        candidate.z = std::min(candidate.z, -nudge);
        break;
    default:
        return {};
    }
    const float length = candidate.Length2D();
    return length > 0.001f && std::isfinite(length)
        ? candidate / length : QuadrantCenter(quadrant);
}

inline int ForwardDirectionCount(const Vec3& desired,
                                 const std::array<bool, 4>& ready,
                                 float minimumDot = 0.35f) {
    if (desired.IsZero()) return 0;
    int count = 0;
    for (int i = 0; i < 4; ++i) {
        if (!ready[static_cast<std::size_t>(i)]) continue;
        if (QuadrantCenter(static_cast<Quadrant>(i)).Dot(desired) >= minimumDot) {
            ++count;
        }
    }
    return count;
}

inline float QPerDirectionCooldownSeconds(int rank,
                                          float bonusAttackSpeedPercent) {
    static constexpr std::array<float, 6> cooldown = {
        16.0f, 16.0f, 15.0f, 14.0f, 13.0f, 12.0f,
    };
    const float base = RankValue(cooldown, rank);
    // Live tooltip: each 1% bonus AS grants the equivalent of 0.2 Q haste.
    const float haste = std::max(0.0f, bonusAttackSpeedPercent) * 0.20f;
    return base * 100.0f / (100.0f + haste);
}

inline float QGlobalLockSeconds(int rank) {
    static constexpr std::array<float, 6> cooldown = {
        4.0f, 4.0f, 3.25f, 2.50f, 1.75f, 1.0f,
    };
    return RankValue(cooldown, rank);
}

inline float QDashSpeed(int rank, float movementSpeed, bool throughWall) {
    (void)rank;
    (void)movementSpeed;
    return throughWall ? 1500.0f : 850.0f;
}

struct DirectionState {
    std::array<int, 4> ReadyTick = {};
    int GlobalLockUntil = 0;

    bool Ready(Quadrant quadrant, int now) const {
        const int index = QuadrantIndex(quadrant);
        return index >= 0 && now >= GlobalLockUntil &&
            now >= ReadyTick[static_cast<std::size_t>(index)];
    }

    int CountReady(int now) const {
        if (now < GlobalLockUntil) return 0;
        int count = 0;
        for (int tick : ReadyTick) {
            if (now >= tick) ++count;
        }
        return count;
    }

    void Spend(Quadrant quadrant,
               int now,
               float perDirectionCooldownSeconds,
               float globalLockSeconds) {
        const int index = QuadrantIndex(quadrant);
        if (index < 0) return;
        ReadyTick[static_cast<std::size_t>(index)] = now +
            static_cast<int>(std::max(0.0f, perDirectionCooldownSeconds) * 1000.0f);
        GlobalLockUntil = std::max(
            GlobalLockUntil,
            now + static_cast<int>(std::max(0.0f, globalLockSeconds) * 1000.0f));
    }

    void Refresh(std::uint8_t quadrants, int now) {
        for (int i = 0; i < 4; ++i) {
            if ((quadrants & static_cast<std::uint8_t>(1u << i)) != 0u) {
                ReadyTick[static_cast<std::size_t>(i)] = now;
            }
        }
    }
};

// BelvethQHudIcon0..15 is a 4-bit ready-state presentation, but the bit to
// world-quadrant ordering is an implementation detail. Learn it from actual
// local casts instead of hard-coding a patch-fragile orientation.
struct HudCalibration {
    std::array<int, 4> BitForQuadrant = { -1, -1, -1, -1 };

    bool LearnSpend(int previousMask,
                    int currentMask,
                    Quadrant spent) {
        const int quadrant = QuadrantIndex(spent);
        if (quadrant < 0 || previousMask < 0 || currentMask < 0) return false;
        const int removed = (previousMask & ~currentMask) & 0xF;
        if (removed == 0 || (removed & (removed - 1)) != 0) return false;
        int bit = 0;
        while (((removed >> bit) & 1) == 0 && bit < 4) ++bit;
        if (bit >= 4) return false;
        for (int i = 0; i < 4; ++i) {
            if (i != quadrant && BitForQuadrant[static_cast<std::size_t>(i)] == bit) {
                return false;
            }
        }
        BitForQuadrant[static_cast<std::size_t>(quadrant)] = bit;
        return true;
    }

    bool LearnRefresh(int previousMask,
                      int currentMask,
                      Quadrant refreshed) {
        const int quadrant = QuadrantIndex(refreshed);
        if (quadrant < 0 || previousMask < 0 || currentMask < 0) return false;
        const int added = (~previousMask & currentMask) & 0xF;
        if (added == 0 || (added & (added - 1)) != 0) return false;
        int bit = 0;
        while (((added >> bit) & 1) == 0 && bit < 4) ++bit;
        if (bit >= 4) return false;
        for (int i = 0; i < 4; ++i) {
            if (i != quadrant && BitForQuadrant[static_cast<std::size_t>(i)] == bit) {
                return false;
            }
        }
        BitForQuadrant[static_cast<std::size_t>(quadrant)] = bit;
        return true;
    }

    bool Knows(Quadrant quadrant) const {
        const int index = QuadrantIndex(quadrant);
        return index >= 0 &&
            BitForQuadrant[static_cast<std::size_t>(index)] >= 0;
    }

    bool HudReady(int mask, Quadrant quadrant, bool fallback) const {
        const int index = QuadrantIndex(quadrant);
        if (mask < 0 || index < 0) return fallback;
        const int bit = BitForQuadrant[static_cast<std::size_t>(index)];
        return bit >= 0 ? ((mask & (1 << bit)) != 0) : fallback;
    }
};

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Champion = false;
    bool Monster = false;
    bool Epic = false;
    float Health = 0.0f;
    float MaximumHealth = 1.0f;

    float HealthPercent() const {
        return MaximumHealth > 0.001f
            ? std::clamp(Health / MaximumHealth, 0.0f, 1.0f)
            : 1.0f;
    }
};

inline bool CapsuleHits(const Vec3& start,
                        const Vec3& end,
                        float halfWidth,
                        const Body& body) {
    if (!body.Valid || !start.IsValid() || !end.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(
        body.Position, start, end);
    return projection.Distance <= std::max(0.0f, halfWidth) +
        std::clamp(body.Radius, 0.0f, 200.0f);
}

inline bool QHits(const Vec3& start,
                  const Vec3& end,
                  const Body& body) {
    return CapsuleHits(start, end, kQHalfWidth, body);
}

inline int FirstQBodyIndex(const Vec3& start,
                           const Vec3& end,
                           const std::vector<Body>& bodies) {
    int best = -1;
    float bestT = FLT_MAX;
    float bestDistance = FLT_MAX;
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        if (!QHits(start, end, bodies[i])) continue;
        const auto projection = ProjectPointToSegment2D(
            bodies[i].Position, start, end);
        if (projection.T < bestT - 0.0001f ||
            (std::fabs(projection.T - bestT) <= 0.0001f &&
             projection.Distance < bestDistance)) {
            best = static_cast<int>(i);
            bestT = projection.T;
            bestDistance = projection.Distance;
        }
    }
    return best;
}

inline float QRawDamage(int rank,
                        float totalAd,
                        bool monster = false,
                        bool minion = false) {
    static constexpr std::array<float, 6> base = {
        0.0f, 12.0f, 14.0f, 16.0f, 18.0f, 20.0f,
    };
    if (rank <= 0) return 0.0f;
    float damage = RankValue(base, rank) +
        1.05f * std::max(0.0f, totalAd);
    if (monster) damage += 70.0f;
    (void)minion;
    return damage;
}

enum class QPurpose : int {
    None,
    Weave,
    Chase,
    Evade,
    Flee,
    Execute,
    WallFlank,
    WallCancel,
    Farm,
};

struct QContext {
    bool DirectionReady = false;
    bool GlobalReady = false;
    bool EndpointValid = false;
    bool TerrainCrossed = false;
    bool TrueForm = false;
    bool HasWallExit = false;
    bool PlayerGrounded = false;
    bool PlayerWindingUp = false;
    bool AttackJustCompleted = false;
    bool TargetHit = false;
    bool TargetInAttackRangeAfter = false;
    bool TargetInAttackRangeBefore = false;
    bool TargetKillable = false;
    bool IncomingSkillshot = false;
    bool DodgesIncomingSkillshot = false;
    bool CursorAgrees = false;
    bool DestinationSafe = false;
    bool WReady = false;
    bool WCanRefreshSpentDirection = false;
    bool TargetCommitted = false;
    bool TargetEscaping = false;
    bool FarmTarget = false;
    bool MonsterTarget = false;
    bool NearWall = false;
    int ReadyDirectionCount = 0;
    int ForwardDirectionCount = 0;
    int EnemiesAtDestination = 0;
    int AlliesAtDestination = 0;
    float CursorDot = -1.0f;
    float TargetDistanceBefore = FLT_MAX;
    float TargetDistanceAfter = FLT_MAX;
    QPurpose Purpose = QPurpose::None;
};

struct QEvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline QEvaluation EvaluateQ(const QContext& context) {
    QEvaluation result{};
    if (!context.GlobalReady || !context.DirectionReady ||
        !context.EndpointValid || context.PlayerGrounded) {
        result.Reason = "direction unavailable";
        return result;
    }
    if (context.PlayerWindingUp && !context.TargetKillable &&
        !context.IncomingSkillshot) {
        result.Reason = "preserve attack windup";
        return result;
    }
    if (context.TerrainCrossed &&
        (!context.TrueForm || !context.HasWallExit)) {
        result.Reason = "illegal wall path";
        return result;
    }
    if (!context.DestinationSafe && !context.TargetKillable &&
        !context.DodgesIncomingSkillshot) {
        result.Reason = "unsafe endpoint";
        return result;
    }
    if (context.EnemiesAtDestination >
            context.AlliesAtDestination + 1 &&
        !context.TargetKillable && context.Purpose != QPurpose::Flee &&
        context.Purpose != QPurpose::Evade) {
        result.Reason = "outnumbered endpoint";
        return result;
    }

    float score = context.CursorAgrees ? 105.0f :
        std::max(-80.0f, context.CursorDot * 60.0f);
    score += static_cast<float>(context.AlliesAtDestination) * 70.0f;
    score -= static_cast<float>(context.EnemiesAtDestination) * 95.0f;

    if (context.TargetHit) score += 220.0f;
    if (context.AttackJustCompleted) score += 310.0f;
    if (context.TargetInAttackRangeAfter) score += 145.0f;
    if (!context.TargetInAttackRangeBefore &&
        context.TargetInAttackRangeAfter) score += 175.0f;
    if (context.TargetKillable) score += 1200.0f;
    if (context.DodgesIncomingSkillshot) score += 1500.0f;
    if (context.TargetCommitted) score += 95.0f;
    if (context.TargetEscaping &&
        context.TargetDistanceAfter < context.TargetDistanceBefore) {
        score += 145.0f;
    }
    if (context.WReady && context.WCanRefreshSpentDirection) score += 120.0f;

    switch (context.Purpose) {
    case QPurpose::Weave: score += 260.0f; break;
    case QPurpose::Chase: score += 100.0f; break;
    case QPurpose::Evade: score += 900.0f; break;
    case QPurpose::Flee: score += 520.0f; break;
    case QPurpose::Execute: score += 900.0f; break;
    case QPurpose::WallFlank: score += context.TrueForm ? 180.0f : -1000.0f; break;
    case QPurpose::WallCancel:
        score += context.MonsterTarget && context.NearWall &&
            context.AttackJustCompleted ? 280.0f : -500.0f;
        break;
    case QPurpose::Farm:
        score += context.FarmTarget && context.AttackJustCompleted
            ? 130.0f : -260.0f;
        break;
    default: break;
    }

    // The classic error is spending both forward arrows only to enter range,
    // then having no Q damage/reset left. Preserve the second forward sector
    // unless W will refund this one, the target is executable, or the dash is
    // answering a real skillshot.
    if (context.Purpose == QPurpose::Chase &&
        context.ForwardDirectionCount <= 1 && !context.TargetHit &&
        !context.WCanRefreshSpentDirection && !context.TargetKillable) {
        score -= 700.0f;
    }
    if (context.ReadyDirectionCount <= 1 &&
        context.Purpose != QPurpose::Flee &&
        context.Purpose != QPurpose::Evade &&
        !context.TargetKillable && !context.WCanRefreshSpentDirection) {
        score -= 320.0f;
    }
    if (context.TargetInAttackRangeBefore &&
        !context.AttackJustCompleted && !context.TargetHit &&
        context.Purpose != QPurpose::Evade) {
        score -= 420.0f;
    }

    result.Score = score;
    result.Cast = score >= 180.0f;
    result.Reason = result.Cast ? "one-trick Q window" : "hold Q arrow";
    return result;
}

inline bool WLineHits(const Vec3& origin,
                      const Vec3& castPosition,
                      const Body& target) {
    if (!target.Valid || !origin.IsValid() || !castPosition.IsValid()) {
        return false;
    }
    const Vec3 direction = Direction2D(origin, castPosition);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * kWGameplayRange;
    return CapsuleHits(origin, end, kWHalfWidth, target);
}

inline std::uint8_t WResetMask(const Vec3& castOrigin,
                               const std::vector<Body>& hitChampions) {
    std::uint8_t result = 0u;
    for (const auto& champion : hitChampions) {
        if (!champion.Valid || !champion.Champion) continue;
        result = static_cast<std::uint8_t>(
            result | QuadrantMask(
                QuadrantForPoints(castOrigin, champion.Position)));
    }
    return result;
}

inline float WRawDamage(int rank, float bonusAd, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 80.0f, 140.0f, 200.0f, 260.0f, 320.0f,
    };
    if (rank <= 0) return 0.0f;
    (void)bonusAd;
    return RankValue(base, rank) +
        1.50f * std::max(0.0f, abilityPower);
}

struct WContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionHits = false;
    bool HighHitchance = false;
    bool TargetHardCrowdControlled = false;
    bool TargetSlowed = false;
    bool TargetDashSpent = false;
    bool TargetCommitted = false;
    bool Interrupt = false;
    bool Gapcloser = false;
    bool Peel = false;
    bool Lethal = false;
    bool PlayerWindingUp = false;
    bool PlayerInE = false;
    bool ResetsSpentQ = false;
    bool ResetsMultipleQ = false;
    bool FollowupAvailable = false;
    bool TargetInAttackRange = false;
    bool Farm = false;
    bool Jungle = false;
    bool EnemyChampionNearby = false;
    int HitCount = 0;
    int FarmHits = 0;
};

inline bool ShouldCastW(const WContext& context) {
    if (!context.Ready || !context.TargetValid || !context.PredictionHits ||
        context.PlayerInE) return false;
    if (context.PlayerWindingUp && !context.Lethal &&
        !context.Interrupt && !context.Gapcloser) return false;
    if (context.Interrupt || context.Gapcloser || context.Peel) return true;
    if (context.Farm) {
        if (context.EnemyChampionNearby) return false;
        return context.FarmHits >= (context.Jungle ? 2 : 3);
    }
    const bool reliable = context.HighHitchance ||
        context.TargetHardCrowdControlled || context.TargetSlowed ||
        context.TargetDashSpent || context.TargetCommitted;
    if (!reliable && !context.Lethal) return false;
    if (context.ResetsMultipleQ) return true;
    if (context.ResetsSpentQ && context.FollowupAvailable) return true;
    if (context.Lethal) return true;
    // A W that resets nothing is still a valid close-range peel/setup, but it
    // is not thrown raw at a mobile target merely because prediction says hit.
    return context.TargetInAttackRange && context.FollowupAvailable && reliable;
}

inline int SelectETargetIndex(const Vec3& center,
                              const std::vector<Body>& targets,
                              float radius = kERadius) {
    int best = -1;
    float bestHealthPercent = FLT_MAX;
    float bestDistance = FLT_MAX;
    for (std::size_t i = 0; i < targets.size(); ++i) {
        const Body& target = targets[i];
        if (!target.Valid || target.Health <= 0.0f ||
            center.Distance2D(target.Position) >
                radius + std::clamp(target.Radius, 0.0f, 200.0f)) {
            continue;
        }
        const float healthPercent = target.HealthPercent();
        const float distance = center.Distance2D(target.Position);
        if (healthPercent < bestHealthPercent - 0.0001f ||
            (std::fabs(healthPercent - bestHealthPercent) <= 0.0001f &&
             distance < bestDistance)) {
            best = static_cast<int>(i);
            bestHealthPercent = healthPercent;
            bestDistance = distance;
        }
    }
    return best;
}

inline int EStrikeCount(float bonusAttackSpeedPercent) {
    return 6 + static_cast<int>(std::floor(
        std::max(0.0f, bonusAttackSpeedPercent) / 40.0f + 0.0001f));
}

inline float EMissingHealthMultiplier(float currentHealth,
                                      float maximumHealth) {
    if (maximumHealth <= 0.001f) return 1.0f;
    const float missing = std::clamp(
        1.0f - currentHealth / maximumHealth, 0.0f, 1.0f);
    return 1.0f + missing;
}

inline float EStrikeRawDamage(int rank,
                              float totalAd,
                              float currentHealth,
                              float maximumHealth,
                              bool monster = false) {
    static constexpr std::array<float, 6> base = {
        0.0f, 10.0f, 12.0f, 14.0f, 16.0f, 18.0f,
    };
    if (rank <= 0) return 0.0f;
    float damage = (RankValue(base, rank) +
        0.12f * std::max(0.0f, totalAd)) *
        EMissingHealthMultiplier(currentHealth, maximumHealth);
    if (monster) damage *= 2.0f;
    return damage;
}

inline float EOnHitEffectiveness(float currentHealth,
                                 float maximumHealth) {
    return 0.12f * EMissingHealthMultiplier(
        currentHealth, maximumHealth);
}

inline float ESimulatedRawDamage(int rank,
                                 float totalAd,
                                 float targetHealth,
                                 float targetMaximumHealth,
                                 float bonusAttackSpeedPercent,
                                 float rawOnHitDamage = 0.0f,
                                 bool monster = false) {
    float health = std::max(0.0f, targetHealth);
    float total = 0.0f;
    const int strikes = EStrikeCount(bonusAttackSpeedPercent);
    for (int i = 0; i < strikes && health > 0.0f; ++i) {
        const float strike = EStrikeRawDamage(
            rank, totalAd, health, targetMaximumHealth, monster);
        const float onHit = std::max(0.0f, rawOnHitDamage) *
            EOnHitEffectiveness(health, targetMaximumHealth);
        const float dealt = strike + onHit;
        total += dealt;
        health = std::max(0.0f, health - dealt);
    }
    return total;
}

inline float EDamageReduction(int rank) {
    static constexpr std::array<float, 6> reduction = {
        0.0f, 0.20f, 0.30f, 0.40f, 0.50f, 0.60f,
    };
    return RankValue(reduction, rank);
}

inline float ELifesteal(int rank) {
    static constexpr std::array<float, 6> lifesteal = {
        0.0f, 0.20f, 0.25f, 0.30f, 0.35f, 0.40f,
    };
    return RankValue(lifesteal, rank);
}

struct EStartContext {
    bool Ready = false;
    bool CanDeclareAttacks = true;
    bool PlayerWindingUp = false;
    bool ForcedTargetValid = false;
    bool ForcedTargetIsDesired = false;
    bool ForcedTargetChampion = false;
    bool ForcedTargetMonster = false;
    bool ForcedTargetEpic = false;
    bool TargetHardCrowdControlled = false;
    bool TargetSlowed = false;
    bool TargetCommitted = false;
    bool TargetEscaping = false;
    bool PositionSafe = false;
    bool Execute = false;
    bool Defensive = false;
    bool IncomingReducibleBurst = false;
    bool IncomingTrueDamageOnly = false;
    bool JungleSustain = false;
    bool ObjectiveSecure = false;
    bool RecentAbilityCast = false;
    bool QEscapeAvailableAfter = false;
    int EnemiesNearby = 0;
    int AlliesNearby = 0;
    float PlayerHealthPercent = 100.0f;
    float ForcedTargetHealthPercent = 100.0f;
    float DesiredTargetHealthPercent = 100.0f;
    float ExpectedDamage = 0.0f;
    float ForcedTargetHealth = FLT_MAX;
};

inline bool ShouldStartE(const EStartContext& context) {
    if (!context.Ready || !context.CanDeclareAttacks) return false;
    if (context.PlayerWindingUp && !context.Defensive && !context.Execute) {
        return false;
    }
    if (context.Defensive) {
        if (!context.IncomingReducibleBurst || context.IncomingTrueDamageOnly) {
            return false;
        }
        return context.PlayerHealthPercent <= 58.0f ||
            context.EnemiesNearby > context.AlliesNearby;
    }
    if (!context.ForcedTargetValid) return false;
    if (!context.ForcedTargetIsDesired && !context.ForcedTargetMonster) {
        return false;
    }
    if (!context.PositionSafe && !context.QEscapeAvailableAfter &&
        !context.Execute) return false;
    if (context.TargetEscaping && !context.TargetHardCrowdControlled &&
        !context.TargetSlowed && !context.TargetCommitted) {
        return false;
    }
    if (context.Execute && context.ForcedTargetIsDesired) {
        return context.ExpectedDamage + 1.0f >= context.ForcedTargetHealth ||
            context.ForcedTargetHealthPercent <= 34.0f;
    }
    if (context.ObjectiveSecure && context.ForcedTargetEpic) {
        return context.ForcedTargetHealthPercent <= 36.0f;
    }
    if (context.JungleSustain && context.ForcedTargetMonster) {
        return context.PlayerHealthPercent <= 66.0f &&
            context.ForcedTargetHealthPercent <= 62.0f;
    }
    return context.ForcedTargetChampion &&
        context.ForcedTargetIsDesired && context.RecentAbilityCast &&
        context.ForcedTargetHealthPercent <= 48.0f &&
        (context.TargetHardCrowdControlled || context.TargetSlowed ||
         context.TargetCommitted);
}

struct ECancelContext {
    bool Active = false;
    float ElapsedSeconds = 0.0f;
    bool IncomingLethalSkillshot = false;
    bool SafeQEvadeAvailable = false;
    bool ForcedTargetValid = true;
    bool ForcedTargetIsDesired = true;
    bool DesiredTargetEscaped = false;
    bool SafeQChaseAvailable = false;
    bool PlayerExplicitlyRetreating = false;
    bool DefensiveWindowFinished = false;
};

inline bool ShouldCancelE(const ECancelContext& context) {
    if (!context.Active || context.ElapsedSeconds < kEEarlyCancelSeconds) {
        return false;
    }
    if (context.IncomingLethalSkillshot && context.SafeQEvadeAvailable) {
        return true;
    }
    if (context.PlayerExplicitlyRetreating &&
        context.DefensiveWindowFinished) return true;
    if ((!context.ForcedTargetValid || !context.ForcedTargetIsDesired) &&
        context.DesiredTargetEscaped && context.SafeQChaseAvailable) {
        return true;
    }
    return false;
}

inline float RPassiveProcRawDamage(int rank,
                                   float bonusAd,
                                   int existingProcStacks,
                                   bool epicMonster) {
    static constexpr std::array<float, 4> base = {
        0.0f, 2.0f, 4.0f, 6.0f,
    };
    if (rank <= 0) return 0.0f;
    const int stack = epicMonster
        ? std::clamp(existingProcStacks, 0, kRMaximumEpicProcs)
        : std::max(0, existingProcStacks);
    const int multiplier = epicMonster && stack >= kRMaximumEpicProcs
        ? kRMaximumEpicProcs : stack + 1;
    return (RankValue(base, rank) +
            0.03f * std::max(0.0f, bonusAd)) *
        static_cast<float>(multiplier);
}

struct RPassiveTracker {
    int TargetId = 0;
    int LastApplicationTick = 0;
    int ProcStacks = 0;
    bool EpicMonster = false;

    void Reset() { *this = {}; }

    float ObserveApplication(int targetId,
                             int now,
                             int rank,
                             float bonusAd,
                             bool epicMonster) {
        if (targetId == 0) return 0.0f;
        if (LastApplicationTick <= 0 ||
            now - LastApplicationTick >
                static_cast<int>(kRPassiveStackDurationSeconds * 1000.0f)) {
            ProcStacks = 0;
        }
        TargetId = targetId;
        LastApplicationTick = now;
        EpicMonster = epicMonster;
        const float damage = RPassiveProcRawDamage(
            rank, bonusAd, ProcStacks, epicMonster);
        if (!epicMonster || ProcStacks < kRMaximumEpicProcs) ++ProcStacks;
        return damage;
    }
};

inline float RExplosionRawDamage(int rank,
                                 float abilityPower,
                                 float targetCurrentHealth,
                                 float targetMaximumHealth,
                                 bool monster = false) {
    static constexpr std::array<float, 4> base = {
        0.0f, 150.0f, 200.0f, 250.0f,
    };
    if (rank <= 0) return 0.0f;
    const float missing = std::max(
        0.0f, targetMaximumHealth - targetCurrentHealth);
    float damage = RankValue(base, rank) +
        1.50f * std::max(0.0f, abilityPower) + 0.20f * missing;
    if (monster) damage = std::min(1500.0f, damage);
    return damage;
}

inline float RHeal(int rank, float bonusAd, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 100.0f, 250.0f, 400.0f,
    };
    if (rank <= 0) return 0.0f;
    return RankValue(base, rank) + 1.50f * std::max(0.0f, bonusAd) +
        1.50f * std::max(0.0f, abilityPower);
}

inline float RFormDurationSeconds(int lavenderStacks) {
    if (lavenderStacks >= 80) {
        return std::numeric_limits<float>::infinity();
    }
    return lavenderStacks >= 40
        ? kRLongFormSeconds : kRShortFormSeconds;
}

struct Coral {
    Vec3 Position = {};
    int Id = 0;
    float SecondsRemaining = kRCoralSeconds;
    bool Valid = true;
    bool Castable = true;
    bool Enhanced = false;
};

struct RTarget {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Health = 0.0f;
    float MaximumHealth = 1.0f;
    float ProjectedHealthAtExplosion = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Champion = true;
    bool Monster = false;
};

struct RContext {
    bool Ready = false;
    bool PlayerGrounded = false;
    bool DestinationSafe = false;
    bool UnderEnemyTurret = false;
    bool CursorAgrees = false;
    bool AnyEnhancedCoral = false;
    bool CurrentlyTrueForm = false;
    bool CurrentlyEnhanced = false;
    bool ObjectiveWindow = false;
    bool WaveMacroWindow = false;
    bool PlayerLow = false;
    bool IncomingLethalDamage = false;
    bool PlayerWindingUp = false;
    bool PlayerExplicitlyRequested = false;
    float CurrentFormSeconds = 0.0f;
    float PlayerMissingHealth = 0.0f;
    float Heal = 0.0f;
    int EnemiesAtCoral = 0;
    int AlliesAtCoral = 0;
    int CoralsConsumed = 1;
    int KillCount = 0;
    int HitCount = 0;
    float ExplosionDamage = 0.0f;
    float SoonestCoralExpiry = kRCoralSeconds;
};

struct REvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline REvaluation EvaluateR(const RContext& context) {
    REvaluation result{};
    if (!context.Ready || context.PlayerGrounded) {
        result.Reason = "R unavailable";
        return result;
    }
    if (!context.DestinationSafe && !context.IncomingLethalDamage &&
        context.KillCount <= 0) {
        result.Reason = "unsafe coral channel";
        return result;
    }
    if (context.UnderEnemyTurret && context.KillCount <= 0 &&
        !context.IncomingLethalDamage) {
        result.Reason = "turret coral rejected";
        return result;
    }
    if (context.PlayerWindingUp && context.KillCount <= 0 &&
        !context.IncomingLethalDamage) {
        result.Reason = "preserve attack";
        return result;
    }

    const bool formWaste = context.CurrentlyTrueForm &&
        !context.AnyEnhancedCoral && !context.CurrentlyEnhanced &&
        context.CurrentFormSeconds > 24.0f;
    const bool enhancedExtensionWaste = context.CurrentlyEnhanced &&
        !context.AnyEnhancedCoral && context.CurrentFormSeconds > 120.0f;
    const bool expiryUrgent = context.SoonestCoralExpiry <= 2.25f;
    const bool healMeaningful = context.Heal >=
        std::max(75.0f, context.PlayerMissingHealth * 0.35f);
    const bool explosionMeaningful = context.KillCount > 0 ||
        context.HitCount >= 2 || context.ExplosionDamage >= 450.0f;
    const bool formMeaningful = !context.CurrentlyTrueForm ||
        context.AnyEnhancedCoral || expiryUrgent;
    const bool macroMeaningful = context.AnyEnhancedCoral &&
        (context.WaveMacroWindow || context.ObjectiveWindow);
    const bool survivalMeaningful = context.PlayerLow && healMeaningful;
    if (!context.PlayerExplicitlyRequested && !explosionMeaningful &&
        !formMeaningful && !macroMeaningful && !survivalMeaningful) {
        result.Reason = "no coral payoff";
        return result;
    }
    if (!context.PlayerExplicitlyRequested && (formWaste || enhancedExtensionWaste) &&
        !expiryUrgent && !explosionMeaningful && !survivalMeaningful &&
        !context.AnyEnhancedCoral) {
        result.Reason = "hold form refresh";
        return result;
    }

    float score = context.CursorAgrees ? 90.0f : -25.0f;
    score += static_cast<float>(context.AlliesAtCoral) * 80.0f;
    score -= static_cast<float>(context.EnemiesAtCoral) * 95.0f;
    score += static_cast<float>(context.HitCount) * 115.0f;
    score += static_cast<float>(context.KillCount) * 900.0f;
    score += std::min(900.0f, context.ExplosionDamage * 0.42f);
    score += std::min(context.PlayerMissingHealth, context.Heal) * 0.65f;
    score += static_cast<float>(std::max(1, context.CoralsConsumed)) * 35.0f;
    if (context.AnyEnhancedCoral) score += 520.0f;
    if (context.WaveMacroWindow && context.AnyEnhancedCoral) score += 240.0f;
    if (context.ObjectiveWindow) score += 130.0f;
    if (expiryUrgent) score += 360.0f;
    if (!context.CurrentlyTrueForm) score += 240.0f;
    if (context.IncomingLethalDamage && healMeaningful) score += 850.0f;
    if (context.PlayerExplicitlyRequested) score += 1000.0f;
    if (formWaste) score -= 360.0f;
    if (enhancedExtensionWaste) score -= 480.0f;

    result.Score = score;
    result.Cast = score >= 260.0f;
    result.Reason = result.Cast ? "coral payoff" : "hold coral";
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Belveth::Geometry
