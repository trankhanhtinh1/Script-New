#pragma once

#include <algorithm>
#include <cmath>
#include <limits>

namespace ZDPrediction::Math {

inline constexpr double Epsilon = 1e-8;
inline constexpr double Pi = 3.1415926535897932384626433832795;

struct Vector2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vector2() = default;
    constexpr Vector2(double xValue, double yValue) : x(xValue), y(yValue) {}

    constexpr Vector2 operator+(const Vector2& other) const {
        return {x + other.x, y + other.y};
    }

    constexpr Vector2 operator-(const Vector2& other) const {
        return {x - other.x, y - other.y};
    }

    constexpr Vector2 operator*(double scalar) const {
        return {x * scalar, y * scalar};
    }

    Vector2 operator/(double scalar) const {
        return std::abs(scalar) > Epsilon ? Vector2{x / scalar, y / scalar} : Vector2{};
    }

    Vector2& operator+=(const Vector2& other) {
        x += other.x;
        y += other.y;
        return *this;
    }

    Vector2& operator-=(const Vector2& other) {
        x -= other.x;
        y -= other.y;
        return *this;
    }

    constexpr double Dot(const Vector2& other) const {
        return x * other.x + y * other.y;
    }

    constexpr double Cross(const Vector2& other) const {
        return x * other.y - y * other.x;
    }

    constexpr double LengthSquared() const {
        return x * x + y * y;
    }

    double Length() const {
        return std::sqrt(LengthSquared());
    }

    bool IsFinite() const {
        return std::isfinite(x) && std::isfinite(y);
    }

    bool IsZero(double tolerance = Epsilon) const {
        return LengthSquared() <= tolerance * tolerance;
    }

    Vector2 Normalized() const {
        const double length = Length();
        return length > Epsilon ? *this / length : Vector2{};
    }
};

inline constexpr Vector2 operator*(double scalar, const Vector2& vector) {
    return vector * scalar;
}

inline double Clamp(double value, double minimum, double maximum) {
    return std::clamp(value, minimum, maximum);
}

inline double DistanceSquared(const Vector2& left, const Vector2& right) {
    return (left - right).LengthSquared();
}

inline double Distance(const Vector2& left, const Vector2& right) {
    return std::sqrt(DistanceSquared(left, right));
}

inline bool NearlyEqual(const Vector2& left, const Vector2& right, double tolerance = 1e-4) {
    return DistanceSquared(left, right) <= tolerance * tolerance;
}

inline Vector2 Lerp(const Vector2& from, const Vector2& to, double amount) {
    return from + (to - from) * amount;
}

inline Vector2 Rotate(const Vector2& vector, double radians) {
    const double cosine = std::cos(radians);
    const double sine = std::sin(radians);
    return {
        vector.x * cosine - vector.y * sine,
        vector.x * sine + vector.y * cosine
    };
}

inline double AngleBetween(const Vector2& left, const Vector2& right) {
    const double divisor = left.Length() * right.Length();
    if (divisor <= Epsilon) return 0.0;
    return std::acos(Clamp(left.Dot(right) / divisor, -1.0, 1.0));
}

struct SegmentProjection {
    Vector2 point = {};
    double parameter = 0.0;
    bool onSegment = false;
};

inline SegmentProjection ProjectPointOnSegment(const Vector2& point,
                                               const Vector2& start,
                                               const Vector2& end) {
    const Vector2 segment = end - start;
    const double lengthSquared = segment.LengthSquared();
    if (lengthSquared <= Epsilon) return {start, 0.0, false};
    const double raw = (point - start).Dot(segment) / lengthSquared;
    const double parameter = Clamp(raw, 0.0, 1.0);
    return {start + segment * parameter, parameter, raw >= 0.0 && raw <= 1.0};
}

inline double DistanceSquaredToSegment(const Vector2& point,
                                       const Vector2& start,
                                       const Vector2& end) {
    return DistanceSquared(point, ProjectPointOnSegment(point, start, end).point);
}

inline double SafeFinite(double value, double fallback = 0.0) {
    return std::isfinite(value) ? value : fallback;
}

}
