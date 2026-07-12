#pragma once

#include "../Math/Vector2.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <utility>
#include <vector>

namespace ZDPrediction {

struct AoePoint {
    int id = 0;
    Math::Vector2 position = {};
    bool primary = false;
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
                              double range,
                              int primaryId) {
        std::vector<Math::Vector2> candidates;
        candidates.reserve(points.size() * points.size() + 1);
        for (const auto& point : points) candidates.push_back(point.position);
        for (std::size_t left = 0; left < points.size(); ++left) {
            for (std::size_t right = left + 1; right < points.size(); ++right) {
                const Math::Vector2 delta = points[right].position - points[left].position;
                const double distance = delta.Length();
                if (distance > 2.0 * radius || distance <= Math::Epsilon) continue;
                const Math::Vector2 middle = (points[left].position + points[right].position) * 0.5;
                const double height = std::sqrt(std::max(0.0, radius * radius - distance * distance * 0.25));
                const Math::Vector2 perpendicular{-delta.y / distance, delta.x / distance};
                candidates.push_back(middle + perpendicular * height);
                candidates.push_back(middle - perpendicular * height);
            }
        }
        return SelectBest(source, points, candidates, range, primaryId,
            [radius](const Math::Vector2& point, const Math::Vector2& cast) {
                return Math::DistanceSquared(point, cast) <= radius * radius;
            });
    }

    static AoeSolution Line(const Math::Vector2& source,
                            const std::vector<AoePoint>& points,
                            double radius,
                            double range,
                            int primaryId) {
        std::vector<Math::Vector2> candidates;
        for (const auto& point : points) AddLineCandidates(source, point.position, range, candidates);
        for (std::size_t left = 0; left < points.size(); ++left) {
            for (std::size_t right = left + 1; right < points.size(); ++right) {
                AddLineCandidates(source,
                                  (points[left].position + points[right].position) * 0.5,
                                  range,
                                  candidates);
            }
        }
        return SelectBest(source, points, candidates, range, primaryId,
            [source, radius](const Math::Vector2& point, const Math::Vector2& cast) {
                return Math::DistanceSquaredToSegment(point, source, cast) <= radius * radius;
            });
    }

    static AoeSolution Cone(const Math::Vector2& source,
                            const std::vector<AoePoint>& points,
                            double angleRadians,
                            double range,
                            int primaryId) {
        std::vector<Math::Vector2> directions;
        for (const auto& point : points) {
            const Math::Vector2 direction = (point.position - source).Normalized();
            if (!direction.IsZero()) directions.push_back(direction);
        }

        std::vector<Math::Vector2> candidates;
        candidates.reserve(directions.size() * directions.size() + 1);
        for (const auto& direction : directions) candidates.push_back(source + direction * range);
        for (std::size_t left = 0; left < directions.size(); ++left) {
            for (std::size_t right = left + 1; right < directions.size(); ++right) {
                const Math::Vector2 direction = (directions[left] + directions[right]).Normalized();
                if (!direction.IsZero()) candidates.push_back(source + direction * range);
            }
        }

        const double halfAngle = std::clamp(angleRadians * 0.5, 0.01, Math::Pi * 0.5);
        return SelectBest(source, points, candidates, range, primaryId,
            [source, halfAngle, range](const Math::Vector2& point, const Math::Vector2& cast) {
                const Math::Vector2 target = point - source;
                const Math::Vector2 direction = cast - source;
                return target.LengthSquared() <= range * range &&
                    !target.IsZero() && !direction.IsZero() &&
                    Math::AngleBetween(target, direction) <= halfAngle + 1e-6;
            });
    }

private:
    static void AddLineCandidates(const Math::Vector2& source,
                                  const Math::Vector2& target,
                                  double range,
                                  std::vector<Math::Vector2>& candidates) {
        const Math::Vector2 delta = target - source;
        const double distance = delta.Length();
        if (distance <= Math::Epsilon) return;
        const Math::Vector2 direction = delta / distance;
        candidates.push_back(source + direction * std::min(distance, range));
        candidates.push_back(source + direction * range);
    }

    template <typename Predicate>
    static AoeSolution SelectBest(const Math::Vector2& source,
                                  const std::vector<AoePoint>& points,
                                  const std::vector<Math::Vector2>& candidates,
                                  double range,
                                  int primaryId,
                                  Predicate predicate) {
        AoeSolution best;
        double bestDistance = std::numeric_limits<double>::infinity();
        for (const auto& candidate : candidates) {
            if (!candidate.IsFinite() || Math::Distance(source, candidate) > range + Math::Epsilon) continue;
            std::vector<int> hits;
            bool primaryHit = false;
            for (const auto& point : points) {
                if (!predicate(point.position, candidate)) continue;
                hits.push_back(point.id);
                if (point.id == primaryId || point.primary) primaryHit = true;
            }
            if (!primaryHit || hits.empty()) continue;
            const double distance = Math::Distance(source, candidate);
            if (!best.valid || hits.size() > best.hitIds.size() ||
                (hits.size() == best.hitIds.size() && distance < bestDistance)) {
                best.valid = true;
                best.castPosition = candidate;
                best.hitIds = std::move(hits);
                bestDistance = distance;
            }
        }
        return best;
    }
};

}
