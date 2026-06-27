#pragma once

// ============================================================================
// Polygon.h - 1:1 port of EnsoulSharp.SDK / Core/Math/Polygons/Polygon.cs
// ----------------------------------------------------------------------------
// Base class representing a polygon. Holds an ordered list of `Vec2` points
// (X / Z plane in world coordinates) and provides:
//   - Add()       : append a single point or merge another polygon
//   - Draw()      : render every edge by lifting each Vec2 back to a Vec3 at
//                   the local player's Z height, then world-to-screen +
//                   line-draw via the SDK's Drawing module
//   - IsInside()  : test a point against the polygon (Vec2/Vec3 overloads)
//   - IsOutside() : negated IsInside; uses Clipper-style PointInPolygon
//                   (returns +1 inside / 0 on-edge / -1 outside, anything
//                    !=1 is treated as outside, matching the C# behaviour)
//   - ToClipperPath() : project the points into Clipper's IntPoint format
//
// Dependencies that are intentionally OUTSIDE this header (forward-declared):
//   - SDK::Drawing::WorldToScreen / DrawLine          (NightSharp/SDK/UI/Drawing.h)
//   - SDK::GameObjects::PlayerPosition                (NightSharp/SDK/GameObjects/...)
// They will be defined once those modules are ported. Polygon.h compiles on
// its own — no NightSharp/Core changes needed.
// ============================================================================

#include "../../../Core/Vector.h"
#include "../../Core/Objects.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace SDK {

    // ─── Lightweight Clipper subset ─────────────────────────────────────────
    // Mirrors `EnsoulSharp.SDK.Clipper.IntPoint` and `Clipper.PointInPolygon`
    // so the polygon module does not pull the full third-party clipper port.
    namespace Clipper {

        struct IntPoint {
            long long X = 0;
            long long Y = 0;

            IntPoint() = default;
            IntPoint(double xValue, double yValue)
                : X(static_cast<long long>(xValue))
                , Y(static_cast<long long>(yValue)) {}
            IntPoint(long long xValue, long long yValue)
                : X(xValue), Y(yValue) {}
        };

        // Returns:
        //   +1 : strictly inside polygon
        //    0 : on polygon edge
        //   -1 : outside polygon
        // Same return contract as Clipper's PointInPolygon.
        inline int PointInPolygon(const IntPoint& pt, const std::vector<IntPoint>& path) {
            int result = 0;
            const std::size_t cnt = path.size();
            if (cnt < 3) {
                return 0;
            }

            IntPoint ip = path[0];
            for (std::size_t i = 1; i <= cnt; ++i) {
                const IntPoint ipNext = (i == cnt) ? path[0] : path[i];
                if (ipNext.Y == pt.Y) {
                    if ((ipNext.X == pt.X) ||
                        (ip.Y == pt.Y && ((ipNext.X > pt.X) == (ip.X < pt.X)))) {
                        return 0;
                    }
                }
                if ((ip.Y < pt.Y) != (ipNext.Y < pt.Y)) {
                    if (ip.X >= pt.X) {
                        if (ipNext.X > pt.X) {
                            result = 1 - result;
                        } else {
                            const double d =
                                static_cast<double>(ip.X - pt.X) *
                                static_cast<double>(ipNext.Y - pt.Y) -
                                static_cast<double>(ipNext.X - pt.X) *
                                static_cast<double>(ip.Y - pt.Y);
                            if (d == 0.0) {
                                return 0;
                            }
                            if ((d > 0.0) == (ipNext.Y > ip.Y)) {
                                result = 1 - result;
                            }
                        }
                    } else if (ipNext.X > pt.X) {
                        const double d =
                            static_cast<double>(ip.X - pt.X) *
                            static_cast<double>(ipNext.Y - pt.Y) -
                            static_cast<double>(ipNext.X - pt.X) *
                            static_cast<double>(ip.Y - pt.Y);
                        if (d == 0.0) {
                            return 0;
                        }
                        if ((d > 0.0) == (ipNext.Y > ip.Y)) {
                            result = 1 - result;
                        }
                    }
                }
                ip = ipNext;
            }
            return result;
        }

    } // namespace Clipper

    // ─── 2D math helpers used by every derived polygon ──────────────────────
    namespace PolygonsDetail {

        // C# `Vector2.Rotated(angle)` (radians, counter-clockwise).
        inline Vec2 Rotate2D(const Vec2& v, float angle) {
            const float c = std::cos(angle);
            const float s = std::sin(angle);
            return Vec2(v.x * c - v.y * s, v.x * s + v.y * c);
        }

        // C# `Vector2.Perpendicular()` (default offset 0).
        inline Vec2 Perpendicular2D(const Vec2& v) {
            return Vec2(-v.y, v.x);
        }

    } // namespace PolygonsDetail

    // ─── Forward declarations ───────────────────────────────────────────────
    // Real definitions live in the Drawing / GameObjects modules. Forward-
    // declaring here keeps Polygon.h self-contained and lets it parse before
    // those modules exist.
    namespace Drawing {
        bool WorldToScreen(const Vec3& world, Vec2& screen);
        void DrawLine(float x1, float y1, float x2, float y2, float width, std::uint32_t color);
    }

    namespace GameObjects {
        Vec3 PlayerPosition();
    }

    // ────────────────────────────────────────────────────────────────────────
    // Base polygon
    // ────────────────────────────────────────────────────────────────────────
    class Polygon {
    public:
        // Ordered list of polygon vertices (X / Z world plane).
        std::vector<Vec2> Points;

        Polygon() = default;
        virtual ~Polygon() = default;

        // Adds a Vec2 to the points
        void Add(const Vec2& point) {
            Points.push_back(point);
        }

        // Converts Vec3 to 2D, then adds it to the points
        void Add(const Vec3& point) {
            Points.push_back(point.To2D());
        }

        // Adds all of the points in `polygon` to this instance
        void Add(const Polygon& polygon) {
            Points.insert(Points.end(), polygon.Points.begin(), polygon.Points.end());
        }

        // Draws all of the points in the polygon connected (color is ARGB / ImU32).
        virtual void Draw(std::uint32_t color, int width = 1) const {
            const std::size_t pointCount = Points.size();
            if (pointCount == 0) {
                return;
            }

            const Vec3 playerPosition = SDK::GameObjects::PlayerPosition();
            const float playerPositionZ = playerPosition.y; // Vec3 height axis

            for (std::size_t i = 0; i < pointCount; ++i) {
                const std::size_t nextIndex = (pointCount - 1 == i) ? 0 : (i + 1);

                const Vec3 fromWorld = Vec3(Points[i].x, playerPositionZ, Points[i].y);
                const Vec3 toWorld   = Vec3(Points[nextIndex].x, playerPositionZ, Points[nextIndex].y);

                Vec2 from = {};
                Vec2 to   = {};
                if (!SDK::Drawing::WorldToScreen(fromWorld, from)) {
                    continue;
                }
                if (!SDK::Drawing::WorldToScreen(toWorld, to)) {
                    continue;
                }

                SDK::Drawing::DrawLine(from.x, from.y, to.x, to.y, static_cast<float>(width), color);
            }
        }

        // Whether the Vec2 is inside the polygon
        bool IsInside(const Vec2& point) const {
            return !IsOutside(point);
        }

        // Whether the Vec3 (projected to 2D) is inside the polygon
        bool IsInside(const Vec3& point) const {
            return !IsOutside(point.To2D());
        }

        // Whether the GameObject's position is inside the polygon
        // DLL: Geometry.Polygon.IsInside(GameObject)
        bool IsInside(const GameObject& gameObject) const {
            return !IsOutside(gameObject.Position().To2D());
        }

        // Whether the position is OUTSIDE the polygon (PointInPolygon != 1).
        bool IsOutside(const Vec2& point) const {
            const Clipper::IntPoint p(point.x, point.y);
            return Clipper::PointInPolygon(p, ToClipperPath()) != 1;
        }

        // Converts all the points to the Clipper Library format
        std::vector<Clipper::IntPoint> ToClipperPath() const {
            std::vector<Clipper::IntPoint> result;
            result.reserve(Points.size());
            for (const auto& point : Points) {
                result.emplace_back(point.x, point.y);
            }
            return result;
        }
    };

} // namespace SDK
