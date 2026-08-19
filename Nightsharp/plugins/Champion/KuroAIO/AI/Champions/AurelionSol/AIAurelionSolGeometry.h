#pragma once

// Deterministic Aurelion Sol mechanics and decisions. The runtime controller
// supplies prediction, spell/buff telemetry, terrain and threat samples; this
// file keeps the live Stardust arithmetic, first-body Q beam, continuous-burst
// clock, W route value, E execute/stacking and both R impact shapes testable.

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::AurelionSol::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::kPi;

inline constexpr float kQBeamWidth = 140.0f;
inline constexpr float kQBeamHalfWidth = kQBeamWidth * 0.5f;
inline constexpr float kQTurnDegreesPerSecond = 180.0f;
inline constexpr float kQBurstSeconds = 1.0f;
inline constexpr float kQTapThresholdSeconds = 0.25f;
inline constexpr float kQTapLockoutSeconds = 1.0f;
inline constexpr float kQCooldownSeconds = 3.0f;
inline constexpr float kQMonsterPercentCap = 300.0f;
inline constexpr float kQSplashModifier = 0.50f;
// The server-side splash shape is not exported as a targeter. Runtime plans
// may override this empirical neighborhood; first-body ordering never depends
// on it.
inline constexpr float kDefaultQSplashRadius = 185.0f;

inline constexpr float kWBaseRange = 1500.0f;
inline constexpr float kWRangePerStardust = 7.5f;
inline constexpr float kWRecastLockSeconds = 0.50f;
inline constexpr float kWTakedownWindowSeconds = 3.0f;
inline constexpr float kWCooldownRefundRatio = 0.90f;
inline constexpr float kWFlightBaseMoveSpeed = 340.0f;
inline constexpr float kWQSpeedMultiplier = 0.50f;
inline constexpr float kWFlyingECastRange = 1100.0f;

inline constexpr float kEStartingRadius = 275.0f;
inline constexpr float kEOuterAreaPerStardust = 900.0f;
inline constexpr float kEStartingInnerRadius = 120.0f;
inline constexpr float kEInnerAreaPerStardust = 180.0f;
inline constexpr float kEDurationSeconds = 5.0f;
inline constexpr float kEAppearanceDelaySeconds = 0.50f;
inline constexpr float kECastSeconds = 0.20f;
inline constexpr float kEExecuteBasePercent = 5.0f;
inline constexpr float kEExecutePercentPerStardust = 0.026f;
inline constexpr float kEChampionStardustPerSecond = 1.0f;

inline constexpr float kRRange = 1250.0f;
inline constexpr float kRRegularDelaySeconds = 1.25f;
inline constexpr float kREmpoweredDelaySeconds = 2.0f;
inline constexpr float kRRegularStartingRadius = 275.0f;
inline constexpr float kRRegularAreaPerStardust = 900.0f;
inline constexpr float kREmpoweredBaseAreaMultiplier = 2.0f;
inline constexpr float kREmpoweredAreaPerStardust = 1500.0f;
inline constexpr float kRShockwaveRadius = 5000.0f;
inline constexpr float kRShockwaveSeconds = 3.0f;
inline constexpr float kRStardustPerChampion = 5.0f;
inline constexpr int kRUpgradeRequirement = 75;

inline float LevelScaledCastRange(int championLevel) {
    return 740.0f + 10.0f * static_cast<float>(
        std::clamp(championLevel, 1, 18));
}

inline float QRange(int championLevel) {
    return LevelScaledCastRange(championLevel);
}

inline float ERange(int championLevel, bool flying) {
    return flying ? kWFlyingECastRange : LevelScaledCastRange(championLevel);
}

inline float QMaximumChannelSeconds(int rank, bool flying = false) {
    return flying || rank >= 5 ? 160.0f : 3.25f;
}

inline float QInitialManaCost(int rank) {
    static constexpr std::array<float, 6> values = {
        0.0f, 30.0f, 35.0f, 40.0f, 45.0f, 50.0f,
    };
    return values[std::clamp(rank, 0, 5)];
}

inline float QManaPerSecond(int rank) {
    static constexpr std::array<float, 6> values = {
        0.0f, 35.0f, 40.0f, 45.0f, 50.0f, 55.0f,
    };
    return values[std::clamp(rank, 0, 5)];
}

inline float QExpectedManaCost(int rank, float channelSeconds) {
    return QInitialManaCost(rank) + QManaPerSecond(rank) *
        std::clamp(channelSeconds, 0.0f, QMaximumChannelSeconds(rank));
}

inline float QReleaseCooldownSeconds(float channelSeconds) {
    return channelSeconds + 0.0001f < kQTapThresholdSeconds
        ? kQTapLockoutSeconds
        : kQCooldownSeconds;
}

inline float QDamagePerSecond(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 45.0f, 60.0f, 75.0f, 90.0f, 105.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.55f;
}

inline float QBurstPercentHealthDamage(int stardust,
                                       float targetMaximumHealth,
                                       bool monster) {
    const float damage = static_cast<float>(std::max(0, stardust)) *
        0.00031f * std::max(0.0f, targetMaximumHealth);
    return monster ? std::min(kQMonsterPercentCap, damage) : damage;
}

inline float QBurstRawDamage(int rank,
                             float abilityPower,
                             int stardust,
                             float targetMaximumHealth,
                             bool monster = false) {
    static constexpr std::array<float, 6> base = {
        0.0f, 60.0f, 70.0f, 80.0f, 90.0f, 100.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
           std::max(0.0f, abilityPower) * 0.30f +
           QBurstPercentHealthDamage(
               stardust, targetMaximumHealth, monster);
}

inline float WQDamageMultiplier(int rank) {
    static constexpr std::array<float, 6> values = {
        1.0f, 1.08f, 1.09f, 1.10f, 1.11f, 1.12f,
    };
    return values[std::clamp(rank, 0, 5)];
}

struct BeamUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Champion = false;
    bool Valid = false;
};

inline float BeamEntryDistance(const Vec3& origin,
                               const Vec3& direction,
                               const BeamUnit& unit,
                               float range,
                               float halfWidth = kQBeamHalfWidth) {
    if (!unit.Valid || direction.IsZero()) return FLT_MAX;
    Vec3 relative = unit.Position - origin;
    relative.y = 0.0f;
    const float along = relative.Dot(direction);
    const Vec3 closest = origin + direction * along;
    const float perpendicular = unit.Position.Distance2D(closest);
    const float radius = std::max(0.0f, halfWidth) +
        std::clamp(unit.Radius, 0.0f, 250.0f);
    if (perpendicular > radius) return FLT_MAX;
    const float chord = std::sqrt(std::max(
        0.0f, radius * radius - perpendicular * perpendicular));
    const float entry = along - chord;
    if (along + chord < 0.0f || entry > std::max(0.0f, range)) {
        return FLT_MAX;
    }
    return std::max(0.0f, entry);
}

inline int FirstBeamCollisionIndex(const Vec3& origin,
                                   const Vec3& direction,
                                   const std::vector<BeamUnit>& units,
                                   float range) {
    int bestIndex = -1;
    float bestEntry = FLT_MAX;
    for (std::size_t index = 0; index < units.size(); ++index) {
        const float entry = BeamEntryDistance(
            origin, direction, units[index], range);
        if (entry + 0.001f < bestEntry) {
            bestEntry = entry;
            bestIndex = static_cast<int>(index);
        }
    }
    return bestIndex;
}

inline bool BeamFirstHitsId(const Vec3& origin,
                            const Vec3& direction,
                            const std::vector<BeamUnit>& units,
                            float range,
                            int requestedId) {
    const int index = FirstBeamCollisionIndex(
        origin, direction, units, range);
    return index >= 0 && units[static_cast<std::size_t>(index)].Id ==
        requestedId;
}

inline bool QSplashHits(const BeamUnit& primary,
                        const BeamUnit& secondary,
                        float splashRadius = kDefaultQSplashRadius) {
    return primary.Valid && secondary.Valid && primary.Id != secondary.Id &&
           primary.Position.Distance2D(secondary.Position) <=
               std::max(0.0f, splashRadius) + secondary.Radius;
}

struct QContactState {
    int TargetId = 0;
    float ContinuousSeconds = 0.0f;
    int Bursts = 0;
};

inline QContactState AdvanceQContact(QContactState state,
                                     int targetId,
                                     float elapsedSeconds,
                                     bool touching) {
    if (!touching || targetId == 0) return {};
    if (state.TargetId != targetId) {
        state = {};
        state.TargetId = targetId;
    }
    state.ContinuousSeconds += std::max(0.0f, elapsedSeconds);
    while (state.ContinuousSeconds + 0.0001f >= kQBurstSeconds) {
        state.ContinuousSeconds -= kQBurstSeconds;
        ++state.Bursts;
    }
    return state;
}

inline int QStardustFromBursts(int championBursts) {
    return std::max(0, championBursts) * 2;
}

struct QStartContext {
    bool TargetValid = false;
    bool RequestedTargetFirst = false;
    bool CursorAgrees = false;
    bool PlayerAttackWindup = false;
    bool IncomingHardCrowdControl = false;
    bool UnsafeMelee = false;
    bool PlayerRecentlyCast = false;
    bool Flying = false;
    bool TargetControlled = false;
    float ExpectedContactSeconds = 0.0f;
    float AvailableMana = 0.0f;
    float RequiredMana = 0.0f;
};

inline bool ShouldStartQ(const QStartContext& context) {
    if (!context.TargetValid || !context.RequestedTargetFirst ||
        !context.CursorAgrees || context.PlayerAttackWindup ||
        context.IncomingHardCrowdControl || context.UnsafeMelee ||
        context.PlayerRecentlyCast ||
        context.AvailableMana + 0.5f < context.RequiredMana) {
        return false;
    }
    return context.Flying || context.TargetControlled ||
           context.ExpectedContactSeconds >= 0.82f;
}

struct QStopContext {
    bool ControllerOwned = false;
    bool TapPurpose = false;
    bool IncomingHardCrowdControl = false;
    bool UnsafeMelee = false;
    bool PrimaryContact = true;
    bool TargetAlive = true;
    bool ManaReserveBroken = false;
    bool PlayerSteeringAway = false;
    float ChannelSeconds = 0.0f;
    float NoContactSeconds = 0.0f;
    float BurstDueSeconds = 1.0f;
};

inline bool ShouldStopQ(const QStopContext& context) {
    if (!context.ControllerOwned) return false;
    if (context.IncomingHardCrowdControl || context.UnsafeMelee ||
        !context.TargetAlive || context.ManaReserveBroken ||
        context.PlayerSteeringAway) {
        return true;
    }
    if (context.TapPurpose) {
        return context.ChannelSeconds >= 0.10f;
    }
    if (!context.PrimaryContact && context.NoContactSeconds >= 0.18f &&
        context.BurstDueSeconds > 0.20f) {
        return true;
    }
    return false;
}

inline float WRange(int stardust) {
    return kWBaseRange + kWRangePerStardust *
        static_cast<float>(std::max(0, stardust));
}

inline float WFlightSpeed(float bonusMoveSpeed, bool channelingQ) {
    const float speed = kWFlightBaseMoveSpeed +
        std::max(0.0f, bonusMoveSpeed);
    return speed * (channelingQ ? kWQSpeedMultiplier : 1.0f);
}

inline float WRemainingCooldownAfterTakedown(float remainingSeconds) {
    return std::max(0.0f, remainingSeconds) *
        (1.0f - kWCooldownRefundRatio);
}

struct FlightSample {
    Vec3 Position = {};
    float TargetDistance = FLT_MAX;
    int NearbyEnemies = 0;
    int NearbyAllies = 0;
    bool QHasPrimaryContact = false;
    bool EnemyTurret = false;
    bool ReadyHardCrowdControl = false;
    bool PointClickLockdown = false;
    bool DashHazard = false;
    bool TerrainSeparatesThreat = false;
};

struct FlightContext {
    Vec3 Origin = {};
    Vec3 Destination = {};
    Vec3 Target = {};
    float QRange = 750.0f;
    float CursorDot = 1.0f;
    float PlayerHealthPercent = 100.0f;
    bool GroundedOrImmobilized = false;
    bool PlayerRecentlyCast = false;
    bool TargetValid = true;
    bool TakedownReset = false;
    bool EscapeRoute = false;
    bool DirectDive = false;
    std::vector<FlightSample> Samples = {};
};

inline float FlightRouteScore(const FlightContext& context) {
    if (!context.TargetValid || context.GroundedOrImmobilized ||
        context.PlayerRecentlyCast || context.Origin.Distance2D(
            context.Destination) < 125.0f ||
        context.CursorDot < -0.15f || context.Samples.empty()) {
        return -100000.0f;
    }
    float score = context.CursorDot * 240.0f +
        std::clamp(context.PlayerHealthPercent, 0.0f, 100.0f) * 1.4f;
    int contactSamples = 0;
    for (const auto& sample : context.Samples) {
        if (sample.TargetDistance <= context.QRange + 80.0f &&
            sample.QHasPrimaryContact) {
            ++contactSamples;
            score += 170.0f;
        } else if (sample.TargetDistance <= context.QRange + 180.0f) {
            score += 35.0f;
        } else {
            score -= 80.0f;
        }
        score += static_cast<float>(sample.NearbyAllies) * 42.0f;
        score -= static_cast<float>(sample.NearbyEnemies) * 58.0f;
        if (sample.TerrainSeparatesThreat) score += 105.0f;
        if (sample.EnemyTurret) score -= 1450.0f;
        if (sample.ReadyHardCrowdControl) score -= 520.0f;
        if (sample.PointClickLockdown) score -= 760.0f;
        if (sample.DashHazard) score -= 560.0f;
    }
    if (contactSamples >= static_cast<int>(context.Samples.size()) - 1) {
        score += 260.0f;
    }
    if (context.DirectDive) score -= 310.0f;
    if (context.TakedownReset) score += context.EscapeRoute ? 360.0f : 90.0f;
    if (context.PlayerHealthPercent < 34.0f && !context.EscapeRoute) {
        score -= 620.0f;
    }
    return score;
}

struct FlightStopContext {
    bool Flying = false;
    bool RecastUnlocked = false;
    bool TargetOutsideQ = false;
    bool EndpointTurret = false;
    bool LockdownAhead = false;
    bool PlayerCursorReversed = false;
    bool SafeResetExitReached = false;
    bool CurrentRouteStillScores = true;
};

inline bool ShouldStopFlight(const FlightStopContext& context) {
    return context.Flying && context.RecastUnlocked &&
        (context.EndpointTurret || context.LockdownAhead ||
         context.PlayerCursorReversed || context.SafeResetExitReached ||
         (context.TargetOutsideQ && !context.CurrentRouteStillScores));
}

inline float RadiusFromArea(float startingRadius,
                            float areaPerStack,
                            int stardust) {
    const float baseArea = std::max(0.0f, startingRadius) *
        std::max(0.0f, startingRadius);
    const float growth = static_cast<float>(std::max(0, stardust)) *
        std::max(0.0f, areaPerStack) / kPi;
    return std::sqrt(baseArea + growth);
}

inline float SingularityRadius(int stardust) {
    return RadiusFromArea(
        kEStartingRadius, kEOuterAreaPerStardust, stardust);
}

inline float SingularityInnerRadius(int stardust) {
    return RadiusFromArea(
        kEStartingInnerRadius, kEInnerAreaPerStardust, stardust);
}

inline float SingularityExecutePercent(int stardust) {
    return kEExecuteBasePercent + kEExecutePercentPerStardust *
        static_cast<float>(std::max(0, stardust));
}

inline bool SingularityDamageContains(const Vec3& center,
                                      const Vec3& target,
                                      float targetRadius,
                                      int stardust) {
    return center.Distance2D(target) <= SingularityRadius(stardust) +
        std::clamp(targetRadius, 0.0f, 250.0f);
}

inline bool SingularityCenterContains(const Vec3& center,
                                      const Vec3& target,
                                      int stardust) {
    return center.Distance2D(target) <= SingularityInnerRadius(stardust);
}

inline bool SingularityExecutes(float health,
                                float maximumHealth,
                                bool epicMonster,
                                bool centerContains,
                                int stardust) {
    if (epicMonster || !centerContains || maximumHealth <= 0.0f) return false;
    return std::max(0.0f, health) / maximumHealth * 100.0f <=
        SingularityExecutePercent(stardust);
}

inline float SingularityDamagePerSecond(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {
        0.0f, 10.0f, 15.0f, 20.0f, 25.0f, 30.0f,
    };
    return base[std::clamp(rank, 0, 5)] +
        std::max(0.0f, abilityPower) * 0.12f;
}

enum class EUnitKind : std::uint8_t {
    SmallMinion,
    LargeMinion,
    SmallMonster,
    LargeMonster,
    Champion,
    EpicMonster,
};

inline int DeathStardust(EUnitKind kind) {
    switch (kind) {
    case EUnitKind::LargeMinion:
    case EUnitKind::LargeMonster:
    case EUnitKind::Champion:
    case EUnitKind::EpicMonster:
        return 2;
    default:
        return 1;
    }
}

struct SingularityUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Health = 0.0f;
    float MaximumHealth = 1.0f;
    float Priority = 1.0f;
    float ExpectedSecondsInside = 0.0f;
    EUnitKind Kind = EUnitKind::SmallMinion;
    bool ExpectedToDie = false;
    bool Primary = false;
    bool HardCrowdControlled = false;
    bool Valid = false;
};

struct SingularityEvaluation {
    int Units = 0;
    int Champions = 0;
    int ExpectedDeaths = 0;
    int ExpectedStardust = 0;
    int Executions = 0;
    float ChampionSeconds = 0.0f;
    float Score = -FLT_MAX;
};

inline SingularityEvaluation EvaluateSingularity(
    const Vec3& center,
    int stardust,
    const std::vector<SingularityUnit>& units,
    bool farming) {
    SingularityEvaluation result{};
    result.Score = 0.0f;
    for (const auto& unit : units) {
        if (!unit.Valid || !SingularityDamageContains(
                center, unit.Position, unit.Radius, stardust)) {
            continue;
        }
        ++result.Units;
        const bool champion = unit.Kind == EUnitKind::Champion;
        const bool epic = unit.Kind == EUnitKind::EpicMonster;
        if (champion) {
            ++result.Champions;
            result.ChampionSeconds += std::clamp(
                unit.ExpectedSecondsInside, 0.0f, kEDurationSeconds);
            result.ExpectedStardust += static_cast<int>(std::floor(
                std::clamp(unit.ExpectedSecondsInside, 0.0f,
                           kEDurationSeconds) + 0.0001f));
        }
        const bool centerContains = SingularityCenterContains(
            center, unit.Position, stardust);
        const bool execute = SingularityExecutes(
            unit.Health, unit.MaximumHealth, epic,
            centerContains, stardust);
        if (execute) ++result.Executions;
        if (unit.ExpectedToDie || execute) {
            ++result.ExpectedDeaths;
            result.ExpectedStardust += DeathStardust(unit.Kind);
        }
        float value = std::max(0.15f, unit.Priority) *
            (champion ? 3.4f : 0.75f);
        value += unit.Primary ? 2.2f : 0.0f;
        value += unit.HardCrowdControlled ? 0.8f : 0.0f;
        value += (unit.ExpectedToDie || execute)
            ? static_cast<float>(DeathStardust(unit.Kind)) * 1.8f
            : 0.0f;
        if (farming && champion) value += 2.4f;
        result.Score += value;
    }
    result.Score += static_cast<float>(result.ExpectedStardust) * 1.3f;
    if (result.Units == 0) result.Score = -FLT_MAX;
    return result;
}

inline int CannonWaveStardust(int smallMinions = 6) {
    return std::max(0, smallMinions) *
               DeathStardust(EUnitKind::SmallMinion) +
           DeathStardust(EUnitKind::LargeMinion);
}

inline float FallingStarRadius(int stardust) {
    return RadiusFromArea(
        kRRegularStartingRadius, kRRegularAreaPerStardust, stardust);
}

inline float SkiesDescendRadius(int stardust) {
    const float baseSquared = kRRegularStartingRadius *
        kRRegularStartingRadius * kREmpoweredBaseAreaMultiplier;
    const float growth = static_cast<float>(std::max(0, stardust)) *
        kREmpoweredAreaPerStardust / kPi;
    return std::sqrt(baseSquared + growth);
}

inline float UltimateImpactDelay(bool empowered) {
    return empowered ? kREmpoweredDelaySeconds : kRRegularDelaySeconds;
}

inline float FallingStarRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {
        0.0f, 150.0f, 250.0f, 350.0f,
    };
    return base[std::clamp(rank, 0, 3)] +
        std::max(0.0f, abilityPower) * 0.75f;
}

inline float SkiesDescendDirectRawDamage(int rank, float abilityPower) {
    return FallingStarRawDamage(rank, abilityPower) * 1.25f;
}

inline float SkiesDescendShockwaveRawDamage(int rank, float abilityPower) {
    return FallingStarRawDamage(rank, abilityPower) * 0.90f;
}

inline Vec3 ResolveUltimateImpactCenter(const Vec3& requested,
                                        const Vec3& projectileWallContact,
                                        bool intercepted) {
    return intercepted && !projectileWallContact.IsZero()
        ? projectileWallContact
        : requested;
}

struct UltimateUnit {
    Vec3 Position = {};
    float Radius = 0.0f;
    float Priority = 1.0f;
    bool Champion = true;
    bool EpicMonster = false;
    bool Primary = false;
    bool SpellShield = false;
    bool HardCrowdControlled = false;
    bool Dashing = false;
    bool Lethal = false;
    bool Valid = false;
};

struct UltimateEvaluation {
    Vec3 ImpactCenter = {};
    int DirectHits = 0;
    int DirectChampionHits = 0;
    int ShockwaveDamageHits = 0;
    int ShockwaveSlowHits = 0;
    int ExpectedStardust = 0;
    bool PrimaryDirect = false;
    float Score = -FLT_MAX;
};

inline UltimateEvaluation EvaluateUltimate(
    const Vec3& center,
    int stardust,
    bool empowered,
    const std::vector<UltimateUnit>& units) {
    UltimateEvaluation result{};
    result.ImpactCenter = center;
    result.Score = 0.0f;
    const float radius = empowered
        ? SkiesDescendRadius(stardust)
        : FallingStarRadius(stardust);
    for (const auto& unit : units) {
        if (!unit.Valid) continue;
        const bool direct = center.Distance2D(unit.Position) <=
            radius + std::clamp(unit.Radius, 0.0f, 250.0f);
        if (direct) {
            ++result.DirectHits;
            if (unit.Champion) ++result.DirectChampionHits;
            if (unit.Primary) result.PrimaryDirect = true;
            if (unit.Champion && !unit.SpellShield) {
                result.ExpectedStardust +=
                    static_cast<int>(kRStardustPerChampion);
            }
            float value = std::max(0.15f, unit.Priority) * 3.1f;
            value += unit.Primary ? 2.1f : 0.0f;
            value += unit.Dashing ? 1.0f : 0.0f;
            value += unit.Lethal ? 2.8f : 0.0f;
            value -= unit.HardCrowdControlled ? 0.85f : 0.0f;
            value -= unit.SpellShield ? 2.5f : 0.0f;
            result.Score += value;
            continue;
        }
        if (!empowered || center.Distance2D(unit.Position) >
                kRShockwaveRadius + unit.Radius) {
            continue;
        }
        ++result.ShockwaveSlowHits;
        if (unit.Champion || unit.EpicMonster) {
            ++result.ShockwaveDamageHits;
            result.Score += std::max(0.15f, unit.Priority) * 1.35f;
            if (unit.Lethal) result.Score += 1.5f;
        } else {
            result.Score += 0.18f;
        }
    }
    result.Score += static_cast<float>(result.ExpectedStardust) * 0.32f;
    if (result.DirectHits == 0 && result.ShockwaveSlowHits == 0) {
        result.Score = -FLT_MAX;
    }
    return result;
}

inline bool UltimateCanInterrupt(float remainingChannelSeconds,
                                 bool empowered,
                                 float reactionAllowance = 0.10f) {
    return UltimateImpactDelay(empowered) <=
        std::max(0.0f, remainingChannelSeconds) +
        std::max(0.0f, reactionAllowance);
}

struct CalamityState {
    int Progress = 0;
    bool Ready = false;
};

inline CalamityState AdvanceCalamity(CalamityState state,
                                     int stardustGained,
                                     bool empoweredUltimateConsumed) {
    if (empoweredUltimateConsumed) state = {};
    state.Progress = std::clamp(
        state.Progress + std::max(0, stardustGained),
        0, kRUpgradeRequirement);
    state.Ready = state.Progress >= kRUpgradeRequirement;
    return state;
}

inline bool ShouldSpendEmpoweredUltimate(
    const UltimateEvaluation& evaluation,
    bool objectiveFight,
    bool peelWinCondition,
    bool primaryThreatCommitted,
    int minimumDirectHits = 2) {
    if (evaluation.Score <= -FLT_MAX * 0.5f) return false;
    if (evaluation.DirectHits >= std::max(1, minimumDirectHits)) return true;
    if (evaluation.PrimaryDirect &&
        (peelWinCondition || primaryThreatCommitted)) return true;
    return objectiveFight &&
        (evaluation.DirectHits >= 1 || evaluation.ShockwaveDamageHits >= 2);
}

} // namespace Plugins::KuroAIO::AI::Controllers::AurelionSol::Geometry
