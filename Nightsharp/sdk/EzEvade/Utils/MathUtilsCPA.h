#pragma once
#include "core/Vector.h"
#include "sdk/EzEvade/Utils/MathUtils.h"
#include <cmath>

namespace EzEvade {
namespace MathUtilsCPA {

struct Line {
    Vec2 P0;
    Vec2 P1;
};

struct Segment {
    Vec2 P0;
    Vec2 P1;
};

struct Track {
    Vec2 P0;
    Vec2 v;
};

inline constexpr float SMALL_NUM = 0.00000001f;

inline float Dot(const Vec2& u, const Vec2& v) {
    return u.Dot(v);
}

inline float Norm(const Vec2& v) {
    return sqrtf(Dot(v, v));
}

inline float Dist(const Vec2& u, const Vec2& v) {
    return Norm(u - v);
}

inline float dist3D_Line_to_Line(const Line& L1, const Line& L2) {
    const Vec2 u = L1.P1 - L1.P0;
    const Vec2 v = L2.P1 - L2.P0;
    const Vec2 w = L1.P0 - L2.P0;
    const float a = Dot(u, u);
    const float b = Dot(u, v);
    const float c = Dot(v, v);
    const float d = Dot(u, w);
    const float e = Dot(v, w);
    const float D = a * c - b * b;

    float sc = 0.0f;
    float tc = 0.0f;

    if (D < SMALL_NUM) {
        sc = 0.0f;
        tc = (b > c ? d / b : e / c);
    } else {
        sc = (b * e - c * d) / D;
        tc = (a * e - b * d) / D;
    }

    const Vec2 dP = w + u * sc - v * tc;
    return Norm(dP);
}

inline float dist3D_Segment_to_Segment(const Segment& S1, const Segment& S2) {
    const Vec2 u = S1.P1 - S1.P0;
    const Vec2 v = S2.P1 - S2.P0;
    const Vec2 w = S1.P0 - S2.P0;
    const float a = Dot(u, u);
    const float b = Dot(u, v);
    const float c = Dot(v, v);
    const float d = Dot(u, w);
    const float e = Dot(v, w);
    const float D = a * c - b * b;

    float sc = 0.0f, sN = 0.0f, sD = D;
    float tc = 0.0f, tN = 0.0f, tD = D;

    if (D < SMALL_NUM) {
        sN = 0.0f;
        sD = 1.0f;
        tN = e;
        tD = c;
    } else {
        sN = (b * e - c * d);
        tN = (a * e - b * d);
        if (sN < 0.0f) {
            sN = 0.0f;
            tN = e;
            tD = c;
        } else if (sN > sD) {
            sN = sD;
            tN = e + b;
            tD = c;
        }
    }

    if (tN < 0.0f) {
        tN = 0.0f;
        if (-d < 0.0f) sN = 0.0f;
        else if (-d > a) sN = sD;
        else { sN = -d; sD = a; }
    } else if (tN > tD) {
        tN = tD;
        if ((-d + b) < 0.0f) sN = 0.0f;
        else if ((-d + b) > a) sN = sD;
        else { sN = (-d + b); sD = a; }
    }

    sc = (fabsf(sN) < SMALL_NUM ? 0.0f : sN / sD);
    tc = (fabsf(tN) < SMALL_NUM ? 0.0f : tN / tD);

    const Vec2 dP = w + u * sc - v * tc;
    return Norm(dP);
}

inline float cpa_time(const Track& Tr1, const Track& Tr2) {
    const Vec2 dv = Tr1.v - Tr2.v;
    const float dv2 = Dot(dv, dv);
    if (dv2 < SMALL_NUM) {
        return 0.0f;
    }

    const Vec2 w0 = Tr1.P0 - Tr2.P0;
    return -Dot(w0, dv) / dv2;
}

inline float cpa_distance(const Track& Tr1, const Track& Tr2) {
    const float ctime = cpa_time(Tr1, Tr2);
    const Vec2 P1 = Tr1.P0 + Tr1.v * ctime;
    const Vec2 P2 = Tr2.P0 + Tr2.v * ctime;
    return Dist(P1, P2);
}

inline float cpa_distance(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
    return cpa_distance(Track{ p1, v1 }, Track{ p2, v2 });
}

inline float CPAPoints(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2,
                       Vec2& ret1, Vec2& ret2) {
    const Track Tr1{ p1, v1 };
    const Track Tr2{ p2, v2 };

    float ctime = cpa_time(Tr1, Tr2);
    Vec2 P1 = Tr1.P0 + Tr1.v * ctime;
    Vec2 P2 = Tr2.P0 + Tr2.v * ctime;

    if (ctime <= 0.0f) {
        P1 = Tr1.P0;
        P2 = Tr2.P0;
    }

    ret1 = P1;
    ret2 = P2;
    return Dist(P1, P2);
}

inline float CPAPointsEx(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2,
                         const Vec2& p1end, const Vec2& p2end) {
    const Track Tr1{ p1, v1 };
    const Track Tr2{ p2, v2 };

    const float ctime = std::max(0.0f, cpa_time(Tr1, Tr2));
    Vec2 P1 = Tr1.P0 + Tr1.v * ctime;
    Vec2 P2 = Tr2.P0 + Tr2.v * ctime;

    P1 = (Dist(p1, P1) > Dist(p1, p1end)) ? p1end : P1;
    P2 = (Dist(p2, P2) > Dist(p2, p2end)) ? p2end : P2;
    return Dist(P1, P2);
}

inline float CPAPointsEx(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2,
                         const Vec2& p1end, const Vec2& p2end,
                         Vec2& p1out, Vec2& p2out) {
    const Track Tr1{ p1, v1 };
    const Track Tr2{ p2, v2 };

    float ctime = cpa_time(Tr1, Tr2);
    if (ctime == 0.0f) {
        bool collision = false;
        const float collisionTime = MathUtils::GetCollisionTime(p1, p2, v1, v2, 10.0f, 10.0f, collision);
        if (collision) {
            ctime = collisionTime;
        }
    }

    const Vec2 P1 = Tr1.P0 + Tr1.v * ctime;
    const Vec2 P2 = Tr2.P0 + Tr2.v * ctime;
    (void)p1end;
    (void)p2end;

    p1out = P1;
    p2out = P2;
    return Dist(P1, P2);
}

inline float CPATime(const Vec2& p1, const Vec2& v1, const Vec2& p2, const Vec2& v2) {
    return cpa_time(Track{ p1, v1 }, Track{ p2, v2 });
}

} // namespace MathUtilsCPA
} // namespace EzEvade

