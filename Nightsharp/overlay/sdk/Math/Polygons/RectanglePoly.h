#pragma once

// ============================================================================
// RectanglePoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/RectanglePoly.cs
// ----------------------------------------------------------------------------
// Represents a Rectangle Polygon defined by Start, End and Width. Direction
// (Start -> End normalized) and Perpendicular (90° rotation of Direction) are
// computed on demand and never need a manual update. UpdatePolygon emits the
// 4 corners in the same order as the C# version.
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class RectanglePoly : public Polygon {
    public:
        Vec2  Start = {};
        Vec2  End   = {};
        float Width = 0.0f;

        RectanglePoly(const Vec3& start, const Vec3& end, float width)
            : RectanglePoly(start.To2D(), end.To2D(), width) {}

        RectanglePoly(const Vec2& start, const Vec2& end, float width) {
            Start = start;
            End   = end;
            Width = width;

            UpdatePolygon();
        }

        // Direction (Start -> End normalized). Does not need an update.
        Vec2 Direction() const {
            return (End - Start).Normalized();
        }

        // Perpendicular direction (90° rotation of Direction).
        Vec2 Perpendicular() const {
            return PolygonsDetail::Perpendicular2D(Direction());
        }

        // Call this after changing something.
        // `offset`        : extra width AND extra length (extends ends outward)
        // `overrideWidth` : if > 0, used as the half-width instead of Width+offset
        void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
            Points.clear();

            const float halfWidth = overrideWidth > 0.0f
                ? overrideWidth
                : Width + static_cast<float>(offset);
            const float endOffset = static_cast<float>(offset);
            const Vec2  dir       = Direction();
            const Vec2  perp      = Perpendicular();

            Points.push_back(Start + (perp * halfWidth) - (dir * endOffset));
            Points.push_back(Start - (perp * halfWidth) - (dir * endOffset));
            Points.push_back(End   - (perp * halfWidth) + (dir * endOffset));
            Points.push_back(End   + (perp * halfWidth) + (dir * endOffset));
        }
    };

} // namespace SDK
