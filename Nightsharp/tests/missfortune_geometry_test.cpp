#include "../plugins/Champion/KuroAIO/AI/Controllers/AIMissFortuneGeometry.h"

#include <cstdlib>
#include <iostream>

using namespace Plugins::KuroAIO::AI::Controllers::MissFortune::Geometry;

void Require(bool value, const char* message) {
    if (!value) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); }
}

int main() {
    Require(BounceConeContains({0, 0}, {500, 0}, {900, 50}),
            "target behind primary should be in bounce cone");
    Require(!BounceConeContains({0, 0}, {500, 0}, {300, 300}),
            "target behind player should not be in bounce cone");
    Require(std::abs(BounceCriticalMultiplier(2.0f) - 1.5f) < 0.001f,
            "a lethal first Q target guarantees the current bounce multiplier");
    Require(ShouldUseBounceRoute(true, true, true),
            "guaranteed critical bounce beats an available direct Q");
    Require(!ShouldUseBounceRoute(true, true, false),
            "ordinary bounce does not replace a clean direct Q");
    Require(ShouldUseBounceRoute(false, true, false),
            "bounce supplies the route when direct Q cannot reach");
    Require(BulletTimeWaves(1) == 14 && BulletTimeWaves(3) == 18,
            "R uses the live rank-scaled wave counts");
    Require(std::abs(BulletTimeRawPerWave(1, 100.0f, 0.0f) - 80.0f) <
                0.001f,
            "R uses live per-wave base and total AD ratio");
    Require(std::abs(MakeItRainRaw(1, 100.0f) - 190.0f) < 0.001f,
            "E uses live full-duration damage instead of one tick");
    Require(MakeItRainSecureFraction(false) <
                MakeItRainSecureFraction(true),
            "E kill secure does not assume full DoT on a mobile target");
    Require(ConservativeBulletTimeHits(1, false) == 6,
            "mobile R target is not credited with a full channel");
    Require(ConservativeBulletTimeHits(1, true) == 9,
            "controlled R target receives a conservative channel estimate");
    Require(DirectionConeContains({0, 0}, {1000, 0}, {900, 100}, 1400, 20),
            "R cone should include aligned target");
    Require(!DirectionConeContains({0, 0}, {1000, 0}, {0, 900}, 1400, 20),
            "R cone should reject side target");

    DoubleUpContext q{};
    q.Direct = q.AttackAvailable = true;
    Require(ShouldCastDoubleUp(q), "instant Q does not wait for an attack");
    q.RecentlyAttacked = true;
    Require(ShouldCastDoubleUp(q), "Q weaves after attack");
    q.ProjectileWall = true;
    Require(!ShouldCastDoubleUp(q), "Q rejects a projectile wall");

    Require(ShouldPrimeBulletTime(false, true, true),
            "E may prime R when both spells are affordable");
    Require(!ShouldPrimeBulletTime(false, true, false),
            "E must not consume R mana");

    Require(ShouldSwapLoveTap(true, true, false, true),
            "Love Tap may switch to reachable unmarked target");
    Require(!ShouldSwapLoveTap(true, true, true, true),
            "Love Tap must not abandon a near kill");

    BulletTimeContext r{};
    r.SafeChannel = r.TargetInCone = true;
    r.TargetsInCone = r.MinimumTargets = 2;
    Require(ShouldStartBulletTime(r), "safe multi-target R should start");
    r.BetterAttack = true;
    Require(!ShouldStartBulletTime(r), "R preserves a better attack");
    r.LethalChannel = true;
    Require(ShouldStartBulletTime(r),
            "lethal R does not wait and lose kill secure");
    r.ProjectileWall = true;
    Require(!ShouldStartBulletTime(r),
            "R never channels into a projectile wall, even when lethal");

    std::cout << "Miss Fortune geometry/state tests passed\n";
    return 0;
}
