#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Hwei::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kQQRange = 1100.0f;
inline constexpr float kQQWidth = 90.0f;
inline constexpr float kQQDelay = 0.25f;
inline constexpr float kQQSpeed = 1400.0f;
inline constexpr float kQWRange = 1900.0f;
inline constexpr float kQWWidth = 70.0f;
inline constexpr float kQWDelay = 0.90f;
inline constexpr float kQWSpeed = 1800.0f;
inline constexpr float kQERange = 1200.0f;
inline constexpr float kQEWidth = 120.0f;
inline constexpr float kQEDelay = 0.25f;
inline constexpr float kQEDuration = 3.0f;
inline constexpr float kWQRange = 950.0f;
inline constexpr float kWWRange = 650.0f;
inline constexpr float kWERange = 650.0f;
inline constexpr float kEWRange = 1200.0f;
inline constexpr float kEWRadius = 180.0f;
inline constexpr float kEERange = 800.0f;
inline constexpr float kEERadius = 275.0f;
inline constexpr float kEQRange = 800.0f;
inline constexpr float kEQAngleDegrees = 70.0f;
inline constexpr float kRRange = 1300.0f;
inline constexpr float kRRadius = 325.0f;
inline constexpr float kRDelay = 0.25f;
inline constexpr float kRDuration = 3.0f;

inline constexpr float RankValue(int rank, const std::array<float, 5>& values) {
    return values[std::clamp(rank, 1, 5) - 1];
}
inline constexpr float UltimateRankValue(int rank,
                                         const std::array<float, 3>& values) {
    return values[std::clamp(rank, 1, 3) - 1];
}

inline constexpr float QQRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
        0.65f * std::max(0.0f, abilityPower);
}
inline constexpr float QWRawDamage(int rank, float abilityPower,
                                   float targetMissingHealthPercent) {
    const float missing = std::clamp(targetMissingHealthPercent, 0.0f, 100.0f);
    return RankValue(rank, {70.0f, 105.0f, 140.0f, 175.0f, 210.0f}) +
        0.70f * std::max(0.0f, abilityPower) +
        0.45f * missing;
}
inline constexpr float QERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {45.0f, 70.0f, 95.0f, 120.0f, 145.0f}) +
        0.55f * std::max(0.0f, abilityPower);
}
inline constexpr float WEOnHitRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {20.0f, 30.0f, 40.0f, 50.0f, 60.0f}) +
        0.25f * std::max(0.0f, abilityPower);
}
inline constexpr float EQRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {70.0f, 105.0f, 140.0f, 175.0f, 210.0f}) +
        0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float EWRawDamage(int rank, float abilityPower) {
    return RankValue(rank, {60.0f, 90.0f, 120.0f, 150.0f, 180.0f}) +
        0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float EERawDamage(int rank, float abilityPower) {
    return RankValue(rank, {50.0f, 75.0f, 100.0f, 125.0f, 150.0f}) +
        0.50f * std::max(0.0f, abilityPower);
}
inline constexpr float RRawDamage(int rank, float abilityPower) {
    return UltimateRankValue(rank, {100.0f, 200.0f, 300.0f}) +
        0.60f * std::max(0.0f, abilityPower);
}
inline constexpr float PassiveRawDamage(int level, float abilityPower) {
    return 35.0f + 2.0f * static_cast<float>(std::clamp(level, 1, 18)) +
        0.30f * std::max(0.0f, abilityPower);
}

inline bool SegmentHits(const Vec3& start, const Vec3& end,
                        const Vec3& target, float halfWidth,
                        float targetRadius = 0.0f) {
    if (!start.IsValid() || !end.IsValid() || !target.IsValid() ||
        start.IsZero() && end.IsZero()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        target, start, end);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, halfWidth) +
            std::max(0.0f, targetRadius);
}
inline bool CircleContains(const Vec3& center, const Vec3& target,
                           float radius, float targetRadius = 0.0f) {
    return center.IsValid() && target.IsValid() && !center.IsZero() &&
        !target.IsZero() && center.Distance2D(target) <=
            std::max(0.0f, radius) + std::max(0.0f, targetRadius);
}
inline bool ConeContains(const Vec3& origin, const Vec3& directionPoint,
                         const Vec3& target, float range, float halfAngle,
                         float targetRadius = 0.0f) {
    const Vec3 direction = Direction2D(origin, directionPoint);
    const Vec3 toTarget = Direction2D(origin, target);
    if (direction.IsZero() || toTarget.IsZero() ||
        origin.Distance2D(target) > std::max(0.0f, range) +
            std::max(0.0f, targetRadius)) return false;
    const float dot = std::clamp(direction.x * toTarget.x +
        direction.z * toTarget.z, -1.0f, 1.0f);
    return std::acos(dot) * 180.0f / 3.14159265358979323846f <=
        std::max(0.0f, halfAngle) + 0.01f;
}
inline Vec3 ClampRange(const Vec3& origin, const Vec3& requested, float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero() || !origin.IsValid() || !requested.IsValid()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

struct TargetPolicy {
    bool Valid = false;
    bool Predicted = false;
    bool CollisionFree = false;
    bool Protected = false;
    bool UnderTurret = false;
    bool Selected = false;
};
inline bool ShouldCastAt(const TargetPolicy& policy, bool defensive = false) {
    if (!policy.Valid || policy.Protected || !policy.Predicted ||
        !policy.CollisionFree) return false;
    return defensive || !policy.UnderTurret;
}

struct ZoneSafety {
    bool PositionValid = false;
    bool InEnemyTurret = false;
    bool NewTurretCommit = false;
    int EnemiesInside = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
};
inline bool SafeZone(const ZoneSafety& safety) {
    return safety.PositionValid && (!safety.NewTurretCommit || safety.Defensive) &&
        (safety.Defensive || (!safety.InEnemyTurret &&
            safety.EnemiesInside <= std::max(0, safety.MaximumEnemies)));
}

struct ResourceState {
    float Mana = 0.0f;
    float MaximumMana = 0.0f;
    float Cost = 0.0f;
    bool CooldownReady = false;
    bool StanceReady = true;
};
inline bool CanSpend(const ResourceState& state, float reservePercent = 0.0f) {
    if (!state.CooldownReady || !state.StanceReady || state.Cost < 0.0f ||
        state.MaximumMana <= 0.0f) return false;
    const float reserve = state.MaximumMana *
        std::clamp(reservePercent, 0.0f, 100.0f) / 100.0f;
    return state.Mana >= state.Cost + reserve;
}

enum class Stance : std::uint8_t { Disaster, Serenity, Turmoil };
enum class Paint : std::uint8_t { Q, W, E };
struct SpellbookState {
    Stance SelectedStance = Stance::Disaster;
    Paint SelectedPaint = Paint::Q;
    int LastCastTick = 0;
    int LastSpellSlot = -1;
    int StanceExpireTick = 0;
    int PassiveMarkTargetId = 0;
    int PassiveMarkExpireTick = 0;
    bool ZoneActive = false;
};
inline void SelectStance(SpellbookState& state, Stance stance, int now,
                         int stanceWindowMs = 700) {
    state.SelectedStance = stance;
    state.SelectedPaint = Paint::Q;
    state.LastCastTick = now;
    state.StanceExpireTick = now + std::max(0, stanceWindowMs);
}
inline void SelectPaint(SpellbookState& state, Paint paint, int now) {
    state.SelectedPaint = paint;
    state.LastCastTick = now;
}
inline bool StanceWindowOpen(const SpellbookState& state, int now) {
    return state.StanceExpireTick > now;
}
inline void Reconcile(SpellbookState& state, int now, bool zoneObserved,
                      bool passiveMarkObserved) {
    state.ZoneActive = zoneObserved;
    if (!passiveMarkObserved && state.PassiveMarkExpireTick <= now) {
        state.PassiveMarkTargetId = 0;
        state.PassiveMarkExpireTick = 0;
    }
    if (!StanceWindowOpen(state, now) && !zoneObserved) {
        state.SelectedPaint = Paint::Q;
    }
}
inline bool ComboIs(Stance stance, Paint paint, Stance wantedStance,
                    Paint wantedPaint) {
    return stance == wantedStance && paint == wantedPaint;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Hwei::Geometry
