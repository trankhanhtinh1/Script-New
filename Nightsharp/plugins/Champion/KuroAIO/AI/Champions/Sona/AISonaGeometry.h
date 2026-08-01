#pragma once

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Plugins::KuroAIO::AI::Controllers::Sona::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 825.0f;
inline constexpr float kQMissileWidth = 70.0f;
inline constexpr float kQMissileSpeed = 1300.0f;
inline constexpr float kRRange = 900.0f;
inline constexpr float kRWidth = 140.0f;
inline constexpr float kRSpeed = 2400.0f;
inline constexpr float kCastDelay = 0.25f;
inline constexpr float kAuraRange = 400.0f;

// Sona's three basic spells leave a distinct aura state; the next basic attack
// consumes the third spell stack using the aura that was active when it formed.
enum class Aura : unsigned char { None, HymnOfValor, AriaOfPerseverance, SongOfCelerity };
enum class PowerChord : unsigned char { None, Staccato, Diminuendo, Tempo };

inline Aura AuraFromSpell(int slot) {
    switch (slot) {
    case 0: return Aura::HymnOfValor;
    case 1: return Aura::AriaOfPerseverance;
    case 2: return Aura::SongOfCelerity;
    default: return Aura::None;
    }
}

inline PowerChord ChordForAura(Aura aura) {
    switch (aura) {
    case Aura::HymnOfValor: return PowerChord::Staccato;
    case Aura::AriaOfPerseverance: return PowerChord::Diminuendo;
    case Aura::SongOfCelerity: return PowerChord::Tempo;
    default: return PowerChord::None;
    }
}

inline int AdvanceChordStacks(int stacks) {
    return std::clamp(stacks + 1, 0, 3);
}

inline bool ChordReady(int stacks) { return stacks >= 3; }

inline PowerChord ChordToConsume(int stacks, Aura aura) {
    return ChordReady(stacks) ? ChordForAura(aura) : PowerChord::None;
}

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline float TravelSeconds(float distance, float delay = kCastDelay,
                           float speed = kQMissileSpeed) {
    if (!std::isfinite(distance) || !std::isfinite(delay) ||
        !std::isfinite(speed) || distance < 0.0f) return 0.0f;
    return std::max(0.0f, delay) + distance / std::max(1.0f, speed);
}

inline bool QTargetReachable(const Vec3& origin, const Vec3& predicted,
                             float targetRadius = 0.0f) {
    if (!FinitePoint(origin) || !FinitePoint(predicted)) return false;
    return origin.Distance2D(predicted) <= kQRange +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool QProjectileContacts(const Vec3& origin, const Vec3& predicted,
                                const Vec3& target, float targetRadius = 0.0f,
                                float width = kQMissileWidth) {
    if (!FinitePoint(origin) || !FinitePoint(predicted) || !FinitePoint(target))
        return false;
    const auto projection = ProjectPointToSegment2D(target, origin, predicted);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 150.0f);
}

inline bool ConeContacts(const Vec3& origin, const Vec3& endpoint,
                         const Vec3& target, float targetRadius = 0.0f,
                         float width = kRWidth) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) || !FinitePoint(target))
        return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.T > 0.0f && projection.T <= 1.0f &&
        projection.Distance <= std::max(0.0f, width) * 0.5f +
            std::clamp(targetRadius, 0.0f, 150.0f);
}

inline Vec3 ClampUltimateEndpoint(const Vec3& origin, const Vec3& requested,
                                  float range = kRRange) {
    if (!FinitePoint(origin) || !FinitePoint(requested)) return {};
    const Vec3 direction = Direction2D(origin, requested);
    if (direction.IsZero()) return {};
    return origin + direction * std::min(std::max(0.0f, range),
                                         origin.Distance2D(requested));
}

inline bool UltimateWallSafe(const Vec3& origin, const Vec3& endpoint,
                             bool wallBlocked) {
    return FinitePoint(origin) && FinitePoint(endpoint) && !wallBlocked &&
        origin.Distance2D(endpoint) <= kRRange + 1.0f;
}

struct AllyProximity {
    float Distance = 0.0f;
    float HealthPercent = 100.0f;
    int ThreatCount = 0;
    bool CrowdControlled = false;
    bool Alive = true;
};

inline bool WithinAura(const AllyProximity& ally,
                       float range = kAuraRange) {
    return ally.Alive && std::isfinite(ally.Distance) &&
        ally.Distance <= std::max(0.0f, range);
}

inline bool NeedsAria(const AllyProximity& ally, float threshold = 62.0f) {
    return WithinAura(ally) && std::isfinite(ally.HealthPercent) &&
        ally.HealthPercent <= std::clamp(threshold, 0.0f, 100.0f) &&
        (ally.ThreatCount > 0 || ally.HealthPercent <= threshold * 0.65f);
}

inline bool NeedsCelerity(const AllyProximity& ally, float threshold = 1.0f) {
    return WithinAura(ally) && (ally.ThreatCount >= 1 || ally.CrowdControlled) &&
        std::isfinite(threshold);
}

inline int BestAllyIndex(const std::vector<AllyProximity>& allies,
                         float range = kAuraRange) {
    int best = -1;
    float bestScore = -1.0e30f;
    for (std::size_t i = 0; i < allies.size(); ++i) {
        const auto& ally = allies[i];
        if (!WithinAura(ally, range)) continue;
        const float missing = std::clamp(100.0f - ally.HealthPercent, 0.0f, 100.0f);
        const float score = missing * 2.2f +
            static_cast<float>(ally.ThreatCount) * 28.0f +
            (ally.CrowdControlled ? 36.0f : 0.0f) - ally.Distance * 0.035f;
        if (score > bestScore) { bestScore = score; best = static_cast<int>(i); }
    }
    return best;
}

inline bool UnsafeAuraCommit(int enemiesAtPlayer, bool underTurret,
                             int maximumEnemies = 2, bool defensive = false) {
    return !underTurret || defensive ||
        enemiesAtPlayer <= std::max(0, maximumEnemies);
}

} // namespace Plugins::KuroAIO::AI::Controllers::Sona::Geometry
