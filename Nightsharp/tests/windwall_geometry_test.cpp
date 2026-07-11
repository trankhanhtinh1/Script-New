#include <cstdio>
#include <string>

#include "../sdk/Math/WindwallGeometry.h"

using SDK::WindwallGeo::BuildWall;
using SDK::WindwallGeo::IsWindwallName;
using SDK::WindwallGeo::ParseLevel;
using SDK::WindwallGeo::Segment;
using SDK::WindwallGeo::SpanDirection;

static int g_failures = 0;

static void ExpectTrue(const char* name, bool cond) {
    if (!cond) { std::printf("FAIL: %s\n", name); ++g_failures; }
}

static void ExpectNear(const char* name, float a, float b) {
    const float d = a - b;
    if (d > 0.001f || d < -0.001f) {
        std::printf("FAIL: %s expected %.4f got %.4f\n", name, b, a);
        ++g_failures;
    }
}

static void ExpectEqInt(const char* name, int a, int b) {
    if (a != b) { std::printf("FAIL: %s expected %d got %d\n", name, b, a); ++g_failures; }
}

int main() {
    // Name matching (runtime name Yasuo_Base_W_windwallN)
    ExpectTrue("match level5", IsWindwallName("Yasuo_Base_W_windwall5"));
    ExpectTrue("match level1", IsWindwallName("yasuo_base_w_windwall1"));
    ExpectTrue("reject non-yasuo", !IsWindwallName("Malphite_Base_R_impact"));
    ExpectTrue("reject yasuo-non-wall", !IsWindwallName("Yasuo_Base_Q_effect"));

    // Level parsing
    ExpectEqInt("level5", ParseLevel("Yasuo_Base_W_windwall5"), 5);
    ExpectEqInt("level2", ParseLevel("Yasuo_Base_W_windwall2"), 2);
    ExpectEqInt("level default 1", ParseLevel("Yasuo_Base_W_windwall1"), 1);
    ExpectEqInt("level no-suffix", ParseLevel("Yasuo_Base_W_windwall"), 1);

    // SpanDirection: primary from matrix row0 (m[0][0], m[0][2])
    float mat[4][4] = {};
    mat[0][0] = 1.0f; mat[0][2] = 0.0f;
    Vec2 span = SpanDirection(mat, Vec2{0.0f, 1.0f});
    ExpectNear("span primary x", span.x, 1.0f);
    ExpectNear("span primary y", span.y, 0.0f);

    // SpanDirection: degenerate matrix -> perpendicular of fallback forward (0,1) -> (-1,0)
    float zero[4][4] = {};
    Vec2 spanFb = SpanDirection(zero, Vec2{0.0f, 1.0f});
    ExpectNear("span fallback x", spanFb.x, -1.0f);
    ExpectNear("span fallback y", spanFb.y, 0.0f);

    // BuildWall geometry
    Segment seg = BuildWall(Vec2{0.0f, 0.0f}, Vec2{1.0f, 0.0f}, 400.0f);
    ExpectNear("wall start x", seg.start.x, 200.0f);
    ExpectNear("wall end x", seg.end.x, -200.0f);

    if (g_failures == 0) {
        std::printf("ALL WINDWALL GEOMETRY TESTS PASSED\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
