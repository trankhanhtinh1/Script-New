#pragma once

#include <algorithm>

namespace Plugins::KuroAIO::AI::Controllers::Varus::Geometry {

inline constexpr float kMinimumQRange = 925.0f;
inline constexpr float kMaximumQRange = 1625.0f;
inline constexpr float kFullChargeSeconds = 1.25f;

inline float ChargedQRange(float elapsedSeconds) {
    const float t = std::clamp(
        elapsedSeconds / kFullChargeSeconds, 0.0f, 1.0f);
    return kMinimumQRange + (kMaximumQRange - kMinimumQRange) * t;
}

inline float RequiredChargeSeconds(float distance) {
    if (distance <= kMinimumQRange) return 0.0f;
    return std::clamp(
        (distance - kMinimumQRange) /
            (kMaximumQRange - kMinimumQRange) * kFullChargeSeconds,
        0.0f, kFullChargeSeconds);
}

inline bool ShouldDetonateBlight(int stacks,
                                 bool safeAdditionalAuto,
                                 bool targetEscaping,
                                 bool lethal) {
    stacks = std::clamp(stacks, 0, 3);
    if (stacks >= 3) return true;
    if (stacks < 2) return lethal;
    return lethal || targetEscaping || !safeAdditionalAuto;
}

struct QStartContext {
    int BlightStacks = 0;
    bool PredictionHits = false;
    bool InMaximumRange = false;
    bool SafeAdditionalAuto = false;
    bool TargetEscaping = false;
    bool OutsideAttackRange = false;
    bool Lethal = false;
    bool ProjectileWall = false;
};

inline bool ShouldStartQ(const QStartContext& context) {
    return context.PredictionHits && context.InMaximumRange &&
           !context.ProjectileWall &&
           (context.OutsideAttackRange || context.Lethal ||
            ShouldDetonateBlight(context.BlightStacks,
                                 context.SafeAdditionalAuto,
                                 context.TargetEscaping,
                                 context.Lethal));
}

struct QReleaseContext {
    int BlightStacks = 0;
    bool Charging = false;
    bool PredictionHits = false;
    bool InCurrentRange = false;
    bool SafeAdditionalAuto = false;
    bool TargetEscaping = false;
    bool Lethal = false;
    bool ProjectileWall = false;
    bool ChargeExpiring = false;
};

inline bool ShouldReleaseQ(const QReleaseContext& context) {
    if (!context.Charging || !context.PredictionHits ||
        !context.InCurrentRange || context.ProjectileWall) {
        return false;
    }
    return context.ChargeExpiring || context.Lethal ||
           ShouldDetonateBlight(context.BlightStacks,
                                context.SafeAdditionalAuto,
                                context.TargetEscaping,
                                context.Lethal);
}

inline bool ShouldEmpowerQ(bool wReady,
                           bool alreadyEmpowered,
                           bool committedQ,
                           bool lethalOrLowHealth) {
    return wReady && !alreadyEmpowered && committedQ && lethalOrLowHealth;
}

struct ChainContext {
    bool Manual = false;
    bool PredictionVeryHigh = false;
    bool InRange = false;
    bool ProjectileWall = false;
    bool Interrupt = false;
    bool SelfPeel = false;
    bool AlliedFollowup = false;
    bool TargetLethal = false;
};

inline bool ShouldCastChain(const ChainContext& context) {
    return context.InRange && context.PredictionVeryHigh &&
           !context.ProjectileWall &&
           (context.Manual || context.Interrupt || context.SelfPeel ||
            (context.AlliedFollowup && context.TargetLethal));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Varus::Geometry
