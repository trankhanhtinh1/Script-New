#pragma once

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>

namespace Plugins::KuroEvade::MathUtilsCPA {

inline constexpr float kSmallNum = 0.00000001f;

inline float Dot(const Vec2& u, const Vec2& v) {
    return u.x * v.x + u.y * v.y;
}

inline float Norm(const Vec2& v) {
    return std::sqrt(Dot(v, v));
}

inline float Dist(const Vec2& u, const Vec2& v) {
    return Norm(u - v);
}

inline float CpaTime(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
    const Vec2 dv = v1 - v2;
    const float dv2 = Dot(dv, dv);
    if (dv2 < kSmallNum) {
        return 0.0f;
    }

    const Vec2 w0 = p1 - p2;
    return -Dot(w0, dv) / dv2;
}

inline float GetCollisionTime(const Vec2& pa, const Vec2& pb,
                              const Vec2& va, const Vec2& vb,
                              float ra, float rb, bool& collision) {
    const Vec2 pab = pa - pb;
    const Vec2 vab = va - vb;
    const float a = Dot(vab, vab);
    const float b = 2.0f * Dot(pab, vab);
    const float c = Dot(pab, pab) - (ra + rb) * (ra + rb);
    const float disc = b * b - 4.0f * a * c;

    float t = 0.0f;
    if (disc < 0.0f || a == 0.0f) {
        t = (a != 0.0f) ? (-b / (2.0f * a)) : 0.0f;
        collision = false;
    } else {
        const float sq = std::sqrt(disc);
        const float t0 = (-b + sq) / (2.0f * a);
        const float t1 = (-b - sq) / (2.0f * a);
        if (t0 >= 0.0f && t1 >= 0.0f) {
            t = std::min(t0, t1);
        } else {
            t = std::max(t0, t1);
        }
        collision = t >= 0.0f;
    }

    return std::max(0.0f, t);
}

inline Vec2 ProjectOn(const Vec2& point, const Vec2& a, const Vec2& b, bool& isOnSegment) {
    const Vec2 ab = b - a;
    const float lenSqr = Dot(ab, ab);
    if (lenSqr < kSmallNum) {
        isOnSegment = false;
        return a;
    }

    const float t = Dot(point - a, ab) / lenSqr;
    isOnSegment = (t >= 0.0f && t <= 1.0f);
    const float ct = std::clamp(t, 0.0f, 1.0f);
    return a + ab * ct;
}

inline float CpaPointsEx(const Vec2& p1, const Vec2& v1,
                         const Vec2& p2, const Vec2& v2,
                         Vec2& out1, Vec2& out2) {
    const float ctime = std::max(0.0f, CpaTime(p1, v1, p2, v2));
    out1 = p1 + v1 * ctime;
    out2 = p2 + v2 * ctime;
    return Dist(out1, out2);
}

} // namespace Plugins::KuroEvade::MathUtilsCPA

