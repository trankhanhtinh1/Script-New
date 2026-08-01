#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Leblanc::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 700.0f;
inline constexpr float kQMarkDurationSeconds = 3.5f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kWRadius = 240.0f;
inline constexpr float kWReturnSeconds = 4.0f;
inline constexpr float kERange = 925.0f;
inline constexpr float kEWidth = 110.0f;
inline constexpr float kESpeed = 1750.0f;
inline constexpr float kETetherDistance = 865.0f;
inline constexpr float kETetherSeconds = 1.5f;
inline constexpr float kPassiveTriggerHealthPercent = 40.0f;
inline constexpr float kPassiveCloneSeconds = 8.0f;
inline constexpr float kPassiveCooldownSeconds = 60.0f;

enum class MimicKind : std::uint8_t { None, Q, W, E };

inline float QInitialRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{ 0.0f, 65.0f, 90.0f, 115.0f, 140.0f, 165.0f };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, abilityPower);
}

inline float QMarkRawDamage(int rank, float abilityPower) {
    return QInitialRawDamage(rank, abilityPower);
}

inline float WRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{ 0.0f, 75.0f, 115.0f, 155.0f, 195.0f, 235.0f };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, abilityPower);
}

inline float EInitialRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{ 0.0f, 50.0f, 70.0f, 90.0f, 110.0f, 130.0f };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, abilityPower);
}

inline float EDelayedRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base{ 0.0f, 80.0f, 120.0f, 160.0f, 200.0f, 240.0f };
    return RankValue(base, rank) + 0.85f * std::max(0.0f, abilityPower);
}

inline float RQInitialRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{ 0.0f, 70.0f, 150.0f, 230.0f };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, abilityPower);
}

inline float RQMarkRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{ 0.0f, 140.0f, 300.0f, 460.0f };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, abilityPower);
}

inline float RWRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base{ 0.0f, 150.0f, 315.0f, 480.0f };
    return RankValue(base, rank) + 0.80f * std::max(0.0f, abilityPower);
}

inline float REInitialRawDamage(int rank, float abilityPower) {
    return RQInitialRawDamage(rank, abilityPower);
}

inline float REDelayedRawDamage(int rank, float abilityPower) {
    return RQMarkRawDamage(rank, abilityPower) + 0.05f * std::max(0.0f, abilityPower);
}

struct MarkState {
    int TargetId = 0;
    int ExpiresAt = 0;
    bool Mimic = false;
};

inline bool MarkActive(const MarkState& mark, int targetId, int now) {
    return targetId != 0 && mark.TargetId == targetId && mark.ExpiresAt > now;
}

inline bool SpellDetonatesMark(MimicKind spell) {
    return spell != MimicKind::None;
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Target = false;
    bool Valid = true;
};

struct ChainLine {
    bool TargetHit = false;
    bool Collision = false;
    int FirstBodyId = 0;
    float TargetT = 0.0f;
};

inline ChainLine EvaluateChainLine(const Vec3& origin, const Vec3& endpoint,
                                   const std::vector<Body>& bodies, int targetId) {
    ChainLine result{};
    std::vector<std::pair<float, Body>> contacts;
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, origin, endpoint);
        if (projection.Distance <= kEWidth * 0.5f + std::max(0.0f, body.Radius)) {
            contacts.emplace_back(projection.T, body);
        }
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });
    if (contacts.empty()) return result;
    result.FirstBodyId = contacts.front().second.Id;
    result.TargetHit = result.FirstBodyId == targetId || contacts.front().second.Target;
    result.Collision = !result.TargetHit;
    if (result.TargetHit) result.TargetT = contacts.front().first;
    return result;
}

inline bool TetherIntact(const Vec3& player, const Vec3& target,
                         float targetRadius = 0.0f) {
    return player.IsValid() && target.IsValid() &&
           player.Distance2D(target) <= kETetherDistance + std::max(0.0f, targetRadius);
}

inline Vec3 TetherPursuitPoint(const Vec3& player, const Vec3& predictedTarget,
                               const Vec3& fallback) {
    if (!player.IsValid() || !predictedTarget.IsValid()) return fallback;
    if (TetherIntact(player, predictedTarget)) return player;
    const Vec3 direction = Direction2D(player, predictedTarget);
    if (direction.IsZero()) return fallback;
    return player + direction * std::max(0.0f,
        player.Distance2D(predictedTarget) - kETetherDistance + 75.0f);
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kWRange, origin.Distance2D(requested));
}

struct DashContext {
    bool EndpointValid = false;
    bool EndpointWall = false;
    bool EndpointTurret = false;
    bool OriginTurret = false;
    bool ReturnAvailable = false;
    bool OriginSafe = false;
    bool Lethal = false;
    bool Fleeing = false;
    int EndpointEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool MayStartDash(const DashContext& context) {
    if (!context.EndpointValid || context.EndpointWall) return false;
    if (context.EndpointTurret && !context.OriginTurret && !context.Lethal && !context.Fleeing) {
        return false;
    }
    if (context.EndpointEnemies > std::max(0, context.MaximumEnemies) &&
        !context.Lethal && !context.Fleeing) return false;
    return context.ReturnAvailable || context.Lethal || context.Fleeing;
}

struct ReturnContext {
    bool Active = false;
    bool OriginValid = false;
    bool OriginSafe = false;
    bool CurrentSafe = true;
    bool TargetReachable = true;
    bool LowHealth = false;
    bool IncomingCrowdControl = false;
    bool TetherNeedsCurrentPosition = false;
    bool LethalContinuation = false;
};

inline bool ShouldReturn(const ReturnContext& context) {
    if (!context.Active || !context.OriginValid || !context.OriginSafe) return false;
    if (context.TetherNeedsCurrentPosition && !context.LowHealth &&
        !context.IncomingCrowdControl) return false;
    if (context.LethalContinuation && context.CurrentSafe &&
        !context.IncomingCrowdControl) return false;
    return !context.CurrentSafe || !context.TargetReachable || context.LowHealth ||
           context.IncomingCrowdControl;
}

struct CloneState {
    int NetworkId = 0;
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Allied = false;
};

inline bool CloneActive(const CloneState& clone, int now) {
    return clone.NetworkId != 0 && clone.Allied && clone.SpawnTick <= now &&
           clone.ExpireTick > now;
}

struct AutomaticContext {
    bool DefensiveReturn = false;
    bool KillSecure = false;
    bool PreserveTether = false;
    bool StartEngage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.StartEngage &&
           (context.DefensiveReturn || context.KillSecure || context.PreserveTether);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Leblanc::Geometry
