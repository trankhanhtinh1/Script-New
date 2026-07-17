#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAlistarGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Alistar::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main() {
    const float travel = HeadbuttTravelSeconds(650.0f);
    Require(travel > 0.419f && travel < 0.422f,
            "maximum-range W must travel only the exposed radius gap");
    const float speed = HeadbuttCenterSpeed(650.0f);
    Require(speed > 1540.0f && speed < 1550.0f,
            "radius-adjusted maximum-range W center speed must be about 1544");
    Require(BufferedWQImpactMs(650.0f) >= 570 &&
                BufferedWQImpactMs(650.0f) <= 572,
            "buffered Q impact must include W travel and reduced 0.15s Q cast");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 target{ 300.0f, 0.0f, 0.0f };
    const Vec3 full = KnockbackEndpoint(origin, target, false);
    const Vec3 buffered = KnockbackEndpoint(origin, target, true);
    Require(std::fabs(full.x - 1000.0f) < 0.01f,
            "ordinary Headbutt must preserve its full 700 displacement");
    Require(std::fabs(buffered.x - 500.0f) < 0.01f,
            "W-Q buffer must model the reduced 200 displacement");

    const Vec3 wallStop = StopBeforeWall(
        target, full, Vec3{ 650.0f, 0.0f, 0.0f }, 20.0f);
    Require(std::fabs(wallStop.x - 630.0f) < 0.01f,
            "wall pin endpoint must stop before the collision surface");
    Require(TowardPointGain(target, full, Vec3{ 1200.0f, 0.0f, 0.0f }) >
                690.0f,
            "insec score must reward a W that moves toward allied control");
    Require(PeelSeparationGain(
                Vec3{ 0.0f, 0.0f, 0.0f }, target, full) > 690.0f,
            "peel score must reward displacement away from the protected ally");
    Require(PeelSeparationGain(
                Vec3{ 1500.0f, 0.0f, 0.0f }, target, full) < -690.0f,
            "peel score must reject knocking a diver into the carry");

    Require(PulverizeHits(origin, Vec3{ 420.0f, 0.0f, 0.0f }, 50.0f),
            "Q should include target gameplay radius at its edge");
    Require(!PulverizeHits(origin, Vec3{ 440.0f, 0.0f, 0.0f }, 50.0f),
            "Q must reject a target beyond effect plus gameplay radius");
    Require(PulverizeHitCount(
                origin,
                { Vec3{ 100.0f, 0.0f, 0.0f },
                  Vec3{ 250.0f, 0.0f, 0.0f },
                  Vec3{ 500.0f, 0.0f, 0.0f } }) == 2,
            "AoE Q count must use the current 375 radius");

    Require(TramplePulseCount(0.0f) == 1 &&
                TramplePulseCount(0.49f) == 1 &&
                TramplePulseCount(0.50f) == 2 &&
                TramplePulseCount(5.0f) == 10,
            "E must pulse immediately then every 0.5 seconds, capped at ten");
    Require(TrampleStacksFromContinuousContact(2.0f) == 5,
            "continuous champion contact must reach five stacks at pulse five");
    Require(MillisecondsToNextTramplePulse(1750) == 250,
            "next-pulse clock must align to the 500ms cadence");
    Require(FourStackAttackWillStun(4, 180, 220, true),
            "four-stack AA should prime when pulse five precedes impact");
    Require(!FourStackAttackWillStun(4, 240, 180, true),
            "four-stack AA must wait when it would impact before pulse five");
    Require(!FourStackAttackWillStun(4, 100, 220, false),
            "four-stack AA must not assume a target remains in E radius");
    Require(std::fabs(EmpoweredTrampleRawDamage(1) - 20.0f) < 0.001f &&
                std::fabs(EmpoweredTrampleRawDamage(18) - 275.0f) < 0.001f,
            "current empowered E must scale 20 to 275 by character level");

    Require(std::fabs(UltimateDamageReduction(1) - 0.55f) < 0.001f &&
                std::fabs(UltimateDamageReduction(3) - 0.75f) < 0.001f,
            "R must use current 55/65/75 percent mitigation");
    Require(std::fabs(DamageTakenWithUltimate(3, 1000.0f, 200.0f) -
                      450.0f) < 0.001f,
            "R must reduce mitigable damage while leaving true damage intact");

    const Vec3 average = AveragePoint({
        Vec3{ 0.0f, 0.0f, 0.0f },
        Vec3{ 100.0f, 0.0f, 100.0f },
        Vec3{ 200.0f, 0.0f, 200.0f },
    });
    Require(std::fabs(average.x - 100.0f) < 0.001f &&
                std::fabs(average.z - 100.0f) < 0.001f,
            "allied centroid must be deterministic for insec direction");

    std::cout << "ALL ALISTAR GEOMETRY TESTS PASSED\n";
    return 0;
}
