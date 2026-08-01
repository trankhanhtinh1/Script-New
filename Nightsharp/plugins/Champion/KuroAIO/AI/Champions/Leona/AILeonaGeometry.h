#pragma once

// Deterministic Leona mechanics. Runtime target selection, prediction, collision,
// NavMesh and casts stay in AILeonaController; this header is SDK-free so the
// reset, resistance, first-hit dash, Solar Flare and ally safety policies can
// be regression-tested in isolation.
#include "../../AIGeometry.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cfloat>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Leona::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 125.0f;
inline constexpr float kWRadius = 275.0f;
inline constexpr float kERange = 875.0f;
inline constexpr float kEWidth = 70.0f;
inline constexpr float kESpeed = 1200.0f;
inline constexpr float kRRange = 1200.0f;
inline constexpr float kRRadius = 300.0f;
inline constexpr float kRDelaySeconds = 0.625f;

inline bool QResetAllowed(bool targetInAttackRange, bool attackWindupActive,
                          bool urgentStun, bool preserveWindup) {
    if (!targetInAttackRange || preserveWindup) return false;
    return attackWindupActive || urgentStun;
}

inline float WResistanceBonus(int rank, float bonusResistance) {
    static constexpr std::array<float, 6> base = {0.0f, 20.0f, 25.0f, 30.0f,
                                                   35.0f, 40.0f};
    if (rank <= 0) return 0.0f;
    const int clamped = std::clamp(rank, 1, 5);
    return base[clamped] + 0.20f * std::max(0.0f, bonusResistance);
}

inline float WRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 45.0f, 80.0f, 115.0f,
                                                   150.0f, 185.0f};
    if (rank <= 0) return 0.0f;
    return base[std::clamp(rank, 1, 5)] + 0.40f * std::max(0.0f, abilityPower);
}

struct ZenithBody {
    Vec3 Position = {};
    float Radius = 0.0f;
    int Id = 0;
    bool Valid = true;
    bool Champion = true;
    bool Targetable = true;
};

struct ZenithContact {
    bool Hit = false;
    int BodyIndex = -1;
    int BodyId = 0;
    float Along = FLT_MAX;
    float Distance = FLT_MAX;
    Vec3 Position = {};
};

inline ZenithContact FirstZenithChampion(const Vec3& origin,
                                         const Vec3& requested,
                                         const std::vector<ZenithBody>& bodies,
                                         float range = kERange,
                                         float width = kEWidth) {
    ZenithContact best{};
    if (!origin.IsValid() || !requested.IsValid()) return best;
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return best;
    const float maxRange = std::max(0.0f, range);
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        const ZenithBody& body = bodies[i];
        if (!body.Valid || !body.Targetable || !body.Champion || body.Id == 0 ||
            !body.Position.IsValid()) continue;
        const Vec3 delta = body.Position - origin;
        const float along = delta.Dot(direction);
        if (along < -std::max(0.0f, body.Radius) || along > maxRange) continue;
        const float lateral = std::fabs(delta.x * direction.z - delta.z * direction.x);
        const float contactRadius = std::max(0.0f, width) * 0.5f +
            std::clamp(body.Radius, 0.0f, 150.0f);
        if (lateral > contactRadius) continue;
        if (!best.Hit || along < best.Along - 0.01f ||
            (std::fabs(along - best.Along) <= 0.01f && body.Id < best.BodyId)) {
            best.Hit = true;
            best.BodyIndex = static_cast<int>(i);
            best.BodyId = body.Id;
            best.Along = along;
            best.Distance = lateral;
            best.Position = origin + direction * std::max(0.0f, along);
            best.Position.y = origin.y;
        }
    }
    return best;
}

inline Vec3 ZenithDashEndpoint(const Vec3& origin, const ZenithContact& contact) {
    if (!contact.Hit || !origin.IsValid() || !contact.Position.IsValid()) return {};
    return contact.Position;
}

inline float ZenithTravelSeconds(float distance, float delay = 0.25f,
                                float speed = kESpeed) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline bool SolarFlareHits(const Vec3& center, const Vec3& predicted,
                           float targetRadius = 0.0f,
                           float radius = kRRadius) {
    return center.IsValid() && predicted.IsValid() &&
           center.Distance2D(predicted) <= std::max(0.0f, radius) +
               std::clamp(targetRadius, 0.0f, 150.0f);
}

inline int SolarFlareHitCount(const Vec3& center, const std::vector<Vec3>& targets,
                              float radius = kRRadius) {
    int count = 0;
    for (const Vec3& target : targets) {
        if (SolarFlareHits(center, target, 0.0f, radius)) ++count;
    }
    return count;
}

inline float SolarFlareImpactSeconds(float distance, float delay = kRDelaySeconds,
                                     float projectileSpeed = 0.0f) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    const float flight = projectileSpeed > 1.0f ? distance / projectileSpeed : 0.0f;
    return std::max(0.0f, delay) + flight;
}

inline bool SolarFlareSafe(bool allyFollowup, bool peelBranch, int enemiesAtCast,
                           int maximumEnemies, bool underEnemyTurret,
                           bool lethal, bool playerUnderTurret) {
    if (enemiesAtCast < 0 || enemiesAtCast > std::max(0, maximumEnemies)) return false;
    if (underEnemyTurret && !playerUnderTurret && !lethal && !peelBranch) return false;
    return allyFollowup || peelBranch || lethal;
}

inline bool AllyEngageSafe(bool allyValid, bool allyCanFollow, int enemiesAtLanding,
                           int maximumEnemies, bool underTurret, bool playerLow,
                           bool hasWStance) {
    if (!allyValid || !allyCanFollow || !hasWStance) return false;
    if (enemiesAtLanding < 0 || enemiesAtLanding > std::max(0, maximumEnemies)) return false;
    return !underTurret || playerLow;
}

inline bool AllyPeelSafe(bool allyValid, bool allyThreatened, bool targetInRadius,
                        bool playerLow, bool underTurret) {
    if (!allyValid || !allyThreatened || !targetInRadius) return false;
    return !underTurret || playerLow;
}

inline float QRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 10.0f, 30.0f, 50.0f,
                                                   70.0f, 90.0f};
    if (rank <= 0) return 0.0f;
    return base[std::clamp(rank, 1, 5)] + 0.30f * std::max(0.0f, abilityPower);
}

inline float ERawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 6> base = {0.0f, 50.0f, 90.0f, 130.0f,
                                                   170.0f, 210.0f};
    if (rank <= 0) return 0.0f;
    return base[std::clamp(rank, 1, 5)] + 0.40f * std::max(0.0f, abilityPower);
}

inline float RRawDamage(int rank, float abilityPower) {
    static constexpr std::array<float, 4> base = {0.0f, 150.0f, 225.0f, 300.0f};
    if (rank <= 0) return 0.0f;
    return base[std::clamp(rank, 1, 3)] + 0.80f * std::max(0.0f, abilityPower);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Leona::Geometry
