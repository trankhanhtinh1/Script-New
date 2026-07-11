#pragma once

#include "Vector2.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <vector>

namespace ZDPrediction::Math {

struct InterceptSolution {
    bool valid = false;
    double time = 0.0;
    Vector2 position = {};
    double residual = std::numeric_limits<double>::infinity();
};

inline double InterceptResidual(const Vector2& source,
                                const Vector2& targetPosition,
                                double projectileSpeed,
                                double launchDelay,
                                double absoluteTime) {
    if (!targetPosition.IsFinite() || absoluteTime < launchDelay) {
        return std::numeric_limits<double>::infinity();
    }
    if (!std::isfinite(projectileSpeed) || projectileSpeed >= 1e12) return 0.0;
    return std::abs(Distance(source, targetPosition) -
                    projectileSpeed * (absoluteTime - launchDelay));
}

inline bool SelectQuadraticRoot(double a,
                                double b,
                                double c,
                                double minimum,
                                double maximum,
                                double& root) {
    if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c)) return false;
    if (std::abs(a) <= Epsilon) {
        if (std::abs(b) <= Epsilon) return false;
        const double candidate = -c / b;
        if (candidate + Epsilon < minimum || candidate - Epsilon > maximum) return false;
        root = std::clamp(candidate, minimum, maximum);
        return true;
    }

    const double discriminant = b * b - 4.0 * a * c;
    if (discriminant < -Epsilon) return false;
    const double squareRoot = std::sqrt(std::max(0.0, discriminant));
    const double q = -0.5 * (b + std::copysign(squareRoot, b));
    const double first = q / a;
    const double second = std::abs(q) > Epsilon ? c / q : (-b - squareRoot) / (2.0 * a);

    bool found = false;
    double best = maximum;
    for (const double candidate : {first, second}) {
        if (!std::isfinite(candidate) || candidate + Epsilon < minimum ||
            candidate - Epsilon > maximum) continue;
        const double bounded = std::clamp(candidate, minimum, maximum);
        if (!found || bounded < best) {
            best = bounded;
            found = true;
        }
    }
    if (found) root = best;
    return found;
}

inline InterceptSolution SolveLinearInterceptInterval(const Vector2& source,
                                                       const Vector2& targetAtTimeZero,
                                                       const Vector2& targetVelocity,
                                                       double projectileSpeed,
                                                       double launchDelay,
                                                       double minimumTime,
                                                       double maximumTime) {
    InterceptSolution result;
    if (!source.IsFinite() || !targetAtTimeZero.IsFinite() ||
        !targetVelocity.IsFinite() || launchDelay < 0.0 ||
        maximumTime < minimumTime) return result;

    const double minimum = std::max(launchDelay, minimumTime);
    if (!std::isfinite(projectileSpeed) || projectileSpeed >= 1e12) {
        result.valid = minimum <= maximumTime;
        result.time = minimum;
        result.position = targetAtTimeZero + targetVelocity * minimum;
        result.residual = 0.0;
        return result;
    }
    if (projectileSpeed <= Epsilon) return result;

    const Vector2 relative = targetAtTimeZero - source;
    const double speedSquared = projectileSpeed * projectileSpeed;
    const double a = targetVelocity.LengthSquared() - speedSquared;
    const double b = 2.0 * (relative.Dot(targetVelocity) + speedSquared * launchDelay);
    const double c = relative.LengthSquared() - speedSquared * launchDelay * launchDelay;
    double time = 0.0;
    if (!SelectQuadraticRoot(a, b, c, minimum, maximumTime, time)) return result;

    result.valid = true;
    result.time = time;
    result.position = targetAtTimeZero + targetVelocity * time;
    result.residual = InterceptResidual(
        source, result.position, projectileSpeed, launchDelay, time);
    return result;
}

inline InterceptSolution SolveLinearIntercept(const Vector2& source,
                                               const Vector2& targetPosition,
                                               const Vector2& targetVelocity,
                                               double projectileSpeed,
                                               double launchDelay,
                                               double maximumTime = 8.0) {
    return SolveLinearInterceptInterval(source,
                                        targetPosition,
                                        targetVelocity,
                                        projectileSpeed,
                                        launchDelay,
                                        launchDelay,
                                        maximumTime);
}

inline Vector2 PositionOnPath(const std::vector<Vector2>& path,
                              double moveSpeed,
                              double time) {
    if (path.empty()) return {};
    if (path.size() == 1 || moveSpeed <= Epsilon || time <= 0.0) return path.front();

    double remaining = moveSpeed * time;
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        const Vector2& start = path[index];
        const Vector2& end = path[index + 1];
        const double length = Distance(start, end);
        if (length <= Epsilon) continue;
        if (remaining <= length) return start + (end - start) * (remaining / length);
        remaining -= length;
    }
    return path.back();
}

inline double PathLength(const std::vector<Vector2>& path) {
    double total = 0.0;
    for (std::size_t index = 1; index < path.size(); ++index) {
        total += Distance(path[index - 1], path[index]);
    }
    return total;
}

inline InterceptSolution SolvePathIntercept(const Vector2& source,
                                             const std::vector<Vector2>& path,
                                             double targetSpeed,
                                             double projectileSpeed,
                                             double launchDelay,
                                             double maximumTime = 8.0) {
    InterceptSolution result;
    if (path.empty() || targetSpeed < 0.0 || launchDelay < 0.0) return result;
    if (path.size() == 1 || targetSpeed <= Epsilon) {
        return SolveLinearIntercept(source,
                                    path.front(),
                                    {},
                                    projectileSpeed,
                                    launchDelay,
                                    maximumTime);
    }
    if (!std::isfinite(projectileSpeed) || projectileSpeed >= 1e12) {
        result.valid = launchDelay <= maximumTime;
        result.time = launchDelay;
        result.position = PositionOnPath(path, targetSpeed, launchDelay);
        result.residual = 0.0;
        return result;
    }

    double segmentStartTime = 0.0;
    for (std::size_t index = 0; index + 1 < path.size(); ++index) {
        const Vector2& start = path[index];
        const Vector2& end = path[index + 1];
        const Vector2 segment = end - start;
        const double length = segment.Length();
        if (length <= Epsilon) continue;
        const double duration = length / std::max(targetSpeed, Epsilon);
        const double segmentEndTime = segmentStartTime + duration;
        const Vector2 velocity = segment * (targetSpeed / length);
        const Vector2 targetAtTimeZero = start - velocity * segmentStartTime;
        result = SolveLinearInterceptInterval(source,
                                              targetAtTimeZero,
                                              velocity,
                                              projectileSpeed,
                                              launchDelay,
                                              segmentStartTime,
                                              std::min(segmentEndTime, maximumTime));
        if (result.valid) return result;
        segmentStartTime = segmentEndTime;
        if (segmentStartTime > maximumTime) return {};
    }

    result = SolveLinearIntercept(source,
                                  path.back(),
                                  {},
                                  projectileSpeed,
                                  launchDelay,
                                  maximumTime);
    if (result.valid && result.time + Epsilon < segmentStartTime) return {};
    return result;
}

inline Vector2 PositionWithTurn(const Vector2& position,
                                const Vector2& velocity,
                                double angularVelocity,
                                double time) {
    if (std::abs(angularVelocity) <= 1e-5 || time <= 0.0) {
        return position + velocity * std::max(0.0, time);
    }
    const double angle = angularVelocity * time;
    const double sine = std::sin(angle);
    const double cosine = std::cos(angle);
    return position + Vector2{
        (velocity.x * sine + velocity.y * (cosine - 1.0)) / angularVelocity,
        (velocity.x * (1.0 - cosine) + velocity.y * sine) / angularVelocity
    };
}

inline InterceptSolution SolveTurnIntercept(const Vector2& source,
                                             const Vector2& targetPosition,
                                             const Vector2& targetVelocity,
                                             double angularVelocity,
                                             double projectileSpeed,
                                             double launchDelay,
                                             double maximumTime = 8.0) {
    InterceptSolution result;
    if (!source.IsFinite() || !targetPosition.IsFinite() ||
        !targetVelocity.IsFinite() || launchDelay < 0.0 ||
        maximumTime < launchDelay) return result;
    if (!std::isfinite(projectileSpeed) || projectileSpeed >= 1e12) {
        result.valid = true;
        result.time = launchDelay;
        result.position = PositionWithTurn(
            targetPosition, targetVelocity, angularVelocity, launchDelay);
        result.residual = 0.0;
        return result;
    }
    if (projectileSpeed <= Epsilon) return result;

    const auto function = [&](double time) {
        const Vector2 position = PositionWithTurn(
            targetPosition, targetVelocity, angularVelocity, time);
        return Distance(source, position) - projectileSpeed * (time - launchDelay);
    };

    constexpr int scanSteps = 256;
    double previousTime = launchDelay;
    double previousValue = function(previousTime);
    for (int step = 1; step <= scanSteps; ++step) {
        const double currentTime = launchDelay +
            (maximumTime - launchDelay) * static_cast<double>(step) /
                static_cast<double>(scanSteps);
        const double currentValue = function(currentTime);
        if (currentValue <= 0.0 || previousValue * currentValue <= 0.0) {
            double left = previousTime;
            double right = currentTime;
            for (int iteration = 0; iteration < 48; ++iteration) {
                const double middle = (left + right) * 0.5;
                if (function(middle) > 0.0) left = middle;
                else right = middle;
            }
            result.valid = true;
            result.time = (left + right) * 0.5;
            result.position = PositionWithTurn(
                targetPosition, targetVelocity, angularVelocity, result.time);
            result.residual = InterceptResidual(
                source, result.position, projectileSpeed, launchDelay, result.time);
            return result;
        }
        previousTime = currentTime;
        previousValue = currentValue;
    }
    return result;
}

inline InterceptSolution SolveAcceleratedIntercept(const Vector2& source,
                                                    const Vector2& targetPosition,
                                                    const Vector2& targetVelocity,
                                                    const Vector2& targetAcceleration,
                                                    double projectileSpeed,
                                                    double launchDelay,
                                                    double maximumTime = 8.0) {
    if (!std::isfinite(projectileSpeed) || projectileSpeed >= 1e12) {
        const double time = std::clamp(launchDelay, 0.0, maximumTime);
        const Vector2 position = targetPosition + targetVelocity * time +
            targetAcceleration * (0.5 * time * time);
        return {true, time, position, 0.0};
    }

    InterceptSolution linear = SolveLinearIntercept(source,
                                                    targetPosition,
                                                    targetVelocity,
                                                    projectileSpeed,
                                                    launchDelay,
                                                    maximumTime);
    double time = linear.valid ? linear.time : launchDelay + Distance(source, targetPosition) /
        std::max(projectileSpeed, Epsilon);
    time = std::clamp(time, launchDelay, maximumTime);

    for (int iteration = 0; iteration < 12; ++iteration) {
        const Vector2 position = targetPosition + targetVelocity * time +
            targetAcceleration * (0.5 * time * time);
        const Vector2 velocity = targetVelocity + targetAcceleration * time;
        const Vector2 relative = position - source;
        const double distance = relative.Length();
        if (distance <= Epsilon) break;
        const double function = distance - projectileSpeed * (time - launchDelay);
        const double derivative = relative.Dot(velocity) / distance - projectileSpeed;
        if (std::abs(derivative) <= Epsilon) break;
        const double next = std::clamp(time - function / derivative, launchDelay, maximumTime);
        if (std::abs(next - time) <= 1e-6) {
            time = next;
            break;
        }
        time = next;
    }

    const Vector2 position = targetPosition + targetVelocity * time +
        targetAcceleration * (0.5 * time * time);
    const double residual = InterceptResidual(
        source, position, projectileSpeed, launchDelay, time);
    const bool valid = time >= launchDelay && time <= maximumTime &&
        std::isfinite(residual) && residual <= std::max(1.0, projectileSpeed * 0.002);
    return {valid, time, position, residual};
}

}
