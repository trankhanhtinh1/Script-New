#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Seraphine::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 900.0f;
inline constexpr float kQRadius = 350.0f;
inline constexpr float kERange = 1300.0f;
inline constexpr float kEWidth = 80.0f;
inline constexpr float kRRange = 1200.0f;
inline constexpr float kRWidth = 160.0f;
inline constexpr float kCastDelay = 0.25f;
inline constexpr float kMissileSpeed = 1200.0f;
inline constexpr float kEchoThreshold = 3.0f;

enum class EchoMode : unsigned char { None, Ready, Consuming };
enum class EControl : unsigned char { Slow, Root, Stun };
enum class RTarget : unsigned char { EnemyCharm, AllyExtension, Invalid };

inline int AdvanceNotes(int notes, int nearbyAlliedChampions = 0) {
    const int earned = 1 + std::clamp(nearbyAlliedChampions, 0, 4);
    return std::clamp(notes + earned, 0, 4);
}
inline bool EchoReady(int notes) { return notes >= static_cast<int>(kEchoThreshold); }
inline int ConsumeEcho(int notes) { return EchoReady(notes) ? std::max(0, notes - 3) : notes; }
inline EchoMode EchoState(int notes) {
    return EchoReady(notes) ? EchoMode::Ready : (notes > 0 ? EchoMode::Consuming : EchoMode::None);
}

inline float QDamageMultiplier(float targetHealthPercent) {
    if (!std::isfinite(targetHealthPercent)) return 1.0f;
    const float missing = std::clamp((25.0f - targetHealthPercent) / 25.0f, 0.0f, 1.0f);
    return 1.0f + 0.75f * missing;
}
inline bool QExecuteWindow(float targetHealthPercent, float damage, float health,
                           float shield = 0.0f) {
    if (!std::isfinite(targetHealthPercent) || !std::isfinite(damage) ||
        !std::isfinite(health) || !std::isfinite(shield)) return false;
    return targetHealthPercent <= 25.0f && damage * QDamageMultiplier(targetHealthPercent) >=
        std::max(0.0f, health) + std::max(0.0f, shield);
}

inline bool FinitePoint(const Vec3& point) { return point.IsValid(); }
inline float TravelSeconds(float distance, float delay = kCastDelay,
                           float speed = kMissileSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(delay) || !std::isfinite(speed) ||
        distance < 0.0f || speed <= 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / speed;
}
inline bool TargetReachable(const Vec3& origin, const Vec3& predicted, float range,
                            float radius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(predicted) && std::isfinite(radius) &&
        origin.Distance2D(predicted) <= std::max(0.0f, range) + std::clamp(radius, 0.0f, 200.0f);
}
inline bool LineContacts(const Vec3& origin, const Vec3& endpoint, const Vec3& target,
                         float width, float radius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) * 0.5f + std::clamp(radius, 0.0f, 200.0f);
}
inline Vec3 ClampEndpoint(const Vec3& origin, const Vec3& requested, float range) {
    if (!FinitePoint(origin) || !FinitePoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range), origin.Distance2D(requested));
}
inline bool WallSafe(const Vec3& origin, const Vec3& endpoint, bool blocked, float range) {
    return FinitePoint(origin) && FinitePoint(endpoint) && !blocked &&
        origin.Distance2D(endpoint) <= std::max(0.0f, range) + 1.0f;
}

inline EControl ControlForTarget(bool slowed, bool rooted, bool echoed) {
    if (rooted || (slowed && echoed)) return EControl::Stun;
    if (slowed) return EControl::Root;
    return EControl::Slow;
}
inline bool ShouldCastW(float playerHealthPercent, float lowestAllyHealthPercent,
                        int nearbyEnemies, bool underTurret, int maxTurretEnemies,
                        bool defensive) {
    const bool low = playerHealthPercent <= 58.0f || lowestAllyHealthPercent <= 58.0f;
    return low && (!underTurret || defensive || nearbyEnemies <= std::max(0, maxTurretEnemies));
}
inline RTarget ClassifyRTarget(bool enemy, bool ally, bool protectedTarget) {
    if (protectedTarget) return RTarget::Invalid;
    if (enemy) return RTarget::EnemyCharm;
    if (ally) return RTarget::AllyExtension;
    return RTarget::Invalid;
}
inline float ExtendedRRange(int championContacts) {
    return kRRange + 400.0f * static_cast<float>(std::clamp(championContacts, 0, 4));
}
inline Vec3 ExtendedEndpoint(const Vec3& origin, const Vec3& requested,
                             int championContacts) {
    return ClampEndpoint(origin, requested, ExtendedRRange(championContacts));
}

struct AllyState {
    float Distance = 0.0f;
    float HealthPercent = 100.0f;
    bool CrowdControlled = false;
    bool Alive = true;
};
inline bool AllyInWRange(const AllyState& ally) {
    return ally.Alive && std::isfinite(ally.Distance) && ally.Distance <= 800.0f;
}
inline int LowestAllyIndex(const std::vector<AllyState>& allies) {
    int result = -1;
    float health = 101.0f;
    for (std::size_t i = 0; i < allies.size(); ++i) {
        if (!AllyInWRange(allies[i])) continue;
        if (allies[i].HealthPercent < health) { health = allies[i].HealthPercent; result = static_cast<int>(i); }
    }
    return result;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Seraphine::Geometry
