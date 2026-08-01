#pragma once

// Pure, live-memory-free Aatrox Q geometry.  Kept separate so the exact model
// used by the controller can be compiled and exercised as a standalone test.

#include "../../AIGeometry.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroAIO::AI::Controllers::Aatrox::Geometry {

using SharedGeometry::Cross2D;
using SharedGeometry::Direction2D;

enum class QStage : int {
    First = 1,
    Second = 2,
    Third = 3,
};

inline float QRange(QStage stage) {
    switch (stage) {
    case QStage::First: return 650.0f;
    case QStage::Second: return 525.0f;
    case QStage::Third: return 400.0f;
    default: return 650.0f;
    }
}

inline float QIdealForward(QStage stage) {
    switch (stage) {
    case QStage::First: return 565.0f;
    case QStage::Second: return 410.0f;
    case QStage::Third: return 200.0f;
    default: return 500.0f;
    }
}

inline float SweetspotScore(QStage stage,
                            const Vec3& source,
                            const Vec3& direction,
                            const Vec3& targetPosition,
                            float targetRadius) {
    if (direction.IsZero() || !targetPosition.IsValid()) {
        return 0.0f;
    }
    Vec3 relative = targetPosition - source;
    relative.y = 0.0f;
    const float forward = relative.Dot(direction);
    const float lateral = std::fabs(Cross2D(direction, relative));
    const float radius = std::clamp(targetRadius, 25.0f, 85.0f);

    if (stage == QStage::First) {
        const float minForward = 475.0f - radius * 0.35f;
        const float maxForward = 650.0f + radius * 0.25f;
        const float halfWidth = 90.0f + radius * 0.55f;
        if (forward < minForward || forward > maxForward || lateral > halfWidth) {
            return 0.0f;
        }
        const float radial = 1.0f -
            std::min(1.0f, std::fabs(forward - 565.0f) / 110.0f);
        const float centered = 1.0f - std::min(1.0f, lateral / halfWidth);
        return radial * 0.68f + centered * 0.32f;
    }

    if (stage == QStage::Second) {
        const float minForward = 300.0f - radius * 0.30f;
        const float maxForward = 500.0f + radius * 0.35f;
        const float widening = std::clamp((forward + 100.0f) / 575.0f, 0.0f, 1.0f);
        const float halfWidth = 150.0f + widening * 100.0f + radius * 0.45f;
        if (forward < minForward || forward > maxForward || lateral > halfWidth) {
            return 0.0f;
        }
        const float radial = 1.0f -
            std::min(1.0f, std::fabs(forward - 410.0f) / 125.0f);
        const float centered = 1.0f - std::min(1.0f, lateral / halfWidth);
        return radial * 0.75f + centered * 0.25f;
    }

    const Vec3 center = source + direction * 200.0f;
    const float distance = center.Distance2D(targetPosition);
    const float sweetRadius = 180.0f + radius * 0.35f;
    if (distance > sweetRadius) {
        return 0.0f;
    }
    return 1.0f - std::min(1.0f, distance / sweetRadius);
}

inline bool BodyCanHit(QStage stage,
                       const Vec3& source,
                       const Vec3& direction,
                       const Vec3& targetPosition,
                       float targetRadius) {
    Vec3 relative = targetPosition - source;
    relative.y = 0.0f;
    const float forward = relative.Dot(direction);
    const float lateral = std::fabs(Cross2D(direction, relative));
    const float radius = std::clamp(targetRadius, 25.0f, 85.0f);
    if (stage == QStage::First) {
        return forward >= -radius && forward <= 625.0f + radius &&
               lateral <= 90.0f + radius;
    }
    if (stage == QStage::Second) {
        const float widening = std::clamp((forward + 100.0f) / 575.0f, 0.0f, 1.0f);
        const float halfWidth = 150.0f + widening * 100.0f + radius;
        return forward >= -100.0f - radius && forward <= 475.0f + radius &&
               lateral <= halfWidth;
    }
    const Vec3 center = source + direction * 200.0f;
    return center.Distance2D(targetPosition) <= 300.0f + radius;
}

} // namespace Plugins::KuroAIO::AI::Controllers::Aatrox::Geometry
