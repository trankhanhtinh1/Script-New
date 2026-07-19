#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAmumuGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Amumu::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.001f) {
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main() {
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 direction{ 1.0f, 0.0f, 0.0f };

    Require(BandageHits(origin, direction,
                        Vec3{ 900.0f, 0.0f, 120.0f }, 45.0f),
            "Q collision must include target radius around its 80 half-width");
    Require(!BandageHits(origin, direction,
                         Vec3{ 900.0f, 0.0f, 126.0f }, 45.0f),
            "Q collision must reject a unit beyond width plus radius");
    Require(BandageHits(origin, direction,
                        Vec3{ 1160.0f, 0.0f, 0.0f }, 65.0f),
            "Q range must be measured to collision-circle entry, not center");
    Require(!BandageHits(origin, direction,
                         Vec3{ 1250.0f, 0.0f, 0.0f }, 65.0f),
            "Q must reject a collision circle whose entry is beyond 1100");

    std::vector<LineUnit> blockers = {
        { Vec3{ 600.0f, 0.0f, 0.0f }, 35.0f, 10, true },
        { Vec3{ 650.0f, 0.0f, 90.0f }, 120.0f, 20, true },
        { Vec3{ 900.0f, 0.0f, 0.0f }, 65.0f, 30, true },
    };
    Require(FirstBandageCollisionIndex(origin, direction, blockers) == 1,
            "large offset monster can collide before a closer-center minion");
    blockers[1].Valid = false;
    Require(FirstBandageCollisionIndex(origin, direction, blockers) == 0,
            "Q collision ordering must ignore dead or invalid units");

    Require(Near(BandageMissileSeconds(1000.0f), 0.75f),
            "Q impact clock must add 0.25 cast and 2000-speed missile travel");
    const float arrival = BandageArrivalSeconds(
        1000.0f, 855.0f, 55.0f, 65.0f);
    Require(arrival > 1.165f && arrival < 1.168f,
            "Q arrival must include radius-adjusted 1800-speed Amumu dash");

    Require(DespairHits(origin, Vec3{ 390.0f, 0.0f, 0.0f }, 40.0f) &&
                !DespairHits(origin, Vec3{ 391.0f, 0.0f, 0.0f }, 40.0f),
            "W must use 350 effect radius plus gameplay radius");
    Require(TantrumHits(origin, Vec3{ 410.0f, 0.0f, 0.0f }, 60.0f) &&
                !TantrumHits(origin, Vec3{ 411.0f, 0.0f, 0.0f }, 60.0f),
            "E must use 350 effect radius plus gameplay radius");
    Require(CurseHits(origin, Vec3{ 615.0f, 0.0f, 0.0f }, 65.0f) &&
                !CurseHits(origin, Vec3{ 616.0f, 0.0f, 0.0f }, 65.0f),
            "R must use 550 effect radius plus gameplay radius");

    std::vector<UltimateUnit> ultimateUnits = {
        { Vec3{ 300.0f, 0.0f, 0.0f }, 50.0f, 1.0f,
          0.0f, false, false, false, true },
        { Vec3{ 450.0f, 0.0f, 0.0f }, 50.0f, 2.0f,
          0.0f, true, false, false, true },
        { Vec3{ 520.0f, 0.0f, 0.0f }, 50.0f, 1.4f,
          1.2f, false, false, false, true },
        { Vec3{ 540.0f, 0.0f, 0.0f }, 50.0f, 1.0f,
          0.0f, false, false, true, true },
    };
    Require(UltimateHitCount(origin, ultimateUnits) == 3,
            "R hit count must exclude spell-shielded targets by default");
    Require(UltimateHitCount(origin, ultimateUnits, true) == 4,
            "R raw coverage can count spell shields when explicitly requested");
    Require(UltimateScore(origin, ultimateUnits) > 3.2f &&
                UltimateScore(origin, ultimateUnits) < 3.3f,
            "R quality must reward dash denial and penalize shield/CC overlap");

    Require(DespairTickCount(0.0f) == 1 &&
                DespairTickCount(0.49f) == 1 &&
                DespairTickCount(0.50f) == 2,
            "W must tick immediately and every 0.5 seconds");
    Require(Near(DespairPercentMaxHealthPerSecond(1, 100.0f), 0.015f) &&
                Near(DespairPercentMaxHealthPerSecond(5, 200.0f), 0.030f),
            "W current percent-health scaling must include 0.5% per 100 AP");
    Require(Near(DespairRawDamagePerTick(1, 100.0f, 1000.0f), 12.5f),
            "W tick must halve both the 10-per-second base and percent term");
    Require(Near(TantrumRawDamage(5, 100.0f), 235.0f),
            "E must use current 185 plus 50 percent AP at rank five");
    Require(Near(TantrumRemainingCooldown(2.1f, 2), 0.6f) &&
                Near(TantrumRemainingCooldown(1.0f, 2), 0.0f),
            "each incoming basic attack must refund exactly 0.75s of E cooldown");

    Require(Near(UltimateRawDamage(3, 100.0f), 480.0f),
            "R must use current 400 plus 80 percent AP at rank three");
    Require(Near(UltimatePackageRawDamage(3, 100.0f, false), 480.0f) &&
                Near(UltimatePackageRawDamage(3, 100.0f, true), 528.0f),
            "R applies Curse after damage and is amplified only by an old mark");
    Require(ShouldLayerBandage(0.45f, 0.40f, 0.08f) &&
                !ShouldLayerBandage(1.20f, 0.40f, 0.08f),
            "Q2 must be layered near CC expiry instead of overlapping it early");

    Require(ArrivalSafetyScore(2, 2, false, false, 80.0f) > 0.0f &&
                ArrivalSafetyScore(2, 2, true, false, 80.0f) < -1000.0f &&
                ArrivalSafetyScore(2, 2, false, true, 80.0f) < 0.0f,
            "Q arrival safety must veto turret and anti-dash hazards");
    Require(BridgeImprovesReach(
                origin, Vec3{ 900.0f, 0.0f, 0.0f },
                Vec3{ 1750.0f, 0.0f, 0.0f }) &&
                !BridgeImprovesReach(
                    origin, Vec3{ 300.0f, 0.0f, 0.0f },
                    Vec3{ 1750.0f, 0.0f, 0.0f }),
            "Q bridge must create real second-Q reach, not merely spend a charge");

    std::cout << "ALL AMUMU GEOMETRY TESTS PASSED\n";
    return 0;
}
