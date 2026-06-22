#pragma once

// ============================================================================
// SectorPoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/SectorPoly.cs
// ----------------------------------------------------------------------------
// Represents a Sector / pie slice Polygon defined by Center, Direction (a
// unit vector), Angle and Radius. UpdatePolygon emits the apex (Center)
// followed by `quality+1` evenly-spaced points along the arc, matching the
// C# version exactly.
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class SectorPoly : public Polygon {
    public:
        Vec2  Center    = {};
        Vec2  Direction = {};   // unit vector from center toward arc midpoint
        float Angle     = 0.0f;
        float Radius    = 0.0f;

        SectorPoly(const Vec3& center, const Vec3& direction, float angle, float radius, int quality = 20)
            : SectorPoly(center.To2D(), direction.To2D(), angle, radius, quality) {}

        SectorPoly(const Vec2& center, const Vec2& endPosition, float angle, float radius, int quality = 20)
            : quality_(quality) {
            Center    = center;
            Direction = (endPosition - center).Normalized();
            Angle     = angle;
            Radius    = radius;

            UpdatePolygon();
        }

        // Call this after changing something. `offset` adds extra radius.
        void UpdatePolygon(int offset = 0) {
            Points.clear();

            const float twoPi = 6.28318530717958647692f;
            const float outRadius =
                (Radius + static_cast<float>(offset)) /
                std::cos(twoPi / static_cast<float>(quality_));

            Points.push_back(Center);
            const Vec2 side1 = PolygonsDetail::Rotate2D(Direction, -Angle * 0.5f);

            for (int i = 0; i <= quality_; ++i) {
                const Vec2 cDirection =
                    PolygonsDetail::Rotate2D(side1, static_cast<float>(i) * Angle / static_cast<float>(quality_)).Normalized();
                Points.emplace_back(
                    Center.x + (outRadius * cDirection.x),
                    Center.y + (outRadius * cDirection.y));
            }
        }

    private:
        int quality_;
    };

} // namespace SDK
