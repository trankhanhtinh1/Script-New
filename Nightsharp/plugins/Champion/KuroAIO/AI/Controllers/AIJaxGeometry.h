#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Jax::Geometry {

inline constexpr int kMaximumPassiveStacks = 8;
inline constexpr int kPassiveDurationMs = 2500;
inline constexpr float kQRange = 700.0f;
inline constexpr float kERadius = 375.0f;
inline constexpr int kEMinimumRecastMs = 1000;
inline constexpr int kEMaximumDurationMs = 2000;
inline constexpr int kRDurationMs = 8000;

struct PassiveLedger {
    int Stacks = 0;
    int ExpiresAt = 0;
};

inline PassiveLedger NormalizePassive(PassiveLedger ledger, int now) {
    ledger.Stacks = std::clamp(ledger.Stacks, 0, kMaximumPassiveStacks);
    if (ledger.ExpiresAt <= 0 || now >= ledger.ExpiresAt) return {};
    return ledger;
}

inline PassiveLedger ObservePassiveAttack(PassiveLedger ledger, int now) {
    ledger = NormalizePassive(ledger, now);
    ledger.Stacks = std::min(kMaximumPassiveStacks, ledger.Stacks + 1);
    ledger.ExpiresAt = now + kPassiveDurationMs;
    return ledger;
}

inline float PassiveAttackSpeedPercent(int stacks, float perStackPercent) {
    return static_cast<float>(std::clamp(stacks, 0, kMaximumPassiveStacks)) *
           std::max(0.0f, perStackPercent);
}

inline bool GrandmasterPassiveProc(int completedAttacks) {
    return completedAttacks > 0 && completedAttacks % 3 == 0;
}

inline Vec3 LeapEndpoint(const Vec3& targetPosition) {
    return targetPosition.IsValid() && !targetPosition.IsZero() ? targetPosition : Vec3{};
}

struct LeapContext {
    bool TargetValid = false;
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool UnderEnemyTurret = false;
    bool StartedUnderEnemyTurret = false;
    bool EnemyTarget = true;
    bool Lethal = false;
    bool Fleeing = false;
    bool RetreatProgress = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool LeapEndpointSafe(const LeapContext& context) {
    if (!context.TargetValid || !context.EndpointValid || context.EndpointWall) {
        return false;
    }
    if (context.UnderEnemyTurret && !context.StartedUnderEnemyTurret &&
        !(context.EnemyTarget && context.Lethal)) {
        return false;
    }
    if (context.Fleeing && !context.RetreatProgress) return false;
    if (context.NearbyEnemies > std::max(0, context.MaximumEnemies) &&
        !(context.EnemyTarget && context.Lethal)) {
        return false;
    }
    return true;
}

struct WResetContext {
    bool Ready = false;
    bool AttackCompleted = false;
    bool ExactTarget = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    int MillisecondsSinceAttack = 0;
};

inline bool MayResetWithW(const WResetContext& context) {
    if (!context.Ready) return false;
    if (context.AttackWindingUp && !context.Lethal) return false;
    const bool resetWindow = context.AttackCompleted && context.ExactTarget &&
        context.MillisecondsSinceAttack >= 0 && context.MillisecondsSinceAttack <= 360;
    return resetWindow || context.Lethal;
}

struct CounterStrikeState {
    bool Active = false;
    int StartedAt = 0;
    int DodgedBasicAttacks = 0;
};

inline CounterStrikeState NormalizeCounterStrike(CounterStrikeState state, int now) {
    state.DodgedBasicAttacks = std::clamp(state.DodgedBasicAttacks, 0, 5);
    if (!state.Active || state.StartedAt <= 0 ||
        now - state.StartedAt >= kEMaximumDurationMs) {
        return {};
    }
    return state;
}

inline bool CounterStrikeAvoidsBasicAttack(bool active,
                                           bool incomingBasicAttack,
                                           bool targetsJax,
                                           bool turretAttack) {
    return active && incomingBasicAttack && targetsJax && !turretAttack;
}

inline float CounterStrikeDamageMultiplier(int dodgedBasicAttacks) {
    return 1.0f + 0.20f * static_cast<float>(std::clamp(dodgedBasicAttacks, 0, 5));
}

struct EStartContext {
    bool Ready = false;
    bool IncomingBasicAttack = false;
    bool Defensive = false;
    bool ComboCommit = false;
    bool TargetInRadius = false;
    bool AttackWindingUp = false;
};

inline bool ShouldStartCounterStrike(const EStartContext& context) {
    if (!context.Ready) return false;
    if (context.IncomingBasicAttack || context.Defensive) return true;
    if (context.AttackWindingUp) return false;
    return context.ComboCommit && context.TargetInRadius;
}

struct ERecastContext {
    bool Active = false;
    int ElapsedMs = 0;
    bool TargetInRadius = false;
    bool TargetEscaping = false;
    bool Interrupt = false;
    bool Lethal = false;
    bool Fleeing = false;
    int DodgedBasicAttacks = 0;
};

inline bool ShouldRecastCounterStrike(const ERecastContext& context) {
    if (!context.Active || context.ElapsedMs < kEMinimumRecastMs) return false;
    if (context.ElapsedMs >= kEMaximumDurationMs - 80) return true;
    if (context.Interrupt || context.Lethal || context.Fleeing ||
        context.TargetEscaping || !context.TargetInRadius) {
        return true;
    }
    return context.ElapsedMs >= 1750 && context.DodgedBasicAttacks >= 1;
}

struct RContext {
    bool Ready = false;
    bool AlreadyActive = false;
    bool TargetValid = false;
    bool Duel = false;
    bool TargetCommitted = false;
    bool TargetDangerous = false;
    bool IncomingBurst = false;
    float PlayerHealthPercent = 100.0f;
    float DefensiveHealthPercent = 45.0f;
    int NearbyEnemies = 0;
    int NearbyAllies = 0;
};

inline bool MayActivateGrandmaster(const RContext& context) {
    if (!context.Ready || context.AlreadyActive || !context.TargetValid) return false;
    const bool emergency = context.IncomingBurst ||
        context.PlayerHealthPercent <= context.DefensiveHealthPercent;
    const bool outnumbered = context.NearbyEnemies > std::max(1, context.NearbyAllies + 1);
    const bool meaningfulDuel = context.Duel && context.TargetCommitted &&
        (context.TargetDangerous || context.PlayerHealthPercent <= 70.0f);
    return emergency || outnumbered || meaningfulDuel;
}

struct AutomaticContext {
    bool ManualOwnership = false;
    bool Defensive = false;
    bool IncomingBasicAttack = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    if (context.ManualOwnership || context.Engage) return false;
    return context.Defensive || context.IncomingBasicAttack ||
           context.Interrupt || context.KillSecure;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Jax::Geometry
