#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAmbessaGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Ambessa::Geometry;

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
    Require(Near(PassiveEnergyRefund(1), 40.0f) &&
                Near(PassiveEnergyRefund(6), 40.0f) &&
                Near(PassiveEnergyRefund(7), 55.0f) &&
                Near(PassiveEnergyRefund(12), 55.0f) &&
                Near(PassiveEnergyRefund(13), 70.0f),
            "passive energy breakpoints must be levels 7 and 13");
    Require(Near(PassiveBonusRawDamage(1, 100.0f), 30.0f) &&
                Near(PassiveBonusRawDamage(18, 100.0f), 55.0f),
            "passive hit must use current 5-30 plus 25 percent bonus AD");
    Require(Near(PassiveDashSpeed(5, 335.0f), 1105.0f) &&
                Near(PassiveDashSpeed(6, 335.0f), 1165.0f) &&
                Near(PassiveDashSpeed(16, 335.0f), 1285.0f),
            "passive dash speed must honor level 6/11/16 breakpoints");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 direction{ 1.0f, 0.0f, 0.0f };
    const Vec3 noDash = PassiveDashEndpoint(
        origin, Vec3{ 120.0f, 0.0f, 0.0f });
    const Vec3 shortDash = PassiveDashEndpoint(
        origin, Vec3{ 200.0f, 0.0f, 0.0f });
    const Vec3 longDash = PassiveDashEndpoint(
        origin, Vec3{ 900.0f, 0.0f, 0.0f });
    Require(noDash.Distance2D(origin) < 0.001f,
            "orders inside 175 must remain a deliberate no-dash branch");
    Require(Near(shortDash.x, 200.0f) && Near(longDash.x, 350.0f),
            "valid passive dash travel must retain short orders and cap at 350");
    Require(Near(EnergyAfter(200.0f, 3, 2, 6), 70.0f) &&
                Near(EnergyAfter(200.0f, 3, 2, 13), 130.0f),
            "energy planner must apply per-level empowered-AA refunds");
    Require(!CanPaySequentially(130.0f, 3, 0, 6) &&
                CanPaySequentially(130.0f, 3, 2, 6),
            "three-spell chain must require woven refunds below full energy");
    Require(!CanPaySequentially(140.0f, 2, 0, 6, 10.0f) &&
                CanPaySequentially(150.0f, 2, 0, 6, 10.0f),
            "energy planner must preserve a configured safety reserve");

    Require(ClassifyQ1(origin, direction,
                       Vec3{ 200.0f, 0.0f, 0.0f }) == Q1Region::Inner,
            "Q1 inner semicircle must be half damage");
    Require(ClassifyQ1(origin, direction,
                       Vec3{ 350.0f, 0.0f, 0.0f }) == Q1Region::Outer,
            "Q1 blade edge must be classified as full damage");
    Require(ClassifyQ1(origin, direction,
                       Vec3{ -80.0f, 0.0f, 0.0f }, 50.0f) == Q1Region::Miss,
            "Q1 must reject a collision circle fully behind the facing plane");
    Require(ClassifyQ1(origin, direction,
                       Vec3{ 430.0f, 0.0f, 0.0f }, 35.0f) == Q1Region::Outer,
            "Q1 edge contact must include target gameplay radius");
    Require(ClassifyQ1(origin, direction,
                       Vec3{ 460.0f, 0.0f, 0.0f }, 35.0f) == Q1Region::Miss,
            "Q1 must reject targets outside radius plus gameplay radius");
    Require(Q1SweetspotScore(origin, direction,
                             Vec3{ 350.0f, 0.0f, 0.0f }) > 0.90f,
            "Q1 should strongly prefer the center of the blade edge");
    Require(Near(Q1SweetspotScore(origin, direction,
                                  Vec3{ 220.0f, 0.0f, 0.0f }), 0.0f),
            "Q1 body hits must not masquerade as sweetspots");

    Require(Q2Hits(origin, direction,
                   Vec3{ 600.0f, 0.0f, 70.0f }, 35.0f),
            "Q2 line must include target radius around its 40 half-width");
    Require(!Q2Hits(origin, direction,
                    Vec3{ 600.0f, 0.0f, 80.0f }, 35.0f),
            "Q2 line must reject targets beyond width plus gameplay radius");
    std::vector<LineTarget> q2Targets = {
        { Vec3{ 500.0f, 0.0f, 0.0f }, 35.0f, 1, true },
        { Vec3{ 260.0f, 0.0f, 20.0f }, 35.0f, 2, true },
        { Vec3{ 170.0f, 0.0f, 130.0f }, 35.0f, 3, true },
    };
    Require(FirstQ2TargetIndex(origin, direction, q2Targets) == 1,
            "Q2 full damage must belong to the first colliding unit");
    q2Targets[1].Valid = false;
    Require(FirstQ2TargetIndex(origin, direction, q2Targets) == 0,
            "Q2 ordering must skip dead or otherwise invalid blockers");

    std::vector<LineTarget> rTargets = {
        { Vec3{ 420.0f, 0.0f, 0.0f }, 55.0f, 10, true },
        { Vec3{ 980.0f, 0.0f, 30.0f }, 55.0f, 20, true },
        { Vec3{ 1180.0f, 0.0f, 145.0f }, 55.0f, 30, true },
    };
    Require(FarthestRTargetIndex(origin, direction, rTargets) == 1,
            "R must select the farthest champion actually inside its line");
    rTargets.push_back(
        { Vec3{ 1210.0f, 0.0f, 0.0f }, 55.0f, 40, true });
    Require(FarthestRTargetIndex(origin, direction, rTargets) == 3,
            "a rear champion must steal R selection exactly as the live spell does");
    const Vec3 landing = RLandingPoint(
        origin, Vec3{ 800.0f, 0.0f, 0.0f });
    Require(Near(landing.x, 935.0f),
            "R safety model must evaluate Ambessa behind the seized target");

    Require(Near(QPercentMaxHealth(1, 100.0f, false), 0.07f) &&
                Near(QPercentMaxHealth(1, 100.0f, true), 0.08f),
            "26.10 Q health scaling must include 4 percent base and bAD ratio");
    Require(Near(Q1RawDamage(1, 100.0f, 1000.0f, true), 170.0f),
            "Q1 sweetspot must use full current flat, bAD and max-health damage");
    Require(Near(Q1RawDamage(1, 100.0f, 1000.0f, false), 85.0f),
            "Q1 body must halve the complete damage package");
    Require(Near(Q2RawDamage(1, 100.0f, 1000.0f, true), 220.0f) &&
                Near(Q2RawDamage(1, 100.0f, 1000.0f, false), 110.0f),
            "Q2 must double only the first target's complete damage package");
    Require(Near(MonsterPercentHealthDamageCap(1), 100.0f) &&
                Near(MonsterPercentHealthDamageCap(18), 300.0f),
            "Q monster max-health term must retain its level-scaled cap");
    Require(Near(Q1RawDamage(5, 500.0f, 100000.0f, true, true, 18),
                 795.0f),
            "monster Q must cap health damage and add current 75 flat bonus");

    Require(Near(WShieldRaw(1, 100.0f), 200.0f) &&
                Near(WShieldRaw(18, 100.0f), 470.0f),
            "W shield must scale 50-320 plus 150 percent bonus AD");
    Require(Near(WRawDamage(5, 100.0f, false), 200.0f) &&
                Near(WRawDamage(5, 100.0f, true), 300.0f),
            "W damage must gain exactly 50 percent after blocking valid damage");
    Require(Near(ERawDamage(5, 100.0f, 1), 170.0f) &&
                Near(ERawDamage(5, 100.0f, 2), 340.0f),
            "E must count a second hit only after a real passive dash");
    Require(Near(RRawDamage(3, 100.0f), 430.0f),
            "R must use 350 plus 80 percent bonus AD");
    Require(Near(RAbilityHealingRate(1), 0.15f) &&
                Near(RAbilityHealingRate(3, 20.0f), 0.30f),
            "26.10 R passive must use 15/17.5/20 plus half life steal");
    Require(Near(ExpectedAbilityHealing(3, 1000.0f), 200.0f) &&
                Near(ExpectedAbilityHealing(3, 1000.0f, 0.0f, false, true),
                     50.0f),
            "R healing must use only 25 percent effectiveness on monsters");

    std::cout << "ALL AMBESSA GEOMETRY TESTS PASSED\n";
    return 0;
}
