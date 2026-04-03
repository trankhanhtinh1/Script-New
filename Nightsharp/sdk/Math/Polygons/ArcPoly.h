#pragma once

#include "Polygon.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK::Geometry {

/// <summary>
///     Represents an Arc Polygon.
///     Port of EnsoulSharp.SDK ArcPoly.cs — exact behavioral parity.
///     Parameters: startPos, direction (normalized end-start), angle (total sweep), radius.
/// </summary>
class ArcPoly : public Polygon {
public:
    Vec2  StartPos  = {};
    Vec2  EndPos    = {};   // Normalized direction (end - start).Normalized()
    float Angle     = 0.0f; // Total sweep angle in radians
    float Radius    = 0.0f;
    int   Quality   = 20;

    ArcPoly() = default;

    /// <summary>
    ///     Construct from Vec3 start + direction, converting to 2D.
    /// </summary>
    ArcPoly(const Vec3& start, const Vec3& direction, float angle, float radius, int quality = 20)
        : ArcPoly(start.To2D(), direction.To2D(), angle, radius, quality) {}

    /// <summary>
    ///     Construct from Vec2 start + end.  EndPos is stored as the normalized direction.
    /// </summary>
    ArcPoly(const Vec2& start, const Vec2& end, float angle, float radius, int quality = 20) {
        StartPos = start;
        // EndPos = (end - start).Normalized()
        EndPos = (end - start).Normalized();
        Angle   = angle;
        Radius  = radius;
        Quality = quality;

        UpdatePolygon();
    }

    /// <summary>
    ///     Updates the Arc polygon.  Matches C# UpdatePolygon(int offset = 0) exactly.
    /// </summary>
    void UpdatePolygon(int offset = 0) {
        Points.clear();

        const float outRadius = (Radius + static_cast<float>(offset))
                                / static_cast<float>(std::cos(2.0 * M_PI / static_cast<double>(Quality)));

        // side1 = EndPos.Rotated(-Angle/2)
        Vec2 side1 = EndPos.Rotated(-Angle * 0.5f);

        for (int i = 0; i <= Quality; ++i) {
            // cDirection = side1.Rotated(i * Angle / Quality).Normalized()
            const float rotAngle = static_cast<float>(i) * Angle / static_cast<float>(Quality);
            Vec2 cDir = side1.Rotated(rotAngle).Normalized();
            Points.emplace_back(
                StartPos.x + outRadius * cDir.x,
                StartPos.y + outRadius * cDir.y
            );
        }
    }
};

} // namespace SDK::Geometry
