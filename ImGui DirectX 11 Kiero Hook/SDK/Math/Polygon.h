#pragma once
// ============================================================================
// Polygon.h — Geometry Polygon System (EnsoulSharp SDK Port)
// ============================================================================
// Full port of EnsoulSharp.SDK/Core/Math/Polygons/*
//   - Polygon (base), CirclePoly, LinePoly, RectanglePoly
//   - SectorPoly (cone), RingPoly, ArcPoly
//   - Clipper-free: uses ray-casting for point-in-polygon
//   - Advanced: ConvexHull, SegmentIntersection, MovementCollision
// ============================================================================

#include "core/Vector.h"
#include <vector>
#include <algorithm>
#include <cmath>
#include <limits>
#include <optional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace SDK {

// ============================================================================
// Polygon — Base class
// ============================================================================
class Polygon {
public:
    std::vector<Vec2> Points;

    Polygon() = default;
    virtual ~Polygon() = default;

    // ---- Add points ----
    void Add(const Vec2& point) {
        Points.push_back(point);
    }

    void Add(const Vec3& point) {
        Points.push_back(point.To2D());
    }

    void Add(const Polygon& other) {
        Points.insert(Points.end(), other.Points.begin(), other.Points.end());
    }

    // ---- Point-in-polygon (ray casting algorithm) ----
    bool IsInside(const Vec2& point) const {
        return !IsOutside(point);
    }

    bool IsInside(const Vec3& point) const {
        return IsInside(point.To2D());
    }

    bool IsOutside(const Vec2& point) const {
        // Ray casting algorithm
        int n = (int)Points.size();
        if (n < 3) return true;

        bool inside = false;
        for (int i = 0, j = n - 1; i < n; j = i++) {
            if (((Points[i].y > point.y) != (Points[j].y > point.y)) &&
                (point.x < (Points[j].x - Points[i].x) * (point.y - Points[i].y) /
                 (Points[j].y - Points[i].y) + Points[i].x))
            {
                inside = !inside;
            }
        }
        return !inside;
    }

    bool IsOutside(const Vec3& point) const {
        return IsOutside(point.To2D());
    }

    // ---- Distance from point to nearest edge ----
    float DistanceToEdge(const Vec2& point) const {
        float minDist = (std::numeric_limits<float>::max)();
        int n = (int)Points.size();
        if (n < 2) return minDist;

        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            float d = Geometry::PointToSegmentDistance(point, Points[i], Points[j]);
            if (d < minDist) minDist = d;
        }
        return minDist;
    }

    // ---- Closest point on polygon edge ----
    Vec2 ClosestPointOnEdge(const Vec2& point) const {
        Vec2 closest;
        float minDist = (std::numeric_limits<float>::max)();
        int n = (int)Points.size();

        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            Vec2 cp = Geometry::ClosestPointOnSegment(point, Points[i], Points[j]);
            float d = point.Distance(cp);
            if (d < minDist) {
                minDist = d;
                closest = cp;
            }
        }
        return closest;
    }

    // ---- Check if this polygon intersects another ----
    bool Intersects(const Polygon& other) const {
        // SAT (Separating Axis Theorem) simplified: check if any point of either is inside the other
        for (auto& p : Points) {
            if (other.IsInside(p)) return true;
        }
        for (auto& p : other.Points) {
            if (IsInside(p)) return true;
        }
        // Also check edge intersections
        int n1 = (int)Points.size();
        int n2 = (int)other.Points.size();
        for (int i = 0; i < n1; i++) {
            int ni = (i + 1) % n1;
            for (int j = 0; j < n2; j++) {
                int nj = (j + 1) % n2;
                if (SegmentsIntersect(Points[i], Points[ni], other.Points[j], other.Points[nj]))
                    return true;
            }
        }
        return false;
    }

    // ---- Area of polygon (shoelace formula) ----
    float Area() const {
        float area = 0.0f;
        int n = (int)Points.size();
        for (int i = 0; i < n; i++) {
            int j = (i + 1) % n;
            area += Points[i].x * Points[j].y;
            area -= Points[j].x * Points[i].y;
        }
        return fabsf(area) * 0.5f;
    }

    // ---- Center of polygon ----
    Vec2 Center() const {
        if (Points.empty()) return Vec2();
        Vec2 sum;
        for (auto& p : Points) {
            sum.x += p.x;
            sum.y += p.y;
        }
        return sum / (float)Points.size();
    }

private:
    static bool SegmentsIntersect(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
        Vec2 r = b - a;
        Vec2 s = d - c;
        float rxs = r.Cross(s);
        if (fabsf(rxs) < 1e-10f) return false;

        Vec2 qp = c - a;
        float t = qp.Cross(s) / rxs;
        float u = qp.Cross(r) / rxs;
        return (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);
    }
};

// ============================================================================
// CirclePoly — Circle approximated as polygon
// ============================================================================
class CirclePoly : public Polygon {
public:
    Vec2 CircleCenter;
    float Radius;
    int Quality;

    CirclePoly() : Radius(0), Quality(20) {}

    CirclePoly(const Vec2& center, float radius, int quality = 20)
        : CircleCenter(center), Radius(radius), Quality(quality)
    {
        UpdatePolygon();
    }

    CirclePoly(const Vec3& center, float radius, int quality = 20)
        : CirclePoly(center.To2D(), radius, quality)
    {
    }

    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        Points.clear();

        float outRadius = overrideWidth > 0
            ? overrideWidth
            : (offset + Radius) / cosf(2.0f * (float)M_PI / Quality);

        for (int i = 1; i <= Quality; i++) {
            float angle = i * 2.0f * (float)M_PI / Quality;
            Points.push_back(Vec2(
                CircleCenter.x + outRadius * cosf(angle),
                CircleCenter.y + outRadius * sinf(angle)
            ));
        }
    }
};

// ============================================================================
// LinePoly — Simple line (2 points)
// ============================================================================
class LinePoly : public Polygon {
public:
    Vec2 LineStart;
    Vec2 LineEnd;

    LinePoly() = default;

    LinePoly(const Vec2& start, const Vec2& end, float length = -1.0f)
        : LineStart(start), LineEnd(end)
    {
        if (length > 0) {
            SetLength(length);
        }
        UpdatePolygon();
    }

    LinePoly(const Vec3& start, const Vec3& end, float length = -1.0f)
        : LinePoly(start.To2D(), end.To2D(), length)
    {
    }

    float GetLength() const {
        return LineStart.Distance(LineEnd);
    }

    void SetLength(float value) {
        LineEnd = (LineEnd - LineStart).Normalized() * value + LineStart;
    }

    void UpdatePolygon() {
        Points.clear();
        Points.push_back(LineStart);
        Points.push_back(LineEnd);
    }
};

// ============================================================================
// RectanglePoly — Rectangle around a line (used for line skillshots)
// ============================================================================
class RectanglePoly : public Polygon {
public:
    Vec2 Start;
    Vec2 End;
    float Width;

    RectanglePoly() : Width(0) {}

    RectanglePoly(const Vec2& start, const Vec2& end, float width)
        : Start(start), End(end), Width(width)
    {
        UpdatePolygon();
    }

    RectanglePoly(const Vec3& start, const Vec3& end, float width)
        : RectanglePoly(start.To2D(), end.To2D(), width)
    {
    }

    Vec2 Direction() const {
        return (End - Start).Normalized();
    }

    Vec2 Perpendicular() const {
        return Direction().Perpendicular();
    }

    void UpdatePolygon(int offset = 0, float overrideWidth = -1.0f) {
        Points.clear();

        float w = (overrideWidth > 0) ? overrideWidth : (Width + offset);
        Vec2 dir = Direction();
        Vec2 perp = dir.Perpendicular();

        Points.push_back(Start + perp * w - dir * (float)offset);
        Points.push_back(Start - perp * w - dir * (float)offset);
        Points.push_back(End   - perp * w + dir * (float)offset);
        Points.push_back(End   + perp * w + dir * (float)offset);
    }
};

// ============================================================================
// SectorPoly — Cone/sector (used for cone skillshots: Annie W, etc.)
// ============================================================================
class SectorPoly : public Polygon {
public:
    Vec2 SectorCenter;
    Vec2 Direction;
    float Angle;    // In radians
    float Radius;
    int Quality;

    SectorPoly() : Angle(0), Radius(0), Quality(20) {}

    SectorPoly(const Vec2& center, const Vec2& endPosition, float angle, float radius, int quality = 20)
        : SectorCenter(center), Angle(angle), Radius(radius), Quality(quality)
    {
        Direction = (endPosition - center).Normalized();
        UpdatePolygon();
    }

    SectorPoly(const Vec3& center, const Vec3& endPosition, float angle, float radius, int quality = 20)
        : SectorPoly(center.To2D(), endPosition.To2D(), angle, radius, quality)
    {
    }

    void UpdatePolygon(int offset = 0) {
        Points.clear();

        float outRadius = (Radius + offset) / cosf(2.0f * (float)M_PI / Quality);
        Points.push_back(SectorCenter);

        Vec2 side1 = Direction.Rotated(-Angle * 0.5f);

        for (int i = 0; i <= Quality; i++) {
            Vec2 cDir = side1.Rotated(i * Angle / Quality).Normalized();
            Points.push_back(Vec2(
                SectorCenter.x + outRadius * cDir.x,
                SectorCenter.y + outRadius * cDir.y
            ));
        }
    }
};

// ============================================================================
// RingPoly — Ring/donut (used for Veigar E cage zone)
// ============================================================================
class RingPoly : public Polygon {
public:
    Vec2 RingCenter;
    float RingWidth;      // Width of the ring band
    float OuterRadius;
    int Quality;

    RingPoly() : RingWidth(0), OuterRadius(0), Quality(20) {}

    RingPoly(const Vec2& center, float ringWidth, float outerRadius, int quality = 20)
        : RingCenter(center), RingWidth(ringWidth), OuterRadius(outerRadius), Quality(quality)
    {
        UpdatePolygon();
    }

    RingPoly(const Vec3& center, float ringWidth, float outerRadius, int quality = 20)
        : RingPoly(center.To2D(), ringWidth, outerRadius, quality)
    {
    }

    void UpdatePolygon(int offset = 0) {
        Points.clear();

        float outR = (offset + RingWidth + OuterRadius) / cosf(2.0f * (float)M_PI / Quality);
        float inR = RingWidth - OuterRadius - offset;
        if (inR < 0) inR = 0;

        // Outer ring (CW)
        for (int i = 0; i <= Quality; i++) {
            float angle = i * 2.0f * (float)M_PI / Quality;
            Points.push_back(Vec2(
                RingCenter.x - outR * cosf(angle),
                RingCenter.y - outR * sinf(angle)
            ));
        }

        // Inner ring (CCW — creates hole)
        for (int i = 0; i <= Quality; i++) {
            float angle = i * 2.0f * (float)M_PI / Quality;
            Points.push_back(Vec2(
                RingCenter.x + inR * cosf(angle),
                RingCenter.y - inR * sinf(angle)
            ));
        }
    }

    // Simplified IsInside for ring: between inner and outer radius
    bool IsInsideRing(const Vec2& point) const {
        float dist = RingCenter.Distance(point);
        float inner = RingWidth - OuterRadius;
        float outer = RingWidth + OuterRadius;
        return (dist >= inner && dist <= outer);
    }
};

// ============================================================================
// ArcPoly — Arc shape (used for Diana Q)
// ============================================================================
class ArcPoly : public Polygon {
public:
    Vec2 StartPos;
    Vec2 EndDirection;    // Normalized direction from start
    float Angle;          // In radians
    float Radius;
    int Quality;

    ArcPoly() : Angle(0), Radius(0), Quality(20) {}

    ArcPoly(const Vec2& start, const Vec2& end, float angle, float radius, int quality = 20)
        : StartPos(start), Angle(angle), Radius(radius), Quality(quality)
    {
        EndDirection = (end - start).Normalized();
        UpdatePolygon();
    }

    ArcPoly(const Vec3& start, const Vec3& end, float angle, float radius, int quality = 20)
        : ArcPoly(start.To2D(), end.To2D(), angle, radius, quality)
    {
    }

    void UpdatePolygon(int offset = 0) {
        Points.clear();

        float outRadius = (Radius + offset) / cosf(2.0f * (float)M_PI / Quality);
        Vec2 side1 = EndDirection.Rotated(-Angle * 0.5f);

        for (int i = 0; i <= Quality; i++) {
            Vec2 cDir = side1.Rotated(i * Angle / Quality).Normalized();
            Points.push_back(Vec2(
                StartPos.x + outRadius * cDir.x,
                StartPos.y + outRadius * cDir.y
            ));
        }
    }
};

// ============================================================================
// Advanced Geometry Functions
// ============================================================================
namespace GeometryAdv {

    // ---- Segment-Segment Intersection ----
    struct IntersectionResult {
        bool Intersects = false;
        Vec2 Point;
    };

    inline IntersectionResult SegmentIntersection(
        const Vec2& a1, const Vec2& a2,
        const Vec2& b1, const Vec2& b2)
    {
        IntersectionResult result;

        Vec2 r = a2 - a1;
        Vec2 s = b2 - b1;
        float rxs = r.Cross(s);

        if (fabsf(rxs) < 1e-10f) return result; // Parallel

        Vec2 qp = b1 - a1;
        float t = qp.Cross(s) / rxs;
        float u = qp.Cross(r) / rxs;

        if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
            result.Intersects = true;
            result.Point = a1 + r * t;
        }
        return result;
    }

    // ---- Line-Circle Intersection points ----
    inline std::vector<Vec2> LineCircleIntersection(
        const Vec2& lineStart, const Vec2& lineEnd,
        const Vec2& center, float radius)
    {
        std::vector<Vec2> result;

        Vec2 d = lineEnd - lineStart;
        Vec2 f = lineStart - center;

        float a = d.Dot(d);
        float b = 2.0f * f.Dot(d);
        float c = f.Dot(f) - radius * radius;
        float discriminant = b * b - 4 * a * c;

        if (discriminant < 0) return result;

        discriminant = sqrtf(discriminant);

        float t1 = (-b - discriminant) / (2.0f * a);
        float t2 = (-b + discriminant) / (2.0f * a);

        if (t1 >= 0.0f && t1 <= 1.0f)
            result.push_back(lineStart + d * t1);
        if (t2 >= 0.0f && t2 <= 1.0f && fabsf(discriminant) > 1e-6f)
            result.push_back(lineStart + d * t2);

        return result;
    }

    // ---- Path-Line Intersection ----
    // Finds the first intersection of a multi-segment path with a line segment
    inline IntersectionResult PathLineIntersection(
        const std::vector<Vec2>& path,
        const Vec2& lineStart, const Vec2& lineEnd)
    {
        for (size_t i = 0; i + 1 < path.size(); i++) {
            auto r = SegmentIntersection(path[i], path[i + 1], lineStart, lineEnd);
            if (r.Intersects) return r;
        }
        return {};
    }

    // ---- Movement Collision ----
    // Predicts when two moving objects will be at closest approach
    // Returns time (seconds) until collision, or -1 if no collision within maxTime
    inline float MovementCollision(
        const Vec2& posA, const Vec2& velA,
        const Vec2& posB, const Vec2& velB,
        float radiusSum,
        float maxTime = 5.0f)
    {
        Vec2 dp = posB - posA;
        Vec2 dv = velB - velA;

        float a = dv.Dot(dv);
        float b = 2.0f * dp.Dot(dv);
        float c = dp.Dot(dp) - radiusSum * radiusSum;

        // Already overlapping
        if (c <= 0) return 0.0f;

        // Not moving relative to each other
        if (fabsf(a) < 1e-10f) return -1.0f;

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0) return -1.0f;

        float sqrtD = sqrtf(discriminant);
        float t1 = (-b - sqrtD) / (2.0f * a);
        float t2 = (-b + sqrtD) / (2.0f * a);

        float t = (t1 >= 0.0f) ? t1 : t2;
        if (t >= 0.0f && t <= maxTime) return t;
        return -1.0f;
    }

    // ---- Point projection onto line ----
    struct ProjectionResult {
        Vec2 SegmentPoint;     // Closest point on segment
        float LineParameter;   // t parameter [0,1] if on segment
        bool IsOnSegment;
    };

    inline ProjectionResult ProjectOn(
        const Vec2& point,
        const Vec2& segStart, const Vec2& segEnd)
    {
        ProjectionResult result;
        Vec2 seg = segEnd - segStart;
        float segLenSqr = seg.LengthSqr();

        if (segLenSqr < 1e-10f) {
            result.SegmentPoint = segStart;
            result.LineParameter = 0.0f;
            result.IsOnSegment = false;
            return result;
        }

        float t = (point - segStart).Dot(seg) / segLenSqr;
        result.LineParameter = t;
        result.IsOnSegment = (t >= 0.0f && t <= 1.0f);

        float clamped = t;
        if (clamped < 0.0f) clamped = 0.0f;
        if (clamped > 1.0f) clamped = 1.0f;
        result.SegmentPoint = segStart + seg * clamped;

        return result;
    }

    // ---- Convex Hull (Graham scan) ----
    inline std::vector<Vec2> ConvexHull(std::vector<Vec2> points) {
        int n = (int)points.size();
        if (n < 3) return points;

        // Find bottom-most (then left-most) point
        int minIdx = 0;
        for (int i = 1; i < n; i++) {
            if (points[i].y < points[minIdx].y ||
                (points[i].y == points[minIdx].y && points[i].x < points[minIdx].x)) {
                minIdx = i;
            }
        }
        std::swap(points[0], points[minIdx]);
        Vec2 pivot = points[0];

        // Sort by polar angle relative to pivot
        std::sort(points.begin() + 1, points.end(),
            [&pivot](const Vec2& a, const Vec2& b) {
                Vec2 da = a - pivot;
                Vec2 db = b - pivot;
                float cross = da.Cross(db);
                if (fabsf(cross) < 1e-10f)
                    return da.LengthSqr() < db.LengthSqr();
                return cross > 0;
            });

        // Build hull
        std::vector<Vec2> hull;
        hull.push_back(points[0]);
        hull.push_back(points[1]);

        for (int i = 2; i < n; i++) {
            while (hull.size() > 1) {
                Vec2 top = hull.back();
                Vec2 second = hull[hull.size() - 2];
                Vec2 a = top - second;
                Vec2 b = points[i] - second;
                if (a.Cross(b) <= 0)
                    hull.pop_back();
                else
                    break;
            }
            hull.push_back(points[i]);
        }
        return hull;
    }

    // ---- Minimum enclosing circle (Welzl's algorithm, simplified) ----
    struct MinCircle {
        Vec2 Center;
        float Radius;
    };

    inline MinCircle MinEnclosingCircle(const std::vector<Vec2>& points) {
        if (points.empty()) return { Vec2(), 0.0f };
        if (points.size() == 1) return { points[0], 0.0f };

        // Simple O(n^3) brute force for small sets
        // For evade we typically have < 20 points
        Vec2 center;
        float radius = 0.0f;

        // Start with bounding of first two points
        center = (points[0] + points[1]) * 0.5f;
        radius = points[0].Distance(points[1]) * 0.5f;

        for (size_t i = 2; i < points.size(); i++) {
            if (center.Distance(points[i]) <= radius + 1e-6f)
                continue;

            // Point i is outside, must be on boundary
            center = (points[0] + points[i]) * 0.5f;
            radius = points[0].Distance(points[i]) * 0.5f;

            for (size_t j = 1; j < i; j++) {
                if (center.Distance(points[j]) <= radius + 1e-6f)
                    continue;

                center = (points[j] + points[i]) * 0.5f;
                radius = points[j].Distance(points[i]) * 0.5f;

                for (size_t k = 0; k < j; k++) {
                    if (center.Distance(points[k]) <= radius + 1e-6f)
                        continue;

                    // Three points define the circle
                    // Circumcenter of triangle
                    Vec2 A = points[k], B = points[j], C = points[i];
                    float ax = A.x, ay = A.y;
                    float bx = B.x, by = B.y;
                    float cx = C.x, cy = C.y;
                    float D = 2.0f * (ax * (by - cy) + bx * (cy - ay) + cx * (ay - by));
                    if (fabsf(D) < 1e-10f) continue;
                    float ux = ((ax * ax + ay * ay) * (by - cy) +
                                (bx * bx + by * by) * (cy - ay) +
                                (cx * cx + cy * cy) * (ay - by)) / D;
                    float uy = ((ax * ax + ay * ay) * (cx - bx) +
                                (bx * bx + by * by) * (ax - cx) +
                                (cx * cx + cy * cy) * (bx - ax)) / D;
                    center = Vec2(ux, uy);
                    radius = center.Distance(A);
                }
            }
        }
        return { center, radius };
    }

    // ---- Closest pair of points between two polygons ----
    struct ClosestPair {
        Vec2 PointA, PointB;
        float Distance;
    };

    inline ClosestPair ClosestPointsBetween(const Polygon& a, const Polygon& b) {
        ClosestPair result;
        result.Distance = (std::numeric_limits<float>::max)();

        for (auto& pa : a.Points) {
            Vec2 cb = b.ClosestPointOnEdge(pa);
            float d = pa.Distance(cb);
            if (d < result.Distance) {
                result.Distance = d;
                result.PointA = pa;
                result.PointB = cb;
            }
        }
        for (auto& pb : b.Points) {
            Vec2 ca = a.ClosestPointOnEdge(pb);
            float d = pb.Distance(ca);
            if (d < result.Distance) {
                result.Distance = d;
                result.PointA = ca;
                result.PointB = pb;
            }
        }
        return result;
    }

    // ---- Create skillshot polygon from parameters ----
    // Type: 0=Line, 1=Circle, 2=Cone
    inline Polygon CreateSkillshotPolygon(
        int type,
        const Vec2& start,
        const Vec2& end,
        float radius,       // width/2 for line, radius for circle, radius for cone
        float angle = 0.0f, // Only for cone type (radians)
        int quality = 20)
    {
        switch (type) {
        case 0: { // Line
            return RectanglePoly(start, end, radius);
        }
        case 1: { // Circle
            return CirclePoly(end, radius, quality);
        }
        case 2: { // Cone
            return SectorPoly(start, end, angle, radius, quality);
        }
        default:
            return Polygon();
        }
    }

} // namespace GeometryAdv

} // namespace SDK
