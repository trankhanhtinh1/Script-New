#pragma once

// ============================================================================
// WindwallGeometry.h — pure geometry for Yasuo windwall collision.
// No live memory access; unit-testable standalone (tests/windwall_geometry_test.cpp).
// Runtime emitter name verified via CheatEngine: Yasuo_Base_W_windwall<1-5>.
// ============================================================================

#include "../../core/Vector.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>

namespace SDK::WindwallGeo {

inline std::string ToLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

inline bool IsWindwallName(const std::string& name) {
    const std::string lower = ToLower(name);
    return lower.find("yasuo") != std::string::npos &&
           lower.find("_w_windwall") != std::string::npos;
}

inline int ParseLevel(const std::string& name) {
    const std::string lower = ToLower(name);
    for (int level = 5; level >= 2; --level) {
        char suffix[16] = {};
        std::snprintf(suffix, sizeof(suffix), "windwall%d", level);
        if (lower.find(suffix) != std::string::npos) {
            return level;
        }
    }
    return 1;
}

// Span (spread) direction of the wall, normalized.
// Primary: matrix row 0 (m[0][0], m[0][2]) — EnsoulSharp Vector2(Orientation.M11, M13).
// Fallback: perpendicular of the emitter forward vector.
inline Vec2 SpanDirection(const float m[4][4], Vec2 fallbackForward) {
    Vec2 primary{ m[0][0], m[0][2] };
    if (primary.LengthSqr() >= 0.001f) {
        return primary.Normalized();
    }
    if (fallbackForward.LengthSqr() >= 0.001f) {
        const Vec2 perp{ -fallbackForward.y, fallbackForward.x };
        return perp.Normalized();
    }
    return Vec2{ 1.0f, 0.0f };
}

struct Segment {
    Vec2 start;
    Vec2 end;
};

inline Segment BuildWall(Vec2 center, Vec2 spanDir, float width) {
    Segment seg;
    seg.start = center + spanDir * (width * 0.5f);
    seg.end = seg.start - spanDir * width;
    return seg;
}

} // namespace SDK::WindwallGeo
