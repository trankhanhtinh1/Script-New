#pragma once

// ============================================================================
// ArcPoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/ArcPoly.cs
// ----------------------------------------------------------------------------
// Represents an arc shaped polygon. Note: matches the (slightly unusual)
// C# semantics where `EndPos` actually stores the *normalized direction*
// from `start` to `end`, not the end position itself. UpdatePolygon walks
// `quality+1` evenly-spaced angles around the center direction.
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class ArcPoly : public Polygon {
    public:
        // Public properties (mirror C# auto-properties).
        Vec2  StartPos = {};
        Vec2  EndPos   = {};   // direction unit-vector, NOT raw end point
        float Angle    = 0.0f;
        float Radius   = 0.0f;

        // 3D constructor: drops Y, then forwards to the 2D constructor.
        ArcPoly(const Vec3& start, const Vec3& direction, float angle, float radius, int quality = 20)
            : ArcPoly(start.To2D(), direction.To2D(), angle, radius, quality) {}

        ArcPoly(const Vec2& start, const Vec2& end, float angle, float radius, int quality = 20)
            : quality_(quality) {
            StartPos = start;
            EndPos   = (end - start).Normalized();
            Angle    = angle;
            Radius   = radius;

            UpdatePolygon();
        }

        // Use this after changing something. `offset` adds extra radius.
        void UpdatePolygon(int offset = 0) {
            Points.clear();

            const float twoPi = 6.28318530717958647692f;
            const float outRadius =
                (Radius + static_cast<float>(offset)) /
                std::cos(twoPi / static_cast<float>(quality_));
            const Vec2 side1 = PolygonsDetail::Rotate2D(EndPos, -Angle * 0.5f);

            for (int i = 0; i <= quality_; ++i) {
                const Vec2 cDirection =
                    PolygonsDetail::Rotate2D(side1, static_cast<float>(i) * Angle / static_cast<float>(quality_)).Normalized();
                Points.emplace_back(
                    StartPos.x + (outRadius * cDirection.x),
                    StartPos.y + (outRadius * cDirection.y));
            }
        }

    private:
        // Arc Quality (number of segments around the arc).
        int quality_;
    };

} // namespace SDK
