#pragma once
#include <cstdint>
#include <cmath>

struct Vec2 {
    float x, y;

    Vec2() : x(0), y(0) {}
    Vec2(float x, float y) : x(x), y(y) {}

    Vec2 operator+(const Vec2& v) const { return { x + v.x, y + v.y }; }
    Vec2 operator-(const Vec2& v) const { return { x - v.x, y - v.y }; }
    Vec2 operator*(float s) const { return { x * s, y * s }; }

    float Length() const { return sqrtf(x * x + y * y); }
    float Distance(const Vec2& v) const { return (*this - v).Length(); }
};

struct Vec3 {
    float x, y, z;

    Vec3() : x(0), y(0), z(0) {}
    Vec3(float x, float y, float z) : x(x), y(y), z(z) {}

    Vec3 operator+(const Vec3& v) const { return { x + v.x, y + v.y, z + v.z }; }
    Vec3 operator-(const Vec3& v) const { return { x - v.x, y - v.y, z - v.z }; }
    Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }

    float Length() const { return sqrtf(x * x + y * y + z * z); }
    float Length2D() const { return sqrtf(x * x + z * z); }
    float Distance(const Vec3& v) const { return (*this - v).Length(); }
    float Distance2D(const Vec3& v) const { return (Vec2(x, z) - Vec2(v.x, v.z)).Length(); }

    Vec3 Normalized() const {
        float len = Length();
        if (len < 0.0001f) return { 0, 0, 0 };
        return { x / len, y / len, z / len };
    }

    bool IsZero() const { return x == 0 && y == 0 && z == 0; }
    bool IsValid() const { return !isnan(x) && !isnan(y) && !isnan(z); }
};

struct Vec4 {
    float x, y, z, w;
    Vec4() : x(0), y(0), z(0), w(0) {}
    Vec4(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}
};
