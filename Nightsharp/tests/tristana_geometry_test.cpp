#include "../plugins/Champion/KuroAIO/AI/Controllers/AITristanaGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Tristana::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    Require(DynamicTargetedRange(600.0f) == 600.0f,
            "E/R range follows attack range");
    Require(ChargeStacks(8) == 4, "E stacks clamp to four");
    Require(!WillDetonateCharge(2, 1),
            "W/R cannot fake a detonation from two stacks");
    Require(WillDetonateCharge(3, 1),
            "W/R final stack triggers the E detonation");
    Require(ExplosiveChargeRaw(1, 0, 0.0f, 0.0f, 0.0f, 2.0f) == 60.0f,
            "rank-one zero-stack E base damage");
    Require(ExplosiveChargeRaw(1, 4, 0.0f, 0.0f, 0.0f, 2.0f) == 120.0f,
            "four E stacks double the active damage");
    Require(ShouldFocusCharge(true, 0, true, false),
            "active zero-stack E is still a focus candidate");
    Require(ShouldFocusCharge(true, 2, true, false),
            "orbwalker should focus a reachable partial E");
    Require(!ShouldFocusCharge(true, 4, true, false),
            "full E no longer needs stacking focus");
    Require(!ShouldFocusCharge(true, 2, true, true),
            "focus must preserve a different immediate attack kill");
    Require(!ShouldCastExplosiveCharge(true, true, true, false, true),
            "E rejects a target behind a projectile wall");

    BusterContext r{};
    r.InRange = r.DetonationLethal = true;
    Require(ShouldCastBusterShot(r), "R may trigger lethal E detonation");
    r.AttackAvailable = true;
    Require(ShouldCastBusterShot(r),
            "lethal instant R does not wait for an attack");
    r.Gapcloser = true;
    Require(ShouldCastBusterShot(r), "R peels a gapcloser immediately");
    r.ProjectileWall = true;
    Require(!ShouldCastBusterShot(r),
            "R cannot peel or execute through a projectile wall");

    JumpContext w{};
    w.PredictionHits = w.LandingSafe = w.Lethal = true;
    Require(ShouldRocketJump(w), "safe lethal W may jump");
    w.EnemiesAtLanding = 2;
    w.MaximumEnemies = 1;
    Require(!ShouldRocketJump(w), "W rejects excess enemies at landing");

    std::cout << "Tristana geometry/state tests passed\n";
    return 0;
}
