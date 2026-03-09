#pragma once
#include "sdk/SDK.h"
#include <cfloat>
#include <cmath>
#include <tuple>
#include <vector>

namespace EzEvade {
namespace MathUtils {

inline bool CheckLineIntersection(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    auto r = SDK::GeometryAdv::SegmentIntersection(a, b, c, d);
    return r.Intersects;
}

inline std::tuple<float, float> LineToLineIntersection(float x1, float y1, float x2, float y2,
                                                        float x3, float y3, float x4, float y4) {
    const float denom = (y4 - y3) * (x2 - x1) - (x4 - x3) * (y2 - y1);
    if (fabsf(denom) < 1e-10f) {
        return std::make_tuple(FLT_MAX, FLT_MAX);
    }

    const float t1 = ((x4 - x3) * (y1 - y3) - (y4 - y3) * (x1 - x3)) / denom;
    const float t2 = ((x2 - x1) * (y1 - y3) - (y2 - y1) * (x1 - x3)) / denom;
    return std::make_tuple(t1, t2);
}

inline bool CheckLineIntersectionEx(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    const auto [t1, t2] = LineToLineIntersection(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    return t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f;
}

inline Vec2 CheckLineIntersectionEx2(const Vec2& a, const Vec2& b, const Vec2& c, const Vec2& d) {
    const auto [t1, t2] = LineToLineIntersection(a.x, a.y, b.x, b.y, c.x, c.y, d.x, d.y);
    if (t1 >= 0.0f && t1 <= 1.0f && t2 >= 0.0f && t2 <= 1.0f) {
        return Vec2(t1, t2);
    }
    return Vec2();
}

inline Vec2 RotateVector(const Vec2& start, const Vec2& end, float angleDegree) {
    const float angle = angleDegree * (float)M_PI / 180.0f;
    return Vec2(
        cosf(angle) * (end.x - start.x) - sinf(angle) * (end.y - start.y) + start.x,
        sinf(angle) * (end.x - start.x) + cosf(angle) * (end.y - start.y) + start.y
    );
}

inline float VectorMovementCollisionEx(const Vec2& targetPosIn, const Vec2& targetDir, float targetSpeed,
                                       const Vec2& sourcePos, float projSpeed,
                                       bool& collision, float extraDelay = 0.0f, float extraDist = 0.0f) {
    const Vec2 velocity = targetDir * targetSpeed;
    const Vec2 targetPos = targetPosIn - velocity * (extraDelay / 1000.0f);

    const float velocityX = velocity.x;
    const float velocityY = velocity.y;

    const Vec2 relStart = targetPos - sourcePos;
    const float relStartX = relStart.x;
    const float relStartY = relStart.y;

    const float A = velocityX * velocityX + velocityY * velocityY - projSpeed * projSpeed;
    const float B = 2.0f * velocityX * relStartX + 2.0f * velocityY * relStartY;
    const float C = std::max(0.0f, relStartX * relStartX + relStartY * relStartY + extraDist * extraDist);
    const float disc = B * B - 4.0f * A * C;

    if (disc >= 0.0f && fabsf(A) > 1e-10f) {
        const float sqrtDisc = sqrtf(disc);
        const float t1 = -(B + sqrtDisc) / (2.0f * A);
        const float t2 = -(B - sqrtDisc) / (2.0f * A);

        collision = true;
        if (t1 > 0.0f && t2 > 0.0f) return std::min(t1, t2);
        if (t1 > 0.0f) return t1;
        if (t2 > 0.0f) return t2;
    }

    collision = false;
    return 0.0f;
}

inline bool PointOnLineSegment(const Vec2& point, const Vec2& start, const Vec2& end) {
    const float dot = (end - start).Dot(point - start);
    if (dot < 0.0f) return false;

    const float lenSq = start.DistanceSqr(end);
    if (dot > lenSq) return false;
    return true;
}

inline bool IsPointOnLineSegment(const Vec2& point, const Vec2& start, const Vec2& end) {
    return std::max(start.x, end.x) > point.x && point.x > std::min(start.x, end.x)
        && std::max(start.y, end.y) > point.y && point.y > std::min(start.y, end.y);
}

inline float GetCollisionTime(const Vec2& Pa, const Vec2& Pb, const Vec2& Va, const Vec2& Vb,
                              float Ra, float Rb, bool& collision) {
    const Vec2 Pab = Pa - Pb;
    const Vec2 Vab = Va - Vb;
    const float a = Vab.Dot(Vab);
    const float b = 2.0f * Pab.Dot(Vab);
    const float c = Pab.Dot(Pab) - (Ra + Rb) * (Ra + Rb);
    const float disc = b * b - 4.0f * a * c;

    float t = 0.0f;
    if (disc < 0.0f || fabsf(a) < 1e-10f) {
        t = (fabsf(a) < 1e-10f) ? 0.0f : (-b / (2.0f * a));
        collision = false;
    } else {
        const float sqrtDisc = sqrtf(disc);
        const float t0 = (-b + sqrtDisc) / (2.0f * a);
        const float t1 = (-b - sqrtDisc) / (2.0f * a);

        if (t0 >= 0.0f && t1 >= 0.0f) t = std::min(t0, t1);
        else t = std::max(t0, t1);
        collision = t >= 0.0f;
    }

    if (t < 0.0f) t = 0.0f;
    return t;
}

inline float GetCollisionDistanceEx(const Vec2& Pa, const Vec2& Va, float Ra,
                                    const Vec2& Pb, const Vec2& Vb, float Rb,
                                    Vec2& PA, Vec2& PB) {
    bool collision = false;
    const float t = GetCollisionTime(Pa, Pb, Va, Vb, Ra, Rb, collision);
    if (!collision) {
        PA = Vec2();
        PB = Vec2();
        return FLT_MAX;
    }

    PA = Pa + Va * t;
    PB = Pb + Vb * t;
    return PA.Distance(PB);
}

inline float GetCollisionDistance(const Vec2& Pa, const Vec2& PaEnd, const Vec2& Va, float Ra,
                                  const Vec2& Pb, const Vec2& PbEnd, const Vec2& Vb, float Rb) {
    bool collision = false;
    const float t = GetCollisionTime(Pa, Pb, Va, Vb, Ra, Rb, collision);
    if (!collision) return FLT_MAX;

    Vec2 PA = Pa + Va * t;
    Vec2 PB = Pb + Vb * t;

    PA = SDK::GeometryAdv::ProjectOn(PA, Pa, PaEnd).SegmentPoint;
    PB = SDK::GeometryAdv::ProjectOn(PB, Pb, PbEnd).SegmentPoint;
    return PA.Distance(PB);
}

inline int FindLineCircleIntersections(const Vec2& center, float radius,
                                       const Vec2& from, const Vec2& to,
                                       Vec2& intersection1, Vec2& intersection2) {
    const auto intersections = SDK::GeometryAdv::LineCircleIntersection(from, to, center, radius);
    if (intersections.empty()) {
        intersection1 = Vec2(NAN, NAN);
        intersection2 = Vec2(NAN, NAN);
        return 0;
    }

    intersection1 = intersections[0];
    if (intersections.size() == 1) {
        intersection2 = Vec2(NAN, NAN);
        return 1;
    }

    intersection2 = intersections[1];
    return 2;
}

} // namespace MathUtils
} // namespace EzEvade
