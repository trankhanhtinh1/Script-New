#pragma once

#include "Polygon.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK::Geometry {

/// <summary>
///     Represents a Sector (cone/pie) Polygon.
///     Port of EnsoulSharp.SDK SectorPoly.cs — exact behavioral parity.
///     Parameters: center, endPosition (direction is computed as (end-center).Normalized()),
///                 angle (total sweep), radius.
///     Uses rotation-based sweep like ArcPoly (Rotated + Normalized per step).
/// </summary>
class SectorPoly : public Polygon {
public:
    Vec2  Center    = {};
    Vec2  Direction = {};   // Normalized direction from center to end
    float Angle     = 0.0f; // Total sweep angle in radians
    float Radius    = 0.0f;
    int   Quality   = 20;

    SectorPoly() = default;

    SectorPoly(const Vec3& center, const Vec3& direction, float angle, float radius, int quality = 20)
        : SectorPoly(center.To2D(), direction.To2D(), angle, radius, quality) {}

    /// <summary>
    ///     C# constructor: Direction = (endPosition - center).Normalized()
    /// </summary>
    SectorPoly(const Vec2& center, const Vec2& endPosition, float angle, float radius, int quality = 20) {
        Center = center;
        // C#: this.Direction = (endPosition - center).Normalized();
        Direction = (endPosition - center).Normalized();
        Angle   = angle;
        Radius  = radius;
        Quality = quality;

        UpdatePolygon();
    }

    /// <summary>
    ///     Updates the Sector polygon.
    ///     Matches C# UpdatePolygon(int offset = 0) exactly.
    ///     outRadius = (Radius + offset) / cos(2π/quality)
    ///     Starts with center point, then sweeps from -Angle/2 to +Angle/2.
    /// </summary>
    void UpdatePolygon(int offset = 0) {
        Points.clear();

        const float outRadius = (Radius + static_cast<float>(offset))
                                / static_cast<float>(std::cos(2.0 * M_PI / static_cast<double>(Quality)));

        // First point is the center (apex of the cone)
        Points.push_back(Center);

        // side1 = Direction.Rotated(-Angle * 0.5f)
        Vec2 side1 = Direction.Rotated(-Angle * 0.5f);

        for (int i = 0; i <= Quality; ++i) {
            // cDirection = side1.Rotated(i * Angle / quality).Normalized()
            float rotAngle = static_cast<float>(i) * Angle / static_cast<float>(Quality);
            Vec2 cDir = side1.Rotated(rotAngle).Normalized();
            Points.emplace_back(
                Center.x + outRadius * cDir.x,
                Center.y + outRadius * cDir.y
            );
        }
    }
};

} // namespace SDK::Geometry
