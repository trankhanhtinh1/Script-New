#include "../plugins/Champion/KuroAIO/AI/Controllers/AIJhinGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Jhin::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    const Vec3 origin{0, 0, 0};
    const Vec3 forward{1, 0, 0};
    Require(InsideCurtainCone(origin, forward, {2000, 0, 300}),
            "target inside Curtain Call cone should be accepted");
    Require(!InsideCurtainCone(origin, forward, {0, 0, 2000}),
            "side target outside Curtain Call cone should be rejected");

    GrenadeContext q{};
    q.InRange = q.Reloading = true;
    Require(ShouldCastGrenade(q), "Q should fill reload downtime");
    q.Reloading = false;
    q.AttackAvailable = true;
    Require(!ShouldCastGrenade(q), "Q must preserve a ready attack");

    FlourishContext w{};
    w.InRange = w.PredictionHits = w.FirstChampionIsTarget = w.Marked = true;
    Require(ShouldCastFlourish(w), "marked target should receive W root");
    w.FirstChampionIsTarget = false;
    Require(!ShouldCastFlourish(w), "W must not aim through another champion");

    CurtainShotContext shot{};
    shot.InCone = shot.PredictionVeryHigh =
        shot.FirstChampionIsTarget = shot.TargetDamageable = true;
    const float normal = CurtainShotScore(shot);
    shot.Lethal = true;
    Require(CurtainShotScore(shot) > normal + 400.0f,
            "lethal Curtain Call target must be strongly prioritized");

    std::cout << "Jhin geometry/state tests passed\n";
    return 0;
}
