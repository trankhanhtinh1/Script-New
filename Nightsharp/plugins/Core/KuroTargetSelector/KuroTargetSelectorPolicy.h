#pragma once

#include "KuroTargetSelectorContracts.h"
#include <cfloat>

#include <algorithm>
#include <cmath>

namespace SDK::KuroTargetSelector {

struct TargetProfileWeights {
    float Priority = 36.0f;
    float Health = 70.0f;
    float Distance = 28.0f;
    float Threat = 20.0f;
    float CrowdControl = 12.0f;
    float Dash = 8.0f;
    float EffectiveDamage = 24.0f;
    float Stickiness = 80.0f;
    // Allies near the target can follow up and share pressure.
    float FollowUp = 20.0f;
    // Enemy champions hugging the target make an engage harder to survive.
    float EnemyDensity = 24.0f;
    // Confidence from the active prediction provider.  FsPred already folds
    // path stability, reaction horizon, cast distance, and immobility into
    // HitChance, so the selector should consume that calibrated result rather
    // than trying to duplicate its motion model.
    float Prediction = 125.0f;
};

class KuroTargetSelectorPolicy final {
public:
    static TargetProfile ProfileFor(TargetPurpose purpose) {
        switch (purpose) {
        case TargetPurpose::AutoAttack: return TargetProfile::AutoAttack;
        case TargetPurpose::ComboPrimary: return TargetProfile::Burst;
        case TargetPurpose::Harass:
        case TargetPurpose::Poke: return TargetProfile::Poke;
        case TargetPurpose::Execute: return TargetProfile::Execute;
        case TargetPurpose::Peel: return TargetProfile::Peel;
        case TargetPurpose::Interrupt: return TargetProfile::Interrupt;
        case TargetPurpose::AntiGapcloser: return TargetProfile::AntiGapcloser;
        case TargetPurpose::FleeThreat: return TargetProfile::FleeThreat;
        case TargetPurpose::ManualAssist: return TargetProfile::General;
        case TargetPurpose::General:
        default: return TargetProfile::General;
        }
    }

    static TargetProfile ProfileFor(const TargetRequest& request) {
        return request.HasProfileHint
            ? request.ProfileHint
            : ProfileFor(request.Purpose);
    }

    static const char* ProfileName(TargetProfile profile) {
        switch (profile) {
        case TargetProfile::AutoAttack: return "AutoAttack";
        case TargetProfile::Burst: return "Burst";
        case TargetProfile::DPS: return "DPS";
        case TargetProfile::Poke: return "Poke";
        case TargetProfile::Execute: return "Execute";
        case TargetProfile::Peel: return "Peel";
        case TargetProfile::Interrupt: return "Interrupt";
        case TargetProfile::AntiGapcloser: return "AntiGapcloser";
        case TargetProfile::FleeThreat: return "FleeThreat";
        case TargetProfile::General:
        default: return "General";
        }
    }

    static TargetProfileWeights Weights(TargetProfile profile) {
        TargetProfileWeights weights{};
        switch (profile) {
        case TargetProfile::AutoAttack:
            weights = { 44.0f, 42.0f, 34.0f, 10.0f, 12.0f, 6.0f,
                        52.0f, 110.0f, 25.0f, 40.0f, 20.0f };
            break;
        case TargetProfile::Burst:
            weights = { 46.0f, 64.0f, 20.0f, 22.0f, 18.0f, 10.0f,
                        78.0f, 72.0f, 30.0f, 35.0f, 150.0f };
            break;
        case TargetProfile::DPS:
            weights = { 42.0f, 35.0f, 30.0f, 18.0f, 10.0f, 6.0f,
                        42.0f, 130.0f, 20.0f, 25.0f, 110.0f };
            break;
        case TargetProfile::Poke:
            weights = { 34.0f, 18.0f, 32.0f, 24.0f, 22.0f, 6.0f,
                        35.0f, 55.0f, 15.0f, 20.0f, 175.0f };
            break;
        case TargetProfile::Execute:
            weights = { 45.0f, 100.0f, 18.0f, 12.0f, 8.0f, 4.0f,
                        100.0f, 90.0f, 25.0f, 22.0f, 155.0f };
            break;
        case TargetProfile::Peel:
            weights = { 48.0f, 12.0f, 46.0f, 84.0f, 18.0f, 20.0f,
                        12.0f, 100.0f, 8.0f, 30.0f, 140.0f };
            break;
        case TargetProfile::Interrupt:
            weights = { 80.0f, 8.0f, 28.0f, 35.0f, 120.0f, 12.0f,
                        5.0f, 95.0f, 10.0f, 35.0f, 180.0f };
            break;
        case TargetProfile::AntiGapcloser:
            weights = { 72.0f, 12.0f, 38.0f, 82.0f, 18.0f, 130.0f,
                        5.0f, 92.0f, 5.0f, 45.0f, 175.0f };
            break;
        case TargetProfile::FleeThreat:
            // Threat and proximity dominate.  Health is intentionally low so
            // fleeing never turns into a lowest-health target selection.
            weights = { 52.0f, 2.0f, 74.0f, 96.0f, 8.0f, 32.0f,
                        2.0f, 60.0f, 10.0f, 75.0f, 95.0f };
            break;
        case TargetProfile::General:
        default:
            // Value-initialization applies TargetProfileWeights' non-zero
            // member defaults. Keep this explicit so General cannot silently
            // become an all-zero profile after future refactors.
            weights = TargetProfileWeights{};
            break;
        }
        return weights;
    }

    static float PredictionExpectedHitFactor(const TargetRequest& request,
                                              const TargetFacts& facts) {
        if (!facts.PredictionEvaluated) return 1.0f;

        const auto chance = static_cast<HitChance>(
            facts.PredictionHitChance);
        const bool blockingCollision = request.Route.RequireNoCollision &&
            (facts.PredictionCollides || chance == HitChance::Collision);
        if (blockingCollision) return 0.0f;
        if (chance == HitChance::Collision) return 0.55f;
        if (!facts.PredictionAvailable) {
            return chance == HitChance::OutOfRange ? 0.0f : 0.12f;
        }
        switch (chance) {
        case HitChance::Low: return 0.32f;
        case HitChance::Medium: return 0.55f;
        case HitChance::High: return 0.76f;
        case HitChance::VeryHigh: return 0.90f;
        case HitChance::Immobile: return 0.98f;
        case HitChance::Dash: return 0.94f;
        case HitChance::Collision: return 0.55f;
        case HitChance::OutOfRange: return 0.0f;
        case HitChance::None:
        default: return 0.12f;
        }
    }

    // Backward-compatible helper keeps the historical blocking-projectile
    // interpretation for direct policy callers.
    static float PredictionExpectedHitFactor(const TargetFacts& facts) {
        TargetRequest blockingRequest{};
        blockingRequest.Route.RequireNoCollision = true;
        return PredictionExpectedHitFactor(blockingRequest, facts);
    }

    static float PredictionPreferenceFactor(const TargetRequest& request,
                                            const TargetFacts& facts) {
        if (!facts.PredictionEvaluated) return 0.0f;

        const auto chance = static_cast<HitChance>(
            facts.PredictionHitChance);
        const bool blockingCollision = request.Route.RequireNoCollision &&
            (facts.PredictionCollides || chance == HitChance::Collision);
        if (blockingCollision) return -1.25f;
        if (chance == HitChance::Collision) return 0.0f;
        if (!facts.PredictionAvailable) {
            return chance == HitChance::OutOfRange ? -1.25f : -0.90f;
        }
        switch (chance) {
        case HitChance::Low: return -0.70f;
        case HitChance::Medium: return 0.0f;
        case HitChance::High: return 0.65f;
        case HitChance::VeryHigh: return 1.0f;
        case HitChance::Immobile:
        case HitChance::Dash: return 1.15f;
        case HitChance::Collision: return 0.0f;
        case HitChance::OutOfRange: return -1.25f;
        case HitChance::None:
        default: return -0.90f;
        }
    }

    static float PredictionPreferenceFactor(const TargetFacts& facts) {
        TargetRequest blockingRequest{};
        blockingRequest.Route.RequireNoCollision = true;
        return PredictionPreferenceFactor(blockingRequest, facts);
    }

    static float RawHealthPoolFor(const TargetRequest& request,
                                  const TargetFacts& facts,
                                  float health,
                                  bool includeShields) {
        const float safeHealth = std::isfinite(health)
            ? std::max(0.0f, health) : 0.0f;
        if (request.Damage.IgnoreShields ||
            !includeShields || !request.Damage.IncludeShields) {
            return safeHealth;
        }

        const float allShield = std::isfinite(facts.AllShield)
            ? std::max(0.0f, facts.AllShield) : 0.0f;
        const float physicalShield = std::isfinite(facts.PhysicalShield)
            ? std::max(0.0f, facts.PhysicalShield) : 0.0f;
        const float magicalShield = std::isfinite(facts.MagicalShield)
            ? std::max(0.0f, facts.MagicalShield) : 0.0f;

        switch (request.Damage.Type) {
        case DamageType::Physical:
            return safeHealth + allShield + physicalShield;
        case DamageType::Magical:
            return safeHealth + allShield + magicalShield;
        case DamageType::Mixed:
            return safeHealth + allShield +
                (physicalShield + magicalShield) * 0.5f;
        case DamageType::True:
        default:
            return safeHealth + allShield;
        }
    }

    static float StoredEffectiveHealthFor(const TargetRequest& request,
                                          const TargetFacts& facts) {
        switch (request.Damage.Type) {
        case DamageType::Physical:
            return facts.PhysicalEffectiveHealth;
        case DamageType::Magical:
            return facts.MagicalEffectiveHealth;
        case DamageType::Mixed:
            return facts.MixedEffectiveHealth;
        case DamageType::True:
        default:
            return facts.TrueEffectiveHealth;
        }
    }

    static float MitigationMultiplierFor(const TargetRequest& request,
                                         const TargetFacts& facts) {
        // True damage bypasses armor/MR. Shields remain part of the raw health
        // pool and are handled independently by RawHealthPoolFor().
        if (request.Damage.Type == DamageType::True) return 1.0f;

        // Stored effective-health pools include their applicable shields.  Use
        // the same reference pool to isolate mitigation, even when the caller
        // wants the final score to ignore shields.
        TargetRequest reference = request;
        reference.Damage.IncludeShields = true;
        reference.Damage.IgnoreShields = false;
        const float currentPool = RawHealthPoolFor(
            reference, facts, facts.Health, true);
        const float stored = StoredEffectiveHealthFor(request, facts);
        if (currentPool <= 0.0f || !std::isfinite(stored) || stored <= 0.0f) {
            return 1.0f;
        }
        return std::clamp(stored / currentPool, 0.05f, 20.0f);
    }

    static float EffectiveHealthFor(const TargetRequest& request,
                                    const TargetFacts& facts) {
        const float currentPool = RawHealthPoolFor(
            request, facts, facts.Health, request.Damage.IncludeShields);
        if (currentPool <= 0.0f) return 1.0f;

        if (request.Damage.IncludeShields &&
            !request.Damage.IgnoreShields) {
            const float stored = StoredEffectiveHealthFor(request, facts);
            if (std::isfinite(stored) && stored > 0.0f) {
                return stored;
            }
        }
        return currentPool * MitigationMultiplierFor(request, facts);
    }

    static float MaxEffectiveHealthFor(const TargetRequest& request,
                                       const TargetFacts& facts) {
        const float maxPool = RawHealthPoolFor(
            request, facts, facts.MaxHealth, request.Damage.IncludeShields);
        if (maxPool <= 0.0f) {
            return EffectiveHealthFor(request, facts);
        }
        return maxPool * MitigationMultiplierFor(request, facts);
    }

    static float EstimatedDamageFor(const TargetRequest& request,
                                    const TargetFacts& facts) {
        const float expectedHits = std::isfinite(request.Damage.ExpectedHits)
            ? std::max(1.0f, request.Damage.ExpectedHits) : 1.0f;
        if (std::isfinite(request.Damage.RawDamage) &&
            request.Damage.RawDamage > 0.0f) {
            return request.Damage.RawDamage * expectedHits;
        }
        if (std::isfinite(facts.ActionDamageEstimate) &&
            facts.ActionDamageEstimate > 0.0f) {
            return facts.ActionDamageEstimate * expectedHits;
        }

        switch (request.Damage.Type) {
        case DamageType::Physical:
            return std::max(0.0f, facts.AutoAttackDamage) * expectedHits;
        case DamageType::Magical:
            return std::max(0.0f, facts.MagicalDamageEstimate) * expectedHits;
        case DamageType::Mixed:
            return (std::max(0.0f, facts.AutoAttackDamage) +
                    std::max(0.0f, facts.MagicalDamageEstimate)) *
                0.5f * expectedHits;
        case DamageType::True:
        default:
            return 0.0f;
        }
    }

    static float DamageRatioFor(const TargetRequest& request,
                                const TargetFacts& facts) {
        const float effectiveHealth = EffectiveHealthFor(request, facts);
        if (effectiveHealth <= 0.0f) return 0.0f;
        return std::clamp(
            EstimatedDamageFor(request, facts) / effectiveHealth,
            0.0f, 2.0f);
    }

    static RejectReason ValidatePurpose(const TargetRequest& request,
                                        const TargetFacts& facts) {
        if (request.Purpose == TargetPurpose::Interrupt) {
            if (request.Route.ExactEventSenderId != 0 &&
                facts.NetworkId != request.Route.ExactEventSenderId) {
                return RejectReason::PurposeRejected;
            }
            if (!facts.IsChanneling &&
                (request.Route.ExactEventSenderId == 0 ||
                 request.Phase == DecisionPhase::Execution)) {
                return RejectReason::PurposeRejected;
            }
        }
        if (request.Purpose == TargetPurpose::AntiGapcloser) {
            if (request.Route.ExactEventSenderId != 0 &&
                facts.NetworkId != request.Route.ExactEventSenderId) {
                return RejectReason::PurposeRejected;
            }
            if (!facts.IsDashing &&
                (request.Route.ExactEventSenderId == 0 ||
                 request.Phase == DecisionPhase::Execution)) {
                return RejectReason::PurposeRejected;
            }
        }
        return RejectReason::None;
    }

    static float BuildScore(const TargetRequest& request,
                            const TargetFacts& facts,
                            int configuredPriority,
                            int incumbentNetworkId,
                            ScoreBreakdown& breakdown) {
        return BuildScoreForProfile(
            request,
            facts,
            configuredPriority,
            incumbentNetworkId,
            breakdown,
            ProfileFor(request),
            -1.0f);
    }

    static float BuildScoreForProfile(const TargetRequest& request,
                                      const TargetFacts& facts,
                                      int configuredPriority,
                                      int incumbentNetworkId,
                                      ScoreBreakdown& breakdown,
                                      TargetProfile profile,
                                      float stickinessOverride = -1.0f) {
        const TargetProfileWeights weights = Weights(profile);
        const float stickiness = std::isfinite(stickinessOverride) &&
                stickinessOverride >= 0.0f
            ? stickinessOverride
            : weights.Stickiness;
        const float priority = std::clamp(
            static_cast<float>(configuredPriority), 0.0f, 5.0f);
        const float effectiveHealth = EffectiveHealthFor(request, facts);
        const float maxEffectiveHealth = MaxEffectiveHealthFor(request, facts);
        const float healthRatio = maxEffectiveHealth > 0.0f
            ? std::clamp(effectiveHealth / maxEffectiveHealth, 0.0f, 2.0f)
            : 0.0f;
        const float missingHealth = std::clamp(1.0f - healthRatio, -1.0f, 1.0f);
        const bool favorsDamageOpportunity =
            profile != TargetProfile::FleeThreat &&
            profile != TargetProfile::Peel &&
            profile != TargetProfile::Interrupt &&
            profile != TargetProfile::AntiGapcloser;
        const bool allInProfile =
            request.Purpose == TargetPurpose::ComboPrimary ||
            request.Purpose == TargetPurpose::Execute ||
            profile == TargetProfile::AutoAttack ||
            profile == TargetProfile::Burst ||
            profile == TargetProfile::DPS ||
            profile == TargetProfile::Execute;
        const float boundedHealthRatio = std::clamp(healthRatio, 0.0f, 1.0f);
        // A very low-health enemy must be able to break a stale priority/
        // incumbent choice in offensive modes, but not defensive threat
        // selection.  The 60% ramp is deliberately gradual; only a target
        // around 25% or lower can overcome a maximum priority plus stickiness.
        const float lowHealthUrgency = favorsDamageOpportunity
            ? std::clamp((0.60f - boundedHealthRatio) / 0.60f, 0.0f, 1.0f)
            : 0.0f;
        float scoringRange = request.Range;
        if (request.Route.Kind == RouteKind::AutoAttack &&
            !request.Route.RangeIncludesHitboxes &&
            std::isfinite(scoringRange)) {
            scoringRange += std::max(0.0f, request.Route.SourceBoundingRadius) +
                std::max(0.0f, facts.BoundingRadius);
        }
        const float distanceScale = scoringRange > 0.0f
            ? std::clamp(1.0f - facts.DistanceToSource /
                         std::max(scoringRange, 1.0f), -1.0f, 1.0f)
            : 0.0f;
        const float threat = std::clamp(
            (facts.AttackDamage * 0.55f + facts.AbilityPower * 0.35f) /
                180.0f,
            0.0f, 4.0f);
        const float projectedDamage = EstimatedDamageFor(request, facts);
        const float damageRatio = DamageRatioFor(request, facts);
        const float expectedHitFactor =
            PredictionExpectedHitFactor(request, facts);
        const float expectedDamageRatio = damageRatio * expectedHitFactor;

        breakdown.Add("priority", "configured priority",
                      priority * weights.Priority, 0.0f, 260.0f);
        if (incumbentNetworkId > 0 &&
            facts.NetworkId == incumbentNetworkId) {
            breakdown.Add(
                "incumbent-stickiness",
                "current target stickiness",
                stickiness,
                0.0f,
                240.0f);
        }
        if (profile == TargetProfile::FleeThreat) {
            breakdown.Add("health", "health safety", -missingHealth * weights.Health,
                          -15.0f, 15.0f);
        } else {
            breakdown.Add("health", "effective health",
                          missingHealth * weights.Health, -100.0f, 100.0f);
        }
        if (lowHealthUrgency > 0.0f) {
            breakdown.Add(
                "low-health-execute",
                "low-health execute opportunity",
                lowHealthUrgency * 500.0f,
                0.0f,
                500.0f);
        }
        breakdown.Add("distance", "source distance",
                      distanceScale * weights.Distance, -100.0f, 100.0f);
        breakdown.Add("threat", "carry threat",
                      threat * weights.Threat, 0.0f, 400.0f);
        // Nearby allies can follow up and nearby enemies can contest the
        // kill; both only matter on offensive profiles.
        if (favorsDamageOpportunity) {
            const float allyFollowUp = std::clamp(
                static_cast<float>(facts.AlliesNearTarget), 0.0f, 3.0f);
            const float enemyContest = std::clamp(
                static_cast<float>(facts.EnemiesNearTarget), 0.0f, 4.0f);
            breakdown.Add(
                "ally-followup",
                "allies near target",
                allyFollowUp * weights.FollowUp,
                0.0f,
                160.0f);
            breakdown.Add(
                "enemy-contest",
                "enemies near target",
                -enemyContest * weights.EnemyDensity,
                -300.0f,
                0.0f);
        }
        float ccScore = 0.0f;
        if (facts.IsCrowdControlled) ccScore += weights.CrowdControl;
        if (facts.IsKnockedUp) ccScore += 45.0f;
        if (facts.IsSuppressed) ccScore += 50.0f;
        if (facts.IsSlowed) ccScore += 20.0f;
        if (facts.IsGrounded) ccScore += 25.0f;

        breakdown.Add("crowd-control", "crowd control & immobilize",
                      ccScore, 0.0f, 220.0f);

        if (facts.DebuffScore > 0.0f || facts.HasVulnerableMark) {
            const float markBonus = facts.HasVulnerableMark ? 40.0f : 0.0f;
            const float debuffBonus = std::clamp(facts.DebuffScore + markBonus, 0.0f, 300.0f);
            breakdown.Add("debuff-synergy", "debuff & mark synergy bonus",
                          debuffBonus, 0.0f, 300.0f);
        }
        breakdown.Add("dash", "dash state",
                      facts.IsDashing ? weights.Dash : 0.0f, 0.0f, 140.0f);
        if (facts.PredictionEvaluated) {
            breakdown.Add(
                "prediction-confidence",
                "prediction hit confidence",
                PredictionPreferenceFactor(request, facts) * weights.Prediction,
                -260.0f,
                260.0f);
            if (facts.PredictionCollides && request.Route.RequireNoCollision) {
                breakdown.Add(
                    "prediction-collision",
                    "predicted route collision",
                    -weights.Prediction * 1.65f,
                    -320.0f,
                    0.0f);
            }
        }
        breakdown.Add("damage-efficiency", "type-aware damage efficiency",
                      expectedDamageRatio * weights.EffectiveDamage,
                      0.0f, 220.0f);
        if (allInProfile && projectedDamage > 0.0f) {
            breakdown.Add(
                "all-in-feasibility",
                "prediction-adjusted all-in feasibility",
                std::clamp(expectedDamageRatio, 0.0f, 1.0f) * 180.0f,
                0.0f,
                180.0f);
            if (expectedDamageRatio >= 1.0f) {
                breakdown.Add(
                    "lethal-confidence",
                    "all-in can kill target",
                    profile == TargetProfile::Execute ? 180.0f : 140.0f,
                    0.0f,
                    180.0f);
            }
        }
        return breakdown.Total;
    }
};

} // namespace SDK::KuroTargetSelector

namespace Plugins::KuroTargetSelector {
using ::SDK::KuroTargetSelector::KuroTargetSelectorPolicy;
using ::SDK::KuroTargetSelector::TargetProfileWeights;
} // namespace Plugins::KuroTargetSelector
