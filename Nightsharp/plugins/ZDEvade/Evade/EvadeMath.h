#pragma once

#include "../../../Core/Vector.h"

#include <algorithm>
#include <cmath>
#include <cfloat>
#include <vector>

namespace ZDEvade {

class EvadeGeometryMath {
public:
    static float DistanceToSegment(const Vec2& point,
                                   const Vec2& start,
                                   const Vec2& end,
                                   bool* onSegment = nullptr,
                                   Vec2* projectionOut = nullptr) {
        const Vec2 segment = end - start;
        const double lengthSqr = static_cast<double>(segment.x) * segment.x +
            static_cast<double>(segment.y) * segment.y;
        if (!std::isfinite(lengthSqr) || lengthSqr <= 1.0e-12) {
            if (onSegment) *onSegment = false;
            if (projectionOut) *projectionOut = start;
            const float distance = point.Distance(start);
            return std::isfinite(distance) ? distance : FLT_MAX;
        }
        const Vec2 offset = point - start;
        const double raw = (static_cast<double>(offset.x) * segment.x +
            static_cast<double>(offset.y) * segment.y) / lengthSqr;
        if (!std::isfinite(raw)) {
            if (onSegment) *onSegment = false;
            if (projectionOut) *projectionOut = start;
            return FLT_MAX;
        }
        const float t = static_cast<float>(std::clamp(raw, 0.0, 1.0));
        const Vec2 projection = start + segment * t;
        if (onSegment) *onSegment = raw >= 0.0 && raw <= 1.0;
        if (projectionOut) *projectionOut = projection;
        const float distance = point.Distance(projection);
        return std::isfinite(distance) ? distance : FLT_MAX;
    }

    static Vec2 Rotate(const Vec2& value, float radians) {
        if (!std::isfinite(value.x) || !std::isfinite(value.y) || !std::isfinite(radians)) return {};
        const float c = std::cos(radians);
        const float s = std::sin(radians);
        return Vec2(value.x * c - value.y * s, value.x * s + value.y * c);
    }

    static float SignedAngle(const Vec2& from, const Vec2& to) {
        if (!std::isfinite(from.x) || !std::isfinite(from.y) ||
            !std::isfinite(to.x) || !std::isfinite(to.y)) return 0.0f;
        const Vec2 left = from.Normalized();
        const Vec2 right = to.Normalized();
        if (left.IsZero() || right.IsZero()) return 0.0f;
        return std::atan2(left.Cross(right), left.Dot(right));
    }

    static float ClosestApproachTime(const Vec2& firstPosition,
                                     const Vec2& firstVelocity,
                                     const Vec2& secondPosition,
                                     const Vec2& secondVelocity,
                                     float durationSeconds) {
        if (!std::isfinite(durationSeconds) || durationSeconds <= 0.0f) return 0.0f;
        const Vec2 relativePosition = firstPosition - secondPosition;
        const Vec2 relativeVelocity = firstVelocity - secondVelocity;
        const double speedSqr = static_cast<double>(relativeVelocity.x) * relativeVelocity.x +
            static_cast<double>(relativeVelocity.y) * relativeVelocity.y;
        if (!std::isfinite(speedSqr) || speedSqr <= 1.0e-12) return 0.0f;
        const double dot = static_cast<double>(relativePosition.x) * relativeVelocity.x +
            static_cast<double>(relativePosition.y) * relativeVelocity.y;
        if (!std::isfinite(dot)) return 0.0f;
        return static_cast<float>(std::clamp(
            -dot / speedSqr,
            0.0,
            static_cast<double>(durationSeconds)));
    }

    static float ClosestApproachDistance(const Vec2& firstPosition,
                                         const Vec2& firstVelocity,
                                         const Vec2& secondPosition,
                                         const Vec2& secondVelocity,
                                         float durationSeconds) {
        const float time = ClosestApproachTime(
            firstPosition,
            firstVelocity,
            secondPosition,
            secondVelocity,
            durationSeconds);
        const float distance = (firstPosition + firstVelocity * time).Distance(
            secondPosition + secondVelocity * time);
        return std::isfinite(distance) ? distance : FLT_MAX;
    }

    static float FirstContactTime(const Vec2& firstPosition,
                                  const Vec2& firstVelocity,
                                  const Vec2& secondPosition,
                                  const Vec2& secondVelocity,
                                  float radius,
                                  float durationSeconds) {
        if (!std::isfinite(durationSeconds) || durationSeconds < 0.0f ||
            !std::isfinite(radius)) return FLT_MAX;
        const double safeRadius = std::max(0.0, static_cast<double>(radius));
        const Vec2 position = firstPosition - secondPosition;
        const Vec2 velocity = firstVelocity - secondVelocity;
        const double c = static_cast<double>(position.x) * position.x +
            static_cast<double>(position.y) * position.y - safeRadius * safeRadius;
        if (!std::isfinite(c)) return FLT_MAX;
        const double contactTolerance = 1.0e-9 * std::max(1.0, safeRadius * safeRadius);
        if (c <= contactTolerance) return 0.0f;
        const double a = static_cast<double>(velocity.x) * velocity.x +
            static_cast<double>(velocity.y) * velocity.y;
        if (!std::isfinite(a) || a <= 1.0e-12) return FLT_MAX;
        const double halfB = static_cast<double>(position.x) * velocity.x +
            static_cast<double>(position.y) * velocity.y;
        if (!std::isfinite(halfB) || halfB >= 0.0) return FLT_MAX;
        double discriminant = halfB * halfB - a * c;
        const double discriminantTolerance = 1.0e-12 *
            std::max(1.0, halfB * halfB + std::fabs(a * c));
        if (discriminant < -discriminantTolerance) return FLT_MAX;
        discriminant = std::max(0.0, discriminant);
        const double squareRoot = std::sqrt(discriminant);
        const double denominator = -halfB + squareRoot;
        const double root = denominator > 1.0e-15
            ? c / denominator
            : (-halfB - squareRoot) / a;
        const double duration = std::max(0.0, static_cast<double>(durationSeconds));
        const double timeTolerance = 1.0e-7 * std::max(1.0, duration);
        if (root < -timeTolerance || root > duration + timeTolerance) return FLT_MAX;
        return static_cast<float>(std::clamp(root, 0.0, duration));
    }

    static bool LineIntersection(const Vec2& firstOrigin,
                                 const Vec2& firstDirection,
                                 const Vec2& secondOrigin,
                                 const Vec2& secondDirection,
                                 Vec2& intersection,
                                 float* firstParameter = nullptr,
                                 float* secondParameter = nullptr) {
        intersection = {};
        if (firstParameter) *firstParameter = 0.0f;
        if (secondParameter) *secondParameter = 0.0f;
        const double firstLengthSqr = static_cast<double>(firstDirection.x) * firstDirection.x +
            static_cast<double>(firstDirection.y) * firstDirection.y;
        const double secondLengthSqr = static_cast<double>(secondDirection.x) * secondDirection.x +
            static_cast<double>(secondDirection.y) * secondDirection.y;
        if (!std::isfinite(firstLengthSqr) || !std::isfinite(secondLengthSqr) ||
            firstLengthSqr <= 1.0e-12 || secondLengthSqr <= 1.0e-12) return false;
        const double denominator = static_cast<double>(firstDirection.x) * secondDirection.y -
            static_cast<double>(firstDirection.y) * secondDirection.x;
        const double tolerance = 1.0e-7 * std::sqrt(firstLengthSqr * secondLengthSqr);
        if (!std::isfinite(denominator) || std::fabs(denominator) <= tolerance) return false;
        const Vec2 delta = secondOrigin - firstOrigin;
        const double first = (static_cast<double>(delta.x) * secondDirection.y -
            static_cast<double>(delta.y) * secondDirection.x) / denominator;
        const double second = (static_cast<double>(delta.x) * firstDirection.y -
            static_cast<double>(delta.y) * firstDirection.x) / denominator;
        if (!std::isfinite(first) || !std::isfinite(second)) return false;
        intersection = firstOrigin + firstDirection * static_cast<float>(first);
        if (firstParameter) *firstParameter = static_cast<float>(first);
        if (secondParameter) *secondParameter = static_cast<float>(second);
        return intersection.IsValid();
    }

    static int CircleIntersections(const Vec2& firstCenter,
                                   float firstRadius,
                                   const Vec2& secondCenter,
                                   float secondRadius,
                                   Vec2& firstIntersection,
                                   Vec2& secondIntersection) {
        firstIntersection = {};
        secondIntersection = {};
        if (!std::isfinite(firstRadius) || !std::isfinite(secondRadius) ||
            firstRadius < 0.0f || secondRadius < 0.0f) return 0;
        const Vec2 delta = secondCenter - firstCenter;
        const double distanceSqr = static_cast<double>(delta.x) * delta.x +
            static_cast<double>(delta.y) * delta.y;
        if (!std::isfinite(distanceSqr) || distanceSqr <= 1.0e-12) return 0;
        const double distance = std::sqrt(distanceSqr);
        const double first = firstRadius;
        const double second = secondRadius;
        const double sum = first + second;
        const double difference = std::fabs(first - second);
        const double tolerance = 1.0e-6 * std::max({1.0, distance, sum});
        if (distance > sum + tolerance || distance < difference - tolerance) return 0;
        const double along = (first * first - second * second + distanceSqr) /
            (2.0 * distance);
        double heightSqr = first * first - along * along;
        const double heightTolerance = tolerance * tolerance;
        if (heightSqr < -heightTolerance) return 0;
        heightSqr = std::max(0.0, heightSqr);
        const Vec2 direction = delta * static_cast<float>(1.0 / distance);
        const Vec2 base = firstCenter + direction * static_cast<float>(along);
        const Vec2 perpendicular(-direction.y, direction.x);
        const float height = static_cast<float>(std::sqrt(heightSqr));
        firstIntersection = base + perpendicular * height;
        secondIntersection = base - perpendicular * height;
        return heightSqr <= heightTolerance ? 1 : 2;
    }

    static int SegmentCircleIntersections(const Vec2& start,
                                          const Vec2& end,
                                          const Vec2& center,
                                          float radius,
                                          Vec2& firstIntersection,
                                          Vec2& secondIntersection) {
        firstIntersection = {};
        secondIntersection = {};
        if (!std::isfinite(radius) || radius < 0.0f) return 0;
        const Vec2 segment = end - start;
        const Vec2 offset = start - center;
        const double a = static_cast<double>(segment.x) * segment.x +
            static_cast<double>(segment.y) * segment.y;
        const double safeRadius = radius;
        const double c = static_cast<double>(offset.x) * offset.x +
            static_cast<double>(offset.y) * offset.y - safeRadius * safeRadius;
        if (!std::isfinite(c)) return 0;
        const double tolerance = 1.0e-8 * std::max(1.0, safeRadius * safeRadius);
        if (!std::isfinite(a) || a <= 1.0e-12) {
            if (std::fabs(c) <= tolerance) {
                firstIntersection = start;
                return 1;
            }
            return 0;
        }
        const double halfB = static_cast<double>(offset.x) * segment.x +
            static_cast<double>(offset.y) * segment.y;
        if (!std::isfinite(halfB)) return 0;
        double discriminant = halfB * halfB - a * c;
        const double discriminantTolerance = 1.0e-12 *
            std::max(1.0, halfB * halfB + std::fabs(a * c));
        if (discriminant < -discriminantTolerance) return 0;
        discriminant = std::max(0.0, discriminant);
        const double root = std::sqrt(discriminant);
        const double parameters[2] = {
            (-halfB - root) / a,
            (-halfB + root) / a,
        };
        int count = 0;
        for (double parameter : parameters) {
            const double parameterTolerance = 1.0e-7;
            if (parameter < -parameterTolerance || parameter > 1.0 + parameterTolerance) continue;
            const float clipped = static_cast<float>(std::clamp(parameter, 0.0, 1.0));
            const Vec2 point = start + segment * clipped;
            if (count > 0 && point.DistanceSqr(firstIntersection) <= 1.0e-8f) continue;
            if (count == 0) firstIntersection = point;
            else secondIntersection = point;
            ++count;
        }
        return count;
    }

    static float PolylineLength(const std::vector<Vec2>& points) {
        double result = 0.0;
        for (std::size_t index = 1; index < points.size(); ++index) {
            const float distance = points[index - 1].Distance(points[index]);
            if (!std::isfinite(distance)) return FLT_MAX;
            result += distance;
            if (result >= FLT_MAX) return FLT_MAX;
        }
        return static_cast<float>(result);
    }

    static Vec2 PositionAlongPolyline(const std::vector<Vec2>& points,
                                      float distance) {
        if (points.empty()) return {};
        if (points.size() == 1 || distance <= 0.0f) return points.front();
        if (!std::isfinite(distance)) return points.back();
        float remaining = distance;
        for (std::size_t index = 1; index < points.size(); ++index) {
            const Vec2 delta = points[index] - points[index - 1];
            const float length = delta.Length();
            if (length <= 0.0001f) continue;
            if (remaining <= length)
                return points[index - 1] + delta * (remaining / length);
            remaining -= length;
        }
        return points.back();
    }

    static float FirstSegmentIntersectionParameter(const Vec2& pathStart,
                                                   const Vec2& pathEnd,
                                                   const Vec2& segmentStart,
                                                   const Vec2& segmentEnd) {
        const Vec2 path = pathEnd - pathStart;
        const Vec2 segment = segmentEnd - segmentStart;
        const Vec2 offset = segmentStart - pathStart;
        const double pathLengthSqr = static_cast<double>(path.x) * path.x +
            static_cast<double>(path.y) * path.y;
        const double segmentLengthSqr = static_cast<double>(segment.x) * segment.x +
            static_cast<double>(segment.y) * segment.y;
        const double epsilon = 1.0e-8;
        if (!std::isfinite(pathLengthSqr) || !std::isfinite(segmentLengthSqr)) return FLT_MAX;
        if (pathLengthSqr <= 1.0e-12)
            return DistanceToSegment(pathStart, segmentStart, segmentEnd) <= 1.0e-5f
                ? 0.0f
                : FLT_MAX;
        if (segmentLengthSqr <= 1.0e-12) {
            const double parameter = (static_cast<double>(offset.x) * path.x +
                static_cast<double>(offset.y) * path.y) / pathLengthSqr;
            if (parameter < -epsilon || parameter > 1.0 + epsilon) return FLT_MAX;
            const Vec2 point = pathStart + path * static_cast<float>(std::clamp(parameter, 0.0, 1.0));
            return point.Distance(segmentStart) <= 1.0e-5f
                ? static_cast<float>(std::clamp(parameter, 0.0, 1.0))
                : FLT_MAX;
        }
        const double denominator = static_cast<double>(path.x) * segment.y -
            static_cast<double>(path.y) * segment.x;
        const double scale = std::sqrt(pathLengthSqr * segmentLengthSqr);
        if (std::fabs(denominator) > 1.0e-7 * scale) {
            const double first = (static_cast<double>(offset.x) * segment.y -
                static_cast<double>(offset.y) * segment.x) / denominator;
            const double second = (static_cast<double>(offset.x) * path.y -
                static_cast<double>(offset.y) * path.x) / denominator;
            if (first < -epsilon || first > 1.0 + epsilon ||
                second < -epsilon || second > 1.0 + epsilon) return FLT_MAX;
            return static_cast<float>(std::clamp(first, 0.0, 1.0));
        }
        const double collinear = static_cast<double>(offset.x) * path.y -
            static_cast<double>(offset.y) * path.x;
        if (std::fabs(collinear) > 1.0e-7 * std::sqrt(pathLengthSqr *
            std::max(1.0e-12, static_cast<double>(offset.x) * offset.x +
                static_cast<double>(offset.y) * offset.y))) return FLT_MAX;
        const double first = (static_cast<double>(offset.x) * path.x +
            static_cast<double>(offset.y) * path.y) / pathLengthSqr;
        const Vec2 endOffset = segmentEnd - pathStart;
        const double second = (static_cast<double>(endOffset.x) * path.x +
            static_cast<double>(endOffset.y) * path.y) / pathLengthSqr;
        const double entry = std::max(0.0, std::min(first, second));
        const double exit = std::min(1.0, std::max(first, second));
        return entry <= exit + epsilon ? static_cast<float>(std::clamp(entry, 0.0, 1.0)) : FLT_MAX;
    }

    static float SegmentSegmentDistance(const Vec2& firstStart,
                                        const Vec2& firstEnd,
                                        const Vec2& secondStart,
                                        const Vec2& secondEnd) {
        if (FirstSegmentIntersectionParameter(
                firstStart, firstEnd, secondStart, secondEnd) != FLT_MAX) return 0.0f;
        return std::min({
            DistanceToSegment(firstStart, secondStart, secondEnd),
            DistanceToSegment(firstEnd, secondStart, secondEnd),
            DistanceToSegment(secondStart, firstStart, firstEnd),
            DistanceToSegment(secondEnd, firstStart, firstEnd),
        });
    }

    static float SignedDistanceToSector(const Vec2& point,
                                        const Vec2& origin,
                                        const Vec2& direction,
                                        float range,
                                        float halfAngle) {
        if (!point.IsValid() || !origin.IsValid() || !direction.IsValid() ||
            !std::isfinite(range) || !std::isfinite(halfAngle)) return FLT_MAX;
        const Vec2 axis = direction.Normalized();
        const float safeRange = std::max(0.0f, range);
        const float safeAngle = std::clamp(
            halfAngle,
            0.0f,
            3.14159265358979323846f);
        if (axis.IsZero() || safeRange <= 0.0f) return point.Distance(origin);
        const Vec2 relative = point - origin;
        const float radial = relative.Length();
        if (safeAngle >= 3.14159265358979323846f - 1.0e-5f)
            return radial - safeRange;
        const float angle = radial <= 1.0e-7f
            ? 0.0f
            : std::fabs(SignedAngle(axis, relative));
        const Vec2 leftEnd = origin + Rotate(axis, safeAngle) * safeRange;
        const Vec2 rightEnd = origin + Rotate(axis, -safeAngle) * safeRange;
        const float leftDistance = DistanceToSegment(point, origin, leftEnd);
        const float rightDistance = DistanceToSegment(point, origin, rightEnd);
        const bool inside = radial <= safeRange && angle <= safeAngle;
        if (inside)
            return -std::max(0.0f, std::min({
                leftDistance,
                rightDistance,
                safeRange - radial,
            }));
        float distance = std::min(leftDistance, rightDistance);
        if (angle <= safeAngle) distance = std::min(distance, std::fabs(radial - safeRange));
        return distance;
    }

    static float FirstContactMovingPointCircle(const Vec2& start,
                                               const Vec2& end,
                                               const Vec2& center,
                                               float radius) {
        if (!start.IsValid() || !end.IsValid() || !center.IsValid() ||
            !std::isfinite(radius)) return FLT_MAX;
        return FirstContactTime(
            start,
            end - start,
            center,
            Vec2(),
            std::max(0.0f, radius),
            1.0f);
    }

    template <typename SignedDistance>
    static float FirstContactMovingPointBySignedDistance(
        const Vec2& start,
        const Vec2& end,
        SignedDistance&& signedDistance,
        float tolerance = 0.05f,
        int maxDepth = 22) {
        const Vec2 delta = end - start;
        const float length = delta.Length();
        const float safeTolerance = std::max(0.0001f, tolerance);
        float parameter = 0.0f;
        float distance = signedDistance(start);
        if (!std::isfinite(distance)) return FLT_MAX;
        if (distance <= safeTolerance) return 0.0f;
        if (!std::isfinite(length) || length <= 1.0e-7f) return FLT_MAX;
        const int iterations = std::clamp(maxDepth, 1, 64) * 8;
        for (int index = 0; index < iterations; ++index) {
            const float step = distance / length;
            if (!std::isfinite(step) || step <= 0.0f) return parameter;
            const float next = parameter + step * 0.9f;
            if (next >= 1.0f) {
                const float endDistance = signedDistance(end);
                return std::isfinite(endDistance) && endDistance <= safeTolerance
                    ? 1.0f
                    : FLT_MAX;
            }
            parameter = next;
            distance = signedDistance(start + delta * parameter);
            if (!std::isfinite(distance)) return FLT_MAX;
            if (distance <= safeTolerance) return parameter;
        }
        return distance <= safeTolerance ? parameter : FLT_MAX;
    }

    template <typename SignedDistance>
    static float MinimumSignedDistanceAlongSegment(
        const Vec2& start,
        const Vec2& end,
        SignedDistance&& signedDistance,
        int samples = 32) {
        const int count = std::clamp(samples, 2, 256);
        float result = FLT_MAX;
        for (int index = 0; index <= count; ++index) {
            const float parameter = static_cast<float>(index) / static_cast<float>(count);
            const float distance = signedDistance(start + (end - start) * parameter);
            if (std::isfinite(distance)) result = std::min(result, distance);
        }
        if (result == FLT_MAX) return result;
        const float length = start.Distance(end);
        return std::isfinite(length)
            ? result - length / (2.0f * static_cast<float>(count))
            : -FLT_MAX;
    }

    static float FirstContactMovingPointCapsule(const Vec2& start,
                                                const Vec2& end,
                                                const Vec2& capsuleStart,
                                                const Vec2& capsuleEnd,
                                                float radius,
                                                float tolerance = 0.05f) {
        if (!start.IsValid() || !end.IsValid() || !capsuleStart.IsValid() ||
            !capsuleEnd.IsValid() || !std::isfinite(radius)) return FLT_MAX;
        const float safeRadius = std::max(0.0f, radius);
        if (DistanceToSegment(start, capsuleStart, capsuleEnd) <= safeRadius) return 0.0f;
        if (safeRadius <= 1.0e-7f)
            return FirstSegmentIntersectionParameter(start, end, capsuleStart, capsuleEnd);
        const Vec2 axis = capsuleEnd - capsuleStart;
        const float axisLength = axis.Length();
        if (axisLength <= 1.0e-7f)
            return FirstContactMovingPointCircle(start, end, capsuleStart, safeRadius);
        float result = std::min(
            FirstContactMovingPointCircle(start, end, capsuleStart, safeRadius),
            FirstContactMovingPointCircle(start, end, capsuleEnd, safeRadius));
        const Vec2 unit = axis * (1.0f / axisLength);
        const Vec2 path = end - start;
        const float lateralStart = (start - capsuleStart).Cross(unit);
        const float lateralRate = path.Cross(unit);
        const float epsilon = std::max(1.0e-6f, tolerance * 0.001f);
        if (std::fabs(lateralRate) > epsilon) {
            for (int side = -1; side <= 1; side += 2) {
                const float parameter =
                    (safeRadius * static_cast<float>(side) - lateralStart) / lateralRate;
                if (parameter < -epsilon || parameter > 1.0f + epsilon) continue;
                const float clipped = std::clamp(parameter, 0.0f, 1.0f);
                const Vec2 point = start + path * clipped;
                const float along = (point - capsuleStart).Dot(unit);
                if (along >= -epsilon && along <= axisLength + epsilon)
                    result = std::min(result, clipped);
            }
        }
        return result;
    }

    static float FirstContactMovingPointSector(const Vec2& start,
                                               const Vec2& end,
                                               const Vec2& origin,
                                               const Vec2& direction,
                                               float range,
                                               float halfAngle,
                                               float expansion,
                                               float tolerance = 0.05f) {
        if (!start.IsValid() || !end.IsValid() || !origin.IsValid() ||
            !direction.IsValid() || !std::isfinite(range) ||
            !std::isfinite(halfAngle) || !std::isfinite(expansion)) return FLT_MAX;
        const Vec2 axis = direction.Normalized();
        const float safeRange = std::max(0.0f, range);
        const float safeAngle = std::clamp(
            halfAngle,
            0.0f,
            3.14159265358979323846f);
        const float safeExpansion = std::max(0.0f, expansion);
        if (axis.IsZero() || safeRange <= 0.0f)
            return FirstContactMovingPointCircle(start, end, origin, safeExpansion);
        if (SignedDistanceToSector(start, origin, axis, safeRange, safeAngle) <= safeExpansion)
            return 0.0f;
        if (safeAngle >= 3.14159265358979323846f - 1.0e-5f)
            return FirstContactMovingPointCircle(
                start, end, origin, safeRange + safeExpansion);

        const Vec2 leftEnd = origin + Rotate(axis, safeAngle) * safeRange;
        const Vec2 rightEnd = origin + Rotate(axis, -safeAngle) * safeRange;
        float result = std::min(
            FirstContactMovingPointCapsule(
                start, end, origin, leftEnd, safeExpansion, tolerance),
            FirstContactMovingPointCapsule(
                start, end, origin, rightEnd, safeExpansion, tolerance));
        const float radialContact = FirstContactMovingPointCircle(
            start,
            end,
            origin,
            safeRange + safeExpansion);
        if (radialContact != FLT_MAX) {
            const Vec2 point = start + (end - start) * radialContact;
            const Vec2 relative = point - origin;
            const float angle = relative.LengthSqr() <= 1.0e-12f
                ? 0.0f
                : std::fabs(SignedAngle(axis, relative));
            if (angle <= safeAngle + 1.0e-5f)
                result = std::min(result, radialContact);
        }
        return result;
    }
};

}
