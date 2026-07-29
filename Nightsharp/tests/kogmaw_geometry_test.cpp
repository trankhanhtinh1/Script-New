#include "../plugins/Champion/KuroAIO/AI/Controllers/AIKogMawGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::KogMaw::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    Require(BarrageBonusRange(1) == 130.0f, "W rank one range");
    Require(BarrageBonusRange(5) == 210.0f, "W rank five range");
    Require(ArtilleryRange(1) == 1300.0f, "R rank one range");
    Require(ArtilleryRange(3) == 1800.0f, "R rank three range");

    BarrageContext w{};
    w.Ready = w.TargetValid = w.TargetInEmpoweredRange = w.AttackIntent = true;
    Require(ShouldActivateBarrage(w), "W should create a real attack route");
    w.TargetKillableByAttack = true;
    Require(!ShouldActivateBarrage(w), "W should not delay a lethal attack");
    w.TargetKillableByAttack = false;
    w.TargetInEmpoweredRange = false;
    Require(!ShouldActivateBarrage(w), "W must reject unreachable target");

    SpittleContext q{};
    q.PredictionHits = q.AttackAvailable = true;
    Require(ShouldCastSpittle(q), "instant Q does not wait for an attack");
    q.Lethal = true;
    Require(ShouldCastSpittle(q), "lethal Q remains immediate");
    q.Collision = true;
    Require(!ShouldCastSpittle(q), "Q rejects collision even when lethal");

    ArtilleryContext r{};
    r.PredictionVeryHigh = r.InRange = r.LowHealth = r.Escaping = true;
    r.CostStacks = 2;
    r.MaximumStacks = 2;
    Require(!ShouldCastArtillery(r), "R respects stack cap");
    r.Lethal = true;
    Require(ShouldCastArtillery(r), "lethal R ignores stack cap");
    r.Lethal = false;
    r.CostStacks = 0;
    r.AttackAvailable = true;
    Require(ShouldCastArtillery(r),
            "instant R setup does not wait for an attack");

    std::cout << "KogMaw geometry/state tests passed\n";
    return 0;
}
