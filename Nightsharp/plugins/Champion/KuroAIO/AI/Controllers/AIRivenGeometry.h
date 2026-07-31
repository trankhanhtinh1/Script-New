#pragma once

// Pure Riven mechanics.  No SDK objects or live state are required here so
// Q timing, hit shapes, execute arithmetic, and endpoint policy remain testable.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Riven::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 260.0f;
inline constexpr float kQDashDistance = 260.0f;
inline constexpr int kQRecastWindowMs = 4000;
inline constexpr int kQMinimumRecastMs = 250;
inline constexpr float kWRange = 260.0f;
inline constexpr float kERange = 250.0f;
inline constexpr float kRRange = 1100.0f;
inline constexpr float kRWidth = 100.0f;
inline constexpr float kRSpeed = 1600.0f;
inline constexpr int kRBuffDurationMs = 15000;

inline float QTotalAdRatio(int rank) {
    const int clamped = std::clamp(rank, 1, 5);
    return 0.50f + 0.05f * static_cast<float>(clamped - 1);
}

inline float QRawDamage(int rank, float totalAttackDamage, int stage = 1) {
    static constexpr std::array<float, 6> base{0.0f, 15.0f, 35.0f, 55.0f, 75.0f, 95.0f};
    (void)stage; // Q3 changes crowd control/area, not its raw damage.
    return base[std::clamp(rank, 0, 5)] +
           QTotalAdRatio(rank) * std::max(0.0f, totalAttackDamage);
}

inline float WRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{0.0f, 50.0f, 70.0f, 90.0f, 110.0f, 130.0f};
    return base[std::clamp(rank, 0, 5)] + 1.0f * std::max(0.0f, bonusAttackDamage);
}

inline float EShieldAmount(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{0.0f, 10.0f, 30.0f, 50.0f, 70.0f, 90.0f};
    return base[std::clamp(rank, 0, 5)] + 1.10f * std::max(0.0f, bonusAttackDamage);
}

inline float RExecuteMultiplier(float missingHealthPercent) {
    return 1.0f + std::clamp(missingHealthPercent, 0.0f, 100.0f) / 100.0f;
}

inline float RRawDamage(int rank, float bonusAttackDamage, float missingHealthPercent) {
    static constexpr std::array<float, 4> base{0.0f, 100.0f, 150.0f, 200.0f};
    return (base[std::clamp(rank, 0, 3)] + 0.60f * std::max(0.0f, bonusAttackDamage)) *
           RExecuteMultiplier(missingHealthPercent);
}

inline bool QRecastAllowed(int stage, int elapsedMs) {
    return stage >= 1 && stage <= 3 && elapsedMs >= kQMinimumRecastMs &&
           elapsedMs <= kQRecastWindowMs;
}

inline int NextQStage(int stage) {
    return stage >= 3 ? 1 : std::max(1, stage + 1);
}

inline bool QChainExpired(int castTick, int nowTick) {
    return castTick <= 0 || nowTick - castTick > kQRecastWindowMs;
}

inline Vec3 ClampDashEndpoint(const Vec3& origin, const Vec3& requested,
                              float range = kERange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}

inline bool WCanStun(float distance, bool targetProtected = false) {
    return !targetProtected && distance <= kWRange;
}

inline bool EEndpointSafe(bool endpointWalkable, bool endpointUnderTurret,
                          bool playerUnderTurret, int enemiesAtEndpoint,
                          int maximumEnemies, bool lethal, bool fleeing,
                          bool cursorIntent, bool manualConsent = false) {
    if (!endpointWalkable || !cursorIntent) return false;
    if (!fleeing && endpointUnderTurret && !playerUnderTurret && !lethal && !manualConsent) return false;
    return fleeing || lethal || manualConsent || enemiesAtEndpoint <= std::max(1, maximumEnemies);
}

struct Body {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Target = false;
    bool Valid = true;
};

struct LineHit {
    bool Hit = false;
    int TargetIndex = -1;
    std::vector<int> OrderedIds = {};
};

inline LineHit EvaluateRLine(const Vec3& origin, const Vec3& endpoint,
                             const std::vector<Body>& bodies, int targetId,
                             float width = kRWidth) {
    LineHit result{};
    std::vector<std::pair<float, Body>> contacts;
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || !body.Position.IsValid()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, origin, endpoint);
        if (projection.Distance <= width * 0.5f + std::max(0.0f, body.Radius))
            contacts.emplace_back(projection.T, body);
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const auto& a, const auto& b) { return a.first < b.first; });
    for (std::size_t i = 0; i < contacts.size(); ++i) {
        result.OrderedIds.push_back(contacts[i].second.Id);
        if (!result.Hit && (contacts[i].second.Id == targetId || contacts[i].second.Target)) {
            result.Hit = true;
            result.TargetIndex = static_cast<int>(i);
        }
    }
    return result;
}

struct RContext {
    bool Ready = false;
    bool Active = false;
    bool PredictionAccepted = false;
    bool LineHit = false;
    bool WallBlocked = false;
    bool Lethal = false;
    bool ExecuteWindow = false;
    bool Defensive = false;
    bool MultiTarget = false;
    bool UnsafeEndpoint = false;
};

inline bool MayCastR(const RContext& c) {
    if (!c.Ready || c.WallBlocked || c.UnsafeEndpoint) return false;
    if (!c.Active) return true; // R1 activation is a self buff.
    if (!c.PredictionAccepted || !c.LineHit) return false;
    return c.Lethal || c.ExecuteWindow || c.Defensive || c.MultiTarget;
}

struct ModeContext {
    bool SelectedTarget = false;
    bool OrbwalkerTarget = false;
    bool AttackWindingUp = false;
    bool Lethal = false;
    bool ManualAssist = false;
};

inline bool MayUseAbility(const ModeContext& c) {
    if (c.ManualAssist) return false;
    if (c.AttackWindingUp && !c.Lethal) return false;
    return c.SelectedTarget || c.OrbwalkerTarget;
}

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& c) {
    return !c.Engage && (c.Defensive || c.Interrupt || c.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Riven::Geometry
