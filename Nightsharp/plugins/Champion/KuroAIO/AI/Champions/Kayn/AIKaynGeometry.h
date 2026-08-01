#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Kayn::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;
using Vec3 = ::Vec3;

enum class Form { Untransformed, ShadowAssassin, DarkinSlayer };
enum class OrbKind { Melee, Ranged };

inline constexpr float kQRange = 350.0f;
inline constexpr float kQSlashRadius = 300.0f;
inline constexpr float kQWidth = 100.0f;
inline constexpr float kWRange = 700.0f;
inline constexpr float kWAssassinRange = 900.0f;
inline constexpr float kWHalfWidth = 80.0f;
inline constexpr float kWKnockupSeconds = 1.0f;
inline constexpr float kERange = 400.0f;
inline constexpr float kEWallDurationSeconds = 1.5f;
inline constexpr float kEHealDelaySeconds = 0.5f;
inline constexpr float kRBaseRange = 550.0f;
inline constexpr float kRAssassinRange = 750.0f;
inline constexpr float kRInfestMinimumSeconds = 0.5f;
inline constexpr float kRInfestDurationSeconds = 2.5f;
inline constexpr float kRJumpOutDistance = 300.0f;
inline constexpr float kRAssassinJumpOutDistance = 500.0f;
inline constexpr int kTransformationDelayMs = 4000;
inline constexpr int kTransformingTimeoutMs = 8500;

inline constexpr int ClampOrbCount(int count) { return std::clamp(count, 0, 100); }

struct OrbState {
    int MeleeOrbs = 0;
    int RangedOrbs = 0;
    bool Transforming = false;
    Form CurrentForm = Form::Untransformed;
};

inline constexpr OrbState AddOrb(OrbState state, OrbKind kind, int amount = 1) {
    if (state.CurrentForm != Form::Untransformed || state.Transforming) return state;
    if (kind == OrbKind::Melee) state.MeleeOrbs = ClampOrbCount(state.MeleeOrbs + std::max(0, amount));
    else state.RangedOrbs = ClampOrbCount(state.RangedOrbs + std::max(0, amount));
    return state;
}

inline constexpr Form ResolveForm(bool assassinReady, bool slayerReady) {
    if (assassinReady && !slayerReady) return Form::ShadowAssassin;
    if (slayerReady && !assassinReady) return Form::DarkinSlayer;
    return Form::Untransformed;
}

inline constexpr bool CanTransform(const OrbState& state) {
    return state.CurrentForm == Form::Untransformed && !state.Transforming &&
        (state.MeleeOrbs > 0 || state.RangedOrbs > 0);
}

inline float QRawDamage(int rank, float totalAttackDamage) {
    return RankValue(std::array<float, 6>{0.0f, 75.0f, 105.0f, 135.0f, 165.0f, 195.0f}, rank) +
        std::max(0.0f, totalAttackDamage) * 0.85f;
}

inline constexpr float QDarkinBonusDamage(float targetMaximumHealth, float bonusAttackDamage) {
    return std::max(0.0f, targetMaximumHealth) *
        (0.06f + 0.035f * std::max(0.0f, bonusAttackDamage) / 100.0f);
}

inline float WRawDamage(int rank, float bonusAttackDamage) {
    return RankValue(std::array<float, 6>{0.0f, 85.0f, 130.0f, 175.0f, 220.0f, 265.0f}, rank) +
        std::max(0.0f, bonusAttackDamage) * 1.10f;
}

inline float RRawDamage(int rank, float attackDamage) {
    return RankValue(std::array<float, 4>{0.0f, 150.0f, 250.0f, 350.0f}, rank) +
        std::max(0.0f, attackDamage) * 1.50f;
}

inline constexpr float RDarkinDamage(float targetMaximumHealth, float bonusAttackDamage) {
    return std::max(0.0f, targetMaximumHealth) *
        (0.15f + 0.001f * std::max(0.0f, bonusAttackDamage));
}

inline constexpr float RDarkinHeal(float damage) { return std::max(0.0f, damage) * 0.75f; }
inline constexpr float RHostRange(Form form) {
    return form == Form::ShadowAssassin ? kRAssassinRange : kRBaseRange;
}
inline constexpr float RJumpOutRange(Form form) {
    return form == Form::ShadowAssassin ? kRAssassinJumpOutDistance : kRJumpOutDistance;
}
inline constexpr float WRange(Form form) {
    return form == Form::ShadowAssassin ? kWAssassinRange : kWRange;
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested,
                              float range = kQRange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline bool QSlashHits(const Vec3& dashEndpoint, const Vec3& target, float targetRadius = 0.0f) {
    return dashEndpoint.Distance2D(target) <= kQSlashRadius + std::max(0.0f, targetRadius);
}

inline bool WLineHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float targetRadius = 0.0f, Form form = Form::Untransformed) {
    const Vec3 direction = Direction2D(origin, aim);
    if (direction.IsZero()) return false;
    const Vec3 endpoint = origin + direction * WRange(form);
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= kWHalfWidth + std::max(0.0f, targetRadius);
}

struct MobilityContext {
    bool Ready = false;
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool UnderNewTurret = false;
    bool PlayerUnderEnemyTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
    bool Lethal = false;
    Form FormState = Form::Untransformed;
};

inline bool MobilityAllowed(const MobilityContext& context) {
    if (!context.Ready || !context.EndpointValid || !context.EndpointWalkable ||
        context.UnderNewTurret) return false;
    if (context.PlayerUnderEnemyTurret && !context.Defensive && !context.Lethal) return false;
    if (context.EnemiesAtEndpoint > std::max(0, context.MaximumEnemies) &&
        !context.Defensive && !context.Lethal) return false;
    if (context.FormState == Form::ShadowAssassin && context.EnemiesAtEndpoint > 3 &&
        !context.Defensive && !context.Lethal) return false;
    return true;
}

inline bool PathTouchesWall(const Vec3& origin, const Vec3& endpoint,
                            bool sampledWall, float = 24.0f) {
    return origin.Distance2D(endpoint) > 1.0f && sampledWall;
}

struct WallTraversalContext {
    bool Ready = false;
    bool PathHasWall = false;
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool UnderNewTurret = false;
    bool PlayerUnderEnemyTurret = false;
    int EnemiesAtEndpoint = 0;
    int MaximumEnemies = 2;
    bool Defensive = false;
};

inline bool WallTraversalAllowed(const WallTraversalContext& context) {
    if (!context.Ready || !context.PathHasWall || !context.EndpointValid ||
        !context.EndpointWalkable || context.UnderNewTurret) return false;
    if (context.PlayerUnderEnemyTurret || context.Defensive) {
        return context.Defensive ||
            context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
    }
    return context.EnemiesAtEndpoint <= std::max(0, context.MaximumEnemies);
}

struct REntryContext {
    bool Ready = false;
    bool TargetValid = false;
    bool TargetMarked = false;
    bool TargetProtected = false;
    bool InRange = false;
    bool Lethal = false;
    bool Defensive = false;
    bool UnsafeEndpoint = false;
};

inline bool REntryAllowed(const REntryContext& context) {
    return context.Ready && context.TargetValid && context.TargetMarked &&
        !context.TargetProtected && context.InRange &&
        (!context.UnsafeEndpoint || context.Lethal || context.Defensive);
}

struct RRecastContext {
    bool Ready = false;
    bool HostActive = false;
    bool HostProtected = false;
    bool EndpointValid = false;
    bool EndpointWalkable = false;
    bool UnsafeEndpoint = false;
    bool Lethal = false;
    bool Defensive = false;
    bool HostExpiring = false;
};

inline bool RRecastAllowed(const RRecastContext& context) {
    if (!context.Ready || !context.HostActive || context.HostProtected ||
        !context.EndpointValid || !context.EndpointWalkable || context.UnsafeEndpoint) return false;
    return context.Lethal || context.Defensive || context.HostExpiring;
}

struct ModeContext {
    bool SelectedTarget = false;
    bool OrbwalkerTarget = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool ManualOwnership = false;
};

inline bool MayUseAbility(const ModeContext& context) {
    if (context.ManualOwnership) return false;
    if (context.AttackWindingUp && !context.Lethal) return false;
    return context.SelectedTarget || context.OrbwalkerTarget;
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
    bool ManualOwnership = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.ManualOwnership && !context.Engage &&
        (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Kayn::Geometry
