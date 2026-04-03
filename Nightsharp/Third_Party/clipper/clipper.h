/*******************************************************************************
*                                                                              *
* C++ port of Clipper library (Angus Johnson, v6.4.2)                          *
* Ported from: EnsoulSharp.SDK/Third_Party/clipper/clipper.cs                   *
*                                                                              *
* Only the subset actually used by EnsoulSharp.SDK is ported:                   *
*   - IntPoint struct                                                          *
*   - Path / Paths typedefs                                                    *
*   - Clipper::PointInPolygon (public static)                                  *
*   - Clipper::Area (public static)                                            *
*   - Clipper::Orientation (public static)                                     *
*   - IntRect / GetBounds                                                      *
*   - ClipType, PolyType, PolyFillType, JoinType, EndType enums               *
*                                                                              *
* The full clipping engine (ClipperBase, Clipper, ClipperOffset) is NOT        *
* ported because the SDK only uses PointInPolygon.  If the full engine is      *
* ever needed, consider adding the official C++ Clipper2 library instead.      *
*                                                                              *
* Original license: Boost Software License Ver 1.                              *
* http://www.boost.org/LICENSE_1_0.txt                                         *
*                                                                              *
*******************************************************************************/

#pragma once

#include <cstdint>
#include <vector>
#include <cmath>

namespace ClipperLib {

    // ── Integer types ──────────────────────────────────────────────────────
    using cInt = int64_t;

    // ── IntPoint ───────────────────────────────────────────────────────────
    struct IntPoint {
        cInt X;
        cInt Y;

        IntPoint(cInt x = 0, cInt y = 0) : X(x), Y(y) {}
        IntPoint(double x, double y) : X(static_cast<cInt>(x)), Y(static_cast<cInt>(y)) {}

        bool operator==(const IntPoint& o) const { return X == o.X && Y == o.Y; }
        bool operator!=(const IntPoint& o) const { return X != o.X || Y != o.Y; }
    };

    // ── Path / Paths ───────────────────────────────────────────────────────
    using Path  = std::vector<IntPoint>;
    using Paths = std::vector<Path>;

    // ── IntRect ────────────────────────────────────────────────────────────
    struct IntRect {
        cInt left;
        cInt top;
        cInt right;
        cInt bottom;

        IntRect(cInt l = 0, cInt t = 0, cInt r = 0, cInt b = 0)
            : left(l), top(t), right(r), bottom(b) {}
    };

    // ── Enums ──────────────────────────────────────────────────────────────
    enum class ClipType    { ctIntersection, ctUnion, ctDifference, ctXor };
    enum class PolyType    { ptSubject, ptClip };
    enum class PolyFillType{ pftEvenOdd, pftNonZero, pftPositive, pftNegative };
    enum class JoinType    { jtSquare, jtRound, jtMiter };
    enum class EndType     { etClosedPolygon, etClosedLine, etOpenButt, etOpenSquare, etOpenRound };

    // ── Clipper (static utility methods only) ──────────────────────────────
    class Clipper {
    public:
        /// <summary>
        ///     Returns 0 if false, +1 if true, -1 if pt ON polygon boundary.
        ///     See "The Point in Polygon Problem for Arbitrary Polygons"
        ///     by Hormann & Agathos.
        ///     Exact 1:1 port of C# Clipper.PointInPolygon(IntPoint, Path).
        /// </summary>
        static int PointInPolygon(const IntPoint& pt, const Path& path) {
            int result = 0;
            const int cnt = static_cast<int>(path.size());
            if (cnt < 3) return 0;

            IntPoint ip = path[0];
            for (int i = 1; i <= cnt; ++i) {
                IntPoint ipNext = (i == cnt) ? path[0] : path[i];

                if (ipNext.Y == pt.Y) {
                    if ((ipNext.X == pt.X) || (ip.Y == pt.Y &&
                        ((ipNext.X > pt.X) == (ip.X < pt.X))))
                        return -1;
                }

                if ((ip.Y < pt.Y) != (ipNext.Y < pt.Y)) {
                    if (ip.X >= pt.X) {
                        if (ipNext.X > pt.X) {
                            result = 1 - result;
                        } else {
                            double d = static_cast<double>(ip.X - pt.X)
                                     * static_cast<double>(ipNext.Y - pt.Y)
                                     - static_cast<double>(ipNext.X - pt.X)
                                     * static_cast<double>(ip.Y - pt.Y);
                            if (d == 0.0) return -1;
                            if ((d > 0.0) == (ipNext.Y > ip.Y))
                                result = 1 - result;
                        }
                    } else {
                        if (ipNext.X > pt.X) {
                            double d = static_cast<double>(ip.X - pt.X)
                                     * static_cast<double>(ipNext.Y - pt.Y)
                                     - static_cast<double>(ipNext.X - pt.X)
                                     * static_cast<double>(ip.Y - pt.Y);
                            if (d == 0.0) return -1;
                            if ((d > 0.0) == (ipNext.Y > ip.Y))
                                result = 1 - result;
                        }
                    }
                }
                ip = ipNext;
            }
            return result;
        }

        /// <summary>
        ///     Calculates the signed area of a polygon path.
        ///     Positive = counter-clockwise, Negative = clockwise.
        ///     Exact port of C# Clipper.Area(Path).
        /// </summary>
        static double Area(const Path& poly) {
            const int cnt = static_cast<int>(poly.size());
            if (cnt < 3) return 0.0;
            double a = 0.0;
            for (int i = 0, j = cnt - 1; i < cnt; ++i) {
                a += (static_cast<double>(poly[j].X) + poly[i].X)
                   * (static_cast<double>(poly[j].Y) - poly[i].Y);
                j = i;
            }
            return -a * 0.5;
        }

        /// <summary>
        ///     Returns true if the polygon has counter-clockwise orientation.
        ///     Exact port of C# Clipper.Orientation(Path).
        /// </summary>
        static bool Orientation(const Path& poly) {
            return Area(poly) >= 0.0;
        }

        /// <summary>
        ///     Returns the bounding rectangle of a set of paths.
        ///     Exact port of C# ClipperBase.GetBounds(Paths).
        /// </summary>
        static IntRect GetBounds(const Paths& paths) {
            int i = 0;
            const int cnt = static_cast<int>(paths.size());
            while (i < cnt && paths[i].empty()) i++;
            if (i == cnt) return IntRect(0, 0, 0, 0);

            IntRect result;
            result.left   = paths[i][0].X;
            result.right  = paths[i][0].X;
            result.top    = paths[i][0].Y;
            result.bottom = paths[i][0].Y;

            for (; i < cnt; i++) {
                for (size_t j = 0; j < paths[i].size(); j++) {
                    if (paths[i][j].X < result.left)   result.left   = paths[i][j].X;
                    if (paths[i][j].X > result.right)   result.right  = paths[i][j].X;
                    if (paths[i][j].Y < result.top)     result.top    = paths[i][j].Y;
                    if (paths[i][j].Y > result.bottom)  result.bottom = paths[i][j].Y;
                }
            }
            return result;
        }

        /// <summary>
        ///     Reverses all paths in-place.
        /// </summary>
        static void ReversePaths(Paths& polys) {
            for (auto& p : polys) {
                std::reverse(p.begin(), p.end());
            }
        }
    };

} // namespace ClipperLib
