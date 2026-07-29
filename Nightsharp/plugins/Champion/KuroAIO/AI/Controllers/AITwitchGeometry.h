#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Twitch::Geometry {

inline int VenomStacks(int observed) {
    return std::clamp(observed, 0, 6);
}

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};

inline bool SprayLineContains(const Vec2& origin,
                              const Vec2& aim,
                              const Vec2& point,
                              float range,
                              float halfWidth) {
    const float dx = aim.x - origin.x;
    const float dy = aim.y - origin.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 1.0f || range <= 0.0f) return false;
    const float ux = dx / length;
    const float uy = dy / length;
    const float px = point.x - origin.x;
    const float py = point.y - origin.y;
    const float along = px * ux + py * uy;
    if (along < 0.0f || along > range) return false;
    const float perpendicular = std::abs(px * uy - py * ux);
    return perpendicular <= std::max(0.0f, halfWidth);
}

inline float ContaminatePhysicalRaw(int rank,
                                    int stacks,
                                    float bonusAttackDamage) {
    static constexpr float base[] = {0, 20, 30, 40, 50, 60};
    static constexpr float perStack[] = {0, 15, 20, 25, 30, 35};
    rank = std::clamp(rank, 0, 5);
    const float count = static_cast<float>(VenomStacks(stacks));
    return rank == 0 ? 0.0f :
        base[rank] + count *
            (perStack[rank] + 0.35f * std::max(0.0f, bonusAttackDamage));
}

inline float ContaminateMagicRaw(int stacks, float abilityPower) {
    return static_cast<float>(VenomStacks(stacks)) *
           0.35f * std::max(0.0f, abilityPower);
}

struct ContaminateContext {
    int Stacks = 0;
    bool Lethal = false;
    bool EscapingRange = false;
    bool SafeAdditionalAuto = false;
    bool PoisonExpiring = false;
};

inline bool ShouldContaminate(const ContaminateContext& context) {
    const int stacks = VenomStacks(context.Stacks);
    if (stacks <= 0) return false;
    if (context.Lethal || context.PoisonExpiring || stacks >= 6) return true;
    return context.EscapingRange &&
           (stacks >= 3 || !context.SafeAdditionalAuto);
}

struct CaskContext {
    bool PredictionHits = false;
    bool ProjectileWall = false;
    bool AttackAvailable = false;
    bool TargetEscaping = false;
    bool Immobilized = false;
    bool Flee = false;
    bool AddsFirstStack = false;
    bool ContaminateLethal = false;
};

inline bool ShouldThrowCask(const CaskContext& context) {
    if (!context.PredictionHits || context.ProjectileWall) return false;
    if (context.Flee) return true;
    if (context.ContaminateLethal) return false;
    return !context.AttackAvailable &&
           (context.TargetEscaping || context.Immobilized ||
            context.AddsFirstStack);
}

inline bool ShouldMaintainVenomFocus(int stacks,
                                     bool currentlyReachable,
                                     bool protectedImmediateKill) {
    return VenomStacks(stacks) > 0 && VenomStacks(stacks) < 6 &&
           currentlyReachable && !protectedImmediateKill;
}

inline bool ShouldAmbush(bool ready,
                         bool alreadyHidden,
                         bool incomingDamage,
                         bool approachWindow,
                         bool fleeWindow) {
    return ready && !alreadyHidden && !incomingDamage &&
           (approachWindow || fleeWindow);
}

struct SprayContext {
    bool Ready = false;
    bool AlreadyActive = false;
    bool AttackIntent = false;
    bool ProjectileWall = false;
    bool NeedsBonusRange = false;
    bool LethalAttackWindow = false;
    int PiercingTargets = 0;
    int MinimumEnemies = 2;
};

inline bool ShouldSprayAndPray(const SprayContext& context) {
    return context.Ready && !context.AlreadyActive && context.AttackIntent &&
           !context.ProjectileWall &&
           (context.NeedsBonusRange || context.LethalAttackWindow ||
            context.PiercingTargets >=
                std::max(1, context.MinimumEnemies));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Twitch::Geometry
