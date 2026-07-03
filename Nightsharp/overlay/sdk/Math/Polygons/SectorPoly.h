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

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif
#include <cmath>
#include <utility>

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

        // ── RotateLineFromPoint ──────────────────────────────────────────
        // DLL-compatible instance method:
        // EnsoulSharp.SDK.Geometry.Sector.RotateLineFromPoint(
        //     Vector2 point1, Vector2 point2, float value, bool radian = true)
        // rotates point2 around point1 and returns the rotated endpoint.
        Vec2 RotateLineFromPoint(
            const Vec2& point1, const Vec2& point2,
            float value, bool radian = true) const
        {
            const float angle = radian ? value : value * 3.14159265358979323846f / 180.0f;
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            const Vec2 vector = point2 - point1;

            return Vec2(
                point1.x + vector.x * c - vector.y * s,
                point1.y + vector.x * s + vector.y * c);
        }

        Vec3 RotateLineFromPoint(
            const Vec3& point1, const Vec3& point2,
            float value, bool radian = true) const
        {
            return Vec3::From2D(
                RotateLineFromPoint(point1.To2D(), point2.To2D(), value, radian),
                point2.y);
        }

        // Legacy NightSharp helper: rotates both endpoints around an arbitrary
        // pivot and returns the rotated segment. Kept for existing callers.
        static std::pair<Vec2, Vec2> RotateLineFromPoint(
            const Vec2& point1, const Vec2& point2,
            const Vec2& pivot, float value, bool radian = true)
        {
            const float angle = radian ? value : value * 3.14159265358979323846f / 180.0f;
            const float c = std::cos(angle);
            const float s = std::sin(angle);

            // Translate points relative to pivot, rotate, translate back
            auto rotateAround = [&](const Vec2& p) -> Vec2 {
                float dx = p.x - pivot.x;
                float dy = p.y - pivot.y;
                return Vec2(
                    pivot.x + dx * c - dy * s,
                    pivot.y + dx * s + dy * c);
            };

            return { rotateAround(point1), rotateAround(point2) };
        }

    private:
        int quality_;
    };

} // namespace SDK
