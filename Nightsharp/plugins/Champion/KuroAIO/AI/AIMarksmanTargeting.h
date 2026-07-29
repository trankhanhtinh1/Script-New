#pragma once

// Pure target-reach policy shared by the long-range marksman controllers.
// Champion controllers are responsible for prediction, collision and damage
// telemetry; this layer only turns those facts into an auditable decision.

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::MarksmanTargeting {

enum class ReachRoute : std::uint8_t {
    None,
    AutoAttack,
    DirectSpell,
    SetupSpell,
    ExecuteSpell,
};

struct TargetContext {
    bool Valid = true;
    bool Targetable = true;
    bool DamageImmune = false;
    bool AutoReachable = false;
    bool DirectSpellReachable = false;
    bool SetupReachable = false;
    bool ExecuteReachable = false;
    bool Selected = false;
    bool Locked = false;
    bool OrbwalkerTarget = false;
    bool CrowdControlled = false;
    bool Dashing = false;
    bool Escaping = false;
    bool SpellShield = false;
    bool ProjectileBlocked = false;
    bool Killable = false;

    float Distance = FLT_MAX;
    float MaximumReach = 0.0f;
    float HealthPercent = 100.0f;
    float EffectiveHealth = 1.0f;
    float EstimatedDamage = 0.0f;
    float Priority = 0.0f;
};

struct TargetEvaluation {
    ReachRoute Route = ReachRoute::None;
    float Score = -FLT_MAX;
    bool Valid = false;
};

inline TargetEvaluation EvaluateTarget(const TargetContext& context) {
    TargetEvaluation result{};
    if (!context.Valid || !context.Targetable || context.DamageImmune ||
        !std::isfinite(context.Distance) || context.Distance < 0.0f ||
        (context.MaximumReach > 0.0f &&
         context.Distance > context.MaximumReach + 1.0f)) {
        return result;
    }

    if (context.AutoReachable) {
        result.Route = ReachRoute::AutoAttack;
    } else if (context.DirectSpellReachable) {
        result.Route = ReachRoute::DirectSpell;
    } else if (context.ExecuteReachable) {
        result.Route = ReachRoute::ExecuteSpell;
    } else if (context.SetupReachable) {
        result.Route = ReachRoute::SetupSpell;
    } else {
        return result;
    }

    const float health = std::max(1.0f, context.EffectiveHealth);
    const float damageRatio = std::clamp(
        std::max(0.0f, context.EstimatedDamage) / health, 0.0f, 2.0f);
    const float rangeRatio = context.MaximumReach > 1.0f
        ? std::clamp(context.Distance / context.MaximumReach, 0.0f, 1.5f)
        : 1.0f;

    float score = context.Priority;
    score += (100.0f - std::clamp(context.HealthPercent, 0.0f, 100.0f)) * 1.7f;
    score += damageRatio * 185.0f;
    score -= rangeRatio * 145.0f;

    if (context.Killable || damageRatio >= 1.0f) score += 520.0f;
    if (context.Selected) score += 245.0f;
    if (context.Locked) score += 95.0f;
    if (context.OrbwalkerTarget) score += 285.0f;
    if (context.CrowdControlled) score += 105.0f;
    if (context.Dashing) score += 22.0f;
    if (context.Escaping) score += 38.0f;

    switch (result.Route) {
    case ReachRoute::AutoAttack:
        score += 85.0f;
        break;
    case ReachRoute::DirectSpell:
        score += 48.0f;
        break;
    case ReachRoute::ExecuteSpell:
        score += 25.0f;
        break;
    case ReachRoute::SetupSpell:
        score -= 115.0f;
        break;
    default:
        break;
    }

    // A shield or a projectile blocker is not a blanket rejection when the
    // marksman can still auto attack or use a non-projectile route. It does,
    // however, keep that target below an equally reachable clean target.
    if (context.SpellShield && !context.AutoReachable) score -= 175.0f;
    if (context.ProjectileBlocked) score -= 70.0f;

    result.Score = score;
    result.Valid = std::isfinite(score);
    return result;
}

inline bool BetterTarget(const TargetEvaluation& candidate,
                         const TargetEvaluation& incumbent) {
    if (!candidate.Valid) return false;
    if (!incumbent.Valid) return true;
    return candidate.Score > incumbent.Score + 0.001f;
}

} // namespace Plugins::KuroAIO::AI::MarksmanTargeting
