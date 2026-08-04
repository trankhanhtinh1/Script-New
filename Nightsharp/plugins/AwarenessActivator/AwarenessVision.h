#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace NightSharp::Companion::AwarenessVision {

inline constexpr std::size_t kMaxRays = 96;
inline constexpr std::size_t kDefaultRayCount = 96;
inline constexpr float kMinimumDistance = 0.001f;

struct Point final {
    float x = 0.0f;
    float z = 0.0f;

    bool IsValid() const noexcept {
        return std::isfinite(x) && std::isfinite(z);
    }
};

struct CellSample final {
    bool known = true;
    bool wall = false;
    bool brush = false;
};

enum class Blocker : std::uint8_t {
    None = 0,
    Wall,
    Brush,
    Unknown,
};

struct TraceResult final {
    bool visible = true;
    bool terrainAvailable = true;
    Blocker blocker = Blocker::None;
    float distance = 0.0f;
};

struct Coverage final {
    std::array<Point, kMaxRays> boundary{};
    std::array<Blocker, kMaxRays> blockers{};
    std::array<float, kMaxRays> distances{};
    std::size_t count = 0;
    bool terrainAvailable = false;
    bool originInBrush = false;

    bool IsValid() const noexcept {
        return count >= 3 && count <= kMaxRays &&
               terrainAvailable;
    }
};

inline std::size_t ClampRayCount(std::size_t count) noexcept {
    return (std::max)(std::size_t{8},
                      (std::min)(count, kMaxRays));
}

inline Point AddScaled(const Point& origin,
                       const Point& direction,
                       float distance) noexcept {
    return {
        origin.x + direction.x * distance,
        origin.z + direction.z * distance,
    };
}

inline float Distance(const Point& left,
                      const Point& right) noexcept {
    const float dx = left.x - right.x;
    const float dz = left.z - right.z;
    return std::sqrt(dx * dx + dz * dz);
}

template <typename Query>
inline TraceResult TraceRay(const Point& origin,
                            const Point& target,
                            bool originInBrush,
                            float sampleStep,
                            Query&& query) noexcept {
    TraceResult result{};
    result.distance = Distance(origin, target);
    if (!origin.IsValid() || !target.IsValid() ||
        !std::isfinite(sampleStep) || sampleStep <= 0.0f) {
        result.visible = false;
        result.terrainAvailable = false;
        result.blocker = Blocker::Unknown;
        return result;
    }

    const float length = result.distance;
    if (length <= kMinimumDistance) {
        return result;
    }

    const float invLength = 1.0f / length;
    const Point direction{
        (target.x - origin.x) * invLength,
        (target.z - origin.z) * invLength,
    };
    const std::size_t steps = static_cast<std::size_t>(
        std::ceil(length / sampleStep));
    bool insideOriginBrush = originInBrush;
    float distance = 0.0f;

    for (std::size_t step = 1; step <= steps; ++step) {
        distance = (std::min)(length,
                              static_cast<float>(step) * sampleStep);
        const CellSample sample = query(
            AddScaled(origin, direction, distance));
        if (!sample.known) {
            result.terrainAvailable = false;
            result.blocker = Blocker::Unknown;
            result.visible = false;
            return result;
        }
        if (sample.wall) {
            result.visible = false;
            result.blocker = Blocker::Wall;
            // A nav cell is sampled at its center. Stop at the near half of
            // the cell so the rendered polygon never paints through a wall.
            result.distance = (std::max)(0.0f,
                                         distance - sampleStep * 0.5f);
            return result;
        }

        if (originInBrush) {
            if (insideOriginBrush) {
                if (!sample.brush) {
                    insideOriginBrush = false;
                }
            } else if (sample.brush) {
                result.visible = false;
                result.blocker = Blocker::Brush;
                result.distance = (std::max)(
                    0.0f, distance - sampleStep * 0.5f);
                return result;
            }
        } else if (sample.brush) {
            result.visible = false;
            result.blocker = Blocker::Brush;
            result.distance = (std::max)(
                0.0f, distance - sampleStep * 0.5f);
            return result;
        }
    }

    result.distance = length;
    return result;
}

template <typename Query>
inline bool HasLineOfSight(const Point& origin,
                           const Point& target,
                           float sampleStep,
                           Query&& query) noexcept {
    const CellSample source = query(origin);
    if (!source.known) {
        return false;
    }
    const TraceResult result = TraceRay(
        origin, target, source.brush, sampleStep,
        static_cast<Query&&>(query));
    return result.visible && result.terrainAvailable;
}

inline Coverage BuildUnoccludedCoverage(const Point& origin,
                                        float radius,
                                        std::size_t rayCount = kDefaultRayCount) noexcept {
    Coverage result{};
    result.count = ClampRayCount(rayCount);
    result.terrainAvailable = false;
    if (!origin.IsValid() || !std::isfinite(radius) || radius <= 0.0f) {
        result.count = 0;
        return result;
    }

    constexpr float kTwoPi = 6.28318530717958647692f;
    for (std::size_t i = 0; i < result.count; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) /
                            static_cast<float>(result.count);
        const Point direction{ std::cos(angle), std::sin(angle) };
        result.boundary[i] = AddScaled(origin, direction, radius);
        result.blockers[i] = Blocker::Unknown;
        result.distances[i] = radius;
    }
    return result;
}

template <typename Query>
inline Coverage BuildCoverage(const Point& origin,
                              float radius,
                              float sampleStep,
                              Query&& query,
                              std::size_t rayCount = kDefaultRayCount) noexcept {
    Coverage result = BuildUnoccludedCoverage(origin, radius, rayCount);
    if (!origin.IsValid() || !std::isfinite(radius) || radius <= 0.0f ||
        !std::isfinite(sampleStep) || sampleStep <= 0.0f) {
        return result;
    }

    const CellSample source = query(origin);
    if (!source.known) {
        return result;
    }

    result.terrainAvailable = true;
    result.originInBrush = source.brush;
    constexpr float kTwoPi = 6.28318530717958647692f;
    for (std::size_t i = 0; i < result.count; ++i) {
        const float angle = kTwoPi * static_cast<float>(i) /
                            static_cast<float>(result.count);
        const Point direction{ std::cos(angle), std::sin(angle) };
        const Point target = AddScaled(origin, direction, radius);
        const TraceResult trace = TraceRay(
            origin, target, source.brush, sampleStep,
            static_cast<Query&&>(query));
        if (!trace.terrainAvailable) {
            result.terrainAvailable = false;
        }
        result.boundary[i] = AddScaled(origin, direction, trace.distance);
        result.blockers[i] = trace.blocker;
        result.distances[i] = trace.distance;
    }
    return result;
}

} // namespace NightSharp::Companion::AwarenessVision
