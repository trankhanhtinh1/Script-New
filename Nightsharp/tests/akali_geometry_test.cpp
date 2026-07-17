#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAkaliGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Akali::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const Vec3 source{ 0.0f, 0.0f, 0.0f };
    const Vec3 forward{ 1.0f, 0.0f, 0.0f };

    const ConeHit centered = FivePointHit(
        source, forward, Vec3{ 470.0f, 0.0f, 0.0f }, 35.0f);
    Require(centered.Hits && centered.TipSlow && centered.Score > 0.75f,
            "max-range centered Q must hit and activate the tip-slow model");

    const Vec3 extendedTarget{ 545.0f, 0.0f, 0.0f };
    Require(!FivePointHit(source, forward, extendedTarget, 35.0f).Hits,
            "middle knife must not pretend to reach an extended target");
    const FivePointAim edge = BestFivePointAim(source, extendedTarget, 35.0f);
    Require(edge.Hit.Hits && std::fabs(edge.RotationRadians) > 0.08f,
            "an outer knife should recover a narrow beyond-center Q angle");

    const Vec3 akali{ -300.0f, 0.0f, 0.0f };
    const Vec3 victim{ 0.0f, 0.0f, 0.0f };
    const Vec3 center = PassiveRingCenter(akali, victim);
    Require(std::fabs(center.x + 120.0f) < 0.01f,
            "passive ring center must be offset 120 units toward Akali");
    Require(std::fabs(PassiveExitDistance(akali, center) - 338.0f) < 0.01f,
            "passive coach must expose the remaining ring-exit distance");
    const Vec3 exit = PassiveExitPoint(
        akali, center, Vec3{ -1000.0f, 0.0f, 0.0f });
    Require(exit.x < -630.0f,
            "passive exit point must continue away from ring center");

    const Vec3 backflip = ShurikenBackflipEnd(source, forward);
    Require(std::fabs(backflip.x + 400.0f) < 0.01f,
            "E1 should project Akali 400 units opposite its shuriken");

    const Vec3 r1 = R1LandingPoint(source, Vec3{ 675.0f, 0.0f, 0.0f });
    Require(r1.x >= 824.0f && r1.x <= 826.0f,
            "max-range R1 must pass at least 150 units through its target");

    const Vec3 r2End{ 800.0f, 0.0f, 0.0f };
    Require(DashLineHitScore(
                source, r2End, Vec3{ 430.0f, 0.0f, 20.0f }, 55.0f) > 0.65f,
            "R2 line should score a centered victim highly");
    Require(DashLineHitScore(
                source, r2End, Vec3{ 430.0f, 0.0f, 260.0f }, 55.0f) == 0.0f,
            "R2 must reject a victim outside combined collision radii");

    Require(std::fabs(R2ExecuteMultiplier(100.0f) - 1.0f) < 0.001f,
            "R2 is minimum damage at full health");
    Require(std::fabs(R2ExecuteMultiplier(50.0f) - 2.0f) < 0.001f,
            "R2 doubles at half health");
    Require(std::fabs(R2ExecuteMultiplier(0.0f) - 3.0f) < 0.001f,
            "R2 caps at triple damage at zero health");

    std::cout << "ALL AKALI GEOMETRY TESTS PASSED\n";
    return 0;
}

