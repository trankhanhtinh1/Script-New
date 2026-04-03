#pragma once

#include "Polygon.h"

namespace SDK::Geometry {

/// <summary>
///     Represents a Rectangle Polygon.
///     Port of EnsoulSharp.SDK RectanglePoly.cs — exact behavioral parity.
/// </summary>
class RectanglePoly : public Polygon {
public:
    Vec2  Start = {};
    Vec2  End   = {};
    float Width = 0.0f;

    RectanglePoly() = default;

    RectanglePoly(const Vec3& start, const Vec3& end, float width)
        : RectanglePoly(start.To2D(), end.To2D(), width) {}

    RectanglePoly(const Vec2& start, const Vec2& end, float width) {
        Start = start;
        End   = end;
        Width = width;

        UpdatePolygon();
    }

    /// <summary>
    ///     Direction = (End - Start).Normalized()
    /// </summary>
    Vec2 Direction() const {
        return (End - Start).Normalized();
    }

    /// <summary>
    ///     Perpendicular to Direction.
    /// </summary>
    Vec2 Perpendicular() const {
        return Direction().Perpendicular();
    }

    /// <summary>
    ///     Updates the Rectangle polygon.
    ///     Matches C# UpdatePolygon(int offset = 0, float overrideWidth = -1) exactly.
    ///     effectiveWidth = overrideWidth > 0 ? overrideWidth : (Width + offset)
    ///     4 corners built using Direction and Perpendicular vectors.
    /// </summary>
    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        Points.clear();

        const Vec2 dir  = Direction();
        const Vec2 perp = Perpendicular();
        const float w   = overrideWidth > 0.0f ? overrideWidth : (Width + static_cast<float>(offset));
        const float off = static_cast<float>(offset);

        // C# order: Start+wP-oD, Start-wP-oD, End-wP+oD, End+wP+oD
        Points.push_back(Start + (perp * w) - (dir * off));
        Points.push_back(Start - (perp * w) - (dir * off));
        Points.push_back(End   - (perp * w) + (dir * off));
        Points.push_back(End   + (perp * w) + (dir * off));
    }
};

} // namespace SDK::Geometry
