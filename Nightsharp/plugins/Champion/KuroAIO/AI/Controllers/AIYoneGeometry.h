#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Yone::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 475.0f;
inline constexpr float kQWhirlwindRange = 650.0f;
inline constexpr float kQWidth = 60.0f;
inline constexpr float kWRange = 600.0f;
inline constexpr float kWAngleDegrees = 45.0f;
inline constexpr float kWWidth = 80.0f;
inline constexpr float kERange = 400.0f;
inline constexpr int kEDurationMs = 5000;
inline constexpr float kRRange = 1000.0f;
inline constexpr float kRWidth = 120.0f;
inline constexpr float kRSpeed = 1500.0f;

inline float PhysicalShare(float rawDamage) { return std::max(0.0f, rawDamage) * 0.5f; }
inline float MagicalShare(float rawDamage) { return std::max(0.0f, rawDamage) * 0.5f; }

inline float PassiveBonusDamage(int level, float totalAttackDamage) {
    const int clamped = std::clamp(level, 1, 18);
    return (0.05f + 0.01f * static_cast<float>(clamped - 1)) *
           std::max(0.0f, totalAttackDamage);
}

inline float QRawDamage(int rank, float totalAttackDamage) {
    static constexpr std::array<float, 6> base{ 0.0f, 20.0f, 45.0f, 70.0f, 95.0f, 120.0f };
    return RankValue(base, rank) + std::max(0.0f, totalAttackDamage);
}

inline float QDamage(int rank, float totalAttackDamage, int bodyIndex = 0) {
    return QRawDamage(rank, totalAttackDamage) * (bodyIndex == 0 ? 1.0f : 0.75f);
}

inline float WRawDamage(int rank, float totalAttackDamage, float targetMaximumHealth) {
    static constexpr std::array<float, 6> base{ 0.0f, 10.0f, 20.0f, 30.0f, 40.0f, 50.0f };
    static constexpr std::array<float, 6> percent{ 0.0f, 0.11f, 0.12f, 0.13f, 0.14f, 0.15f };
    return RankValue(base, rank) + 0.11f * std::max(0.0f, totalAttackDamage) +
           RankValue(percent, rank) * std::max(0.0f, targetMaximumHealth);
}

inline float EMarkDamage(float rawDamage, float storedRatio = 0.25f) {
    return std::max(0.0f, rawDamage) * std::clamp(storedRatio, 0.0f, 1.0f);
}

inline float RRawDamage(int rank, float totalAttackDamage, float missingHealthPercent) {
    static constexpr std::array<float, 4> base{ 0.0f, 150.0f, 225.0f, 300.0f };
    return RankValue(base, rank) + 0.40f * std::max(0.0f, totalAttackDamage) +
           0.10f * std::clamp(missingHealthPercent, 0.0f, 100.0f);
}

inline bool CanWhirlwind(int qStacks, bool qReady) { return qReady && qStacks >= 2; }
inline bool CanCastQ(int qStacks, bool qReady) {
    return qReady && qStacks >= 0 && qStacks < 2;
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
    int FirstBodyId = 0;
    int TargetIndex = -1;
    std::vector<int> OrderedIds = {};
};

inline LineHit EvaluateLine(const Vec3& origin, const Vec3& endpoint,
                            const std::vector<Body>& bodies, int targetId,
                            float width) {
    LineHit result{};
    std::vector<std::pair<float, Body>> contacts;
    for (const Body& body : bodies) {
        if (!body.Valid || body.Id == 0 || body.Position.IsZero()) continue;
        const auto projection = ProjectPointToSegment2D(body.Position, origin, endpoint);
        if (projection.Distance <= width * 0.5f + std::max(0.0f, body.Radius)) {
            contacts.emplace_back(projection.T, body);
        }
    }
    std::stable_sort(contacts.begin(), contacts.end(),
        [](const auto& left, const auto& right) { return left.first < right.first; });
    for (std::size_t index = 0; index < contacts.size(); ++index) {
        if (index == 0) result.FirstBodyId = contacts[index].second.Id;
        result.OrderedIds.push_back(contacts[index].second.Id);
        if (contacts[index].second.Id == targetId || contacts[index].second.Target) {
            if (!result.Hit) {
                result.Hit = true;
                result.TargetIndex = static_cast<int>(index);
            }
        }
    }
    return result;
}

inline Vec3 ClampSpiritEndpoint(const Vec3& origin, const Vec3& requested) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(kERange, origin.Distance2D(requested));
}

struct SpiritContext {
    bool Active = false;
    bool EndpointValid = false;
    bool EndpointSafe = false;
    bool ReturnAvailable = false;
    bool TargetMarked = false;
    bool Lethal = false;
    bool Fleeing = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline bool SpiritSafe(const SpiritContext& context) {
    if (!context.EndpointValid || !context.EndpointSafe) return false;
    if (context.NearbyEnemies > std::max(0, context.MaximumEnemies) &&
        !(context.Lethal || context.Fleeing)) return false;
    return context.ReturnAvailable || context.Lethal || context.Fleeing;
}

struct RContext {
    bool Ready = false;
    bool PredictionAccepted = false;
    bool LineHit = false;
    bool WallBlocked = false;
    bool Lethal = false;
    bool MultiTarget = false;
    bool Defensive = false;
    bool UnsafeEndpoint = false;
    int ChampionHits = 0;
};

inline bool MayCastR(const RContext& context) {
    if (!context.Ready || !context.PredictionAccepted || !context.LineHit ||
        context.WallBlocked || context.UnsafeEndpoint) return false;
    return context.Lethal || context.MultiTarget || context.Defensive ||
           context.ChampionHits >= 2;
}

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

struct AutomaticContext {
    bool Defensive = false;
    bool Interrupt = false;
    bool KillSecure = false;
    bool Engage = false;
};

inline bool AutomaticAllowed(const AutomaticContext& context) {
    return !context.Engage && (context.Defensive || context.Interrupt || context.KillSecure);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Yone::Geometry
