#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Xayah::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 1100.0f;
inline constexpr float kQWidth = 70.0f;
inline constexpr float kQSpeed = 2000.0f;
inline constexpr float kQFeatherSpread = 32.0f;
inline constexpr float kRRange = 1100.0f;
inline constexpr float kRWidth = 80.0f;
inline constexpr int kFeatherLifetimeMs = 6000;
inline constexpr int kRootThreshold = 3;
inline constexpr int kMaximumTrackedFeathers = 64;

struct Feather {
    Vec3 Position = {};
    int NetworkId = 0;
    int SpawnTick = 0;
    bool Active = false;
};

inline Vec3 ClampFeatherEndpoint(const Vec3& origin,
                                 const Vec3& requested,
                                 float range = kQRange) {
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero() || !origin.IsValid()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool FeatherSegmentHits(const Vec3& feather,
                               const Vec3& origin,
                               const Vec3& target,
                               float targetRadius = 65.0f) {
    if (feather.IsZero() || origin.IsZero() || target.IsZero() ||
        !feather.IsValid() || !origin.IsValid() || !target.IsValid()) {
        return false;
    }
    const auto projection = ProjectPointToSegment2D(
        target, feather, origin);
    return projection.Distance <= std::max(0.0f, targetRadius);
}

template <std::size_t N>
inline int CountFeathersThrough(const std::array<Feather, N>& feathers,
                                const Vec3& origin,
                                const Vec3& target,
                                float targetRadius = 65.0f,
                                int now = 0) {
    int count = 0;
    for (const auto& feather : feathers) {
        if (!feather.Active || !FeatherSegmentHits(
                feather.Position, origin, target, targetRadius)) {
            continue;
        }
        if (now > 0 && feather.SpawnTick > 0 &&
            now - feather.SpawnTick > kFeatherLifetimeMs) continue;
        ++count;
    }
    return count;
}

inline float QRawDamage(int rank, float bonusAttackDamage) {
    static constexpr std::array<float, 6> base{
        0.0f, 45.0f, 60.0f, 75.0f, 90.0f, 105.0f};
    return RankValue(base, rank) + std::max(0.0f, bonusAttackDamage) * 0.5f;
}

inline float FeatherDamagePerHit(int rank, float bonusAttackDamage,
                                int featherIndex) {
    const float attenuation = std::pow(
        0.95f, static_cast<float>(std::max(0, featherIndex)));
    return std::max(0.0f, QRawDamage(rank, bonusAttackDamage)) * attenuation;
}

inline float BladecallerRawDamage(int rank, float bonusAttackDamage,
                                  int featherCount) {
    const int count = std::clamp(featherCount, 0, kMaximumTrackedFeathers);
    static constexpr std::array<float, 6> base{
        0.0f, 55.0f, 65.0f, 75.0f, 85.0f, 95.0f};
    const float perHit = RankValue(base, rank) +
        std::max(0.0f, bonusAttackDamage) * 0.6f;
    float total = 0.0f;
    for (int index = 0; index < count; ++index) {
        total += perHit * std::pow(
            0.95f, static_cast<float>(index));
    }
    return total;
}

inline float FeatherstormRawDamage(int rank, float bonusAttackDamage,
                                   int featherCount) {
    static constexpr std::array<float, 4> base{
        0.0f, 125.0f, 250.0f, 375.0f};
    return RankValue(base, rank) +
           std::max(0.0f, bonusAttackDamage) * 0.5f *
               static_cast<float>(std::clamp(featherCount, 1, 5));
}

struct BladecallerContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionAccepted = false;
    bool RootReady = false;
    bool Lethal = false;
    bool AttackWindingUp = false;
    bool TurretUnsafe = false;
    int FeatherHits = 0;
    int MinimumFeathers = 1;
};

inline bool ShouldBladecaller(const BladecallerContext& context) {
    if (!context.Ready || !context.TargetValid ||
        !context.PredictionAccepted || context.TurretUnsafe) return false;
    if (context.AttackWindingUp && !context.Lethal && !context.RootReady) return false;
    const int minimum = std::max(1, context.MinimumFeathers);
    return context.FeatherHits >= minimum || context.RootReady || context.Lethal;
}

struct WPostureContext {
    bool Ready = false;
    bool TargetValid = false;
    bool AttackRoute = false;
    bool AttackWindingUp = false;
    bool LethalWindow = false;
    bool Defensive = false;
    bool TurretUnsafe = false;
};

inline bool ShouldDeadlyPlumage(const WPostureContext& context) {
    if (!context.Ready || !context.TargetValid || !context.AttackRoute ||
        context.TurretUnsafe) return false;
    if (context.AttackWindingUp && !context.LethalWindow && !context.Defensive) {
        return false;
    }
    return context.LethalWindow || context.Defensive || !context.AttackWindingUp;
}

struct RPostureContext {
    bool Ready = false;
    bool TargetValid = false;
    bool PredictionAccepted = false;
    bool ProjectileBlocked = false;
    bool PlayerLow = false;
    bool MultiTarget = false;
    bool Lethal = false;
    bool Defensive = false;
    bool TurretUnsafe = false;
    int ChampionHits = 0;
    int MinimumHits = 1;
};

inline bool ShouldFeatherstorm(const RPostureContext& context) {
    if (!context.Ready || !context.TargetValid ||
        !context.PredictionAccepted || context.ProjectileBlocked ||
        (context.TurretUnsafe && !context.Defensive &&
         !context.Lethal)) return false;
    if (context.PlayerLow || context.Defensive || context.Lethal) return true;
    return context.MultiTarget &&
           context.ChampionHits >= std::max(1, context.MinimumHits);
}

inline bool RootAvailable(int featherHits, int threshold = kRootThreshold) {
    return featherHits >= std::max(1, threshold);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Xayah::Geometry
