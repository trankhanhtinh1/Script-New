#pragma once
#include <cstdint>
#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2 operator/(float s) const { return { x / s, y / s }; }
    bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vec2& v) const { return !(*this == v); }

    float Length() const { return sqrtf(x * x + y * y); }
    float LengthSqr() const { return x * x + y * y; }
    float Distance(const Vec2& v) const { return (*this - v).Length(); }
    float DistanceSqr(const Vec2& v) const { return (*this - v).LengthSqr(); }

    Vec2 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return { 0, 0 };
        return { x / len, y / len };
    }

    // Extend toward target by distance
    Vec2 Extend(const Vec2& target, float distance) const {
        Vec2 dir = (target - *this).Normalized();
        return *this + dir * distance;
    }

    // Perpendicular (rotated 90° CCW)
    Vec2 Perpendicular() const { return { -y, x }; }

    // Rotate by angle (radians)
    Vec2 Rotated(float angle) const {
        float c = cosf(angle), s = sinf(angle);
        return { x * c - y * s, x * s + y * c };
    }

    // Angle in radians (atan2)
    float Angle() const { return atan2f(y, x); }

    // Angle between two vectors (radians)
    float AngleBetween(const Vec2& other) const {
        float dot = x * other.x + y * other.y;
        float cross = x * other.y - y * other.x;
        return atan2f(cross, dot);
    }

    // Dot product
    float Dot(const Vec2& v) const { return x * v.x + y * v.y; }

    // Cross product (scalar in 2D)
    float Cross(const Vec2& v) const { return x * v.y - y * v.x; }

    bool IsZero() const { return x == 0 && y == 0; }
    bool IsValid() const { return !isnan(x) && !isnan(y); }

    // Alias for DistanceSqr (used by EzEvade Spell.cpp)
    float DistanceSquared(const Vec2& v) const { return DistanceSqr(v); }

    // Rotate this point around another point by angle (radians)
    Vec2 RotateAroundPoint(const Vec2& center, float angle) const {
        float c = cosf(angle), s = sinf(angle);
        Vec2 d = *this - center;
        return Vec2(c * d.x - s * d.y + center.x,
                    s * d.x + c * d.y + center.y);
    }
};

// Projection result — defined after Vec2 is complete
struct Vec2ProjectionInfo {
    Vec2 segmentPoint;     // Closest point on segment
    float lineParameter;   // t parameter [0,1] if on segment
    bool isOnSegment;
};

// ProjectOn — free function (matches C# Vector2Extensions.ProjectOn)
inline Vec2ProjectionInfo Vec2_ProjectOn(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
    Vec2ProjectionInfo result;
    Vec2 seg = segEnd - segStart;
    float segLenSqr = seg.LengthSqr();
    if (segLenSqr < 1e-10f) {
        result.segmentPoint = segStart;
        result.lineParameter = 0.0f;
        result.isOnSegment = false;
        return result;
    }
    float t = (point - segStart).Dot(seg) / segLenSqr;
    result.lineParameter = t;
    result.isOnSegment = (t >= 0.0f && t <= 1.0f);
    float clamped = t;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > 1.0f) clamped = 1.0f;
    result.segmentPoint = segStart + seg * clamped;
    return result;
}

// Segment intersection result
struct Vec2IntersectionResult {
    bool Intersects;
    Vec2 Point;
};

// Segment-segment intersection
inline Vec2IntersectionResult Vec2_SegmentIntersection(const Vec2& a1, const Vec2& b1, const Vec2& a2, const Vec2& b2) {
    Vec2IntersectionResult result = { false, Vec2(0,0) };
    Vec2 d1 = b1 - a1;
    Vec2 d2 = b2 - a2;
    float cross = d1.x * d2.y - d1.y * d2.x;
    if (fabsf(cross) < 1e-10f) return result;
    Vec2 d = a2 - a1;
    float t = (d.x * d2.y - d.y * d2.x) / cross;
    float u = (d.x * d1.y - d.y * d1.x) / cross;
    if (t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f) {
        result.Intersects = true;
        result.Point = a1 + d1 * t;
    }
    return result;
}

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
    bool operator==(const Vec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vec3& v) const { return !(*this == v); }

    float Length() const { return sqrtf(x * x + y * y + z * z); }
    float Length2D() const { return sqrtf(x * x + z * z); }
    float LengthSqr() const { return x * x + y * y + z * z; }
    float LengthSqr2D() const { return x * x + z * z; }

    float Distance(const Vec3& v) const { return (*this - v).Length(); }
    float Distance2D(const Vec3& v) const { return (Vec2(x, z) - Vec2(v.x, v.z)).Length(); }
    float DistanceSqr2D(const Vec3& v) const { return (Vec2(x, z) - Vec2(v.x, v.z)).LengthSqr(); }

    Vec3 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return { 0, 0, 0 };
        return { x / len, y / len, z / len };
    }

    Vec3 Normalized2D() const {
        float len = Length2D();
        if (len < 0.0001f) return { 0, y, 0 };
        return { x / len, y, z / len };
    }

    // Extend toward target by distance (2D — preserves Y)
    Vec3 Extend(const Vec3& target, float distance) const {
        Vec3 dir = target - *this;
        float len = dir.Length2D();
        if (len < 0.0001f) return *this;
        float ratio = distance / len;
        return { x + dir.x * ratio, y, z + dir.z * ratio };
    }

    // Shorten: move away from target
    Vec3 Shorten(const Vec3& target, float distance) const {
        return Extend(target, -distance);
    }

    // Rotate around Y-axis by angle (radians) — for LoL's XZ plane
    Vec3 Rotated(float angle) const {
        float c = cosf(angle), s = sinf(angle);
        return { x * c - z * s, y, x * s + z * c };
    }

    // To Vec2 (XZ plane — game coordinates)
    Vec2 To2D() const { return { x, z }; }

    // From Vec2 (with given Y height)
    static Vec3 From2D(const Vec2& v, float height = 0.0f) { return { v.x, height, v.y }; }

    // Perpendicular in XZ plane
    Vec3 Perpendicular2D() const { return { -z, y, x }; }

    // Dot product
    float Dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }

    // Cross product
    Vec3 Cross(const Vec3& v) const {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }

    // Angle between this and another vector (2D, radians)
    float AngleBetween2D(const Vec3& other) const {
        Vec2 a = To2D().Normalized();
        Vec2 b = other.To2D().Normalized();
        return a.AngleBetween(b);
    }

    bool IsZero() const { return x == 0 && y == 0 && z == 0; }
    bool IsValid() const { return !isnan(x) && !isnan(y) && !isnan(z); }

    // Lerp between two positions
    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }
};

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};

// ============================================================================
// Geometry Helpers
// ============================================================================
namespace Geometry {

    // Distance from point to line segment
    inline float PointToSegmentDistance(const Vec2& point, const Vec2& segA, const Vec2& segB) {
        Vec2 ab = segB - segA;
        Vec2 ap = point - segA;
        float t = ap.Dot(ab) / ab.LengthSqr();
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        Vec2 closest = segA + ab * t;
        return (point - closest).Length();
    }

    // Point on line segment closest to point
    inline Vec2 ClosestPointOnSegment(const Vec2& point, const Vec2& segA, const Vec2& segB) {
        Vec2 ab = segB - segA;
        Vec2 ap = point - segA;
        float t = ap.Dot(ab) / ab.LengthSqr();
        if (t < 0.0f) t = 0.0f;
        if (t > 1.0f) t = 1.0f;
        return segA + ab * t;
    }

    // Line-circle intersection test
    inline bool LineCircleIntersects(const Vec2& lineStart, const Vec2& lineEnd,
                                     const Vec2& center, float radius) {
        return PointToSegmentDistance(center, lineStart, lineEnd) <= radius;
    }

    // Path length from a list of Vec3 waypoints
    inline float PathLength(const std::vector<Vec3>& path) {
        float len = 0.0f;
        for (size_t i = 1; i < path.size(); i++) {
            len += path[i].Distance2D(path[i - 1]);
        }
        return len;
    }

    // Position along path at given distance
    inline Vec3 PositionOnPath(const std::vector<Vec3>& path, float distance) {
        if (path.empty()) return Vec3();
        if (path.size() == 1) return path[0];

        float remaining = distance;
        for (size_t i = 1; i < path.size(); i++) {
            float segLen = path[i].Distance2D(path[i - 1]);
            if (remaining <= segLen) {
                return path[i - 1].Extend(path[i], remaining);
            }
            remaining -= segLen;
        }
        return path.back();
    }

    // Degrees to Radians
    inline float DegToRad(float deg) { return deg * (float)M_PI / 180.0f; }

    // Radians to Degrees
    inline float RadToDeg(float rad) { return rad * 180.0f / (float)M_PI; }
}

// ============================================================================
// SDK::Geometry — Compatibility layer used by EzEvade
// ============================================================================
namespace SDK {
namespace Geometry {
    using ProjectionInfo = Vec2ProjectionInfo;
    using IntersectionResult = Vec2IntersectionResult;

    inline ProjectionInfo ProjectOn(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
        return Vec2_ProjectOn(point, segStart, segEnd);
    }

    inline IntersectionResult SegmentIntersection(const Vec2& a1, const Vec2& b1, const Vec2& a2, const Vec2& b2) {
        return Vec2_SegmentIntersection(a1, b1, a2, b2);
    }

    // Wrappers delegating to global Geometry namespace
    inline float PointToSegmentDistance(const Vec2& point, const Vec2& segA, const Vec2& segB) {
        return ::Geometry::PointToSegmentDistance(point, segA, segB);
    }

    inline Vec2 ClosestPointOnSegment(const Vec2& point, const Vec2& segA, const Vec2& segB) {
        return ::Geometry::ClosestPointOnSegment(point, segA, segB);
    }

    inline bool LineCircleIntersects(const Vec2& lineStart, const Vec2& lineEnd,
                                     const Vec2& center, float radius) {
        return ::Geometry::LineCircleIntersects(lineStart, lineEnd, center, radius);
    }

    inline float PathLength(const std::vector<Vec3>& path) {
        return ::Geometry::PathLength(path);
    }

    inline Vec3 PositionOnPath(const std::vector<Vec3>& path, float distance) {
        return ::Geometry::PositionOnPath(path, distance);
    }
} // namespace Geometry
} // namespace SDK
