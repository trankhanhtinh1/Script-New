#pragma once

// Deterministic live Blitzcrank mechanics and one-trick policy. Runtime object
// discovery, prediction, orbwalker coordination and casts belong to
// AIBlitzcrankController. This layer owns the moving first-body Rocket Grab,
// pull-value/no-grief policy, Overdrive's delayed self-slow, Power Fist timing,
// Static Field mark economy and mana sequencing so each edge is testable.

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Blitzcrank::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using SharedGeometry::SolveMovingCircleContactTime2D;

inline constexpr float kQCastSeconds = 0.25f;
inline constexpr float kQMissileSpeed = 1800.0f;
inline constexpr float kQMissileSegment = 1080.0f;
inline constexpr float kQMaximumTargetCenterRange = 1115.0f;
inline constexpr float kQCollisionRadius = 70.0f;
inline constexpr float kQStunSeconds = 0.65f;
inline constexpr float kQPullDestinationDistance = 75.0f;
inline constexpr float kWDurationSeconds = 5.0f;
inline constexpr float kWDecaySeconds = 2.9f;
inline constexpr float kWTerminalMoveSpeedPercent = 10.0f;
inline constexpr float kWSelfSlowPercent = 30.0f;
inline constexpr float kWSelfSlowSeconds = 1.5f;
inline constexpr float kEDurationSeconds = 5.0f;
inline constexpr float kEKnockupSeconds = 1.0f;
inline constexpr float kEBonusAttackRange = 50.0f;
inline constexpr float kRRadius = 600.0f;
inline constexpr float kRCastSeconds = 0.25f;
inline constexpr float kRSilenceSeconds = 0.5f;
inline constexpr float kRMarkIntervalSeconds = 1.0f;
inline constexpr float kManaBarrierHealthThreshold = 0.30f;
inline constexpr float kManaBarrierMaximumManaRatio = 0.35f;
inline constexpr float kManaBarrierDurationSeconds = 10.0f;
inline constexpr float kManaBarrierCooldownSeconds = 90.0f;

inline float ManaBarrierShield(float maximumMana) {
    return std::max(0.0f, maximumMana) * kManaBarrierMaximumManaRatio;
}

inline bool ManaBarrierWillTrigger(float currentHealth,
                                   float maximumHealth,
                                   float incomingDamage,
                                   bool cooldownReady) {
    if (!cooldownReady || maximumHealth <= 0.0f || incomingDamage <= 0.0f) {
        return false;
    }
    const float remaining = currentHealth - incomingDamage;
    return currentHealth > maximumHealth * kManaBarrierHealthThreshold &&
        remaining <= maximumHealth * kManaBarrierHealthThreshold;
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 110.0f, 160.0f, 210.0f, 260.0f, 310.0f,
    };
    return rank > 0
        ? RankValue(base, rank) + 1.20f * std::max(0.0f, abilityPower)
        : 0.0f;
}

inline float WInitialMoveSpeedPercent(int rank) {
    static constexpr std::array<float, 6> value = {
        0.0f, 60.0f, 65.0f, 70.0f, 75.0f, 80.0f,
    };
    return rank > 0 ? RankValue(value, rank) : 0.0f;
}

inline float WAttackSpeedPercent(int rank) {
    static constexpr std::array<float, 6> value = {
        0.0f, 30.0f, 40.0f, 50.0f, 60.0f, 70.0f,
    };
    return rank > 0 ? RankValue(value, rank) : 0.0f;
}

inline float WMoveSpeedPercentAt(int rank, float elapsedSeconds) {
    if (rank <= 0 || elapsedSeconds < 0.0f ||
        elapsedSeconds >= kWDurationSeconds) {
        return 0.0f;
    }
    const float initial = WInitialMoveSpeedPercent(rank);
    const float progress = std::clamp(
        elapsedSeconds / kWDecaySeconds, 0.0f, 1.0f);
    return initial +
        (kWTerminalMoveSpeedPercent - initial) * progress;
}

// Integrate movement gain rather than pretending W keeps its tooltip speed for
// all five seconds. This estimate is intentionally terrain/path agnostic.
inline float WBonusTravelDistance(float baseMoveSpeed,
                                  int rank,
                                  float elapsedSeconds) {
    if (baseMoveSpeed <= 0.0f || rank <= 0 || elapsedSeconds <= 0.0f) {
        return 0.0f;
    }
    const float duration = std::min(elapsedSeconds, kWDurationSeconds);
    constexpr int slices = 58;
    const float step = duration / static_cast<float>(slices);
    float result = 0.0f;
    for (int i = 0; i < slices; ++i) {
        const float midpoint = (static_cast<float>(i) + 0.5f) * step;
        result += baseMoveSpeed *
            (WMoveSpeedPercentAt(rank, midpoint) / 100.0f) * step;
    }
    return result;
}

// This is the complete empowered attack before mitigation: the ordinary basic
// attack plus E's 100% total-AD and 25% AP bonus. Current live E has no special
// non-champion multiplier; stale inactive CDragon calculations are excluded.
inline float EEmpoweredAttackRawDamage(float totalAttackDamage,
                                       float abilityPower) {
    return 2.0f * std::max(0.0f, totalAttackDamage) +
        0.25f * std::max(0.0f, abilityPower);
}

inline float EAdditionalRawDamage(float totalAttackDamage,
                                  float abilityPower) {
    return std::max(0.0f, totalAttackDamage) +
        0.25f * std::max(0.0f, abilityPower);
}

inline float RPassiveRawDamage(int rank,
                               float abilityPower,
                               float maximumMana) {
    static constexpr std::array<float, 4> base = {
        0.0f, 50.0f, 100.0f, 150.0f,
    };
    static constexpr std::array<float, 4> apRatio = {
        0.0f, 0.30f, 0.40f, 0.50f,
    };
    return rank > 0
        ? RankValue(base, rank) +
            RankValue(apRatio, rank) * std::max(0.0f, abilityPower) +
            0.02f * std::max(0.0f, maximumMana)
        : 0.0f;
}

inline float RActiveRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 275.0f, 400.0f, 525.0f,
    };
    return rank > 0
        ? RankValue(base, rank) + std::max(0.0f, abilityPower)
        : 0.0f;
}

enum class PullArchetype : std::uint8_t {
    Other,
    Carry,
    Enchanter,
    Artillery,
    Assassin,
    Diver,
    Juggernaut,
    EngageBomb,
    Warden,
};

enum class HookContactKind : std::uint8_t {
    None,
    MissileBody,
    EndpointLollipop,
};

struct HookBody {
    Vec3 Position = {};
    Vec3 Velocity = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Targetable = true;
    bool Hostile = true;
    bool Champion = false;
    bool Minion = false;
    bool Monster = false;
    PullArchetype Archetype = PullArchetype::Other;

    Vec3 PositionAt(float castElapsedSeconds) const {
        Vec3 result = Position + Velocity *
            std::max(0.0f, castElapsedSeconds);
        result.y = Position.y;
        return result;
    }
};

struct HookContact {
    bool Hit = false;
    int BodyIndex = -1;
    int BodyId = 0;
    float ProjectileSeconds = FLT_MAX;
    float CastElapsedSeconds = FLT_MAX;
    float MissileDistance = FLT_MAX;
    float TargetCenterDistance = FLT_MAX;
    Vec3 MissilePosition = {};
    Vec3 TargetPosition = {};
    HookContactKind Kind = HookContactKind::None;
};

inline HookContact ContactWithBody(const Vec3& start,
                                   const Vec3& castPosition,
                                   const HookBody& body,
                                   int bodyIndex = -1) {
    HookContact result{};
    if (!start.IsValid() || !castPosition.IsValid() || !body.Valid ||
        !body.Targetable || !body.Hostile || body.Id == 0) {
        return result;
    }
    const Vec3 direction = Direction2D(start, castPosition);
    if (direction.IsZero()) return result;

    const float maximumFlight = kQMissileSegment / kQMissileSpeed;
    Vec3 positionWhenMissileSpawns =
        body.PositionAt(kQCastSeconds) - start;
    positionWhenMissileSpawns.y = 0.0f;
    Vec3 relativeVelocity = body.Velocity - direction * kQMissileSpeed;
    relativeVelocity.y = 0.0f;
    float contactSeconds = 0.0f;
    if (!SolveMovingCircleContactTime2D(
            positionWhenMissileSpawns,
            relativeVelocity,
            kQCollisionRadius + std::clamp(body.Radius, 0.0f, 200.0f),
            maximumFlight,
            contactSeconds)) {
        return result;
    }

    const Vec3 targetPosition = body.PositionAt(
        kQCastSeconds + contactSeconds);
    const Vec3 fromStart = targetPosition - start;
    const float longitudinal = fromStart.Dot(direction);
    const float lateral = std::fabs(
        fromStart.x * direction.z - fromStart.z * direction.x);
    if (longitudinal < -std::max(0.0f, body.Radius) ||
        longitudinal > kQMaximumTargetCenterRange + 0.01f ||
        lateral > kQCollisionRadius + std::max(0.0f, body.Radius) + 0.01f) {
        return result;
    }

    result.Hit = true;
    result.BodyIndex = bodyIndex;
    result.BodyId = body.Id;
    result.ProjectileSeconds = contactSeconds;
    result.CastElapsedSeconds = kQCastSeconds + contactSeconds;
    result.MissileDistance = std::min(
        kQMissileSegment, contactSeconds * kQMissileSpeed);
    result.TargetCenterDistance = longitudinal;
    result.MissilePosition = start + direction * result.MissileDistance;
    result.TargetPosition = targetPosition;
    result.Kind = longitudinal > kQMissileSegment
        ? HookContactKind::EndpointLollipop
        : HookContactKind::MissileBody;
    return result;
}

inline HookContact FirstHookContact(const Vec3& start,
                                    const Vec3& castPosition,
                                    const std::vector<HookBody>& bodies) {
    HookContact best{};
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        HookContact candidate = ContactWithBody(
            start, castPosition, bodies[i], static_cast<int>(i));
        if (!candidate.Hit) continue;
        if (!best.Hit ||
            candidate.ProjectileSeconds < best.ProjectileSeconds - 0.0001f ||
            (std::fabs(candidate.ProjectileSeconds - best.ProjectileSeconds) <=
                 0.0001f &&
             (candidate.TargetCenterDistance < best.TargetCenterDistance - 0.01f ||
              (std::fabs(candidate.TargetCenterDistance -
                         best.TargetCenterDistance) <= 0.01f &&
               candidate.BodyId < best.BodyId)))) {
            best = candidate;
        }
    }
    return best;
}

inline bool HookHitsIntendedFirst(const Vec3& start,
                                  const Vec3& castPosition,
                                  const std::vector<HookBody>& bodies,
                                  int intendedId,
                                  HookContact* contact = nullptr) {
    const HookContact first = FirstHookContact(start, castPosition, bodies);
    if (contact != nullptr) *contact = first;
    return first.Hit && intendedId != 0 && first.BodyId == intendedId;
}

inline Vec3 PullLandingPosition(const Vec3& blitzPosition,
                                const Vec3& targetPosition) {
    const Vec3 direction = Direction2D(blitzPosition, targetPosition);
    return direction.IsZero()
        ? blitzPosition
        : blitzPosition + direction * kQPullDestinationDistance;
}

enum class HookPurpose : std::uint8_t {
    None,
    SelectedPick,
    ImmobilePunish,
    DashEndpoint,
    StasisExit,
    AllyTurret,
    Peel,
    Interrupt,
    Kill,
    ObjectiveJungler,
    ManualCursor,
};

struct HookContext {
    bool Ready = false;
    bool TargetValid = false;
    bool IntendedFirstBody = false;
    bool ProjectileWallBlocked = false;
    bool TargetSpellShield = false;
    bool TargetUnstoppable = false;
    bool HighConfidence = false;
    bool SelectedTarget = false;
    bool CursorAgrees = false;
    bool TargetImmobile = false;
    bool TargetDashEnding = false;
    bool TargetLeavingStasis = false;
    bool TargetCanInstantEscape = false;
    bool TargetEscapeSpent = false;
    bool TargetKillable = false;
    bool TargetIsolated = false;
    bool TargetKeyCooldownsSpent = false;
    bool PullsTowardAlliedTurret = false;
    bool PullsOntoProtectedCarry = false;
    bool ProtectedCarryThreatened = false;
    bool PeelDisplacement = false;
    bool InterruptUrgent = false;
    bool FollowupAvailable = false;
    bool WEReliableWalkup = false;
    bool EPrimedInAttackRange = false;
    bool PlayerAttackWindingUp = false;
    bool ObjectiveContest = false;
    float TargetHealthPercent = 100.0f;
    float TargetPriority = 1.0f;
    float CollisionConfidence = 1.0f;
    int AlliesAtLanding = 0;
    int EnemiesAtLanding = 0;
    PullArchetype Archetype = PullArchetype::Other;
    HookPurpose Purpose = HookPurpose::None;
};

struct HookEvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline bool IsDangerousDelivery(PullArchetype archetype) {
    return archetype == PullArchetype::Assassin ||
        archetype == PullArchetype::Diver ||
        archetype == PullArchetype::Juggernaut ||
        archetype == PullArchetype::EngageBomb ||
        archetype == PullArchetype::Warden;
}

inline bool IsPremiumCatch(PullArchetype archetype) {
    return archetype == PullArchetype::Carry ||
        archetype == PullArchetype::Enchanter ||
        archetype == PullArchetype::Artillery;
}

inline HookEvaluation EvaluateHook(const HookContext& context) {
    HookEvaluation result{};
    if (!context.Ready || !context.TargetValid ||
        !context.IntendedFirstBody) {
        result.Reason = "no clean first body";
        return result;
    }
    if (context.ProjectileWallBlocked || context.TargetUnstoppable) {
        result.Reason = "Q countered";
        return result;
    }
    if (context.TargetSpellShield) {
        result.Reason = "preserve hook into spell shield";
        return result;
    }

    const bool forcedUtility = context.TargetKillable ||
        context.PeelDisplacement || context.InterruptUrgent ||
        (context.ObjectiveContest &&
         context.Purpose == HookPurpose::ObjectiveJungler);
    if (context.PullsOntoProtectedCarry &&
        IsDangerousDelivery(context.Archetype) &&
        !context.TargetIsolated && !context.TargetKeyCooldownsSpent &&
        !forcedUtility) {
        result.Reason = "would deliver threat onto carry";
        return result;
    }
    if (context.EnemiesAtLanding > context.AlliesAtLanding + 1 &&
        !context.PullsTowardAlliedTurret && !forcedUtility) {
        result.Reason = "bad pull numbers";
        return result;
    }
    if (!context.FollowupAvailable &&
        context.Purpose != HookPurpose::Peel &&
        context.Purpose != HookPurpose::Interrupt &&
        context.Purpose != HookPurpose::Kill &&
        context.Purpose != HookPurpose::ObjectiveJungler) {
        result.Reason = "no follow-up";
        return result;
    }
    if (context.PlayerAttackWindingUp && context.EPrimedInAttackRange &&
        !forcedUtility) {
        result.Reason = "finish guaranteed Power Fist attack";
        return result;
    }

    float score = 80.0f;
    score += std::clamp(context.TargetPriority, 0.0f, 5.0f) * 55.0f;
    score += static_cast<float>(context.AlliesAtLanding) * 85.0f;
    score -= static_cast<float>(context.EnemiesAtLanding) * 92.0f;
    score += std::clamp(context.CollisionConfidence, 0.0f, 1.0f) * 120.0f;
    if (context.HighConfidence) score += 120.0f;
    if (context.SelectedTarget) score += 90.0f;
    if (context.CursorAgrees) score += 55.0f;
    if (context.TargetImmobile) score += 210.0f;
    if (context.TargetDashEnding) score += 245.0f;
    if (context.TargetLeavingStasis) score += 265.0f;
    if (context.TargetEscapeSpent) score += 135.0f;
    if (context.TargetCanInstantEscape &&
        !context.TargetImmobile && !context.TargetDashEnding) {
        score -= 165.0f;
    }
    if (context.TargetKillable) score += 900.0f;
    if (context.TargetIsolated) score += 135.0f;
    if (context.TargetKeyCooldownsSpent) score += 90.0f;
    if (context.PullsTowardAlliedTurret) score += 330.0f;
    if (IsPremiumCatch(context.Archetype)) score += 170.0f;
    if (IsDangerousDelivery(context.Archetype) &&
        !context.TargetKeyCooldownsSpent && !context.TargetIsolated) {
        score -= 185.0f;
    }
    if (context.PullsOntoProtectedCarry) score -= 280.0f;
    if (context.ProtectedCarryThreatened && context.PeelDisplacement) {
        score += 520.0f;
    }

    switch (context.Purpose) {
    case HookPurpose::ManualCursor: score += 120.0f; break;
    case HookPurpose::SelectedPick: score += 100.0f; break;
    case HookPurpose::ImmobilePunish: score += 130.0f; break;
    case HookPurpose::DashEndpoint: score += 170.0f; break;
    case HookPurpose::StasisExit: score += 185.0f; break;
    case HookPurpose::AllyTurret: score += 230.0f; break;
    case HookPurpose::Peel: score += 330.0f; break;
    case HookPurpose::Interrupt: score += 410.0f; break;
    case HookPurpose::Kill: score += 450.0f; break;
    case HookPurpose::ObjectiveJungler: score += 340.0f; break;
    default: break;
    }

    // Holding Q while walking into a guaranteed E is itself pressure: the
    // opponent cannot sidestep freely and Q becomes guaranteed after knock-up.
    if (context.WEReliableWalkup && !context.TargetEscapeSpent &&
        !forcedUtility && context.Purpose != HookPurpose::AllyTurret) {
        score -= 620.0f;
    }

    result.Score = score;
    result.Cast = score >= 300.0f;
    result.Reason = result.Cast
        ? "valuable first-body pull"
        : (context.WEReliableWalkup
            ? "hold Q pressure for W-E"
            : "hook value below threshold");
    return result;
}

enum class WPurpose : std::uint8_t {
    None,
    WalkUpE,
    HookAngle,
    PostHook,
    Peel,
    Flee,
    Roam,
};

struct WContext {
    bool Ready = false;
    bool HasMana = false;
    bool PlayerAttackWindingUp = false;
    bool IncomingLethal = false;
    bool PathSafe = false;
    bool DestinationUnderEnemyTurret = false;
    bool PointClickThreatAtDestination = false;
    bool TargetValid = false;
    bool TargetEscaping = false;
    bool TargetCanDisengage = false;
    bool EReady = false;
    bool EWillBeInRange = false;
    bool HookLanded = false;
    bool HookReady = false;
    bool BetterHookAngleCreated = false;
    bool AllyNeedsPeel = false;
    bool FutureSelfSlowUnsafe = false;
    bool CurrentSelfSlow = false;
    bool CursorAgrees = false;
    float TargetDistance = FLT_MAX;
    float DistanceClosedBeforeDecay = 0.0f;
    int EnemiesAtDestination = 0;
    int AlliesAtDestination = 0;
    WPurpose Purpose = WPurpose::None;
};

struct WEvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline WEvaluation EvaluateW(const WContext& context) {
    WEvaluation result{};
    if (!context.Ready || !context.HasMana || context.CurrentSelfSlow) {
        result.Reason = "Overdrive unavailable";
        return result;
    }
    if (context.PlayerAttackWindingUp && !context.IncomingLethal) {
        result.Reason = "preserve attack windup";
        return result;
    }
    const bool survival = context.Purpose == WPurpose::Flee ||
        context.Purpose == WPurpose::Peel || context.IncomingLethal;
    if ((!context.PathSafe || context.DestinationUnderEnemyTurret ||
         context.PointClickThreatAtDestination) && !survival) {
        result.Reason = "unsafe W commitment";
        return result;
    }
    if (context.EnemiesAtDestination > context.AlliesAtDestination + 1 &&
        !survival) {
        result.Reason = "W enters bad numbers";
        return result;
    }

    float score = context.CursorAgrees ? 75.0f : 0.0f;
    if (context.TargetEscaping) score += 105.0f;
    if (context.EReady && context.EWillBeInRange) score += 300.0f;
    if (context.HookLanded) score += 210.0f;
    if (context.HookReady && context.BetterHookAngleCreated) score += 175.0f;
    if (context.AllyNeedsPeel) score += 260.0f;
    if (context.DistanceClosedBeforeDecay + 25.0f >= context.TargetDistance) {
        score += 155.0f;
    }
    if (context.TargetCanDisengage && !context.HookLanded &&
        !context.EWillBeInRange) {
        score -= 180.0f;
    }
    if (context.FutureSelfSlowUnsafe && !survival && !context.HookLanded) {
        score -= 420.0f;
    }

    switch (context.Purpose) {
    case WPurpose::WalkUpE: score += 260.0f; break;
    case WPurpose::HookAngle: score += 120.0f; break;
    case WPurpose::PostHook: score += 210.0f; break;
    case WPurpose::Peel: score += 280.0f; break;
    case WPurpose::Flee: score += 360.0f; break;
    case WPurpose::Roam: score += context.PathSafe ? 115.0f : -300.0f; break;
    default: break;
    }

    result.Score = score;
    result.Cast = score >= 250.0f;
    result.Reason = result.Cast
        ? "Overdrive creates a concrete window"
        : "preserve W and avoid self-slow";
    return result;
}

enum class ETiming : std::uint8_t {
    Hold,
    PreArmDuringHook,
    ImmediateOnArrival,
    ResetAfterAttack,
    DelayForEscapeCast,
    PeelNow,
};

struct EContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetInEmpoweredAttackRange = false;
    bool ExactAttackTarget = false;
    bool AttackJustCompleted = false;
    bool PlayerAttackWindingUp = false;
    bool QInFlightToTarget = false;
    bool HookWillLand = false;
    bool TargetCanInstantEscape = false;
    bool TargetEscapeSpent = false;
    bool EscapeHasInterruptibleStartup = false;
    bool EscapeCastStarted = false;
    bool TargetCannotEscape = false;
    bool TargetChanneling = false;
    bool TargetSpellShield = false;
    bool AttackBlockedByZone = false;
    bool BlindOrDodge = false;
    bool TargetKillableByNormalAttack = false;
    bool TargetKillableByEmpoweredAttack = false;
    bool PeelUrgent = false;
    bool WrongUnitAttackPending = false;
    float HookArrivalSeconds = FLT_MAX;
    float EBuffRemainingSeconds = 0.0f;
};

struct EDecision {
    ETiming Timing = ETiming::Hold;
    bool ArmNow = false;
    bool PreserveAttack = true;
    float DelaySeconds = 0.0f;
    const char* Reason = "hold E";
};

inline EDecision EvaluateE(const EContext& context) {
    EDecision result{};
    if (!context.Ready || !context.TargetValid ||
        context.AttackBlockedByZone) {
        result.Reason = context.AttackBlockedByZone
            ? "attack blocker prevents Power Fist"
            : "Power Fist unavailable";
        return result;
    }
    if (context.TargetKillableByNormalAttack &&
        !context.PeelUrgent && !context.TargetCanInstantEscape) {
        result.Reason = "ordinary attack is enough";
        return result;
    }
    if (context.PlayerAttackWindingUp && !context.AttackJustCompleted &&
        !context.QInFlightToTarget && !context.PeelUrgent) {
        result.Reason = "finish current attack before reset";
        return result;
    }

    if (context.PeelUrgent && context.TargetInEmpoweredAttackRange) {
        result.Timing = ETiming::PeelNow;
        result.ArmNow = true;
        result.Reason = context.TargetSpellShield
            ? "peel damage only; spell shield blocks knock-up"
            : "immediate carry peel";
        return result;
    }

    if (context.QInFlightToTarget && context.HookWillLand &&
        context.HookArrivalSeconds >= 0.0f &&
        context.HookArrivalSeconds < kEDurationSeconds - 0.20f) {
        if (context.TargetCanInstantEscape || context.TargetChanneling) {
            result.Timing = ETiming::PreArmDuringHook;
            result.ArmNow = true;
            result.Reason = "pre-arm E so arrival attack cannot lose escape window";
            return result;
        }
        if (context.TargetCannotEscape && context.AttackJustCompleted) {
            result.Timing = ETiming::ResetAfterAttack;
            result.ArmNow = true;
            result.PreserveAttack = false;
            result.Reason = "safe AA-E-AA reset during pull";
            return result;
        }
    }

    if (context.EscapeHasInterruptibleStartup &&
        context.EscapeCastStarted && context.TargetInEmpoweredAttackRange) {
        result.Timing = ETiming::DelayForEscapeCast;
        result.ArmNow = true;
        result.DelaySeconds = 0.0f;
        result.Reason = "knock up the committed escape startup";
        return result;
    }

    if (context.TargetInEmpoweredAttackRange && context.ExactAttackTarget) {
        if (context.AttackJustCompleted &&
            (context.TargetCannotEscape || context.TargetEscapeSpent)) {
            result.Timing = ETiming::ResetAfterAttack;
            result.ArmNow = true;
            result.PreserveAttack = false;
            result.Reason = "AA-E-AA reset without escape window";
            return result;
        }
        if (context.TargetCanInstantEscape ||
            context.TargetKillableByEmpoweredAttack) {
            result.Timing = ETiming::ImmediateOnArrival;
            result.ArmNow = true;
            result.Reason = "do not donate a Flash or dash window";
            return result;
        }
    }

    result.Reason = context.WrongUnitAttackPending
        ? "preserve E for exact hooked target"
        : "wait for attack-reset or guaranteed control window";
    return result;
}

inline bool ShouldBlockWrongAttackWhileEArmed(
    bool eArmed,
    int desiredTargetId,
    int attackTargetId,
    bool desiredTargetReachable,
    float desiredTargetArrivalSeconds,
    float eBuffRemainingSeconds) {
    if (!eArmed || desiredTargetId == 0 || attackTargetId == 0 ||
        desiredTargetId == attackTargetId || !desiredTargetReachable) {
        return false;
    }
    // Narrow gate only: never freeze the orbwalker for a target that will not
    // arrive before E expires.
    return desiredTargetArrivalSeconds >= 0.0f &&
        desiredTargetArrivalSeconds + 0.15f < eBuffRemainingSeconds;
}

struct RMarkState {
    int TargetId = 0;
    int PendingStacks = 0;
    int NextDetonationTick = 0;
    int LastObservedTick = 0;
};

struct RMarkAdvance {
    int DetonatedStacks = 0;
    int PendingStacks = 0;
    int NextDetonationTick = 0;
};

class RMarkTracker {
public:
    void RecordAttack(int targetId, int now) {
        if (targetId == 0) return;
        RMarkState& state = StateFor(targetId);
        if (state.PendingStacks == 0) {
            state.NextDetonationTick = now +
                static_cast<int>(kRMarkIntervalSeconds * 1000.0f);
        }
        ++state.PendingStacks;
        state.LastObservedTick = now;
    }

    void Synchronize(int targetId, int visibleStacks, int now) {
        if (targetId == 0) return;
        RMarkState& state = StateFor(targetId);
        state.PendingStacks = std::max(0, visibleStacks);
        if (state.PendingStacks == 0) {
            state.NextDetonationTick = 0;
        } else if (state.NextDetonationTick <= 0) {
            state.NextDetonationTick = now +
                static_cast<int>(kRMarkIntervalSeconds * 1000.0f);
        }
        state.LastObservedTick = now;
    }

    RMarkAdvance Advance(int targetId, int now) {
        RMarkState* state = Find(targetId);
        if (state == nullptr) return {};
        RMarkAdvance result{};
        const int interval = static_cast<int>(
            kRMarkIntervalSeconds * 1000.0f);
        while (state->PendingStacks > 0 &&
               state->NextDetonationTick > 0 &&
               now >= state->NextDetonationTick) {
            --state->PendingStacks;
            ++result.DetonatedStacks;
            state->NextDetonationTick += interval;
        }
        if (state->PendingStacks == 0) state->NextDetonationTick = 0;
        result.PendingStacks = state->PendingStacks;
        result.NextDetonationTick = state->NextDetonationTick;
        return result;
    }

    int Pending(int targetId) const {
        const RMarkState* state = FindConst(targetId);
        return state != nullptr ? state->PendingStacks : 0;
    }

    float SecondsToNext(int targetId, int now) const {
        const RMarkState* state = FindConst(targetId);
        if (state == nullptr || state->PendingStacks <= 0 ||
            state->NextDetonationTick <= 0) {
            return FLT_MAX;
        }
        return std::max(0, state->NextDetonationTick - now) / 1000.0f;
    }

    void ClearExpired(int now, int staleAfterMs = 12000) {
        States.erase(
            std::remove_if(
                States.begin(), States.end(),
                [now, staleAfterMs](const RMarkState& state) {
                    return state.PendingStacks == 0 &&
                        now - state.LastObservedTick > staleAfterMs;
                }),
            States.end());
    }

private:
    RMarkState& StateFor(int targetId) {
        for (auto& state : States) {
            if (state.TargetId == targetId) return state;
        }
        States.push_back({ targetId, 0, 0, 0 });
        return States.back();
    }

    RMarkState* Find(int targetId) {
        for (auto& state : States) {
            if (state.TargetId == targetId) return &state;
        }
        return nullptr;
    }

    const RMarkState* FindConst(int targetId) const {
        for (const auto& state : States) {
            if (state.TargetId == targetId) return &state;
        }
        return nullptr;
    }

    std::vector<RMarkState> States;
};

inline float RPassiveOpportunityCost(float oneProcDamage,
                                     int expectedAttacksBeforeReady,
                                     float realizationPercent = 0.65f) {
    return std::max(0.0f, oneProcDamage) *
        static_cast<float>(std::max(0, expectedAttacksBeforeReady)) *
        std::clamp(realizationPercent, 0.0f, 1.0f);
}

enum class RPurpose : std::uint8_t {
    None,
    Lethal,
    ShieldBreak,
    Interrupt,
    MidPullSilence,
    PreHookSilence,
    MultiTarget,
    Peel,
};

struct RContext {
    bool Ready = false;
    bool HasMana = false;
    bool TargetInRange = false;
    bool TargetSpellShield = false;
    bool TargetHasDamageShield = false;
    bool CriticalShieldBreak = false;
    bool ChannelInterruptUrgent = false;
    bool EscapeCastMustBeSilenced = false;
    bool HookInFlight = false;
    bool HookWillLand = false;
    bool QReady = false;
    bool QLineClear = false;
    bool MobilePriorityTarget = false;
    bool PeelUrgent = false;
    bool ActiveDamageLethal = false;
    bool PendingPassiveLethal = false;
    bool PendingTickSoon = false;
    bool PlayerAttackWindingUp = false;
    float HookArrivalSeconds = FLT_MAX;
    float TotalShields = 0.0f;
    float ActiveDamage = 0.0f;
    float PassiveOpportunityCost = 0.0f;
    int EnemyHitCount = 0;
    int PriorityEnemyHitCount = 0;
    int PendingMarkStacks = 0;
    RPurpose Purpose = RPurpose::None;
};

struct REvaluation {
    bool Cast = false;
    float Score = -FLT_MAX;
    const char* Reason = "invalid";
};

inline REvaluation EvaluateR(const RContext& context) {
    REvaluation result{};
    if (!context.Ready || !context.HasMana || !context.TargetInRange) {
        result.Reason = "Static Field unavailable";
        return result;
    }
    const bool urgent = context.ChannelInterruptUrgent ||
        context.EscapeCastMustBeSilenced || context.CriticalShieldBreak ||
        context.PeelUrgent;
    if (context.TargetSpellShield && !context.TargetHasDamageShield &&
        !urgent && !context.ActiveDamageLethal) {
        result.Reason = "spell shield absorbs active R";
        return result;
    }
    if (context.PendingPassiveLethal && context.PendingTickSoon && !urgent) {
        result.Reason = "pending lightning already kills";
        return result;
    }
    if (context.PlayerAttackWindingUp && !urgent &&
        !context.ActiveDamageLethal && context.EnemyHitCount < 2) {
        result.Reason = "preserve attack and passive stack";
        return result;
    }

    float score = 40.0f;
    score += static_cast<float>(context.EnemyHitCount) * 105.0f;
    score += static_cast<float>(context.PriorityEnemyHitCount) * 95.0f;
    score += std::min(400.0f, std::max(0.0f, context.TotalShields) * 0.45f);
    score -= std::max(0.0f, context.PassiveOpportunityCost) * 0.35f;
    score -= static_cast<float>(std::max(0, context.PendingMarkStacks)) * 12.0f;
    if (context.TargetHasDamageShield) score += 125.0f;
    if (context.CriticalShieldBreak) score += 620.0f;
    if (context.ChannelInterruptUrgent) score += 850.0f;
    if (context.EscapeCastMustBeSilenced) score += 650.0f;
    if (context.ActiveDamageLethal) score += 900.0f;
    if (context.PeelUrgent) score += 520.0f;
    if (context.HookInFlight && context.HookWillLand &&
        context.HookArrivalSeconds <= 0.70f) {
        score += context.EscapeCastMustBeSilenced ? 390.0f : 80.0f;
    }
    if (context.QReady && context.QLineClear &&
        context.MobilePriorityTarget &&
        context.Purpose == RPurpose::PreHookSilence) {
        score += 330.0f;
    }
    if (context.EnemyHitCount >= 3) score += 280.0f;

    switch (context.Purpose) {
    case RPurpose::Lethal: score += 250.0f; break;
    case RPurpose::ShieldBreak: score += 220.0f; break;
    case RPurpose::Interrupt: score += 320.0f; break;
    case RPurpose::MidPullSilence: score += 230.0f; break;
    case RPurpose::PreHookSilence: score += 170.0f; break;
    case RPurpose::MultiTarget: score += 140.0f; break;
    case RPurpose::Peel: score += 240.0f; break;
    default: break;
    }

    result.Score = score;
    result.Cast = score >= 330.0f;
    result.Reason = result.Cast
        ? "active R payoff beats passive economy"
        : "preserve R passive pressure";
    return result;
}

struct ManaCosts {
    float Q = 100.0f;
    float W = 75.0f;
    float E = 25.0f;
    float R = 0.0f;
};

enum class ManaSequence : std::uint8_t {
    HookE,
    WalkUpEHook,
    PeelE,
    FullCatch,
    EmergencyR,
};

inline float SequenceCost(const ManaCosts& costs, ManaSequence sequence) {
    switch (sequence) {
    case ManaSequence::HookE:
        return std::max(0.0f, costs.Q) + std::max(0.0f, costs.E);
    case ManaSequence::WalkUpEHook:
        return std::max(0.0f, costs.W) + std::max(0.0f, costs.E) +
            std::max(0.0f, costs.Q);
    case ManaSequence::PeelE:
        return std::max(0.0f, costs.E);
    case ManaSequence::FullCatch:
        return std::max(0.0f, costs.Q) + std::max(0.0f, costs.W) +
            std::max(0.0f, costs.E) + std::max(0.0f, costs.R);
    case ManaSequence::EmergencyR:
        return std::max(0.0f, costs.R);
    default:
        return FLT_MAX;
    }
}

inline bool CanAffordSequence(float currentMana,
                              const ManaCosts& costs,
                              ManaSequence sequence,
                              float emergencyReserve = 0.0f,
                              bool emergency = false) {
    const float reserve = emergency
        ? 0.0f : std::max(0.0f, emergencyReserve);
    return std::max(0.0f, currentMana) + 0.01f >=
        SequenceCost(costs, sequence) + reserve;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Blitzcrank::Geometry
