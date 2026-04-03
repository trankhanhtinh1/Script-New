#pragma once

#include "Polygon.h"

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK::Geometry {

/// <summary>
///     Represents a Circle Polygon.
///     Port of EnsoulSharp.SDK CirclePoly.cs — exact behavioral parity.
/// </summary>
class CirclePoly : public Polygon {
public:
    Vec2  Center  = {};
    float Radius  = 0.0f;
    int   Quality = 20;

    CirclePoly() = default;

    CirclePoly(const Vec3& center, float radius, int quality = 20)
        : CirclePoly(center.To2D(), radius, quality) {}

    CirclePoly(const Vec2& center, float radius, int quality = 20) {
        Center  = center;
        Radius  = radius;
        Quality = quality;

        UpdatePolygon();
    }

    /// <summary>
    ///     Updates the Circle polygon.
    ///     Matches C# UpdatePolygon(int offset = 0, float overrideWidth = -1) exactly.
    ///     When overrideWidth > 0, it is used directly (no cosine expansion).
    ///     Otherwise, (offset + Radius) / cos(2π/quality) is used.
    /// </summary>
    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        Points.clear();

        const float outRadius = overrideWidth > 0.0f
            ? overrideWidth
            : (static_cast<float>(offset) + Radius)
              / static_cast<float>(std::cos(2.0 * M_PI / static_cast<double>(Quality)));

        for (int i = 1; i <= Quality; ++i) {
            const float angle = static_cast<float>(i) * 2.0f * static_cast<float>(M_PI)
                                / static_cast<float>(Quality);
            Points.emplace_back(
                Center.x + outRadius * std::cos(angle),
                Center.y + outRadius * std::sin(angle)
            );
        }
    }
};

} // namespace SDK::Geometry
