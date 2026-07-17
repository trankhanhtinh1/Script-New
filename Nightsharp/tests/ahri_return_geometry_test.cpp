#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAhriGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Ahri::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const Vec3 orb{ 970.0f, 0.0f, 0.0f };
    const Vec3 ahri{ 0.0f, 0.0f, 0.0f };

    const auto center = ProjectPointToSegment2D(
        Vec3{ 500.0f, 0.0f, 0.0f }, orb, ahri);
    Require(std::fabs(center.T - 0.484536f) < 0.001f,
            "projection parameter should identify the crossing point");
    Require(center.Distance < 0.01f, "center-line target must have zero miss distance");

    Require(ReturnHitScore(orb, ahri, Vec3{ 500.0f, 0.0f, 40.0f }, 55.0f) > 0.65f,
            "return Orb should score a centered target highly");
    Require(ReturnHitScore(orb, ahri, Vec3{ 500.0f, 0.0f, 260.0f }, 55.0f) == 0.0f,
            "return Orb must reject a target outside combined radii");

    const Vec3 target{ 520.0f, 0.0f, 220.0f };
    const float current = ReturnHitScore(orb, ahri, target, 55.0f);
    const float redirected = ReturnHitScore(
        orb, Vec3{ 0.0f, 0.0f, 430.0f }, target, 55.0f);
    Require(current == 0.0f && redirected > 0.55f,
            "side dash must be able to turn a miss into a return-Q hit");

    Require(TipDoubleHitScore(
                ahri, orb, Vec3{ 915.0f, 0.0f, 10.0f }, 55.0f) > 0.75f,
            "near-tip target should qualify for the immediate two-pass setup");
    Require(TipDoubleHitScore(
                ahri, orb, Vec3{ 520.0f, 0.0f, 10.0f }, 55.0f) == 0.0f,
            "mid-range target is not a tip double-hit");

    const float seconds = ReturnTravelSecondsToTarget(
        orb, ahri, Vec3{ 485.0f, 0.0f, 0.0f });
    Require(seconds > 0.20f && seconds < 0.28f,
            "return travel estimate should remain in the expected timing band");

    std::cout << "ALL AHRI RETURN GEOMETRY TESTS PASSED\n";
    return 0;
}
