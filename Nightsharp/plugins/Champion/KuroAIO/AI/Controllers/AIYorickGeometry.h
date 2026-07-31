#pragma once

// Deterministic Yorick mechanics for graves, Mist Walkers, cage placement and
// Maiden lifecycle. Runtime object discovery, prediction and casts remain in
// AIYorickController.h.
#include "../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace Plugins::KuroAIO::AI::Controllers::Yorick::Geometry {

using ::Vec3;
using SharedGeometry::RankValue;

inline constexpr float kQRange = 325.0f;
inline constexpr float kQResetWindowSeconds = 0.50f;
inline constexpr float kWRange = 650.0f;
inline constexpr float kWRadius = 210.0f;
inline constexpr float kERange = 700.0f;
inline constexpr float kEWidth = 100.0f;
inline constexpr float kESpeed = 1200.0f;
inline constexpr float kECastDelay = 0.25f;
inline constexpr float kRRange = 700.0f;
inline constexpr float kMaidenDurationSeconds = 60.0f;
inline constexpr int kWalkerCap = 4;

struct GraveState {
    Vec3 Position{};
    int SpawnTick = 0;
    bool Confirmed = false;
    bool Consumed = false;
};

struct MistWalkerState {
    Vec3 Position{};
    int NetworkId = 0;
    int SpawnTick = 0;
    bool Confirmed = false;
    bool Alive = false;
};

enum class MaidenState : std::uint8_t { Absent, Alive, Recalling, Dead };

inline int ClampCount(int count) { return std::clamp(count, 0, kWalkerCap); }

inline bool GraveUsable(const GraveState& grave, const Vec3& origin,
                        float radius = 325.0f) {
    return grave.Confirmed && !grave.Consumed && origin.IsValid() &&
           !origin.IsZero() && grave.Position.IsValid() &&
           !grave.Position.IsZero() && origin.Distance2D(grave.Position) <=
               std::max(0.0f, radius);
}

inline bool CanRaiseWalkers(int graveCount, int walkerCount, bool qReady,
                            bool underEnemyTurret, bool lethal) {
    if (!qReady || graveCount <= 0 || walkerCount >= kWalkerCap) return false;
    return !underEnemyTurret || lethal;
}

inline float QBonusDamage(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 0.0f, 10.0f, 20.0f, 30.0f,
                                           40.0f, 50.0f}, rank);
}

inline float QRawDamage(int rank, float totalAttackDamage) {
    return QBonusDamage(rank) + std::max(0.0f, totalAttackDamage);
}

inline float QDamage(int rank, float totalAttackDamage, bool empowered) {
    const float raw = QRawDamage(rank, totalAttackDamage);
    return empowered ? raw + 0.0f : raw;
}

inline bool QResetActive(int castTick, int nowTick) {
    return castTick > 0 && nowTick >= castTick &&
           nowTick <= castTick + static_cast<int>(kQResetWindowSeconds * 1000.0f);
}

inline float WDurationSeconds(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 4.0f, 4.25f, 4.5f, 4.75f,
                                           5.0f, 5.0f}, rank);
}

struct WPlacementContext {
    bool Ready = false;
    bool CenterValid = false;
    bool Walkable = false;
    bool ProjectileWall = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool WPlacementSafe(const WPlacementContext& context) {
    if (!context.Ready || !context.CenterValid || !context.Walkable ||
        context.ProjectileWall) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.Lethal)
        return false;
    return context.Defensive || context.Lethal ||
           context.NearbyEnemies <= std::max(0, context.MaximumEnemies);
}

inline bool InWRange(const Vec3& origin, const Vec3& center) {
    return origin.IsValid() && center.IsValid() && !origin.IsZero() &&
           !center.IsZero() && origin.Distance2D(center) <= kWRange;
}

inline float EMarkDurationSeconds(int rank) {
    return RankValue(std::array<float, 7>{0.0f, 4.0f, 4.5f, 5.0f, 5.5f,
                                           6.0f, 6.0f}, rank);
}

inline float ERawDamage(int rank, float targetCurrentHealth,
                        float bonusAttackDamage) {
    const float percent = RankValue(std::array<float, 7>{0.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f}, rank);
    return std::max(0.0f, targetCurrentHealth) * (0.15f + percent) +
           0.70f * std::max(0.0f, bonusAttackDamage);
}

inline bool EHit(const Vec3& origin, const Vec3& endpoint,
                 const Vec3& target, float targetRadius = 65.0f) {
    if (!origin.IsValid() || !endpoint.IsValid() || !target.IsValid() ||
        origin.IsZero() || endpoint.IsZero() || target.IsZero()) return false;
    return SharedGeometry::ProjectPointToSegment2D(target, origin, endpoint)
        .Distance <= kEWidth * 0.5f + std::max(0.0f, targetRadius);
}

inline bool EProjectileSafe(bool ready, bool projectileWall, bool collision,
                            bool targetMarked, bool lethal) {
    (void)targetMarked;
    (void)lethal;
    return ready && !projectileWall && !collision;
}

inline float MaidenHealth(int rank, float playerMaxHealth) {
    const float ratio = RankValue(std::array<float, 7>{0.0f, 0.70f, 0.75f,
        0.80f, 0.85f, 0.90f, 0.90f}, rank);
    return std::max(0.0f, playerMaxHealth) * ratio;
}

inline float MaidenDurationSeconds(int rank) {
    (void)rank;
    return kMaidenDurationSeconds;
}

inline bool MaidenActive(MaidenState state, int summonTick, int nowTick,
                         float durationSeconds = kMaidenDurationSeconds) {
    return state == MaidenState::Alive && summonTick > 0 && nowTick >= summonTick &&
           nowTick <= summonTick + static_cast<int>(durationSeconds * 1000.0f);
}

struct SplitPushContext {
    bool MaidenActive = false;
    bool PlayerHealthy = false;
    bool TeleportAvailable = false;
    bool VisibleToEnemy = true;
    bool UnderEnemyTurret = false;
    bool NearbyEnemyThreat = false;
    bool HasMinionWave = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 1;
};

inline constexpr bool SplitPushSafe(const SplitPushContext& context) {
    if (!context.MaidenActive || !context.PlayerHealthy ||
        !context.HasMinionWave || context.UnderEnemyTurret ||
        context.NearbyEnemyThreat || !context.VisibleToEnemy) return false;
    if (context.NearbyEnemies > std::max(0, context.MaximumEnemies)) return false;
    return context.TeleportAvailable || context.NearbyEnemies == 0;
}

struct MaidenCastContext {
    bool Ready = false;
    bool TargetValid = false;
    bool UnderEnemyTurret = false;
    bool Defensive = false;
    bool Lethal = false;
    bool SplitPush = false;
    bool SafeSplit = false;
    int NearbyEnemies = 0;
    int MaximumEnemies = 2;
};

inline constexpr bool ShouldCastMaiden(const MaidenCastContext& context) {
    if (!context.Ready || !context.TargetValid) return false;
    if (context.UnderEnemyTurret && !context.Defensive && !context.Lethal)
        return false;
    if (context.SplitPush && !context.SafeSplit) return false;
    return context.Defensive || context.Lethal ||
           context.NearbyEnemies >= std::max(1, context.MaximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Yorick::Geometry
