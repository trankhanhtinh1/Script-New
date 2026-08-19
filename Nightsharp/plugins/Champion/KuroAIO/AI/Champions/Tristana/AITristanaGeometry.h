#pragma once

#include <algorithm>

namespace Plugins::KuroAIO::AI::Controllers::Tristana::Geometry {

inline float DynamicTargetedRange(float currentAttackRange) {
    // Draw a Bead modifies the live attack range and E/R follow that same
    // value. Target bounding radius is applied by the caller exactly once.
    return std::max(0.0f, currentAttackRange);
}

inline int ChargeStacks(int observed) {
    return std::clamp(observed, 0, 4);
}

inline bool WillDetonateCharge(int currentStacks, int addedStacks = 1) {
    return ChargeStacks(currentStacks) + std::max(0, addedStacks) >= 4;
}

inline float ExplosiveChargeRaw(int rank,
                                int stacks,
                                float bonusAttackDamage,
                                float abilityPower,
                                float critChance,
                                float critDamageMultiplier) {
    static constexpr float base[] = {0.0f, 60.0f, 85.0f, 110.0f, 135.0f, 160.0f};
    rank = std::clamp(rank, 0, 5);
    if (rank <= 0) return 0.0f;
    const float critScale = 1.0f +
        std::clamp(critChance, 0.0f, 1.0f) * 0.40f *
        std::max(0.0f, critDamageMultiplier - 1.0f);
    const float stackScale = 1.0f + 0.25f *
        static_cast<float>(ChargeStacks(stacks));
    return (base[rank] + 0.80f * std::max(0.0f, bonusAttackDamage) +
            0.50f * std::max(0.0f, abilityPower)) *
           critScale * stackScale;
}

inline bool ShouldFocusCharge(bool chargeActive,
                              int stacks,
                              bool reachable,
                              bool protectedImmediateKill) {
    return chargeActive && ChargeStacks(stacks) < 4 && reachable &&
           !protectedImmediateKill;
}

inline bool ShouldCastExplosiveCharge(bool ready,
                                      bool targetReachable,
                                      bool attackIntent,
                                      bool alreadyCharged,
                                      bool projectileWall = false) {
    return ready && targetReachable && attackIntent && !alreadyCharged &&
           !projectileWall;
}

struct BusterContext {
    bool InRange = false;
    bool Lethal = false;
    bool DetonationLethal = false;
    bool Gapcloser = false;
    bool SelfPeel = false;
    bool AttackAvailable = false;
    bool ProjectileWall = false;
};

inline bool ShouldCastBusterShot(const BusterContext& context) {
    if (!context.InRange || context.ProjectileWall) return false;
    if (context.Gapcloser || context.SelfPeel) return true;
    return context.Lethal || context.DetonationLethal;
}

struct JumpContext {
    bool PredictionHits = false;
    bool LandingSafe = false;
    bool Lethal = false;
    bool Flee = false;
    bool BetterAttack = false;
    int EnemiesAtLanding = 0;
    int MaximumEnemies = 1;
};

inline bool ShouldRocketJump(const JumpContext& context) {
    if (!context.PredictionHits || !context.LandingSafe ||
        context.EnemiesAtLanding > std::max(0, context.MaximumEnemies)) {
        return false;
    }
    if (context.Flee) return true;
    return context.Lethal && !context.BetterAttack;
}

inline bool ShouldCommitTargetedRocketJump(bool combatMode,
                                           bool recentBusterShot,
                                           bool targetOutsideAttackRange,
                                           bool meaningfulLanding) {
    return combatMode && !recentBusterShot &&
           targetOutsideAttackRange && meaningfulLanding;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Tristana::Geometry
