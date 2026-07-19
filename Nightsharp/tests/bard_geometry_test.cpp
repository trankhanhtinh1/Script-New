#include "../plugins/Champion/KuroAIO/AI/Controllers/AIBardGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Bard::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.03f) {
    return std::fabs(left - right) <= epsilon;
}

QUnit Enemy(int id, float x, float z = 0.0f, float radius = 40.0f) {
    QUnit unit{};
    unit.Position = unit.PredictedPosition = Vec3{ x, 0.0f, z };
    unit.Radius = radius;
    unit.Priority = 1.0f;
    unit.Id = id;
    unit.Hostile = true;
    unit.Champion = true;
    unit.Targetable = unit.Valid = true;
    return unit;
}

StasisUnit StasisChampion(int id,
                          TeamRelation team,
                          float x,
                          float z = 0.0f) {
    StasisUnit unit{};
    unit.Position = unit.PredictedPosition = Vec3{ x, 0.0f, z };
    unit.Radius = 65.0f;
    unit.Priority = 1.5f;
    unit.Id = id;
    unit.Team = team;
    unit.Champion = unit.Valid = true;
    return unit;
}

} // namespace

int main() {
    Require(Near(QRawDamage(1, 100.0f), 160.0f) &&
                Near(QRawDamage(5, 100.0f), 320.0f),
            "Q must use current 80-240 base and 80 percent AP");
    Require(Near(QDisableSeconds(1), 1.0f) &&
                Near(QDisableSeconds(5), 1.8f),
            "Q slow and stun duration must scale from 1 to 1.8 seconds");
    Require(Near(WMinimumRawHeal(5, 100.0f), 165.0f) &&
                Near(WMaximumRawHeal(5, 100.0f), 270.0f),
            "W must use Riot 25.21 40/70 percent AP ratios");
    Require(Near(WShrineRawHeal(5, 100.0f, 2.5f), 217.5f) &&
                Near(WMovementSpeedPercent(5, 100.0f), 36.0f),
            "W must interpolate over five seconds and use six percent per 100 AP");

    const MeepSnapshot five = MeepState(5, 100.0f);
    const MeepSnapshot thirtyFive = MeepState(35, 100.0f);
    const MeepSnapshot hundred = MeepState(100, 100.0f);
    Require(Near(five.RawMagicDamage, 76.0f) &&
                Near(five.SlowPercent, 0.25f),
            "26.13 meep damage must be 30 plus six per checkpoint plus 40 percent AP");
    Require(thirtyFive.MaximumMeeps == 3 &&
                thirtyFive.HasSplash && thirtyFive.HasExpandedSplash &&
                Near(thirtyFive.RechargeSeconds, 7.0f),
            "35 chimes must unlock the enlarged splash with three meeps");
    Require(hundred.MaximumMeeps == 9 &&
                Near(hundred.RechargeSeconds, 4.0f) &&
                Near(hundred.SlowPercent, 0.75f),
            "late meep count, recharge and slow breakpoints must remain exact");
    Require(Near(ChimeExperience(4.0f), 20.0f) &&
                Near(ChimeExperience(12.9f), 27.0f),
            "chime experience must gain one per minute after minute five");

    ChimeRouteContext route{};
    route.DirectPathDistance = 1000.0f;
    route.ChimePathDistance = 1060.0f;
    route.AllyLaneSafety = 0.9f;
    route.OnPrimaryRoute = true;
    const float safeRoute = ChimeRouteScore(route);
    route.CarryCanFarmSafely = false;
    Require(safeRoute > 300.0f && ChimeRouteScore(route) < 0.0f,
            "chimes must remain secondary to carry safety");

    const Vec3 bard{ 0.0f, 0.0f, 0.0f };
    QUnit first = Enemy(1, 400.0f);
    QEvaluation single = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f }, { first }, {}, first.Id);
    Require(single.Valid && single.FirstId == first.Id &&
                single.FirstSlowed && !single.FirstStunned &&
                single.SecondId == 0,
            "a one-body Q must damage and slow without inventing a stun");

    QUnit second = Enemy(2, 610.0f);
    QEvaluation pair = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f }, { first, second }, {}, first.Id);
    Require(pair.Valid && pair.FirstId == 1 && pair.SecondId == 2 &&
                pair.FirstStunned && pair.SecondStunned &&
                pair.SecondDamaged,
            "Q must stun and damage both ordered enemy bodies");

    QUnit blocker = Enemy(3, 215.0f);
    blocker.Champion = false;
    blocker.Minion = true;
    QEvaluation blocked = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { first, second, blocker }, {}, first.Id);
    Require(!blocked.Valid && blocked.FirstId == blocker.Id,
            "an earlier minion must invalidate a champion-first Q plan");

    QEvaluation wall = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { first }, { Vec3{ 550.0f, 0.0f, 0.0f } }, first.Id);
    Require(wall.Valid && wall.WallSecond && wall.FirstStunned &&
                Near(wall.SecondEntryDistance, 550.0f),
            "terrain inside the 300-unit continuation must stun the first body");
    QEvaluation earlyWall = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { first }, { Vec3{ 150.0f, 0.0f, 0.0f } }, first.Id);
    Require(earlyWall.Valid && !earlyWall.WallSecond &&
                !earlyWall.FirstStunned,
            "terrain before the first body must not create a fake Q shackle");

    QUnit sideFirst = Enemy(4, 400.0f, 90.0f);
    QUnit distantSecond = Enemy(5, 730.0f, 0.0f);
    QEvaluation centerLine = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { first, distantSecond }, {}, first.Id);
    QEvaluation sideLine = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { sideFirst, distantSecond }, {}, sideFirst.Id);
    Require(centerLine.SecondId == 0 && sideLine.SecondId == distantSecond.Id,
            "a side hit must start Q2 later and extend the practical shackle reach");

    QUnit shieldedFirst = first;
    shieldedFirst.SpellShield = true;
    QEvaluation shieldWall = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { shieldedFirst }, { Vec3{ 540.0f, 0.0f, 0.0f } }, first.Id);
    Require(shieldWall.Valid && !shieldWall.FirstDamaged &&
                shieldWall.FirstStunned,
            "a first-target spell shield must block damage but not wall shackle");
    QUnit shieldedSecond = second;
    shieldedSecond.SpellShield = true;
    QEvaluation secondShield = EvaluateCosmicBinding(
        bard, Vec3{ 850.0f, 0.0f, 0.0f },
        { first, shieldedSecond }, {}, first.Id);
    Require(secondShield.FirstStunned && !secondShield.SecondStunned &&
                !secondShield.SecondDamaged,
            "a second-target spell shield must not protect the first target");

    const Vec3 pairAim = PairAlignmentAim(
        bard, first.Position, Vec3{ 600.0f, 0.0f, 30.0f });
    Require(pairAim.IsValid() && Near(bard.Distance2D(pairAim), 850.0f),
            "pair alignment must return a normalized full-range aim");
    Require(ShouldWaitForMeepBeforeQ(true, true, false, false, false) &&
                !ShouldWaitForMeepBeforeQ(true, true, true, false, false),
            "ordinary Q should wait for meep slow but immediate stun should not");

    std::vector<Shrine> shrines = {
        { Vec3{ 250.0f, 0.0f, 0.0f }, 10, 5.0f, true, true },
    };
    Require(!CanPlaceGroundShrine(
                bard, Vec3{ 300.0f, 0.0f, 0.0f }, shrines, 2, false, false) &&
                CanPlaceGroundShrine(
                    bard, Vec3{ 500.0f, 0.0f, 0.0f }, shrines, 2, false, false),
            "W placement must reject overlap while accepting a spaced shrine");
    Require(!CanPlaceGroundShrine(
                bard, Vec3{ 500.0f, 0.0f, 0.0f }, shrines, 1, false, true),
            "the last W charge must remain reservable for emergency healing");

    EmergencyHealContext heal{};
    heal.HealthPercent = 20.0f;
    heal.CurrentHealth = 300.0f;
    heal.IncomingDamage = 400.0f;
    heal.Distance = 600.0f;
    heal.Priority = 2.0f;
    heal.Targeted = true;
    Require(EmergencyHealScore(heal, 150.0f) > 1000.0f,
            "a targeted lethal ally must outrank routine shrine placement");

    const auto wallProbe = [](const Vec3& point) {
        return point.x >= 100.0f && point.x <= 600.0f;
    };
    const PortalTrace portal = TracePortal(
        bard, Vec3{ 300.0f, 0.0f, 0.0f }, wallProbe, 10.0f);
    Require(portal.Valid && portal.TerrainLength >= 500.0f &&
                portal.Exit.x > 620.0f &&
                portal.AllyTravelSeconds < portal.EnemyTravelSeconds,
            "E must trace a one-way terrain corridor and preserve ally speed advantage");
    const PortalTrace noTerrain = TracePortal(
        bard, Vec3{ 300.0f, 0.0f, 100.0f },
        [](const Vec3&) { return false; });
    Require(!noTerrain.Valid,
            "E must never manufacture a portal without terrain");

    PortalSafetyContext safePortal{};
    safePortal.AlliesAtExit = 1;
    safePortal.EnemiesAtExit = 0;
    safePortal.ThreatSeparationGain = 700.0f;
    safePortal.AllyRequestedDirection = true;
    Require(PortalSafetyScore(portal, safePortal, true) > 700.0f,
            "a cursor-agreed defensive portal with separation must score highly");
    safePortal.ExitTerrain = true;
    Require(PortalSafetyScore(portal, safePortal, true) == -FLT_MAX,
            "a corrupt portal exit must be rejected absolutely");

    Require(Near(RTravelSeconds(0.0f), 0.65f) &&
                Near(RTravelSeconds(3400.0f), 1.80f) &&
                Near(RImpactSeconds(3400.0f), 2.30f),
            "R prediction must retain observed point-blank and max-range timings");
    Require(Near(QCastDelayForStasisExit(2.5f, 750.0f), 1.71f),
            "R-Q scheduling must subtract Q cast and missile flight from stasis");

    StasisContext catchContext{};
    catchContext.Catch = true;
    StasisUnit enemyA = StasisChampion(
        20, TeamRelation::Enemy, 800.0f);
    REvaluation catchPlan = EvaluateTemperedFate(
        bard, Vec3{ 800.0f, 0.0f, 0.0f }, { enemyA }, catchContext);
    Require(catchPlan.Valid && catchPlan.EnemyChampions == 1 &&
                catchPlan.FriendlyGrief == 0,
            "R catch must accept a clean isolated enemy");

    enemyA.CurrentAllyFocus = true;
    StasisUnit allyA = StasisChampion(
        21, TeamRelation::Ally, 820.0f);
    REvaluation grief = EvaluateTemperedFate(
        bard, Vec3{ 800.0f, 0.0f, 0.0f },
        { enemyA, allyA }, catchContext);
    Require(!grief.Valid && grief.FriendlyGrief == 1,
            "R must reject freezing an ally and the enemy they are bursting");

    StasisContext saveContext{};
    saveContext.SaveAlly = true;
    allyA.IncomingLethal = true;
    allyA.ProtectedAlly = true;
    REvaluation save = EvaluateTemperedFate(
        bard, allyA.Position, { allyA }, saveContext);
    Require(save.Valid && save.SavedAllies == 1 && save.Score > 900.0f,
            "R must permit a clean lethal-save stasis branch");

    StasisUnit turret{};
    turret.Position = Vec3{ 900.0f, 0.0f, 0.0f };
    turret.Radius = 90.0f;
    turret.Id = 30;
    turret.Team = TeamRelation::Enemy;
    turret.Turret = turret.Valid = true;
    StasisContext dive{};
    dive.DiveTower = true;
    REvaluation divePlan = EvaluateTemperedFate(
        bard, turret.Position, { turret }, dive);
    Require(divePlan.Valid && divePlan.Turrets == 1,
            "R must model turret stasis as a distinct dive-reset purpose");

    StasisUnit epic{};
    epic.Position = Vec3{ 1200.0f, 0.0f, 0.0f };
    epic.Radius = 120.0f;
    epic.Id = 40;
    epic.Team = TeamRelation::Neutral;
    epic.Monster = epic.EpicMonster = epic.Valid = true;
    StasisContext deny{};
    deny.ObjectiveDeny = true;
    REvaluation denyPlan = EvaluateTemperedFate(
        bard, epic.Position, { epic }, deny);
    Require(denyPlan.Valid && denyPlan.EpicMonsters == 1,
            "R must allow deliberate epic-objective denial");
    deny.AlliesSecuringObjective = true;
    REvaluation alliedObjective = EvaluateTemperedFate(
        bard, epic.Position, { epic }, deny);
    Require(!alliedObjective.Valid && alliedObjective.FriendlyGrief == 1,
            "R must not freeze an epic monster the allied team is securing");

    StasisUnit enemyB = StasisChampion(
        22, TeamRelation::Enemy, 830.0f, 100.0f);
    REvaluation twoTargets = EvaluateTemperedFate(
        bard, Vec3{ 810.0f, 0.0f, 40.0f },
        { StasisChampion(20, TeamRelation::Enemy, 800.0f), enemyB },
        catchContext);
    Require(BetterRPlan(twoTargets, catchPlan),
            "R tie-breaking must prefer the higher-value clean hit set");

    std::cout << "ALL AIBARD GEOMETRY TESTS PASSED\n";
    return 0;
}
