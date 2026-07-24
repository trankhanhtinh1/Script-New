#pragma once

#include "../../../Core/Vector.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace ZDEvade {

inline constexpr std::size_t kThreatVisualMaxPoints = 128;
inline constexpr int kThreatVisualCircleSegments = 64;
inline constexpr int kThreatVisualCapsuleCapSegments = 32;
// A mild superellipse rounds line caps less than a circle while remaining
// conservative: its greatest radial expansion is ~7.2% at 45 degrees.
inline constexpr float kThreatVisualCapsuleCapExponent = 2.5f;
inline constexpr int kThreatVisualSectorArcSegments = 48;
inline constexpr float kThreatVisualPi = 3.14159265358979323846f;

struct ThreatVisualPath {
    std::array<Vec2, kThreatVisualMaxPoints> points = {};
    std::size_t count = 0;
    bool closed = false;

    bool Empty() const { return count == 0; }
    const Vec2& operator[](std::size_t index) const { return points[index]; }
};

struct ThreatVisualVisibleRun {
    std::size_t first = 0;
    std::size_t count = 0;
    bool wraps = false;
    bool closed = false;
};

struct ThreatVisualVisibleRuns {
    std::array<ThreatVisualVisibleRun, kThreatVisualMaxPoints> runs = {};
    std::size_t count = 0;
    bool allVisible = false;

    bool Empty() const { return count == 0; }
    const ThreatVisualVisibleRun& operator[](std::size_t index) const {
        return runs[index];
    }
};

namespace ThreatVisualGeometry {

inline bool AppendPoint(ThreatVisualPath& path, const Vec2& point) {
    if (!point.IsValid() || path.count >= path.points.size()) return false;
    path.points[path.count++] = point;
    return true;
}

using ThreatVisualCapsuleCapSamples =
    std::array<Vec2, kThreatVisualMaxPoints / 2>;

inline void BuildCapsuleCapSamples(
        ThreatVisualCapsuleCapSamples& samples,
        int capSegments) {
    const float angularStep =
        kThreatVisualPi / static_cast<float>(capSegments);
    // Uniform polar angles make every chord span exactly angularStep. Scaling
    // by sec(angularStep / 2) circumscribes the unit circle, so the rendered
    // edges cannot sag inside the true circular capsule boundary. At the
    // default 32 segments this inflates axial/lateral extents by ~0.121%.
    const float circumscription =
        1.0f / std::cos(angularStep * 0.5f);
    for (int index = 0; index <= capSegments; ++index) {
        const float angle =
            kThreatVisualPi * 0.5f -
            angularStep * static_cast<float>(index);
        const float cosine = std::cos(angle);
        const float sine = std::sin(angle);
        const float superellipseRadius = std::pow(
            std::pow(std::abs(cosine), kThreatVisualCapsuleCapExponent) +
                std::pow(std::abs(sine), kThreatVisualCapsuleCapExponent),
            -1.0f / kThreatVisualCapsuleCapExponent);
        samples[index] = Vec2(
            cosine * superellipseRadius * circumscription,
            sine * superellipseRadius * circumscription);
    }
}

inline const ThreatVisualCapsuleCapSamples& DefaultCapsuleCapSamples() {
    // Function-local static initialization is thread-safe since C++11. The
    // renderer's default path therefore pays the pow/trig cost only once.
    static const ThreatVisualCapsuleCapSamples samples = [] {
        ThreatVisualCapsuleCapSamples result = {};
        BuildCapsuleCapSamples(result, kThreatVisualCapsuleCapSegments);
        return result;
    }();
    return samples;
}

inline bool AppendVisibleRun(ThreatVisualVisibleRuns& result,
                             std::size_t first,
                             std::size_t count,
                             std::size_t pointCount,
                             bool closed) {
    if (count == 0 || result.count >= result.runs.size()) return false;
    result.runs[result.count++] = {
        first,
        count,
        first + count > pointCount,
        closed
    };
    return true;
}

inline ThreatVisualVisibleRuns SegmentVisibleRuns(
        const std::array<bool, kThreatVisualMaxPoints>& visible,
        std::size_t pointCount,
        bool closed) {
    ThreatVisualVisibleRuns result;
    if (pointCount == 0 || pointCount > visible.size()) return result;

    std::size_t visibleCount = 0;
    for (std::size_t index = 0; index < pointCount; ++index) {
        if (visible[index]) ++visibleCount;
    }
    if (visibleCount == 0) return result;

    if (visibleCount == pointCount) {
        result.allVisible = true;
        (void)AppendVisibleRun(result, 0, pointCount, pointCount, closed);
        return result;
    }

    if (!closed) {
        std::size_t index = 0;
        while (index < pointCount) {
            while (index < pointCount && !visible[index]) ++index;
            const std::size_t first = index;
            while (index < pointCount && visible[index]) ++index;
            if (index > first) {
                (void)AppendVisibleRun(
                    result,
                    first,
                    index - first,
                    pointCount,
                    false);
            }
        }
        return result;
    }

    // Begin immediately after an invalid point. This makes a visible run that
    // crosses the last/first boundary explicit without joining either side to
    // any non-adjacent point.
    std::size_t invalidIndex = 0;
    while (invalidIndex < pointCount && visible[invalidIndex]) ++invalidIndex;
    const std::size_t scanStart = (invalidIndex + 1) % pointCount;
    std::size_t offset = 0;
    while (offset < pointCount) {
        while (offset < pointCount &&
               !visible[(scanStart + offset) % pointCount]) {
            ++offset;
        }
        if (offset == pointCount) break;

        const std::size_t first = (scanStart + offset) % pointCount;
        std::size_t runCount = 0;
        while (offset < pointCount &&
               visible[(scanStart + offset) % pointCount]) {
            ++offset;
            ++runCount;
        }
        (void)AppendVisibleRun(
            result,
            first,
            runCount,
            pointCount,
            false);
    }
    return result;
}

inline std::size_t VisibleRunPointIndex(
        const ThreatVisualVisibleRun& run,
        std::size_t offset,
        std::size_t pointCount) {
    return pointCount == 0 ? 0 : (run.first + offset) % pointCount;
}

inline ThreatVisualPath Circle(const Vec2& center,
                               float radius,
                               int segmentCount = kThreatVisualCircleSegments) {
    ThreatVisualPath path;
    if (!center.IsValid() || !std::isfinite(radius) || radius <= 0.0f ||
        segmentCount < 3) {
        return path;
    }

    const int segments = std::min(
        segmentCount,
        static_cast<int>(kThreatVisualMaxPoints));
    const float step = 2.0f * kThreatVisualPi /
        static_cast<float>(segments);
    for (int index = 0; index < segments; ++index) {
        const float angle = step * static_cast<float>(index);
        if (!AppendPoint(path, center + Vec2(
                std::cos(angle) * radius,
                std::sin(angle) * radius))) {
            return {};
        }
    }
    path.closed = true;
    return path;
}

inline ThreatVisualPath Capsule(
        const Vec2& start,
        const Vec2& end,
        float radius,
        int capSegmentCount = kThreatVisualCapsuleCapSegments) {
    ThreatVisualPath path;
    if (!start.IsValid() || !end.IsValid() ||
        !std::isfinite(radius) || radius <= 0.0f) {
        return path;
    }

    const Vec2 delta = end - start;
    if (delta.LengthSqr() <= 0.00000001f) {
        return Circle(start, radius);
    }

    const int capSegments = std::clamp(
        capSegmentCount,
        2,
        static_cast<int>(kThreatVisualMaxPoints / 2) - 1);
    const Vec2 direction = delta.Normalized();
    if (!direction.IsValid() || direction.IsZero()) return path;
    const Vec2 left(-direction.y, direction.x);
    ThreatVisualCapsuleCapSamples customCapSamples = {};
    const ThreatVisualCapsuleCapSamples* capSamples = nullptr;
    if (capSegments == kThreatVisualCapsuleCapSegments) {
        capSamples = &DefaultCapsuleCapSamples();
    } else {
        BuildCapsuleCapSamples(customCapSamples, capSegments);
        capSamples = &customCapSamples;
    }

    // End cap: left side -> forward tip -> right side. The 2.5-exponent
    // superellipse is mildly flatter than a circle; circumscription keeps its
    // rendered chords outside the circular danger boundary.
    for (int index = 0; index <= capSegments; ++index) {
        const Vec2 sample = (*capSamples)[index];
        const Vec2 offset =
            direction * (sample.x * radius) +
            left * (sample.y * radius);
        if (!AppendPoint(path, end + offset)) return {};
    }

    // Start cap: right side -> rear tip -> left side. The closed edge joins
    // the two left-side points and the middle edge joins the right-side pair.
    for (int index = 0; index <= capSegments; ++index) {
        const Vec2 sample = (*capSamples)[capSegments - index];
        const Vec2 offset =
            direction * (-sample.x * radius) +
            left * (sample.y * radius);
        if (!AppendPoint(path, start + offset)) return {};
    }

    path.closed = true;
    return path;
}

inline ThreatVisualPath Sector(
        const Vec2& center,
        const Vec2& direction,
        float radius,
        float fullAngleRadians,
        int arcSegmentCount = kThreatVisualSectorArcSegments,
        float edgePadding = 0.0f) {
    ThreatVisualPath path;
    if (!center.IsValid() || !direction.IsValid() || direction.IsZero() ||
        !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(fullAngleRadians) ||
        fullAngleRadians <= 0.0f ||
        fullAngleRadians > 2.0f * kThreatVisualPi ||
        !std::isfinite(edgePadding) || edgePadding < 0.0f ||
        arcSegmentCount < 2) {
        return path;
    }

    if (edgePadding > 0.0f &&
        fullAngleRadians >= 2.0f * kThreatVisualPi - 0.0001f) {
        return Circle(center, radius + edgePadding, arcSegmentCount);
    }

    const int arcSegments = std::min(
        arcSegmentCount,
        edgePadding > 0.0f
            ? static_cast<int>(kThreatVisualMaxPoints) - 38
            : static_cast<int>(kThreatVisualMaxPoints) - 2);
    const Vec2 forward = direction.Normalized();
    if (!forward.IsValid() || forward.IsZero()) return path;
    const float baseAngle = std::atan2(forward.y, forward.x);
    const float halfAngle = fullAngleRadians * 0.5f;

    if (edgePadding > 0.0f) {
        constexpr int cornerSegments = 8;
        constexpr int originSegments = 16;
        const float leftAngle = baseAngle + halfAngle;
        const float rightAngle = baseAngle - halfAngle;
        const float leftNormalAngle = leftAngle + kThreatVisualPi * 0.5f;
        const float rightNormalAngle = rightAngle - kThreatVisualPi * 0.5f;
        const auto radial = [](float angle, float length) {
            return Vec2(std::cos(angle) * length, std::sin(angle) * length);
        };

        if (!AppendPoint(path, center + radial(
                leftNormalAngle, edgePadding)) ||
            !AppendPoint(path,
                center + radial(leftAngle, radius) +
                    radial(leftNormalAngle, edgePadding))) {
            return {};
        }
        for (int index = 1; index <= cornerSegments; ++index) {
            const float t =
                static_cast<float>(index) /
                static_cast<float>(cornerSegments);
            const float angle =
                leftNormalAngle - kThreatVisualPi * 0.5f * t;
            if (!AppendPoint(
                    path,
                    center + radial(leftAngle, radius) +
                        radial(angle, edgePadding))) {
                return {};
            }
        }
        for (int index = 1; index <= arcSegments; ++index) {
            const float t =
                static_cast<float>(index) /
                static_cast<float>(arcSegments);
            const float angle =
                leftAngle - fullAngleRadians * t;
            if (!AppendPoint(
                    path,
                    center + radial(angle, radius + edgePadding))) {
                return {};
            }
        }
        for (int index = 1; index <= cornerSegments; ++index) {
            const float t =
                static_cast<float>(index) /
                static_cast<float>(cornerSegments);
            const float angle =
                rightAngle - kThreatVisualPi * 0.5f * t;
            if (!AppendPoint(
                    path,
                    center + radial(rightAngle, radius) +
                        radial(angle, edgePadding))) {
                return {};
            }
        }
        if (!AppendPoint(path, center + radial(
                rightNormalAngle, edgePadding))) {
            return {};
        }
        const float originSweep = kThreatVisualPi - fullAngleRadians;
        for (int index = 1; index <= originSegments; ++index) {
            const float t =
                static_cast<float>(index) /
                static_cast<float>(originSegments);
            if (!AppendPoint(
                    path,
                    center + radial(
                        rightNormalAngle - originSweep * t,
                        edgePadding))) {
                return {};
            }
        }
        path.closed = true;
        return path;
    }

    if (!AppendPoint(path, center)) return {};
    for (int index = 0; index <= arcSegments; ++index) {
        const float t = static_cast<float>(index) /
            static_cast<float>(arcSegments);
        const float angle = baseAngle + halfAngle -
            fullAngleRadians * t;
        if (!AppendPoint(path, center + Vec2(
                std::cos(angle) * radius,
                std::sin(angle) * radius))) {
            return {};
        }
    }

    path.closed = true;
    return path;
}

} // namespace ThreatVisualGeometry
} // namespace ZDEvade
