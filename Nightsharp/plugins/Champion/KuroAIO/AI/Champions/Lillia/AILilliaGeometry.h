#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Lillia::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 450.0f;
inline constexpr float kQOuterStart = 300.0f;
inline constexpr float kQOuterEnd = 450.0f;
inline constexpr float kQRadius = 450.0f;
inline constexpr float kQInnerRadius = 300.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kWRange = 500.0f;
inline constexpr float kWRadius = 160.0f;
inline constexpr float kWCenterRadius = 65.0f;
inline constexpr float kWDelay = 0.75f;
inline constexpr float kERange = 1600.0f;
inline constexpr float kEWidth = 110.0f;
inline constexpr float kESpeed = 1400.0f;
inline constexpr float kEDelay = 0.40f;
inline constexpr float kESlowSeconds = 3.0f;
inline constexpr float kRRange = 1600.0f;
inline constexpr float kRSleepDelay = 2.0f;
inline constexpr float kRDrowsySeconds = 3.0f;
inline constexpr float kDreamDurationSeconds = 6.0f;
inline constexpr int kMaxDreamStacks = 5;
inline constexpr float kBaseMoveSpeedPercent = 0.0f;
inline constexpr float kMoveSpeedPerStackPercent = 7.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[static_cast<std::size_t>(std::clamp(rank, 1, 5) - 1)];
}

inline constexpr float QBaseDamage(int rank) {
    return RankValue(rank, {35.0f, 55.0f, 75.0f, 95.0f, 115.0f});
}
inline constexpr float QApRatio() { return 0.40f; }
inline constexpr float QOuterTrueDamage(int rank, float ap) {
    return QBaseDamage(rank) + std::max(0.0f, ap) * QApRatio();
}
inline constexpr float QInnerMagicDamage(int rank, float ap) {
    return QBaseDamage(rank) + std::max(0.0f, ap) * QApRatio();
}
inline constexpr float WBaseDamage(int rank) {
    return RankValue(rank, {70.0f, 100.0f, 130.0f, 160.0f, 190.0f});
}
inline constexpr float WApRatio() { return 0.30f; }
inline constexpr float WDamage(int rank, float ap, bool center) {
    const float damage = WBaseDamage(rank) + std::max(0.0f, ap) * WApRatio();
    return center ? damage * 3.0f : damage;
}
inline constexpr float EBaseDamage(int rank) {
    return RankValue(rank, {70.0f, 100.0f, 130.0f, 160.0f, 190.0f});
}
inline constexpr float EApRatio() { return 0.40f; }
inline constexpr float RBaseDamage(int rank) {
    return RankValue(rank, {100.0f, 150.0f, 200.0f, 250.0f, 300.0f});
}
inline constexpr float RApRatio() { return 0.40f; }

inline bool ValidPoint(const Vec3& point) {
    return point.IsValid() && !point.IsZero();
}

inline bool CircleHits(const Vec3& center, const Vec3& target, float radius,
                       float targetRadius = 0.0f) {
    return ValidPoint(center) && ValidPoint(target) &&
        center.Distance2D(target) <= std::max(0.0f, radius) + std::max(0.0f, targetRadius);
}

inline bool QOuterRingHits(const Vec3& player, const Vec3& target,
                           float targetRadius = 0.0f) {
    if (!ValidPoint(player) || !ValidPoint(target)) return false;
    const float distance = player.Distance2D(target);
    const float radius = std::max(0.0f, targetRadius);
    return distance + radius >= kQOuterStart && distance - radius <= kQOuterEnd;
}

inline bool QInnerAreaHits(const Vec3& player, const Vec3& target,
                           float targetRadius = 0.0f) {
    if (!ValidPoint(player) || !ValidPoint(target)) return false;
    return player.Distance2D(target) <= kQInnerRadius + std::max(0.0f, targetRadius);
}

inline bool QReachable(const Vec3& player, const Vec3& target, float targetRadius = 0.0f) {
    return ValidPoint(player) && ValidPoint(target) &&
        player.Distance2D(target) <= kQRadius + std::max(0.0f, targetRadius);
}

inline bool LineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                     float range = kERange, float width = kEWidth,
                     float targetRadius = 0.0f) {
    if (!ValidPoint(origin) || !ValidPoint(aim) || !ValidPoint(target)) return false;
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 end = origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(aim));
    const auto projection = ProjectPointToSegment2D(target, origin, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width * 0.5f) + std::max(0.0f, targetRadius);
}

inline Vec3 ClampRange(const Vec3& origin, const Vec3& requested, float range) {
    if (!ValidPoint(origin) || !ValidPoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

struct DreamState {
    int Stacks = 0;
    int ExpireTick = 0;
    int LastAppliedTick = 0;
};

inline constexpr DreamState ApplyDream(DreamState state, int now,
                                       int durationMs = 6000) {
    state.Stacks = std::clamp(state.Stacks + 1, 0, kMaxDreamStacks);
    state.LastAppliedTick = now;
    state.ExpireTick = now + std::max(1, durationMs);
    return state;
}
inline constexpr DreamState ReconcileDream(DreamState state, int observedStacks,
                                           int now, int durationMs = 6000) {
    state.Stacks = std::clamp(observedStacks, 0, kMaxDreamStacks);
    if (state.Stacks == 0) state.ExpireTick = 0;
    else if (state.ExpireTick <= now) state.ExpireTick = now + std::max(1, durationMs);
    state.LastAppliedTick = now;
    return state;
}
inline constexpr DreamState ExpireDream(DreamState state, int now) {
    if (state.ExpireTick > 0 && state.ExpireTick <= now) {
        state.Stacks = 0;
        state.ExpireTick = 0;
    }
    return state;
}
inline constexpr float DreamMoveSpeedPercent(const DreamState& state) {
    return kBaseMoveSpeedPercent + std::clamp(state.Stacks, 0, kMaxDreamStacks) * kMoveSpeedPerStackPercent;
}

struct DreamMark {
    int TargetId = 0;
    int ExpireTick = 0;
};
struct MarkState {
    std::array<DreamMark, 8> Marks{};
};

inline constexpr MarkState MarkTarget(MarkState state, int targetId, int now,
                                      int durationMs = 6000) {
    if (targetId == 0) return state;
    for (auto& mark : state.Marks) {
        if (mark.TargetId == targetId || mark.TargetId == 0) {
            mark.TargetId = targetId;
            mark.ExpireTick = now + std::max(1, durationMs);
            return state;
        }
    }
    state.Marks[0] = {targetId, now + std::max(1, durationMs)};
    return state;
}
inline constexpr MarkState ExpireMarks(MarkState state, int now) {
    for (auto& mark : state.Marks) {
        if (mark.TargetId != 0 && mark.ExpireTick > 0 && mark.ExpireTick <= now)
            mark = {};
    }
    return state;
}
inline constexpr bool HasMark(const MarkState& state, int targetId, int now) {
    if (targetId == 0) return false;
    for (const auto& mark : state.Marks)
        if (mark.TargetId == targetId && mark.ExpireTick > now) return true;
    return false;
}

struct SafetyContext {
    bool TargetValid = false;
    bool Lethal = false;
    bool Defensive = false;
    bool UnderTurret = false;
    bool AttackWindingUp = false;
    int NearbyEnemies = 0;
    int MaxEnemies = 3;
};
inline constexpr bool SafeCommit(const SafetyContext& context) {
    if (!context.TargetValid || context.UnderTurret && !context.Lethal && !context.Defensive)
        return false;
    if (context.AttackWindingUp && !context.Lethal && !context.Defensive)
        return false;
    return context.Lethal || context.Defensive ||
        context.NearbyEnemies <= std::max(0, context.MaxEnemies);
}
inline constexpr bool SafeKite(float distance, float preferredDistance,
                              int nearbyEnemies, int maxEnemies) {
    return distance >= std::max(0.0f, preferredDistance) &&
        nearbyEnemies <= std::max(0, maxEnemies);
}
inline constexpr bool ShouldSleep(bool ready, bool marked, bool lethal,
                                  bool defensive, bool underTurret,
                                  int nearbyEnemies, int minimumTargets = 1) {
    if (!ready || !marked || underTurret && !lethal && !defensive) return false;
    return lethal || defensive || nearbyEnemies >= std::max(1, minimumTargets);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Lillia::Geometry
