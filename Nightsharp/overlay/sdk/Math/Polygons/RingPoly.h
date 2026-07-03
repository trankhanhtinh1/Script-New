#pragma once

// ============================================================================
// RingPoly.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/RingPoly.cs
// ----------------------------------------------------------------------------
// Represents a Ring Polygon (annulus) defined by Center, Width (ring thickness)
// and OuterRadius. UpdatePolygon emits two concentric rings of `quality+1`
// vertices each — the outer loop first, the inner loop second — exactly like
// the C# version.
// ============================================================================

#include "Polygon.h"

namespace SDK {

    class RingPoly : public Polygon {
    public:
        Vec2  Center      = {};
        float Width       = 0.0f;
        float OuterRadius = 0.0f;

        RingPoly(const Vec3& center, float width, float outerRadius, int quality = 20)
            : RingPoly(center.To2D(), width, outerRadius, quality) {}

        RingPoly(const Vec2& center, float width, float outerRadius, int quality = 20)
            : quality_(quality) {
            Center      = center;
            Width       = width;
            OuterRadius = outerRadius;

            UpdatePolygon();
        }

        // Call this after changing something. `offset` adds extra radius.
        void UpdatePolygon(int offset = 0) {
            Points.clear();

            const float twoPi = 6.28318530717958647692f;
            const float pi    = 3.14159265358979323846f;
            const float outRadius =
                (static_cast<float>(offset) + Width + OuterRadius) /
                std::cos(twoPi / static_cast<float>(quality_));
            const float innerRadius = Width - OuterRadius - static_cast<float>(offset);

            for (int i = 0; i <= quality_; ++i) {
                const float angle =
                    static_cast<float>(i) * 2.0f * pi /
                    static_cast<float>(quality_);
                Points.emplace_back(
                    Center.x - (outRadius * std::cos(angle)),
                    Center.y - (outRadius * std::sin(angle)));
            }

            for (int i = 0; i <= quality_; ++i) {
                const float angle =
                    static_cast<float>(i) * 2.0f * pi /
                    static_cast<float>(quality_);
                Points.emplace_back(
                    Center.x + (innerRadius * std::cos(angle)),
                    Center.y - (innerRadius * std::sin(angle)));
            }
        }

    private:
        int quality_;
    };

} // namespace SDK
