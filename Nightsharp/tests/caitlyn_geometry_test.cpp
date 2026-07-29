#include "../plugins/Champion/KuroAIO/AI/Controllers/AICaitlynGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Caitlyn::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    const Vec3 recoil = RecoilPosition({0, 0, 0}, {100, 0, 0});
    Require(recoil.x < -389.0f,
            "net recoil must land opposite the cast direction");

    NetContext net{};
    net.PredictionHits = true;
    net.LandingSafe = true;
    net.TargetStillReachable = true;
    Require(ShouldCastNet(net), "safe net that preserves reach should cast");
    net.AttackAvailable = true;
    Require(!ShouldCastNet(net), "net must not cancel an available auto");
    net.LethalFollowup = true;
    Require(ShouldCastNet(net), "lethal net followup may override auto hold");
    net.LandingSafe = false;
    Require(!ShouldCastNet(net), "net must reject an unsafe recoil landing");

    PeacemakerContext q{};
    q.InRange = q.PredictionHits = true;
    Require(ShouldCastPeacemaker(q), "Q may fire in real attack downtime");
    q.AttackAvailable = true;
    Require(!ShouldCastPeacemaker(q), "Q must preserve a normal attack");
    q.Lethal = true;
    Require(ShouldCastPeacemaker(q), "lethal Q may take the window");

    TrapContext trap{};
    trap.InRange = trap.AmmoReady = trap.Dashing = true;
    Require(ShouldPlaceTrap(trap), "trap should cover a committed dash");
    trap.TrapAlreadyNear = true;
    Require(!ShouldPlaceTrap(trap), "trap should not overlap an existing trap");

    std::cout << "Caitlyn geometry/state tests passed\n";
    return 0;
}
