#pragma once

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Karma::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::ProjectPointToSegment2D;

inline constexpr float kQRange = 950.0f;
inline constexpr float kQSpeed = 902.0f;
inline constexpr float kQDelay = 0.25f;
inline constexpr float kQWidth = 90.0f;
inline constexpr float kQMantraWidth = 80.0f;
inline constexpr float kQExplosionRadius = 275.0f;
inline constexpr float kWRange = 675.0f;
inline constexpr float kWTetherRange = 825.0f;
inline constexpr float kWTetherSeconds = 2.0f;
inline constexpr float kWRootLeadSeconds = 0.35f;
inline constexpr float kERange = 600.0f;
inline constexpr float kEDisplayRange = 700.0f;
inline constexpr float kEAllyRadius = 400.0f;
inline constexpr float kRQSlow = 0.50f;
inline constexpr float kREMoveSpeed = 0.15f;

inline bool FinitePoint(const Vec3& point) {
    return point.IsValid();
}

inline bool SegmentHits(const Vec3& origin, const Vec3& endpoint,
                        const Vec3& target, float targetRadius,
                        float width) {
    if (!FinitePoint(origin) || !FinitePoint(endpoint) ||
        !FinitePoint(target)) return false;
    const auto projection = ProjectPointToSegment2D(target, origin, endpoint);
    return projection.Distance <= std::max(0.0f, width) * 0.5f +
        std::clamp(targetRadius, 0.0f, 180.0f);
}

inline bool QHits(const Vec3& origin, const Vec3& endpoint,
                  const Vec3& target, float targetRadius = 0.0f,
                  bool mantra = false) {
    return SegmentHits(origin, endpoint, target, targetRadius,
                       mantra ? kQMantraWidth : kQWidth);
}

inline bool QDetonationHits(const Vec3& center, const Vec3& target,
                            float targetRadius = 0.0f,
                            float radius = kQExplosionRadius) {
    return FinitePoint(center) && FinitePoint(target) &&
        center.Distance2D(target) <= std::max(0.0f, radius) +
            std::clamp(targetRadius, 0.0f, 180.0f);
}

inline float QTravelSeconds(float distance, bool mantra = false) {
    if (!std::isfinite(distance) || distance < 0.0f) return 0.0f;
    return kQDelay + distance / kQSpeed + (mantra ? 0.02f : 0.0f);
}

inline bool WithinReach(const Vec3& origin, const Vec3& destination,
                        float range, float radius = 0.0f) {
    return FinitePoint(origin) && FinitePoint(destination) &&
        origin.Distance2D(destination) <= std::max(0.0f, range) +
            std::max(0.0f, radius);
}

inline bool TetherCanHold(const Vec3& caster, const Vec3& target,
                          float elapsedSeconds,
                          float leashRange = kWTetherRange) {
    return FinitePoint(caster) && FinitePoint(target) &&
        std::isfinite(elapsedSeconds) && elapsedSeconds >= 0.0f &&
        elapsedSeconds <= kWTetherSeconds &&
        caster.Distance2D(target) <= std::max(0.0f, leashRange);
}

inline bool TetherRootWindow(const Vec3& caster, const Vec3& target,
                             float elapsedSeconds,
                             float leashRange = kWTetherRange) {
    return TetherCanHold(caster, target, elapsedSeconds, leashRange) &&
        elapsedSeconds >= std::max(0.0f, kWTetherSeconds - kWRootLeadSeconds);
}

inline Vec3 ShieldAim(const Vec3& caster, const Vec3& ally,
                      float range = kERange) {
    if (!FinitePoint(caster) || !FinitePoint(ally) ||
        !WithinReach(caster, ally, range)) return {};
    return ally;
}

enum class MantraPosture {
    RQ,
    RW,
    RE,
};

struct MantraChoiceContext {
    bool lethalQ = false;
    bool lethalW = false;
    bool threatenedAlly = false;
    bool threatenedSelf = false;
    bool groupedEnemies = false;
    bool tetherHeld = false;
    bool canReachQ = false;
    bool canReachW = false;
};

inline MantraPosture ChooseMantraPosture(const MantraChoiceContext& context) {
    if (context.threatenedAlly || context.threatenedSelf) return MantraPosture::RE;
    if (context.lethalW && context.canReachW) return MantraPosture::RW;
    if (context.lethalQ && context.canReachQ) return MantraPosture::RQ;
    if (context.groupedEnemies && context.canReachQ) return MantraPosture::RQ;
    if (context.tetherHeld && context.canReachW) return MantraPosture::RW;
    return MantraPosture::RE;
}

inline bool ShieldSpeedWorthwhile(float allyHealthPercent,
                                  bool allyThreatened,
                                  int enemiesNearAlly,
                                  float minimumHealthPercent = 72.0f) {
    return allyThreatened && std::isfinite(allyHealthPercent) &&
        allyHealthPercent <= std::clamp(minimumHealthPercent, 0.0f, 100.0f) &&
        enemiesNearAlly > 0;
}

inline bool SafeShieldDestination(const Vec3& allyPosition,
                                  bool wall,
                                  bool enemyTurret,
                                  int enemiesAtPosition,
                                  int alliesAtPosition,
                                  int maxEnemies = 3) {
    return FinitePoint(allyPosition) && !wall && !enemyTurret &&
        enemiesAtPosition >= 0 && alliesAtPosition >= 0 &&
        enemiesAtPosition <= std::max(0, maxEnemies);
}

inline bool PreserveAaWindup(bool reactive, bool tetherRoot,
                             bool lethalResponse) {
    return !reactive && !tetherRoot && !lethalResponse;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Karma::Geometry
