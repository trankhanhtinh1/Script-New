#pragma once

#include "../../../core/Vector.h"
#include "../../Core/Objects.h"
#include "../../UI/Drawing.h"
#include "../../../Third_Party/clipper/clipper.h"

#include <vector>

namespace SDK::Geometry {

/// <summary>
///     Base class representing a polygon.
///     Port of EnsoulSharp.SDK Polygon.cs — exact behavioral parity.
///     Uses ClipperLib::Clipper::PointInPolygon for IsOutside (matches C#).
/// </summary>
class Polygon {
public:
    std::vector<Vec2> Points = {};

    Polygon() = default;
    explicit Polygon(std::vector<Vec2> points)
        : Points(std::move(points)) {}

    /// <summary>
    ///     Adds a Vec2 point.
    /// </summary>
    void Add(const Vec2& point) {
        Points.push_back(point);
    }

    /// <summary>
    ///     Converts Vec3 to 2D, then adds.
    /// </summary>
    void Add(const Vec3& point) {
        Points.push_back(point.To2D());
    }

    /// <summary>
    ///     Adds all points from another polygon.
    /// </summary>
    void Add(const Polygon& polygon) {
        for (const auto& p : polygon.Points) {
            Points.push_back(p);
        }
    }

    /// <summary>
    ///     Gets if the position is OUTSIDE of the polygon.
    ///     PRIMARY method — matches C#: Clipper.PointInPolygon(p, path) != 1
    ///     Returns true when point is NOT inside.
    /// </summary>
    bool IsOutside(const Vec2& point) const {
        auto p = ClipperLib::IntPoint(static_cast<ClipperLib::cInt>(point.x),
                                      static_cast<ClipperLib::cInt>(point.y));
        return ClipperLib::Clipper::PointInPolygon(p, ToClipperPath()) != 1;
    }

    /// <summary>
    ///     C# definition: return !this.IsOutside(point);
    /// </summary>
    bool IsInside(const Vec2& point) const {
        return !IsOutside(point);
    }

    /// <summary>
    ///     C# definition: return !this.IsOutside(point.ToVector2());
    /// </summary>
    bool IsInside(const Vec3& point) const {
        return !IsOutside(point.To2D());
    }

    /// <summary>
    ///     C# definition: return !this.IsOutside(gameObject.Position.ToVector2());
    /// </summary>
    bool IsInside(const GameObject& gameObject) const {
        return !IsOutside(gameObject.Position().To2D());
    }

    bool IsOutside(const Vec3& point) const {
        return IsOutside(point.To2D());
    }

    bool IsOutside(const GameObject& gameObject) const {
        return IsOutside(gameObject.Position().To2D());
    }

    /// <summary>
    ///     Converts all points to Clipper IntPoint path (integer coordinates).
    ///     Exact match of C# Polygon.ToClipperPath().
    /// </summary>
    ClipperLib::Path ToClipperPath() const {
        ClipperLib::Path result;
        result.reserve(Points.size());
        for (const auto& p : Points) {
            result.emplace_back(static_cast<ClipperLib::cInt>(p.x),
                                static_cast<ClipperLib::cInt>(p.y));
        }
        return result;
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
};

} // namespace SDK::Geometry
