#pragma once

#include <cmath>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Vec2 {
    float x = 0.0f;
    float y = 0.0f;

    Vec2() = default;
    Vec2(float xValue, float yValue) : x(xValue), y(yValue) {}

    Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }
    Vec2 operator/(float s) const { return { x / s, y / s }; }
    bool operator==(const Vec2& v) const { return x == v.x && y == v.y; }
    bool operator!=(const Vec2& v) const { return !(*this == v); }

    float Length() const { return std::sqrt(x * x + y * y); }
    float LengthSqr() const { return x * x + y * y; }
    float Distance(const Vec2& v) const { return (*this - v).Length(); }
    float DistanceSqr(const Vec2& v) const { return (*this - v).LengthSqr(); }
    float DistanceSquared(const Vec2& v) const { return DistanceSqr(v); }

    Vec2 Normalized() const {
        const float len = Length();
        if (len < 0.0001f) {
            return {};
        }
        return { x / len, y / len };
    }

    Vec2 Extend(const Vec2& target, float distance) const {
        return *this + (target - *this).Normalized() * distance;
    }

    Vec2 Perpendicular() const { return { -y, x }; }

    Vec2 Rotated(float angle) const {
        float c = std::cos(angle), s = std::sin(angle);
        return { x * c - y * s, x * s + y * c };
    }

    float Angle() const { return std::atan2(y, x); }

    float AngleBetween(const Vec2& other) const {
        return std::atan2(Cross(other), Dot(other));
    }

    Vec2 RotateAroundPoint(const Vec2& center, float angle) const {
        float c = std::cos(angle), s = std::sin(angle);
        Vec2 d = *this - center;
        return { c * d.x - s * d.y + center.x, s * d.x + c * d.y + center.y };
    }

    float Dot(const Vec2& v) const { return x * v.x + y * v.y; }
    float Cross(const Vec2& v) const { return x * v.y - y * v.x; }
    bool IsZero() const { return x == 0.0f && y == 0.0f; }
    bool IsValid() const { return std::isfinite(x) && std::isfinite(y); }
};

struct Vec2ProjectionInfo {
    Vec2 linePoint{};
    Vec2 segmentPoint{};
    float lineParameter = 0.0f;
    bool isOnSegment = false;
};

inline Vec2ProjectionInfo Vec2_ProjectOn(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
    Vec2ProjectionInfo result{};
    Vec2 seg = segEnd - segStart;
    float segLenSqr = seg.LengthSqr();
    if (segLenSqr < 1e-10f) {
        result.linePoint = segStart;
        result.segmentPoint = segStart;
        result.lineParameter = 0.0f;
        result.isOnSegment = false;
        return result;
    }
    float t = (point - segStart).Dot(seg) / segLenSqr;
    result.lineParameter = t;
    result.isOnSegment = (t >= 0.0f && t <= 1.0f);
    result.linePoint = segStart + seg * t;
    float clamped = t;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > 1.0f) clamped = 1.0f;
    result.segmentPoint = segStart + seg * clamped;
    return result;
}

struct Vec2IntersectionResult {
    bool Intersects = false;
    Vec2 Point{};
};

inline Vec2IntersectionResult Vec2_SegmentIntersection(const Vec2& a1, const Vec2& b1, const Vec2& a2, const Vec2& b2) {
    Vec2IntersectionResult result{};
    Vec2 d1 = b1 - a1;
    Vec2 d2 = b2 - a2;
    float cross = d1.x * d2.y - d1.y * d2.x;
    if (std::fabs(cross) < 1e-10f) return result;
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
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    Vec3() = default;
    Vec3(float xValue, float yValue, float zValue) : x(xValue), y(yValue), z(zValue) {}

    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
    Vec3 operator/(float s) const { return { x / s, y / s, z / s }; }
    bool operator==(const Vec3& v) const { return x == v.x && y == v.y && z == v.z; }
    bool operator!=(const Vec3& v) const { return !(*this == v); }

    float Length() const { return std::sqrt(x * x + y * y + z * z); }
    float Length2D() const { return std::sqrt(x * x + z * z); }
    float LengthSqr() const { return x * x + y * y + z * z; }
    float LengthSqr2D() const { return x * x + z * z; }
    float Distance(const Vec3& v) const { return (*this - v).Length(); }
    float Distance2D(const Vec3& v) const { return Vec2(x, z).Distance(Vec2(v.x, v.z)); }
    float DistanceSqr2D(const Vec3& v) const { return Vec2(x, z).DistanceSqr(Vec2(v.x, v.z)); }

    Vec3 Normalized() const {
        const float len = Length();
        if (len < 0.0001f) {
            return {};
        }
        return { x / len, y / len, z / len };
    }

    Vec3 Normalized2D() const {
        float len = Length2D();
        if (len < 0.0001f) return { 0.0f, y, 0.0f };
        return { x / len, y, z / len };
    }

    Vec3 Extend(const Vec3& target, float distance) const {
        const Vec3 delta = target - *this;
        const float len = delta.Length2D();
        if (len < 0.0001f) {
            return *this;
        }

        const float scale = distance / len;
        return { x + delta.x * scale, y, z + delta.z * scale };
    }

    Vec3 Shorten(const Vec3& target, float distance) const {
        return Extend(target, -distance);
    }

    Vec3 Rotated(float angle) const {
        float c = std::cos(angle), s = std::sin(angle);
        return { x * c - z * s, y, x * s + z * c };
    }

    Vec2 To2D() const { return { x, z }; }
    static Vec3 From2D(const Vec2& v, float height = 0.0f) { return { v.x, height, v.y }; }

    Vec3 Perpendicular2D() const { return { -z, y, x }; }

    float Dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }

    Vec3 Cross(const Vec3& v) const {
        return { y * v.z - z * v.y, z * v.x - x * v.z, x * v.y - y * v.x };
    }

    float AngleBetween2D(const Vec3& other) const {
        Vec2 a = To2D().Normalized();
        Vec2 b = other.To2D().Normalized();
        return a.AngleBetween(b);
    }

    bool IsZero() const { return x == 0.0f && y == 0.0f && z == 0.0f; }
    bool IsValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }

    static Vec3 Lerp(const Vec3& a, const Vec3& b, float t) {
        return a + (b - a) * t;
    }
};

struct Vec4 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
    float w = 0.0f;

    Vec4() = default;
    Vec4(float xValue, float yValue, float zValue, float wValue)
        : x(xValue), y(yValue), z(zValue), w(wValue) {}
};

namespace Geometry {

inline float PointToSegmentDistance(const Vec2& point, const Vec2& segA, const Vec2& segB) {
    Vec2 ab = segB - segA;
    Vec2 ap = point - segA;
    float t = ap.Dot(ab) / ab.LengthSqr();
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    Vec2 closest = segA + ab * t;
    return (point - closest).Length();
}

inline Vec2 ClosestPointOnSegment(const Vec2& point, const Vec2& segA, const Vec2& segB) {
    Vec2 ab = segB - segA;
    Vec2 ap = point - segA;
    float t = ap.Dot(ab) / ab.LengthSqr();
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return segA + ab * t;
}

inline bool LineCircleIntersects(const Vec2& lineStart, const Vec2& lineEnd,
                                  const Vec2& center, float radius) {
    return PointToSegmentDistance(center, lineStart, lineEnd) <= radius;
}

inline float PathLength(const std::vector<Vec3>& path) {
    float len = 0.0f;
    for (size_t i = 1; i < path.size(); i++) {
        len += path[i].Distance2D(path[i - 1]);
    }
    return len;
}

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

inline float DegToRad(float deg) { return deg * (float)M_PI / 180.0f; }
inline float RadToDeg(float rad) { return rad * 180.0f / (float)M_PI; }

} // namespace Geometry

namespace SDK::Geometry {
using ProjectionInfo = Vec2ProjectionInfo;
using IntersectionResult = Vec2IntersectionResult;

inline ProjectionInfo ProjectOn(const Vec2& point, const Vec2& segStart, const Vec2& segEnd) {
    return Vec2_ProjectOn(point, segStart, segEnd);
}

inline IntersectionResult SegmentIntersection(const Vec2& a1, const Vec2& b1, const Vec2& a2, const Vec2& b2) {
    return Vec2_SegmentIntersection(a1, b1, a2, b2);
}

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

inline float DegToRad(float deg) { return ::Geometry::DegToRad(deg); }
inline float RadToDeg(float rad) { return ::Geometry::RadToDeg(rad); }
} // namespace SDK::Geometry
