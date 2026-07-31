#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Vex::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using Vec3 = ::Vec3;

inline constexpr float kQRange = 1200.0f;
inline constexpr float kQWidth = 100.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQSpeed = 1900.0f;
inline constexpr float kWRange = 475.0f;
inline constexpr float kWRadius = 475.0f;
inline constexpr float kEDistance = 800.0f;
inline constexpr float kERadius = 250.0f;
inline constexpr float kEResolveSeconds = 0.75f;
inline constexpr float kRRange = 2000.0f;
inline constexpr float kRWidth = 140.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kRSpeed = 1600.0f;
inline constexpr int kGloomDurationMs = 6000;
inline constexpr int kShadowResetWindowMs = 6000;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}

inline constexpr float QRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 100.0f, 130.0f, 160.0f, 190.0f}) +
           std::max(0.0f, abilityPower) * 0.50f;
}
inline constexpr float WRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
           std::max(0.0f, abilityPower) * 0.35f;
}
inline constexpr float ERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {50.0f, 70.0f, 90.0f, 110.0f, 130.0f}) +
           std::max(0.0f, abilityPower) * 0.30f;
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {75.0f, 125.0f, 175.0f}) +
           std::max(0.0f, abilityPower) * 0.20f;
}
inline constexpr float GloomBonusDamage(int rank, float abilityPower) {
    return RankValue(rank, {40.0f, 60.0f, 80.0f, 100.0f, 120.0f}) +
           std::max(0.0f, abilityPower) * 0.30f;
}

inline bool SegmentHits(const Vec3& start, const Vec3& end,
                        const Vec3& target, float width,
                        float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid()) return false;
    const auto projection = ProjectPointToSegment2D(target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= std::max(0.0f, width) * 0.5f +
               std::max(0.0f, targetRadius);
}

inline bool CircleHits(const Vec3& center, const Vec3& target,
                       float radius, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() &&
           center.Distance2D(target) <= std::max(0.0f, radius) +
               std::max(0.0f, targetRadius);
}

inline float ProjectileTravelSeconds(const Vec3& source, const Vec3& target,
                                     float delay, float speed, float range) {
    if (!source.IsValid() || !target.IsValid()) return 0.0f;
    const float distance = std::min(range, source.Distance2D(target));
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline Vec3 ClampShadowLanding(const Vec3& origin, const Vec3& requested) {
    if (!origin.IsValid() || !requested.IsValid() ||
        requested.IsZero()) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kRRange, origin.Distance2D(requested));
}

struct ShadowLandingContext {
    bool TargetMarked = false;
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointUnderTurret = false;
    bool OriginUnderTurret = false;
    bool PointClickThreat = false;
    bool DashHazard = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Manual = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};

inline bool SafeShadowLanding(const ShadowLandingContext& context) {
    if (!context.TargetMarked || !context.EndpointValid || context.EndpointWall ||
        context.PointClickThreat || context.DashHazard) return false;
    if (context.EndpointUnderTurret && !context.OriginUnderTurret &&
        !context.Lethal && !context.Defensive && !context.Manual) return false;
    return context.Lethal || context.Defensive || context.Manual ||
           context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

enum class GloomState : std::uint8_t { None, Marked, Consumed };
struct GloomMark {
    int TargetId = 0;
    int AppliedTick = 0;
    int ExpireTick = 0;
    bool Confirmed = false;
};

inline void ApplyGloom(GloomMark& mark, int targetId, int tick,
                       int durationMs = kGloomDurationMs, bool confirmed = true) {
    mark.TargetId = targetId;
    mark.AppliedTick = std::max(0, tick);
    mark.ExpireTick = mark.AppliedTick + std::max(1, durationMs);
    mark.Confirmed = confirmed;
}
inline bool GloomActive(const GloomMark& mark, int targetId, int now) {
    return mark.TargetId != 0 && mark.TargetId == targetId && mark.ExpireTick > now;
}
inline GloomState GloomStatus(const GloomMark& mark, int targetId, int now) {
    return GloomActive(mark, targetId, now) ? GloomState::Marked : GloomState::None;
}
inline bool ConsumeGloom(GloomMark& mark, int targetId, int now) {
    if (!GloomActive(mark, targetId, now)) return false;
    mark = {};
    return true;
}

struct ShadowResetContext {
    bool ProjectileHit = false;
    bool TargetDied = false;
    bool WithinWindow = false;
    bool SpellReady = false;
    int TargetId = 0;
};
inline bool ShadowResetAvailable(const ShadowResetContext& context) {
    return context.ProjectileHit && context.TargetDied && context.WithinWindow &&
           context.SpellReady && context.TargetId != 0;
}
inline bool ShouldShadowRecast(const ShadowResetContext& context,
                               const ShadowLandingContext& landing) {
    return (context.ProjectileHit || ShadowResetAvailable(context)) &&
           SafeShadowLanding(landing);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Vex::Geometry
