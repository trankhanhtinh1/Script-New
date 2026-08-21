#pragma once

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Nidalee::Geometry {

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;
};
inline Vec2 operator+(Vec2 a, Vec2 b) { return {a.x + b.x, a.y + b.y}; }
inline Vec2 operator-(Vec2 a, Vec2 b) { return {a.x - b.x, a.y - b.y}; }
inline Vec2 operator*(Vec2 a, float s) { return {a.x * s, a.y * s}; }
inline float Dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }
inline float Length(Vec2 v) { return std::sqrt(Dot(v, v)); }
inline float Distance(Vec2 a, Vec2 b) { return Length(a - b); }
inline Vec2 Normalize(Vec2 v) {
    const float length = Length(v);
    return length > 0.0001f ? v * (1.0f / length) : Vec2{};
}

inline constexpr float kJavelinRange = 1500.0f;
inline constexpr float kJavelinWidth = 40.0f;
inline constexpr float kJavelinDelay = 0.25f;
inline constexpr float kJavelinSpeed = 1300.0f;
inline constexpr float kJavelinMaxDamageDistance = 1300.0f;
inline constexpr float kTrapRange = 900.0f;
inline constexpr float kTrapRadius = 100.0f;
inline constexpr float kTrapDurationSeconds = 2.0f;
inline constexpr float kPounceRange = 375.0f;
inline constexpr float kHuntedPounceRange = 750.0f;
inline constexpr float kPounceRadius = 150.0f;
inline constexpr float kTakedownRange = 400.0f;
inline constexpr float kSwipeRadius = 350.0f;
inline constexpr int kHuntDurationMs = 4000;

inline constexpr float RankValue(int rank, float r1, float r2, float r3,
                                 float r4, float r5) {
    switch (std::clamp(rank, 1, 5)) {
    case 1: return r1; case 2: return r2; case 3: return r3;
    case 4: return r4; default: return r5;
    }
}
inline constexpr float JavelinBaseDamage(int rank) {
    return RankValue(rank, 70.0f, 85.0f, 100.0f, 115.0f, 130.0f);
}
inline constexpr float JavelinDistanceMultiplier(float distance) {
    const float progress = std::clamp(distance / kJavelinMaxDamageDistance, 0.0f, 1.0f);
    return 1.0f + 1.50f * progress;
}
inline constexpr float JavelinRawDamage(int rank, float abilityPower,
                                        float distance) {
    return (JavelinBaseDamage(rank) + 0.50f * std::max(0.0f, abilityPower)) *
           JavelinDistanceMultiplier(distance);
}
inline constexpr float TrapRawDamage(int rank, float abilityPower) {
    return RankValue(rank, 20.0f, 40.0f, 60.0f, 80.0f, 100.0f) +
           0.20f * std::max(0.0f, abilityPower);
}
inline constexpr float PrimalSurgeHeal(int rank, float abilityPower) {
    return RankValue(rank, 50.0f, 75.0f, 100.0f, 125.0f, 150.0f) +
           0.50f * std::max(0.0f, abilityPower);
}
inline constexpr float TakedownBaseDamage(int rank) {
    return RankValue(rank, 5.0f, 30.0f, 55.0f, 80.0f, 105.0f);
}
inline constexpr float TakedownMissingHealthMultiplier(float targetHealthPercent) {
    const float missing = 1.0f - std::clamp(targetHealthPercent, 0.0f, 100.0f) / 100.0f;
    return 1.0f + missing;
}
inline constexpr float TakedownRawDamage(int rank, float abilityPower,
                                         float bonusAttackDamage,
                                         float targetHealthPercent) {
    return (TakedownBaseDamage(rank) + 0.40f * std::max(0.0f, abilityPower) +
            0.40f * std::max(0.0f, bonusAttackDamage)) *
           TakedownMissingHealthMultiplier(targetHealthPercent);
}
inline constexpr float SwipeRawDamage(int rank, float abilityPower,
                                      float bonusAttackDamage) {
    return RankValue(rank, 70.0f, 100.0f, 130.0f, 160.0f, 190.0f) +
           0.45f * std::max(0.0f, abilityPower) +
           0.75f * std::max(0.0f, bonusAttackDamage);
}

inline Vec2 ClampPounceEndpoint(Vec2 origin, Vec2 requested, bool hunted) {
    const Vec2 delta = requested - origin;
    const float distance = Length(delta);
    const float range = hunted ? kHuntedPounceRange : kPounceRange;
    if (distance <= 0.0001f) return {};
    return distance <= range ? requested : origin + delta * (range / distance);
}
inline bool SegmentHits(Vec2 start, Vec2 end, Vec2 target, float radius) {
    const Vec2 segment = end - start;
    const float lengthSquared = Dot(segment, segment);
    const float t = lengthSquared <= 0.0001f ? 0.0f :
        std::clamp(Dot(target - start, segment) / lengthSquared, 0.0f, 1.0f);
    return Distance(target, start + segment * t) <= std::max(0.0f, radius);
}
inline bool JavelinHits(Vec2 origin, Vec2 aim, Vec2 target, float targetRadius = 0.0f) {
    const Vec2 direction = Normalize(aim - origin);
    if (Length(direction) <= 0.0001f) return false;
    return SegmentHits(origin, origin + direction * kJavelinRange, target,
                       kJavelinWidth * 0.5f + std::max(0.0f, targetRadius));
}

struct LeapSafety {
    bool Ready = false;
    bool EndpointValid = false;
    bool PathBlocked = false;
    bool EndpointUnderNewTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
    bool Hunted = false;
};
inline bool ShouldPounce(const LeapSafety& context) {
    if (!context.Ready || !context.EndpointValid || context.PathBlocked ||
        context.EndpointUnderNewTurret) return false;
    return context.Defensive || context.EnemiesAtEndpoint <= context.MaximumEnemies;
}

struct HuntMark {
    int TargetId = 0;
    int ExpireTick = 0;
};
inline bool HuntActive(const HuntMark& mark, int targetId, int now) {
    return mark.TargetId != 0 && mark.TargetId == targetId && now < mark.ExpireTick;
}
inline void RecordHunt(HuntMark& mark, int targetId, int now) {
    mark.TargetId = targetId;
    mark.ExpireTick = targetId == 0 ? 0 : now + kHuntDurationMs;
}

enum class Form { Human, Cougar, Unknown };

} // namespace Plugins::KuroAIO::AI::Controllers::Nidalee::Geometry
