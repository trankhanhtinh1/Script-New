#pragma once

#include "../Math/Vector2.h"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

namespace ZDPrediction {

struct AoePoint {
    int id = 0;
    Math::Vector2 position = {};
};

struct AoeSolution {
    bool valid = false;
    Math::Vector2 castPosition = {};
    std::vector<int> hitIds;
};

class AoeOptimizer {
public:
    static AoeSolution Circle(const Math::Vector2& source,
                              const std::vector<AoePoint>& points,
                              double radius,
                              double range) {
        std::vector<Math::Vector2> candidates;
        candidates.reserve(points.size() * points.size());
        for (const auto& point : points) candidates.push_back(point.position);
        for (std::size_t left = 0; left < points.size(); ++left) {
            for (std::size_t right = left + 1; right < points.size(); ++right) {
                const Math::Vector2 delta = points[right].position - points[left].position;
                const double distance = delta.Length();
                if (distance > 2.0 * radius || distance <= Math::Epsilon) continue;
                const Math::Vector2 middle = (points[left].position + points[right].position) * 0.5;
                const double height = std::sqrt(std::max(0.0, radius * radius -
                    distance * distance * 0.25));
                const Math::Vector2 perpendicular{-delta.y / distance, delta.x / distance};
                candidates.push_back(middle + perpendicular * height);
                candidates.push_back(middle - perpendicular * height);
            }
        }
        return SelectBest(source, points, candidates, range, [radius](const auto& point, const auto& cast) {
            return Math::DistanceSquared(point, cast) <= radius * radius;
        });
    }

    static AoeSolution Line(const Math::Vector2& source,
                            const std::vector<AoePoint>& points,
                            double radius,
                            double range) {
        std::vector<Math::Vector2> candidates;
        for (const auto& point : points) candidates.push_back(ClampToRange(source, point.position, range));
        for (std::size_t left = 0; left < points.size(); ++left) {
            for (std::size_t right = left + 1; right < points.size(); ++right) {
                candidates.push_back(ClampToRange(
                    source, (points[left].position + points[right].position) * 0.5, range));
            }
        }
        return SelectBest(source, points, candidates, range, [source, radius](const auto& point, const auto& cast) {
            return Math::DistanceSquaredToSegment(point, source, cast) <= radius * radius;
        });
    }

    static AoeSolution Cone(const Math::Vector2& source,
                            const std::vector<AoePoint>& points,
                            double angleRadians,
                            double range) {
        std::vector<Math::Vector2> candidates;
        std::vector<Math::Vector2> directions;
        for (const auto& point : points) {
            const Math::Vector2 direction = (point.position - source).Normalized();
            if (!direction.IsZero()) directions.push_back(direction);
        }
        for (const auto& direction : directions) candidates.push_back(source + direction * range);
        for (std::size_t left = 0; left < directions.size(); ++left) {
            for (std::size_t right = left + 1; right < directions.size(); ++right) {
                const Math::Vector2 direction = (directions[left] + directions[right]).Normalized();
                if (!direction.IsZero()) candidates.push_back(source + direction * range);
            }
        }
        const double halfAngle = std::clamp(angleRadians * 0.5, 0.01, Math::Pi * 0.5);
        return SelectBest(source, points, candidates, range, [source, halfAngle, range](const auto& point, const auto& cast) {
            const Math::Vector2 target = point - source;
            if (target.LengthSquared() > range * range) return false;
            return Math::AngleBetween(target, cast - source) <= halfAngle;
        });
    }

private:
    template <typename Predicate>
    static AoeSolution SelectBest(const Math::Vector2& source,
                                  const std::vector<AoePoint>& points,
                                  const std::vector<Math::Vector2>& candidates,
                                  double range,
                                  Predicate predicate) {
        AoeSolution best;
        double bestDistance = 0.0;
        for (const auto& candidate : candidates) {
            const double distance = Math::Distance(source, candidate);
            if (!candidate.IsFinite() || distance > range + Math::Epsilon) continue;
            std::vector<int> hits;
            for (const auto& point : points) {
                if (predicate(point.position, candidate)) hits.push_back(point.id);
            }
            if (!best.valid || hits.size() > best.hitIds.size() ||
                (hits.size() == best.hitIds.size() && distance < bestDistance)) {
                best.valid = !hits.empty();
                best.castPosition = candidate;
                best.hitIds = std::move(hits);
                bestDistance = distance;
            }
        }
        return best;
    }

    static Math::Vector2 ClampToRange(const Math::Vector2& source,
                                      const Math::Vector2& position,
                                      double range) {
        const Math::Vector2 delta = position - source;
        const double distance = delta.Length();
        return distance > range && distance > Math::Epsilon
            ? source + delta * (range / distance)
            : position;
    }
};

}
