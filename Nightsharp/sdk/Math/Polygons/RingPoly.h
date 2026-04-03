#pragma once

#include "Polygon.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK::Geometry {

/// <summary>
///     Represents a Ring Polygon (donut shape: outer circle + inner circle).
///     Port of EnsoulSharp.SDK RingPoly.cs — exact behavioral parity.
///     Parameters: center, width (ring thickness), outerRadius, quality.
///     NOTE: C# uses "Width" not "InnerRadius". Inner is computed as (Width - OuterRadius - offset).
/// </summary>
class RingPoly : public Polygon {
public:
    Vec2  Center      = {};
    float Width       = 0.0f;   // Ring width (NOT inner radius)
    float OuterRadius = 0.0f;
    int   Quality     = 20;

    RingPoly() = default;

    RingPoly(const Vec3& center, float width, float outerRadius, int quality = 20)
        : RingPoly(center.To2D(), width, outerRadius, quality) {}

    RingPoly(const Vec2& center, float width, float outerRadius, int quality = 20) {
        Center      = center;
        Width       = width;
        OuterRadius = outerRadius;
        Quality     = quality;

        UpdatePolygon();
    }

    /// <summary>
    ///     Updates the Ring polygon.
    ///     Matches C# UpdatePolygon(int offset = 0) exactly.
    ///     outRadius   = (offset + Width + OuterRadius) / cos(2π/quality)
    ///     innerRadius = Width - OuterRadius - offset
    ///     Outer ring: Center - outR*cos, Center - outR*sin  (i=0..quality)
    ///     Inner ring: Center + innerR*cos, Center - innerR*sin (i=0..quality)
    /// </summary>
    void UpdatePolygon(int offset = 0) {
        Points.clear();

        const float outRadius = (static_cast<float>(offset) + Width + OuterRadius)
                                / static_cast<float>(std::cos(2.0 * M_PI / static_cast<double>(Quality)));
        const float innerRadius = Width - OuterRadius - static_cast<float>(offset);

        // Outer ring
        for (int i = 0; i <= Quality; ++i) {
            const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI)
                                / static_cast<float>(Quality);
            Points.emplace_back(
                Center.x - outRadius * std::cos(angle),
                Center.y - outRadius * std::sin(angle)
            );
        }

        // Inner ring
        for (int i = 0; i <= Quality; ++i) {
            const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI)
                                / static_cast<float>(Quality);
            Points.emplace_back(
                Center.x + innerRadius * std::cos(angle),
                Center.y - innerRadius * std::sin(angle)
            );
        }
    }
};

} // namespace SDK::Geometry
