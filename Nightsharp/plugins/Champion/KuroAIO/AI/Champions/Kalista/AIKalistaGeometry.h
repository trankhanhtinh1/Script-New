#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Kalista::Geometry {

inline constexpr float kQRange = 1150.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kERange = 1000.0f;
inline constexpr float kRRange = 1400.0f;
inline constexpr int kMaxObservedSpears = 100;
inline constexpr int kDefaultEscapeSpearThreshold = 5;

inline int ClampSpearStacks(int observed) {
    return std::clamp(observed, 0, kMaxObservedSpears);
}

inline float RendRawDamage(int spellRank, int spearStacks,
                           float totalAttackDamage, float abilityPower,
                           bool epicObjective = false) {
    if (spellRank <= 0 || spearStacks <= 0) return 0.0f;
    static constexpr float base[] = {-5.0f, 5.0f, 15.0f, 25.0f, 35.0f, 45.0f};
    static constexpr float additionalBase[] = {0.0f, 7.0f, 14.0f, 21.0f, 28.0f, 35.0f};
    static constexpr float additionalAd[] = {
        0.125f, 0.20f, 0.275f, 0.35f, 0.425f, 0.50f};
    const int rank = std::clamp(spellRank, 1, 5);
    const float ad = std::max(0.0f, totalAttackDamage);
    const float ap = std::max(0.0f, abilityPower);
    const float first = base[rank] + 0.70f * ad + 0.65f * ap;
    const float extra = additionalBase[rank] + additionalAd[rank] * ad + 0.50f * ap;
    const float raw = first + extra * static_cast<float>(ClampSpearStacks(spearStacks) - 1);
    return std::max(0.0f, raw * (epicObjective ? 0.50f : 1.0f));
}

inline float ConservativeObjectiveDamage(float finalRendDamage,
                                          bool largeOrEpicObjective) {
    return std::max(0.0f, finalRendDamage) *
        (largeOrEpicObjective ? 0.50f : 1.0f);
}

enum class RendTargetKind {
    Hero,
    LaneMinion,
    JungleMonster,
    EpicMonster,
};

struct RendDecisionContext {
    RendTargetKind TargetKind = RendTargetKind::Hero;
    int SpearStacks = 0;
    int MinimumEscapeStacks = kDefaultEscapeSpearThreshold;
    bool Lethal = false;
    bool TargetEscaping = false;
    bool SafeAdditionalAuto = false;
    bool SpearExpiryImminent = false;
    bool ObjectiveSecure = false;
    bool UnderEnemyTurret = false;
};

inline bool ShouldRend(const RendDecisionContext& input) {
    const int stacks = ClampSpearStacks(input.SpearStacks);
    if (stacks <= 0 || input.UnderEnemyTurret) return false;

    switch (input.TargetKind) {
    case RendTargetKind::Hero:
        if (input.Lethal || input.SpearExpiryImminent) return true;
        return input.TargetEscaping && !input.SafeAdditionalAuto &&
            stacks >= std::clamp(
                input.MinimumEscapeStacks, 1, kMaxObservedSpears);
    case RendTargetKind::EpicMonster:
        return input.ObjectiveSecure;
    case RendTargetKind::LaneMinion:
    case RendTargetKind::JungleMonster:
        return input.Lethal;
    }
    return false;
}

inline bool RendResetsStacks(bool castAccepted, int spearStacks) {
    return castAccepted && ClampSpearStacks(spearStacks) > 0;
}

struct QLineContext {
    bool PredictionHigh = false;
    bool CollisionFree = false;
    bool ProjectileWall = false;
    bool InRange = false;
    bool AttackWindup = false;
    bool Lethal = false;
};

inline bool ShouldCastPierce(const QLineContext& context) {
    if (!context.PredictionHigh || !context.CollisionFree ||
        context.ProjectileWall || !context.InRange) return false;
    return context.Lethal || !context.AttackWindup;
}

struct HopContext {
    bool AttackConfirmed = false;
    bool PlayerAttackWindup = false;
    bool CursorValid = false;
    bool CursorTowardTarget = false;
    bool EmergencyPeel = false;
    bool DestinationSafe = false;
};

inline bool ShouldHop(const HopContext& context) {
    if (!context.AttackConfirmed || !context.CursorValid ||
        !context.DestinationSafe) return false;
    if (context.PlayerAttackWindup && !context.EmergencyPeel) return false;
    return context.CursorTowardTarget || context.EmergencyPeel;
}

struct FateContext {
    bool OathswornBound = false;
    bool AllyInRange = false;
    bool AllyLowHealth = false;
    bool AllyThreatened = false;
    bool AlliedFollowup = false;
    bool EnemyNearAlly = false;
    bool SavePolicyEnabled = true;
    bool EngagePolicyEnabled = false;
};

inline bool ShouldCallFate(const FateContext& context) {
    if (!context.OathswornBound || !context.AllyInRange) return false;
    if (context.SavePolicyEnabled && context.AllyThreatened &&
        (context.AllyLowHealth || context.EnemyNearAlly)) return true;
    return context.EngagePolicyEnabled && context.AlliedFollowup &&
           context.EnemyNearAlly;
}

struct SegmentPoint {
    float x = 0.0f;
    float y = 0.0f;
};

inline bool SegmentContains(const SegmentPoint& origin,
                            const SegmentPoint& destination,
                            const SegmentPoint& point,
                            float range,
                            float halfWidth) {
    const float dx = destination.x - origin.x;
    const float dy = destination.y - origin.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f || range <= 0.0f || halfWidth < 0.0f) return false;
    const float ux = dx / length;
    const float uy = dy / length;
    const float px = point.x - origin.x;
    const float py = point.y - origin.y;
    const float along = px * ux + py * uy;
    if (along < 0.0f || along > range) return false;
    return std::abs(px * uy - py * ux) <= halfWidth;
}

inline SegmentPoint HopDestination(const SegmentPoint& origin,
                                   const SegmentPoint& cursor,
                                   const SegmentPoint& target,
                                   float hopDistance) {
    const float dx = cursor.x - origin.x;
    const float dy = cursor.y - origin.y;
    const float length = std::sqrt(dx * dx + dy * dy);
    if (length <= 0.001f || hopDistance <= 0.0f) return origin;
    const float scale = std::min(hopDistance, length) / length;
    SegmentPoint result{origin.x + dx * scale, origin.y + dy * scale};
    (void)target;
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kalista::Geometry
