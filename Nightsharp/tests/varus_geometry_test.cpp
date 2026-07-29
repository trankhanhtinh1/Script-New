#include "../plugins/Champion/KuroAIO/AI/Controllers/AIVarusGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Varus::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

bool Near(float left, float right, float eps = 0.01f) {
    return std::fabs(left - right) <= eps;
}

int main() {
    Require(Near(ChargedQRange(0.0f), 925.0f) &&
            Near(ChargedQRange(1.25f), 1625.0f),
            "Q range must interpolate across the charge window");
    Require(RequiredChargeSeconds(925.0f) == 0.0f &&
            RequiredChargeSeconds(1500.0f) > 1.0f,
            "required charge time must follow target distance");

    Require(!ShouldDetonateBlight(2, true, false, false),
            "two stacks should wait when one safe auto remains");
    Require(ShouldDetonateBlight(2, false, false, false),
            "two stacks should detonate when another auto is unsafe");
    Require(ShouldDetonateBlight(3, true, false, false),
            "three stacks should always be consumed by a ready spell");

    QReleaseContext release{};
    release.Charging = release.PredictionHits = release.InCurrentRange = true;
    release.BlightStacks = 3;
    Require(ShouldReleaseQ(release),
            "charged Q should release immediately at sufficient range and stacks");
    release.ProjectileWall = true;
    Require(!ShouldReleaseQ(release),
            "charged Q must hold instead of firing into a projectile wall");

    Require(ShouldEmpowerQ(true, false, true, true),
            "W should empower a committed low-health Q");
    Require(!ShouldEmpowerQ(true, false, false, true),
            "W must not be spent before a speculative Q");

    std::cout << "Varus geometry/state tests passed\n";
    return 0;
}
