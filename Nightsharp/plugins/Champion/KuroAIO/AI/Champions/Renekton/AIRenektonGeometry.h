#pragma once

// Deterministic Renekton mechanics. Runtime prediction, target validity,
// NavMesh safety and event reconciliation remain in AIRenektonController.h.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Renekton::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using Vec3 = ::Vec3;

inline constexpr float kFuryCap = 100.0f;
inline constexpr float kEmpoweredThreshold = 50.0f;
inline constexpr float kQRange = 325.0f;
inline constexpr float kQRadius = 325.0f;
inline constexpr float kWRange = 300.0f;
inline constexpr float kERange = 450.0f;
inline constexpr float kEWidth = 110.0f;
inline constexpr float kRRadius = 375.0f;
inline constexpr int kArmorShredDurationMs = 4000;

inline float ClampFury(float fury) {
    return std::clamp(std::isfinite(fury) ? fury : 0.0f, 0.0f, kFuryCap);
}
inline bool EmpoweredReady(float fury) {
    return ClampFury(fury) + 0.001f >= kEmpoweredThreshold;
}
inline float FuryAfterCast(float fury, float spent) {
    return std::max(0.0f, ClampFury(fury) - std::max(0.0f, spent));
}

enum class EmpoweredChoice : std::uint8_t { None, Q, W, E, R };
struct FuryChoiceContext {
    float Fury = 0.0f;
    bool TargetLow = false;
    bool NeedStun = false;
    bool NeedArmorShred = false;
    bool NeedSustain = false;
    bool AllIn = false;
};

// W is preferred when an observed stun/kill window matters; empowered E is
// the armor-shred opener; Q is the safe sustain/area option. R is returned as
// an ultimate action (Dominus itself is not fury-empowered) only for an all-in.
inline EmpoweredChoice ChooseEmpowered(const FuryChoiceContext& context) {
    if (!EmpoweredReady(context.Fury)) return EmpoweredChoice::None;
    if (context.NeedStun || context.TargetLow) return EmpoweredChoice::W;
    if (context.NeedArmorShred) return EmpoweredChoice::E;
    if (context.NeedSustain) return EmpoweredChoice::Q;
    return context.AllIn ? EmpoweredChoice::R : EmpoweredChoice::None;
}

inline float QBaseDamage(int rank) {
    return RankValue(std::array<float, 6>{0.0f, 60.0f, 90.0f, 120.0f, 150.0f, 180.0f}, rank);
}
inline float QRawDamage(int rank, float bonusAttackDamage, bool empowered) {
    return (empowered ? QBaseDamage(rank) * 1.5f : QBaseDamage(rank)) +
           (empowered ? 1.20f : 0.80f) * std::max(0.0f, bonusAttackDamage);
}
inline float WBaseDamage(int rank) {
    return RankValue(std::array<float, 6>{0.0f, 5.0f, 20.0f, 35.0f, 50.0f, 65.0f}, rank);
}
inline float WRawDamage(int rank, float totalAttackDamage, bool empowered) {
    const float hits = empowered ? 3.0f : 2.0f;
    return hits * (WBaseDamage(rank) + (empowered ? 0.75f : 0.375f) *
                   std::max(0.0f, totalAttackDamage));
}
inline float EBaseDamage(int rank) {
    return RankValue(std::array<float, 6>{0.0f, 40.0f, 70.0f, 100.0f, 130.0f, 160.0f}, rank);
}
inline float ERawDamage(int rank, float bonusAttackDamage, bool empowered) {
    return EBaseDamage(rank) + (empowered ? 1.20f : 0.90f) *
           std::max(0.0f, bonusAttackDamage);
}
inline float EArmorShredPercent(int rank, bool empowered) {
    const float ordinary = RankValue(std::array<float, 6>{0.0f, 25.0f, 27.5f, 30.0f, 32.5f, 35.0f}, rank);
    return empowered ? ordinary : 0.0f;
}
inline float RRawDamagePerTick(int rank, float abilityPower) {
    return RankValue(std::array<float, 4>{0.0f, 30.0f, 60.0f, 90.0f}, rank) +
           0.10f * std::max(0.0f, abilityPower);
}
inline float RDurationSeconds(int rank) {
    return RankValue(std::array<float, 4>{0.0f, 15.0f, 15.0f, 15.0f}, rank);
}

inline bool QHits(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
           origin.Distance2D(target) <= kQRadius + std::max(0.0f, targetRadius);
}
inline bool WHit(const Vec3& origin, const Vec3& target, float targetRadius = 0.0f) {
    return origin.IsValid() && target.IsValid() &&
           origin.Distance2D(target) <= kWRange + std::max(0.0f, targetRadius);
}

inline Vec3 ClampDash(const Vec3& origin, const Vec3& requested,
                      float maximumRange = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, maximumRange),
                                          origin.Distance2D(requested));
}
inline bool DashThroughTarget(const Vec3& origin, const Vec3& endpoint,
                              const Vec3& target, float targetRadius = 0.0f) {
    if (origin.IsZero() || endpoint.IsZero() || target.IsZero()) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T > 0.0f && projection.T < 1.0f &&
           projection.Distance <= kEWidth * 0.5f + std::max(0.0f, targetRadius);
}

struct DashSafetyContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool ThroughTarget = false;
    bool EndpointUnderNewTurret = false;
    bool DashHazard = false;
    bool PointClickThreat = false;
    bool Defensive = false;
    bool Lethal = false;
    bool Fleeing = false;
    bool CursorAgrees = true;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
};
inline constexpr bool DashSafe(const DashSafetyContext& context) {
    if (!context.Ready || !context.EndpointValid || !context.EndpointWalkable ||
        context.DashHazard || context.PointClickThreat || !context.ThroughTarget) return false;
    if (context.EndpointUnderNewTurret && !context.Defensive && !context.Lethal) return false;
    if (!context.CursorAgrees && !context.Defensive && !context.Fleeing) return false;
    return context.Defensive || context.Fleeing || context.Lethal ||
           context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

enum class EStage : std::uint8_t { Slice, Dice };
struct CooldownState {
    std::array<int, 4> ReadyAt{};
    float Fury = 0.0f;
};
inline bool SpellReady(const CooldownState& state, int slot, int now) {
    return slot >= 0 && slot < 4 && now >= state.ReadyAt[static_cast<std::size_t>(slot)];
}
inline void RecordCast(CooldownState& state, int slot, int now, int cooldownMs) {
    if (slot < 0 || slot >= 4) return;
    state.ReadyAt[static_cast<std::size_t>(slot)] = now + std::max(0, cooldownMs);
    state.Fury = ClampFury(state.Fury);
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};
inline constexpr bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage &&
           (context.Defensive || context.Interrupt || context.KillSecure);
}
struct DominusContext {
    bool Ready = false;
    bool TargetValid = false;
    bool InRange = false;
    bool IncomingHardCC = false;
    bool PlayerLow = false;
    bool TargetLow = false;
    bool AllIn = false;
    bool Manual = false;
    int NearbyEnemies = 0;
    int MinimumEnemies = 2;
};
inline constexpr bool ShouldCastDominus(const DominusContext& context) {
    if (!context.Ready || !context.TargetValid || !context.InRange) return false;
    return context.Manual || context.IncomingHardCC || context.PlayerLow ||
           context.TargetLow || (context.AllIn &&
           context.NearbyEnemies >= std::max(1, context.MinimumEnemies));
}

} // namespace Plugins::KuroAIO::AI::Controllers::Renekton::Geometry
