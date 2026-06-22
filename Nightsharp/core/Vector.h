#pragma once

#include <cmath>

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

    float Dot(const Vec2& v) const { return x * v.x + y * v.y; }
    float Cross(const Vec2& v) const { return x * v.y - y * v.x; }
    bool IsZero() const { return x == 0.0f && y == 0.0f; }
    bool IsValid() const { return std::isfinite(x) && std::isfinite(y); }
};

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

    Vec3 Extend(const Vec3& target, float distance) const {
        const Vec3 delta = target - *this;
        const float len = delta.Length2D();
        if (len < 0.0001f) {
            return *this;
        }

        const float scale = distance / len;
        return { x + delta.x * scale, y, z + delta.z * scale };
    }

    Vec2 To2D() const { return { x, z }; }
    static Vec3 From2D(const Vec2& v, float height = 0.0f) { return { v.x, height, v.y }; }
    float Dot(const Vec3& v) const { return x * v.x + y * v.y + z * v.z; }
    bool IsZero() const { return x == 0.0f && y == 0.0f && z == 0.0f; }
    bool IsValid() const { return std::isfinite(x) && std::isfinite(y) && std::isfinite(z); }
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
