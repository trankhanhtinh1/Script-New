#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Viego::Geometry {

inline constexpr float kQRange = 600.0f;
inline constexpr float kQHalfWidth = 62.5f;
inline constexpr float kWMinRange = 500.0f;
inline constexpr float kWMaxRange = 900.0f;
inline constexpr float kWChargeSeconds = 1.0f;
inline constexpr float kWDashDistance = 300.0f;
inline constexpr float kWHalfWidth = 30.0f;
inline constexpr float kERange = 750.0f;
inline constexpr float kRRange = 500.0f;
inline constexpr float kRRadius = 300.0f;
inline constexpr int kPossessionDurationMs = 10000;
inline constexpr int kPossessionInvulnerableMs = 1000;
inline constexpr int kSoulDurationMs = 8000;
inline constexpr int kQMarkDurationMs = 4000;

struct Vec2 {
    float X = 0.0f;
    float Y = 0.0f;
};

inline Vec2 operator+(Vec2 a, Vec2 b) { return { a.X + b.X, a.Y + b.Y }; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return { a.X - b.X, a.Y - b.Y }; }
inline Vec2 operator*(Vec2 a, float scale) { return { a.X * scale, a.Y * scale }; }
inline float Dot(Vec2 a, Vec2 b) { return a.X * b.X + a.Y * b.Y; }
inline float LengthSquared(Vec2 value) { return Dot(value, value); }
inline float Length(Vec2 value) { return std::sqrt(LengthSquared(value)); }
inline float Distance(Vec2 a, Vec2 b) { return Length(a - b); }

inline Vec2 Normalize(Vec2 value) {
    const float length = Length(value);
    return length > 0.0001f ? value * (1.0f / length) : Vec2{};
}

inline Vec2 ClampToRange(Vec2 origin, Vec2 desired, float range) {
    const Vec2 delta = desired - origin;
    const float distance = Length(delta);
    return distance <= range || distance <= 0.0001f
        ? desired : origin + delta * (range / distance);
}

inline float PointSegmentDistance(Vec2 point, Vec2 start, Vec2 end) {
    const Vec2 segment = end - start;
    const float lengthSquared = LengthSquared(segment);
    if (lengthSquared <= 0.0001f) return Distance(point, start);
    const float t = std::clamp(Dot(point - start, segment) / lengthSquared, 0.0f, 1.0f);
    return Distance(point, start + segment * t);
}

inline bool SegmentHitsCircle(Vec2 start, Vec2 end, Vec2 center, float radius) {
    return PointSegmentDistance(center, start, end) <= std::max(0.0f, radius);
}

inline float WRange(float chargedSeconds) {
    const float progress = std::clamp(chargedSeconds / kWChargeSeconds, 0.0f, 1.0f);
    return kWMinRange + (kWMaxRange - kWMinRange) * progress;
}

inline float WStunSeconds(float chargedSeconds) {
    return 0.25f + std::clamp(chargedSeconds / kWChargeSeconds, 0.0f, 1.0f);
}

inline Vec2 WDashEndpoint(Vec2 origin, Vec2 aim) {
    const Vec2 direction = Normalize(aim - origin);
    return LengthSquared(direction) > 0.0f
        ? origin + direction * kWDashDistance : origin;
}

struct WReleaseContext {
    bool Charging = false;
    bool TargetDamageable = false;
    bool PredictionHits = false;
    bool Collision = false;
    bool DashPathBlocked = false;
    bool DashEndpointSafe = false;
    bool TargetInCurrentRange = false;
    bool TargetInAttackRange = false;
    bool Lethal = false;
    bool Peel = false;
    bool ChargeExpiring = false;
};

inline bool ShouldReleaseW(const WReleaseContext& context) {
    if (!context.Charging || !context.TargetDamageable ||
        !context.PredictionHits || context.Collision ||
        context.DashPathBlocked || !context.DashEndpointSafe) return false;
    if (!context.TargetInCurrentRange) return context.ChargeExpiring && context.Peel;
    return context.Lethal || context.Peel || context.TargetInAttackRange ||
           context.ChargeExpiring;
}

struct MistContext {
    bool NativeForm = false;
    bool Ready = false;
    bool NearTerrain = false;
    bool CastPointValid = false;
    bool AlreadyInMist = false;
    bool Pursuing = false;
    bool Retreating = false;
    bool Outnumbered = false;
};

inline bool ShouldCastMist(const MistContext& context) {
    return context.NativeForm && context.Ready && context.CastPointValid &&
           !context.AlreadyInMist && context.NearTerrain &&
           (context.Pursuing || context.Retreating) &&
           (!context.Outnumbered || context.Retreating);
}

struct RBody {
    int Id = 0;
    Vec2 Position = {};
    float Radius = 0.0f;
    float HealthFraction = 1.0f;
    bool Champion = true;
    bool Damageable = true;
};

inline int RPrimaryTarget(Vec2 landing, const std::vector<RBody>& bodies) {
    int bestId = 0;
    float bestHealth = std::numeric_limits<float>::max();
    for (const auto& body : bodies) {
        if (!body.Champion || !body.Damageable ||
            Distance(landing, body.Position) > kRRadius + body.Radius) continue;
        if (body.HealthFraction < bestHealth - 0.0001f ||
            (std::fabs(body.HealthFraction - bestHealth) <= 0.0001f &&
             (bestId == 0 || body.Id < bestId))) {
            bestHealth = body.HealthFraction;
            bestId = body.Id;
        }
    }
    return bestId;
}

inline int RHitCount(Vec2 landing, const std::vector<RBody>& bodies) {
    int hits = 0;
    for (const auto& body : bodies) {
        if (body.Damageable && Distance(landing, body.Position) <=
            kRRadius + body.Radius) ++hits;
    }
    return hits;
}

struct RContext {
    bool Ready = false;
    bool TargetDamageable = false;
    bool LandingValid = false;
    bool LandingSafe = false;
    bool IntendedTargetIsPrimary = false;
    bool Lethal = false;
    bool Defensive = false;
    bool Possessed = false;
    bool PossessionExpiring = false;
    bool PossessionDangerous = false;
    int EnemyHits = 0;
};

inline bool ShouldCastR(const RContext& context) {
    if (!context.Ready || !context.TargetDamageable ||
        !context.LandingValid || !context.LandingSafe) return false;
    if (context.Possessed) {
        return context.Lethal || context.Defensive || context.PossessionExpiring ||
               context.PossessionDangerous;
    }
    if (!context.IntendedTargetIsPrimary) return false;
    return context.Lethal || context.Defensive || context.EnemyHits >= 2;
}

struct PassiveState {
    bool CastingPossession = false;
    bool Untargetable = false;
    bool Possessed = false;
    bool NativeSpellNames = true;
};

inline bool MayIssueNativeKitCast(const PassiveState& state) {
    return !state.CastingPossession && !state.Untargetable &&
           !state.Possessed && state.NativeSpellNames;
}

inline bool MayIssuePossessionRecast(const PassiveState& state) {
    return !state.CastingPossession && !state.Untargetable && state.Possessed;
}

struct SoulContext {
    bool Exists = false;
    bool Attackable = false;
    bool InRange = false;
    bool Safe = false;
    bool PlayerLow = false;
    bool CurrentFightWonWithoutSoul = false;
    bool SoulExpiring = false;
};

inline bool ShouldTakeSoul(const SoulContext& context) {
    return context.Exists && context.Attackable && context.InRange && context.Safe &&
           (context.PlayerLow || context.SoulExpiring ||
            !context.CurrentFightWonWithoutSoul);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Viego::Geometry
