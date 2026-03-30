#pragma once

#include "../../../core/Vector.h"
#include "../../Core/Objects.h"
#include "../../UI/Drawing.h"

#include <vector>

namespace SDK::Geometry {

class Polygon {
public:
    std::vector<Vec2> Points = {};

    Polygon() = default;
    explicit Polygon(std::vector<Vec2> points)
        : Points(std::move(points)) {}

    void Add(const Vec2& point) {
        Points.push_back(point);
    }

    void Add(const Vec3& point) {
        Points.push_back(point.To2D());
    }

    void Add(const Polygon& polygon) {
        Points.insert(Points.end(), polygon.Points.begin(), polygon.Points.end());
    }

    bool IsInside(const Vec2& point) const {
        bool inside = false;
        if (Points.size() < 3) {
            return false;
        }

        size_t j = Points.size() - 1;
        for (size_t i = 0; i < Points.size(); ++i) {
            const auto& pi = Points[i];
            const auto& pj = Points[j];
            const bool crosses = ((pi.y > point.y) != (pj.y > point.y)) &&
                (point.x < (pj.x - pi.x) * (point.y - pi.y) / ((pj.y - pi.y) + 1e-6f) + pi.x);
            if (crosses) {
                inside = !inside;
            }
            j = i;
        }

        return inside;
    }

    bool IsInside(const Vec3& point) const {
        return IsInside(point.To2D());
    }

    bool IsInside(const GameObject& gameObject) const {
        return gameObject.IsValid() && IsInside(gameObject.Position());
    }

    bool IsOutside(const Vec2& point) const {
        return !IsInside(point);
    }

    bool IsOutside(const Vec3& point) const {
        return !IsInside(point);
    }

    bool IsOutside(const GameObject& gameObject) const {
        return !IsInside(gameObject);
    }

    virtual void Draw(ImU32 color, float thickness = 1.0f, float worldY = 0.0f) const {
        if (Points.size() < 2) {
            return;
        }

        for (size_t i = 0; i < Points.size(); ++i) {
            const size_t next = (i + 1) % Points.size();
            Drawing::DrawLine(
                Vector3::From2D(Points[i], worldY),
                Vector3::From2D(Points[next], worldY),
                color,
                thickness);
        }
    }

    std::vector<Vec2> ToClipperPath() const {
        return Points;
    }
};

} // namespace SDK::Geometry
