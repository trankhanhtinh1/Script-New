#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Zyra::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 800.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQRadius = 140.0f;
inline constexpr float kWRange = 850.0f;
inline constexpr float kWSeedLifetimeSeconds = 60.0f;
inline constexpr float kWPlantLifetimeSeconds = 8.0f;
inline constexpr float kERange = 1100.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr float kEDelay = 0.25f;
inline constexpr float kESpeed = 1150.0f;
inline constexpr float kRRange = 700.0f;
inline constexpr float kRRadius = 500.0f;
inline constexpr float kRDelay = 2.0f;
inline constexpr float kRKnockupSeconds = 1.0f;
inline bool FinitePosition(const Vec3& point) {
    return point.IsValid() && !point.IsZero();
}

inline bool SeedPlacementValid(const Vec3& origin, const Vec3& seed,
                               bool terrainBlocked, bool underEnemyTurret,
                               float range = kWRange) {
    return FinitePosition(origin) && FinitePosition(seed) &&
        std::isfinite(range) && range >= 0.0f &&
        origin.Distance2D(seed) <= range && !terrainBlocked &&
        !underEnemyTurret;
}

inline bool QActivatesPlant(const Vec3& castOrigin, const Vec3& aim,
                            const Vec3& plant, float plantRadius = 55.0f) {
    if (!FinitePosition(castOrigin) || !FinitePosition(aim) || !FinitePosition(plant)) {
        return false;
    }
    if (castOrigin.Distance2D(aim) > kQRange) return false;
    const auto projection = ProjectPointToSegment2D(plant, castOrigin, aim);
    return projection.Distance <= kQRadius + std::max(0.0f, plantRadius);
}

inline bool ERootHits(const Vec3& origin, const Vec3& aim, const Vec3& target,
                      float targetRadius = 0.0f, int blockingUnits = 0,
                      bool projectileWall = false) {
    if (!FinitePosition(origin) || !FinitePosition(aim) || !FinitePosition(target) ||
        blockingUnits < 0 || projectileWall) return false;
    if (origin.Distance2D(aim) > kERange + std::max(0.0f, targetRadius)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, aim);
    return projection.Distance <= kEWidth + std::max(0.0f, targetRadius) &&
        projection.T >= 0.0f && projection.T <= 1.0f && blockingUnits == 0;
}

inline bool RZoneContains(const Vec3& center, const Vec3& target,
                          float targetRadius = 0.0f) {
    return FinitePosition(center) && FinitePosition(target) &&
        center.Distance2D(target) <= kRRadius + std::max(0.0f, targetRadius);
}

inline bool RCastSafe(const Vec3& center, bool terrainBlocked,
                      bool underEnemyTurret, int enemiesAtDestination,
                      int maximumEnemies = 2) {
    return FinitePosition(center) && !terrainBlocked && !underEnemyTurret &&
        enemiesAtDestination >= 0 && maximumEnemies >= 0 &&
        enemiesAtDestination <= maximumEnemies;
}

enum class PlantKind : unsigned char { ThornSpitter, VineLasher, Unknown };

struct PlantState {
    int NetworkId = 0;
    PlantKind Kind = PlantKind::Unknown;
    Vec3 Position = {};
    int SpawnTick = 0;
    int ExpireTick = 0;
    bool Alive = false;
};

inline bool PlantAliveAt(const PlantState& plant, int nowTick) {
    return plant.Alive && plant.NetworkId != 0 && FinitePosition(plant.Position) &&
        nowTick >= plant.SpawnTick && nowTick < plant.ExpireTick;
}

inline PlantState BeginPlant(int networkId, PlantKind kind, const Vec3& position,
                             int spawnTick) {
    const int lifetime = static_cast<int>(kWPlantLifetimeSeconds * 1000.0f);
    return {networkId, kind, position, spawnTick, spawnTick + std::max(1, lifetime),
            networkId != 0 && FinitePosition(position)};
}

inline int PrunePlants(std::array<PlantState, 16>& plants, int nowTick) {
    int alive = 0;
    for (auto& plant : plants) {
        if (!PlantAliveAt(plant, nowTick)) {
            plant = {};
        } else {
            ++alive;
        }
    }
    return alive;
}

inline float QRawDamage(int rank, float abilityPower) {
    const std::array<float, 5> base{60.0f, 100.0f, 140.0f, 180.0f, 220.0f};
    const int index = std::clamp(rank, 1, 5) - 1;
    return base[static_cast<std::size_t>(index)] + 0.65f * std::max(0.0f, abilityPower);
}

inline float ERawDamage(int rank, float abilityPower) {
    const std::array<float, 5> base{60.0f, 95.0f, 130.0f, 165.0f, 200.0f};
    const int index = std::clamp(rank, 1, 5) - 1;
    return base[static_cast<std::size_t>(index)] + 0.60f * std::max(0.0f, abilityPower);
}

inline float RRawDamage(int rank, float abilityPower) {
    const std::array<float, 3> base{200.0f, 300.0f, 400.0f};
    const int index = std::clamp(rank, 1, 3) - 1;
    return base[static_cast<std::size_t>(index)] + 0.70f * std::max(0.0f, abilityPower);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Zyra::Geometry
