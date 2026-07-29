#include "../plugins/Champion/KuroAIO/AI/Controllers/AIEzrealGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::Ezreal::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    MysticShotContext q{};
    q.InRange = q.PredictionHits = true;
    Require(ShouldCastMysticShot(q), "clear Q may spam in attack downtime");
    q.Collision = true;
    Require(!ShouldCastMysticShot(q), "Q must reject a blocked target");
    q.Collision = false;
    q.AttackAvailable = true;
    Require(!ShouldCastMysticShot(q), "Q must preserve an available attack");
    q.AfterAttack = true;
    Require(ShouldCastMysticShot(q), "after-attack Q should cast immediately");

    FluxContext w{};
    w.InRange = w.PredictionHits = w.QCanDetonate = true;
    Require(ShouldCastFlux(w), "W should open a real W-Q chain");
    w.QCanDetonate = false;
    Require(!ShouldCastFlux(w), "W must not be thrown without detonation");

    BlinkContext blink{};
    blink.DestinationSafe = blink.DestinationWalkable = true;
    blink.Lethal = blink.CreatesFollowup = true;
    Require(ShouldBlink(blink), "safe lethal E followup should be allowed");
    blink.EnemiesAtDestination = 2;
    Require(!ShouldBlink(blink), "E must reject a two-enemy landing");

    BarrageContext r{};
    r.Lethal = r.PredictionVeryHigh = true;
    r.Distance = 1600.0f;
    Require(ShouldCastBarrage(r), "isolated lethal R should cast");
    r.LocalEnemyNearby = true;
    Require(!ShouldCastBarrage(r), "R must not channel over a local fight");

    std::cout << "Ezreal geometry/state tests passed\n";
    return 0;
}
