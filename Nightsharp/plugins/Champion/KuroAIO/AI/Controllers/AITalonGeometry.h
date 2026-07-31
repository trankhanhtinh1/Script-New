#pragma once

// Deterministic Talon mechanics. Runtime prediction, NavMesh, mitigation and
// spell events remain in AITalonController.h.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Talon::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using Vec3 = ::Vec3;

inline Vec3 ClampCast(const Vec3& origin, const Vec3& requested, float range) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}
inline constexpr float kQRange = 575.0f;
inline constexpr float kQMeleeRadius = 170.0f;
inline constexpr float kWOutboundRange = 650.0f;
inline constexpr float kWReturnRange = 900.0f;
inline constexpr float kWWidth = 75.0f;
inline constexpr float kWSpeed = 2500.0f;
inline constexpr float kERange = 725.0f;
inline constexpr float kRRange = 550.0f;
inline constexpr float kRWidth = 100.0f;
inline constexpr int kPassiveDurationMs = 6000;
inline constexpr int kWReturnWindowMs = 1200;
inline constexpr int kRStealthMs = 2400;
inline constexpr int kRReturnWindowMs = 4000;
struct ModeContext {
    bool SelectedTarget = false;
    bool OrbwalkerTarget = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool ManualAssist = false;
};

inline bool MayUseAbility(const ModeContext& context) {
    if (context.ManualAssist) return false;
    if (context.AttackWindingUp && !context.Lethal) return false;
    return context.SelectedTarget || context.OrbwalkerTarget;
}

inline float Finite(float value, float low, float high) {
    return std::isfinite(value) ? std::clamp(value, low, high) : low;
}

inline bool IsMeleeQ(const Vec3& origin, const Vec3& target, float radius = kQMeleeRadius) {
    return !origin.IsZero() && !target.IsZero() && origin.Distance2D(target) <= radius;
}

inline float PassiveRawDamage(int level, float totalAttackDamage) {
    const float rank = static_cast<float>(std::clamp(level, 1, 18));
    return (75.0f + 5.0f * rank) + 1.0f * std::max(0.0f, totalAttackDamage);
}

struct PassiveState {
    int TargetId = 0;
    int Stacks = 0;
    int ExpireTick = 0;
};

inline PassiveState AddPassiveStack(PassiveState state, int targetId, int nowTick) {
    if (targetId == 0) return {};
    if (state.TargetId != targetId || nowTick >= state.ExpireTick) state.Stacks = 0;
    state.TargetId = targetId;
    state.Stacks = std::clamp(state.Stacks + 1, 0, 3);
    state.ExpireTick = nowTick + kPassiveDurationMs;
    return state;
}

inline bool PassiveReady(const PassiveState& state, int targetId, int nowTick) {
    return targetId != 0 && state.TargetId == targetId && state.Stacks >= 3 &&
           nowTick < state.ExpireTick;
}

inline float QRawDamage(int rank, float totalAttackDamage, bool melee) {
    static constexpr std::array<float, 6> base{0, 65, 90, 115, 140, 165};
    const float value = RankValue(base, rank) + 0.15f * std::max(0.0f, totalAttackDamage);
    return melee ? value + 0.0f : value * 0.80f;
}

inline float WRawDamage(int rank, float bonusAttackDamage, bool returning) {
    static constexpr std::array<float, 6> base{0, 50, 70, 90, 110, 130};
    const float value = RankValue(base, rank) + 0.60f * std::max(0.0f, bonusAttackDamage);
    return returning ? value * 1.25f : value;
}

inline float ERawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{0, 60, 90, 120, 150, 180};
    return RankValue(base, rank) + 0.40f * std::max(0.0f, bonusAttackDamage);
}

inline float RRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 4> base{0, 90, 135, 180};
    return RankValue(base, rank) + 1.10f * std::max(0.0f, bonusAttackDamage);
}

inline Vec3 WReturnEndpoint(const Vec3& origin, const Vec3& outbound, const Vec3& requested) {
    const Vec3 direction = Direction2D(outbound, requested.IsZero() ? origin : requested);
    if (direction.IsZero()) return origin;
    return outbound + direction * std::min(kWReturnRange, outbound.Distance2D(origin));
}

inline bool WBladeHits(const Vec3& start, const Vec3& end, const Vec3& target,
                       float targetRadius = 35.0f) {
    if (start.IsZero() || end.IsZero() || target.IsZero()) return false;
    return ProjectPointToSegment2D(target, start, end).Distance <=
           kWWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline Vec3 QEndpoint(const Vec3& origin, const Vec3& target, bool melee) {
    if (origin.IsZero() || target.IsZero()) return {};
    if (melee) return target;
    return ClampCast(origin, target, kQRange);
}

struct TerrainState {
    bool Traversing = false;
    bool Returning = false;
    Vec3 Start = {};
    Vec3 End = {};
    int StartTick = 0;
    int ExpireTick = 0;
};

inline bool BeginTerrain(TerrainState state, const Vec3& start, const Vec3& end,
                         int nowTick, int durationMs) {
    return !state.Traversing && !start.IsZero() && !end.IsZero() &&
           start.Distance2D(end) <= kERange && durationMs > 0 && nowTick >= 0;
}

inline bool TerrainActive(const TerrainState& state, int nowTick) {
    return state.Traversing && nowTick < state.ExpireTick && !state.End.IsZero();
}

inline bool TerrainEndpointSafe(const Vec3& endpoint, bool walkable, bool wall,
                                bool underTurret, bool lethal, bool exitAvailable,
                                int nearbyEnemies, int maxEnemies) {
    if (endpoint.IsZero() || !walkable || wall) return false;
    if (underTurret && !lethal) return false;
    if (nearbyEnemies > std::max(0, maxEnemies) && !lethal && !exitAvailable) return false;
    return lethal || exitAvailable || nearbyEnemies <= std::max(0, maxEnemies);
}

struct RState {
    bool Active = false;
    bool Stealthed = false;
    bool Returning = false;
    Vec3 Origin = {};
    Vec3 Endpoint = {};
    int StartTick = 0;
    int ExpireTick = 0;
};

inline bool REndpointSafe(const Vec3& endpoint, bool valid, bool wall, bool underTurret,
                          bool lethal, bool exitAvailable, int nearbyEnemies,
                          int maxEnemies) {
    if (!valid || endpoint.IsZero() || wall) return false;
    if (underTurret && !lethal) return false;
    if (nearbyEnemies > std::max(0, maxEnemies) && !lethal && !exitAvailable) return false;
    return lethal || exitAvailable || nearbyEnemies <= std::max(0, maxEnemies);
}

inline bool RReturnAllowed(const RState& state, int nowTick, bool endpointSafe,
                          bool lethal, bool fleeing) {
    if (!state.Active || nowTick >= state.ExpireTick) return false;
    return state.Returning || fleeing || lethal || !endpointSafe;
}

inline bool RLineHits(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                      float targetRadius = 35.0f) {
    return WBladeHits(origin, endpoint, target, targetRadius) &&
           origin.Distance2D(endpoint) > 1.0f;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Talon::Geometry
