#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAkshanGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Akshan::Geometry;

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
    const Vec3 east{ 1.0f, 0.0f, 0.0f };

    const std::vector<QPathUnit> chain = {
        { Vec3{ 700.0f, 0.0f, 0.0f }, 35.0f, 1, true },
        { Vec3{ 1150.0f, 0.0f, 10.0f }, 35.0f, 2, true },
        { Vec3{ 1600.0f, 0.0f, 0.0f }, 55.0f, 99, true },
    };
    const auto extended = SimulateQOutbound(source, east, chain, 99);
    Require(extended.TargetHit && extended.ExtensionHits == 3,
            "Q must extend sequentially through two minions into the champion");
    Require(extended.Reach > 2100.0f && extended.End.x > 2100.0f,
            "each reached unit must contribute the live 500-unit extension");

    const std::vector<QPathUnit> brokenChain = {
        { Vec3{ 700.0f, 0.0f, 0.0f }, 35.0f, 1, true },
        { Vec3{ 1400.0f, 0.0f, 0.0f }, 55.0f, 99, true },
    };
    const auto missed = SimulateQOutbound(source, east, brokenChain, 99);
    Require(!missed.TargetHit && missed.ExtensionHits == 1,
            "a gap beyond the current extension must terminate the Q chain");

    const auto returnHit = QReturnIntersection(
        Vec3{ 1750.0f, 0.0f, 0.0f },
        Vec3{ 0.0f, 0.0f, 300.0f },
        Vec3{ 900.0f, 0.0f, 150.0f }, 55.0f);
    Require(returnHit.Hits && returnHit.Distance < 6.0f,
            "Q return must home to Akshan's new position, not its cast origin");

    const Vec3 anchor{ 0.0f, 0.0f, 0.0f };
    const Vec3 swingStart{ 400.0f, 0.0f, 0.0f };
    const auto ccw = DirectionFromCursor(
        swingStart, anchor, Vec3{ 400.0f, 0.0f, 500.0f });
    Require(ccw == SwingDirection::CounterClockwise,
            "cursor-side test must choose the documented swing direction");
    const float quarterTurnSeconds =
        (0.5f * kPi * 400.0f) / 1200.0f;
    const Vec3 quarter = SwingPoint(
        anchor, swingStart, ccw, quarterTurnSeconds);
    Require(std::fabs(quarter.x) < 1.0f && quarter.z > 399.0f,
            "1200-speed orbital motion must land on the quarter-turn point");

    const auto approach = ClosestSwingApproach(
        anchor, swingStart, ccw, Vec3{ 0.0f, 0.0f, 420.0f },
        55.0f, 1.0f);
    Require(approach.MinimumDistance < 30.0f && approach.Collides,
            "swing solver must flag a champion collision on the orbit");
    Require(EstimatedSwingShots(1.0f) == 7,
            "one second swing should include five periodic, initial and dismount shots");

    const Vec3 dismount = SwingDismountPoint(
        Vec3{ 0.0f, 0.0f, 400.0f }, Vec3{ 500.0f, 0.0f, 400.0f });
    Require(std::fabs(dismount.x - 350.0f) < 0.01f,
            "E3 landing must clamp to its current 350-unit range");

    const std::vector<RBlocker> blockedLine = {
        { Vec3{ 500.0f, 0.0f, 15.0f }, 35.0f, 7 },
        { Vec3{ 400.0f, 0.0f, 300.0f }, 35.0f, 8 },
    };
    const auto blocker = FirstRBlocker(
        source, Vec3{ 1000.0f, 0.0f, 0.0f }, 55.0f, blockedLine);
    Require(blocker.Blocked && blocker.NetworkId == 7 &&
                blocker.TravelFraction > 0.45f &&
                blocker.TravelFraction < 0.55f,
            "R must stop on the first minion/hero/structure intersecting its line");
    Require(!FirstRBlocker(source, Vec3{ 1000.0f, 0.0f, 0.0f }, 55.0f,
                           { blockedLine[1] }).Blocked,
            "off-axis objects must not be treated as Comeuppance blockers");

    Require(StoredRBullets(1, 0.0f) == 1 &&
                StoredRBullets(1, 2.5f) == 5 &&
                StoredRBullets(3, 2.5f) == 7,
            "R ammo model must ramp from one bullet to the 5/6/7 rank cap");
    Require(std::fabs(RMissingHealthMultiplier(50.0f) - 2.0f) < 0.001f,
            "R should double per-bullet damage at half health");
    Require(std::fabs(RRawDamagePerBullet(1, 100.0f, 0.0f, 2.0f, 100.0f) -
                      40.0f) < 0.001f,
            "rank-one current R base plus 15% total AD must be modeled");
    Require(std::fabs(RRawDamagePerBullet(1, 100.0f, 1.0f, 2.0f, 100.0f) -
                      52.0f) < 0.001f,
            "season-2026 R must use 30% of bonus crit damage scaling");

    std::cout << "ALL AKSHAN GEOMETRY TESTS PASSED\n";
    return 0;
}
