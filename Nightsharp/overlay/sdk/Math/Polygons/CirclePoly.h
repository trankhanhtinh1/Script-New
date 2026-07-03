#pragma once

// ============================================================================
// CirclePoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/CirclePoly.cs
// ----------------------------------------------------------------------------
// Represents a Circle Polygon. UpdatePolygon walks `quality` evenly-spaced
// angles around `Center` at radius `Radius` (with optional `offset` /
// `overrideWidth`).
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class CirclePoly : public Polygon {
    public:
        Vec2  Center = {};
        float Radius = 0.0f;

        CirclePoly(const Vec3& center, float radius, int quality = 20)
            : CirclePoly(center.To2D(), radius, quality) {}

        CirclePoly(const Vec2& center, float radius, int quality = 20)
            : quality_(quality) {
            Center = center;
            Radius = radius;

            UpdatePolygon();
        }

        // Call this after changing something.
        // `offset`        : extra radius added to the configured Radius
        // `overrideWidth` : if > 0, used directly as outRadius (ignores offset)
        void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
            Points.clear();

            const float twoPi = 6.28318530717958647692f;
            const float outRadius = overrideWidth > 0.0f
                ? overrideWidth
                : (static_cast<float>(offset) + Radius) /
                  std::cos(twoPi / static_cast<float>(quality_));

            for (int i = 1; i <= quality_; ++i) {
                const float angle =
                    static_cast<float>(i) * 2.0f * 3.14159265358979323846f /
                    static_cast<float>(quality_);
                Points.emplace_back(
                    Center.x + (outRadius * std::cos(angle)),
                    Center.y + (outRadius * std::sin(angle)));
            }
        }

    private:
        int quality_;
    };

} // namespace SDK
