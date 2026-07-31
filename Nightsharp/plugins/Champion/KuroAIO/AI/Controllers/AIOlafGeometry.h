#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Olaf::Geometry {

using Vec3 = ::Vec3;
using SharedGeometry::Direction2D;

inline constexpr float kUndertowRange = 1000.0f;
inline constexpr float kUndertowWidth = 50.0f;
inline constexpr float kUndertowSlowRadius = 80.0f;
inline constexpr float kRecklessSwingRange = 325.0f;
inline constexpr float kRagnarokThreatRadius = 700.0f;
inline constexpr int kAxeLifetimeMs = 5000;

inline bool AxeLandingValid(const Vec3& origin,
                            const Vec3& landing,
                            float targetRadius = 0.0f) {
    if (!origin.IsValid() || !landing.IsValid() || origin.IsZero() ||
        landing.IsZero()) return false;
    return origin.Distance2D(landing) <=
        kUndertowRange + std::max(0.0f, targetRadius);
}

inline bool UndertowLineHits(const Vec3& origin,
                             const Vec3& endpoint,
                             const Vec3& target,
                             float targetRadius = 0.0f) {
    if (!AxeLandingValid(origin, endpoint) || !target.IsValid()) return false;
    const auto projection = SharedGeometry::ProjectPointToSegment2D(
        target, origin, endpoint);
    return projection.T >= 0.0f && projection.T <= 1.0f &&
           projection.Distance <= kUndertowWidth +
               std::max(0.0f, targetRadius);
}

inline bool AxeCanReset(float axeDistance,
                        float targetDistance,
                        bool axeObserved,
                        bool targetValid) {
    return axeObserved && targetValid && axeDistance <= kAxeLifetimeMs &&
           targetDistance <= kUndertowRange + kUndertowSlowRadius;
}

inline bool RecklessSwingLethal(float rawDamage,
                                float health,
                                float shield,
                                float selfHealthCost,
                                float minimumPostCastHealth) {
    return rawDamage >= std::max(0.0f, health) + std::max(0.0f, shield) &&
           selfHealthCost < std::max(0.0f, minimumPostCastHealth);
}

inline bool RagnarokCommitAllowed(bool hardCrowdControlIncoming,
                                  bool attacking,
                                  float playerHealthPercent,
                                  int enemyCount) {
    return attacking &&
           (hardCrowdControlIncoming || playerHealthPercent <= 68.0f) &&
           enemyCount <= 2;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Olaf::Geometry
