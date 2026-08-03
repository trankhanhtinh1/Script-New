#pragma once

#include "KuroTargetSelectorContracts.h"

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
                        52.0f, 110.0f };
            break;
        case TargetProfile::Burst:
            weights = { 46.0f, 64.0f, 20.0f, 22.0f, 18.0f, 10.0f,
                        78.0f, 72.0f };
            break;
        case TargetProfile::DPS:
            weights = { 42.0f, 35.0f, 30.0f, 18.0f, 10.0f, 6.0f,
                        42.0f, 130.0f };
            break;
        case TargetProfile::Poke:
            weights = { 34.0f, 18.0f, 32.0f, 24.0f, 22.0f, 6.0f,
                        35.0f, 55.0f };
            break;
        case TargetProfile::Execute:
            weights = { 45.0f, 100.0f, 18.0f, 12.0f, 8.0f, 4.0f,
                        100.0f, 90.0f };
            break;
        case TargetProfile::Peel:
            weights = { 48.0f, 12.0f, 46.0f, 84.0f, 18.0f, 20.0f,
                        12.0f, 100.0f };
            break;
        case TargetProfile::Interrupt:
            weights = { 80.0f, 8.0f, 28.0f, 35.0f, 120.0f, 12.0f,
                        5.0f, 95.0f };
            break;
        case TargetProfile::AntiGapcloser:
            weights = { 72.0f, 12.0f, 38.0f, 82.0f, 18.0f, 130.0f,
                        5.0f, 92.0f };
            break;
        case TargetProfile::FleeThreat:
            // Threat and proximity dominate.  Health is intentionally low so
            // fleeing never turns into a lowest-health target selection.
            weights = { 52.0f, 2.0f, 74.0f, 96.0f, 8.0f, 32.0f,
                        2.0f, 60.0f };
            break;
        case TargetProfile::General:
        default:
            break;
        }
        return weights;
    }

    static RejectReason ValidatePurpose(const TargetRequest& request,
                                        const TargetFacts& facts) {
        if (request.Purpose == TargetPurpose::Interrupt) {
            if (request.Route.ExactEventSenderId != 0 &&
                facts.NetworkId != request.Route.ExactEventSenderId) {
                return RejectReason::PurposeRejected;
            }
            if (!facts.IsChanneling && request.Route.ExactEventSenderId == 0) {
                return RejectReason::PurposeRejected;
            }
        }
        if (request.Purpose == TargetPurpose::AntiGapcloser) {
            if (request.Route.ExactEventSenderId != 0 &&
                facts.NetworkId != request.Route.ExactEventSenderId) {
                return RejectReason::PurposeRejected;
            }
            if (!facts.IsDashing && request.Route.ExactEventSenderId == 0) {
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
            ProfileFor(request.Purpose));
    }

    static float BuildScoreForProfile(const TargetRequest& request,
                                      const TargetFacts& facts,
                                      int configuredPriority,
                                      int incumbentNetworkId,
                                      ScoreBreakdown& breakdown,
                                      TargetProfile profile) {
        const TargetProfileWeights weights = Weights(profile);
        const float priority = std::clamp(
            static_cast<float>(configuredPriority), 0.0f, 5.0f);
        const float maxHealth = std::isfinite(facts.MaxHealth)
            ? std::max(1.0f, facts.MaxHealth) : 1.0f;
        const float effectiveHealth = std::isfinite(facts.EffectiveHealth)
            ? std::max(0.0f, facts.EffectiveHealth) : maxHealth;
        const float healthRatio = std::clamp(effectiveHealth / maxHealth,
                                             0.0f, 2.0f);
        const float missingHealth = std::clamp(1.0f - healthRatio, -1.0f, 1.0f);
        const bool favorsDamageOpportunity =
            profile != TargetProfile::FleeThreat &&
            profile != TargetProfile::Peel &&
            profile != TargetProfile::Interrupt &&
            profile != TargetProfile::AntiGapcloser;
        const float boundedHealthRatio = std::clamp(healthRatio, 0.0f, 1.0f);
        // A very low-health enemy must be able to break a stale priority/
        // incumbent choice in offensive modes, but not defensive threat
        // selection.  The 60% ramp is deliberately gradual; only a target
        // around 25% or lower can overcome a maximum priority plus stickiness.
        const float lowHealthUrgency = favorsDamageOpportunity
            ? std::clamp((0.60f - boundedHealthRatio) / 0.60f, 0.0f, 1.0f)
            : 0.0f;
        const float distanceScale = request.Range > 0.0f
            ? std::clamp(1.0f - facts.DistanceToSource /
                         std::max(request.Range, 1.0f), -1.0f, 1.0f)
            : 0.0f;
        const float threat = std::clamp(
            (facts.AttackDamage * 0.55f + facts.AbilityPower * 0.35f) /
                180.0f,
            0.0f, 4.0f);
        const float damageRatio = effectiveHealth > 0.0f
            ? std::clamp(
                std::max(0.0f, facts.AutoAttackDamage) *
                    std::max(1.0f, request.Damage.ExpectedHits) /
                    effectiveHealth,
                0.0f, 2.0f)
            : 0.0f;

        breakdown.Add("priority", "configured priority",
                      priority * weights.Priority, 0.0f, 260.0f);
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
        breakdown.Add("crowd-control", "crowd control",
                      facts.IsCrowdControlled ? weights.CrowdControl : 0.0f,
                      0.0f, 100.0f);
        breakdown.Add("dash", "dash state",
                      facts.IsDashing ? weights.Dash : 0.0f, 0.0f, 140.0f);
        breakdown.Add("damage-efficiency", "damage efficiency",
                      damageRatio * weights.EffectiveDamage, 0.0f, 220.0f);
        if (incumbentNetworkId != 0 && facts.NetworkId == incumbentNetworkId) {
            breakdown.Add("stickiness", "incumbent target",
                          weights.Stickiness, 0.0f, 180.0f);
        }

        if (request.Purpose == TargetPurpose::Execute &&
            request.Damage.RawDamage > 0.0f) {
            const float lethal = request.Damage.RawDamage >= facts.EffectiveHealth
                ? 80.0f : 0.0f;
            breakdown.Add("lethal-confidence", "lethal confidence",
                          lethal, 0.0f, 80.0f);
        }
        return breakdown.Total;
    }
};

} // namespace SDK::KuroTargetSelector

namespace Plugins::KuroTargetSelector {
using ::SDK::KuroTargetSelector::KuroTargetSelectorPolicy;
using ::SDK::KuroTargetSelector::TargetProfileWeights;
} // namespace Plugins::KuroTargetSelector
