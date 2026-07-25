#pragma once

// Pure geometry for Akali's one-trick controller.  None of these helpers read
// live game memory, which lets the cone edge, passive-ring path, E backflip and
// both Perfect Execution paths be regression tested outside League.

#include "../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Akali::Geometry {

using SharedGeometry::Direction2D;
using SharedGeometry::kPi;
using SharedGeometry::ProjectPointToSegment2D;
using SharedGeometry::Rotate2D;
using SharedGeometry::SegmentProjection;

struct ConeHit {
    bool Hits = false;
    bool TipSlow = false;
    float Score = 0.0f;
    float Forward = 0.0f;
    float Lateral = 0.0f;
};

// Five Point Strike is represented by five fanned missiles rather than a
// mathematically perfect sector.  The two outer knives reach slightly farther
// in world space; experienced Akali players aim a corner at targets just
// beyond the middle knife.  This model keeps the documented 500/350 cone and
// grants only a conservative 45-unit edge extension.
inline ConeHit FivePointHit(const Vec3& source,
                            const Vec3& castDirection,
                            const Vec3& targetPosition,
                            float targetRadius,
                            float range = 500.0f,
                            float nearWidth = 120.0f,
                            float farWidth = 350.0f) {
    const Vec3 direction = Direction2D({}, castDirection);
    if (direction.IsZero() || !source.IsValid() || !targetPosition.IsValid()) {
        return {};
    }
    Vec3 relative = targetPosition - source;
    relative.y = 0.0f;
    const float forward = relative.Dot(direction);
    const float lateral = std::fabs(direction.x * relative.z -
                                    direction.z * relative.x);
    const float radius = std::clamp(targetRadius, 0.0f, 100.0f);
    const float progress = std::clamp(forward / std::max(1.0f, range), 0.0f, 1.0f);
    const float halfWidth = (nearWidth + (farWidth - nearWidth) * progress) * 0.5f;
    const float edge = halfWidth > 1.0f
        ? std::clamp(lateral / halfWidth, 0.0f, 1.0f)
        : 0.0f;
    const float extendedRange = range + 45.0f * edge;
    const bool inside = forward >= -radius &&
                        forward <= extendedRange + radius &&
                        lateral <= halfWidth + radius;
    if (!inside) return { false, false, 0.0f, forward, lateral };

    const float centered = 1.0f - std::clamp(
        lateral / std::max(1.0f, halfWidth + radius), 0.0f, 1.0f);
    const float depth = 1.0f - std::clamp(
        std::fabs(forward - std::min(range, 470.0f)) /
            std::max(220.0f, range),
        0.0f, 1.0f);
    const bool tip = forward + radius >= range - 55.0f;
    const float score = std::clamp(
        centered * 0.55f + depth * 0.25f + (tip ? 0.20f : 0.0f),
        0.0f, 1.0f);
    return { true, tip, score, forward, lateral };
}

struct FivePointAim {
    Vec3 Direction = {};
    ConeHit Hit = {};
    float RotationRadians = 0.0f;
};

inline FivePointAim BestFivePointAim(const Vec3& source,
                                     const Vec3& targetPosition,
                                     float targetRadius) {
    const Vec3 direct = Direction2D(source, targetPosition);
    if (direct.IsZero()) return {};

    const ConeHit directHit = FivePointHit(source, direct, targetPosition, targetRadius);
    const float dist = source.Distance2D(targetPosition);
    if (directHit.Hits && dist <= 500.0f + std::max(0.0f, targetRadius)) {
        return { direct, directHit, 0.0f };
    }

    constexpr float rotations[] = {
        0.0f,
        9.5f * kPi / 180.0f,
        -9.5f * kPi / 180.0f,
        6.0f * kPi / 180.0f,
        -6.0f * kPi / 180.0f,
    };
    FivePointAim best{ direct, directHit, 0.0f };
    float bestUtility = directHit.Hits ? directHit.Score : -1.0f;
    for (float rotation : rotations) {
        if (rotation == 0.0f) continue;
        const Vec3 candidate = Rotate2D(direct, rotation);
        const ConeHit hit = FivePointHit(
            source, candidate, targetPosition, targetRadius);
        if (!hit.Hits) continue;
        float utility = hit.Score;
        if (hit.TipSlow) utility += 0.08f;
        utility -= std::fabs(rotation) * 2.0f;
        if (utility > bestUtility) {
            bestUtility = utility;
            best = { candidate, hit, rotation };
        }
    }
    return best;
}

inline Vec3 PassiveRingCenter(const Vec3& akaliPosition,
                              const Vec3& targetPosition,
                              float offsetTowardAkali = 120.0f) {
    const Vec3 towardAkali = Direction2D(targetPosition, akaliPosition);
    return towardAkali.IsZero()
        ? targetPosition
        : targetPosition + towardAkali * offsetTowardAkali;
}

inline float PassiveExitDistance(const Vec3& akaliPosition,
                                 const Vec3& ringCenter,
                                 float ringRadius = 500.0f,
                                 float margin = 18.0f) {
    return std::max(0.0f,
        ringRadius + margin - akaliPosition.Distance2D(ringCenter));
}

inline Vec3 PassiveExitPoint(const Vec3& akaliPosition,
                             const Vec3& ringCenter,
                             const Vec3& cursorPosition,
                             float ringRadius = 500.0f,
                             float margin = 18.0f) {
    Vec3 direction = Direction2D(ringCenter, akaliPosition);
    if (direction.IsZero()) direction = Direction2D(ringCenter, cursorPosition);
    if (direction.IsZero()) direction = { 1.0f, 0.0f, 0.0f };
    Vec3 point = ringCenter + direction * (ringRadius + margin);
    point.y = akaliPosition.y;
    return point;
}

inline Vec3 ShurikenBackflipEnd(const Vec3& source,
                                const Vec3& castDirection,
                                float backflipDistance = 400.0f) {
    const Vec3 direction = Direction2D({}, castDirection);
    return direction.IsZero() ? source : source - direction * backflipDistance;
}

inline float DashLineHitScore(const Vec3& source,
                              const Vec3& destination,
                              const Vec3& targetPosition,
                              float targetRadius,
                              float dashRadius = 110.0f) {
    const SegmentProjection projection = ProjectPointToSegment2D(
        targetPosition, source, destination);
    const float hitRadius = std::max(20.0f, dashRadius) +
                            std::clamp(targetRadius, 20.0f, 100.0f);
    if (projection.Distance > hitRadius) return 0.0f;
    const float centered = 1.0f - projection.Distance / hitRadius;
    const float interior = 0.72f + 0.28f *
        (1.0f - std::min(1.0f, std::fabs(projection.T - 0.5f) * 2.0f));
    return std::clamp(centered * interior, 0.0f, 1.0f);
}

inline Vec3 R1LandingPoint(const Vec3& source,
                           const Vec3& targetPosition,
                           float normalDistance = 750.0f,
                           float maximumDistance = 900.0f,
                           float minimumPassThrough = 150.0f) {
    const Vec3 direction = Direction2D(source, targetPosition);
    if (direction.IsZero()) return source;
    const float targetDistance = source.Distance2D(targetPosition);
    const float distance = std::clamp(
        std::max(normalDistance, targetDistance + minimumPassThrough),
        normalDistance, maximumDistance);
    return source + direction * distance;
}

inline float R2ExecuteMultiplier(float targetHealthPercent) {
    const float missing = 1.0f -
        std::clamp(targetHealthPercent, 0.0f, 100.0f) / 100.0f;
    return 1.0f + 2.0f * missing;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Akali::Geometry
