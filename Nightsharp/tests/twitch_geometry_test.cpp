#include "../plugins/Champion/KuroAIO/AI/Controllers/AITwitchGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Twitch::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    Require(VenomStacks(9) == 6, "venom stacks clamp to six");
    Require(ContaminatePhysicalRaw(1, 1, 0.0f) == 35.0f,
            "rank-one one-stack physical E damage");
    Require(ContaminatePhysicalRaw(5, 6, 0.0f) == 270.0f,
            "rank-five six-stack physical E damage");
    Require(std::abs(ContaminateMagicRaw(6, 100.0f) - 210.0f) < 0.01f,
            "six-stack AP portion");
    Require(SprayLineContains({0, 0}, {1000, 0}, {700, 50}, 1200, 60),
            "aligned target belongs to the R piercing line");
    Require(!SprayLineContains({0, 0}, {1000, 0}, {700, 200}, 1200, 60),
            "side target does not add fake R value");

    ContaminateContext e{};
    e.Stacks = 2;
    e.SafeAdditionalAuto = true;
    Require(!ShouldContaminate(e), "E waits while a safe stack is available");
    e.EscapingRange = true;
    e.Stacks = 3;
    Require(ShouldContaminate(e), "E fires before a stacked target leaves range");
    e.Stacks = 6;
    e.EscapingRange = false;
    Require(ShouldContaminate(e), "E fires at six stacks");

    Require(!ShouldAmbush(true, false, true, true, false),
            "Q rejects incoming damage");
    Require(ShouldAmbush(true, false, false, true, false),
            "Q accepts a safe approach window");

    CaskContext w{};
    w.PredictionHits = w.TargetEscaping = true;
    w.ContaminateLethal = true;
    Require(!ShouldThrowCask(w), "W preserves a lethal E");
    w.ContaminateLethal = false;
    w.Flee = w.ProjectileWall = true;
    Require(!ShouldThrowCask(w),
            "W cannot peel through a projectile wall while fleeing");
    Require(ShouldMaintainVenomFocus(3, true, false),
            "orbwalker maintains a reachable venom target");
    Require(!ShouldMaintainVenomFocus(3, true, true),
            "venom focus preserves another immediate kill");

    SprayContext r{};
    r.Ready = r.AttackIntent = r.NeedsBonusRange = true;
    Require(ShouldSprayAndPray(r), "R enables an otherwise unreachable attack");
    r.AlreadyActive = true;
    Require(!ShouldSprayAndPray(r), "R does not recast while active");
    r.AlreadyActive = false;
    r.ProjectileWall = true;
    Require(!ShouldSprayAndPray(r),
            "R does not commit to attacks blocked by a projectile wall");

    std::cout << "Twitch geometry/state tests passed\n";
    return 0;
}
