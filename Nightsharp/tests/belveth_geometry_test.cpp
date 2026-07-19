#include "../plugins/Champion/KuroAIO/AI/Controllers/AIBelvethGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Belveth::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.02f) {
    return std::fabs(left - right) <= epsilon;
}

Body Target(int id,
            float x,
            float z,
            float health = 1000.0f,
            float maximumHealth = 1000.0f) {
    Body body{};
    body.Position = Vec3{ x, 0.0f, z };
    body.Radius = 40.0f;
    body.Id = id;
    body.Champion = true;
    body.Health = health;
    body.MaximumHealth = maximumHealth;
    return body;
}

} // namespace

int main() {
    const Vec3 origin{ 0.0f, 0.0f, 0.0f };

    Require(QuadrantForPoints(origin, Vec3{ 1.0f, 0.0f, 1.0f }) ==
                Quadrant::NorthEast &&
            QuadrantForPoints(origin, Vec3{ -1.0f, 0.0f, 1.0f }) ==
                Quadrant::NorthWest &&
            QuadrantForPoints(origin, Vec3{ -1.0f, 0.0f, -1.0f }) ==
                Quadrant::SouthWest &&
            QuadrantForPoints(origin, Vec3{ 1.0f, 0.0f, -1.0f }) ==
                Quadrant::SouthEast,
            "Q must map all four map-fixed diagonal sectors exactly");
    const Vec3 east{ 1.0f, 0.0f, 0.0f };
    std::array<bool, 4> allReady = { true, true, true, true };
    Require(ForwardDirectionCount(east, allReady) == 2,
            "an axis-facing chase must have two usable forward arrows");
    allReady[0] = false;
    Require(ForwardDirectionCount(east, allReady) == 1,
            "spending one forward quadrant must preserve only one forward arrow");
    const Vec3 southEastAim = BoundaryBiasedDirection(
        east, Quadrant::SouthEast);
    Require(DirectionInsideQuadrant(
                southEastAim, Quadrant::SouthEast, 0.0f) &&
            southEastAim.Dot(east) > 0.95f,
            "boundary bias must spend a second quadrant in nearly the same direction");

    Require(Near(QPerDirectionCooldownSeconds(1, 0.0f), 16.0f) &&
                Near(QPerDirectionCooldownSeconds(1, 100.0f), 12.8f),
            "Q cooldown must convert each bonus-AS percent into 0.25 haste");
    Require(Near(QDashSpeed(1, 350.0f, false), 1150.0f) &&
                Near(QDashSpeed(5, 350.0f, false), 1350.0f) &&
                Near(QDashSpeed(5, 350.0f, true), 1500.0f),
            "Q must use rank plus movement speed off-wall and fixed Rift speed through walls");

    DirectionState directions{};
    directions.Spend(Quadrant::NorthEast, 1000, 12.8f);
    Require(!directions.Ready(Quadrant::NorthEast, 2000) &&
                directions.Ready(Quadrant::SouthWest, 2000) &&
                directions.CountReady(2000) == 3,
            "Q must maintain four independent cooldowns");
    directions.Refresh(QuadrantMask(Quadrant::NorthEast), 2200);
    Require(directions.Ready(Quadrant::NorthEast, 2200),
            "W refresh must release only the hit quadrant");

    HudCalibration calibration{};
    Require(calibration.LearnSpend(15, 11, Quadrant::NorthEast) &&
                calibration.HudReady(11, Quadrant::NorthEast, true) == false &&
                calibration.HudReady(15, Quadrant::NorthEast, false),
            "HUD calibration must learn a removed ready bit from a real Q cast");
    Require(calibration.LearnRefresh(11, 15, Quadrant::NorthEast),
            "HUD calibration must confirm the same bit from W refresh");

    std::vector<Body> qBodies = {
        Target(1, 180.0f, 35.0f),
        Target(2, 340.0f, 0.0f),
        Target(3, 250.0f, 180.0f),
    };
    Require(QHits(origin, Vec3{ 400.0f, 0.0f, 0.0f }, qBodies[0]) &&
                !QHits(origin, Vec3{ 400.0f, 0.0f, 0.0f }, qBodies[2]),
            "Q capsule must include gameplay radius without inventing side hits");
    Require(FirstQBodyIndex(
                origin, Vec3{ 400.0f, 0.0f, 0.0f }, qBodies) == 0,
            "Q on-hit ownership must belong to the first crossed body");
    Require(Near(QRawDamage(1, 100.0f), 100.0f) &&
                Near(QRawDamage(5, 100.0f), 120.0f) &&
                Near(QRawDamage(1, 100.0f, true), 155.0f) &&
                Near(QRawDamage(1, 100.0f, false, true), 60.0f),
            "Q must use Riot 25.15 champion, monster and minion modifiers");

    QContext weave{};
    weave.DirectionReady = weave.GlobalReady = weave.EndpointValid = true;
    weave.DestinationSafe = weave.CursorAgrees = true;
    weave.TargetHit = weave.TargetInAttackRangeAfter = true;
    weave.TargetInAttackRangeBefore = true;
    weave.AttackJustCompleted = true;
    weave.ReadyDirectionCount = 3;
    weave.ForwardDirectionCount = 2;
    weave.Purpose = QPurpose::Weave;
    Require(EvaluateQ(weave).Cast,
            "AA-Q-AA weave must be a first-class Q window");
    weave.AttackJustCompleted = false;
    weave.PlayerWindingUp = true;
    Require(!EvaluateQ(weave).Cast,
            "Q must never clip a live basic-attack windup for ordinary damage");

    QContext waste{};
    waste.DirectionReady = waste.GlobalReady = waste.EndpointValid = true;
    waste.DestinationSafe = waste.CursorAgrees = true;
    waste.TargetEscaping = true;
    waste.TargetDistanceBefore = 800.0f;
    waste.TargetDistanceAfter = 420.0f;
    waste.ReadyDirectionCount = 1;
    waste.ForwardDirectionCount = 1;
    waste.Purpose = QPurpose::Chase;
    Require(!EvaluateQ(waste).Cast,
            "the last forward Q must not be spent merely to enter range");
    waste.WCanRefreshSpentDirection = true;
    waste.WReady = true;
    waste.TargetInAttackRangeAfter = true;
    Require(EvaluateQ(waste).Cast,
            "W-backed Q-W-Q chase must be allowed when the spent arrow is refundable");

    QContext evade{};
    evade.DirectionReady = evade.GlobalReady = evade.EndpointValid = true;
    evade.DestinationSafe = evade.DodgesIncomingSkillshot = true;
    evade.IncomingSkillshot = true;
    evade.ReadyDirectionCount = 1;
    evade.Purpose = QPurpose::Evade;
    Require(EvaluateQ(evade).Cast,
            "lethal skillshot evasion must outrank ordinary Q conservation");
    QContext illegalWall = evade;
    illegalWall.TerrainCrossed = true;
    illegalWall.TrueForm = false;
    Require(!EvaluateQ(illegalWall).Cast,
            "normal-form Q must never fabricate a wall crossing");
    illegalWall.TrueForm = illegalWall.HasWallExit = true;
    Require(EvaluateQ(illegalWall).Cast,
            "true-form Q may use a traced legal wall exit");

    Body wTarget = Target(10, 500.0f, 80.0f);
    Require(WLineHits(origin, Vec3{ 660.0f, 0.0f, 0.0f }, wTarget),
            "W line must include target radius at its real gameplay width");
    wTarget.Position.z = 160.0f;
    Require(!WLineHits(origin, Vec3{ 660.0f, 0.0f, 0.0f }, wTarget),
            "W must reject bodies beyond half-width plus radius");
    Body northEast = Target(11, 300.0f, 300.0f);
    Body southEast = Target(12, 300.0f, -300.0f);
    const std::uint8_t resetMask = WResetMask(origin, { northEast, southEast });
    Require((resetMask & QuadrantMask(Quadrant::NorthEast)) != 0u &&
                (resetMask & QuadrantMask(Quadrant::SouthEast)) != 0u,
            "multi-champion W must refresh every represented Q quadrant");
    Require(Near(WRawDamage(1, 100.0f, 80.0f), 270.0f) &&
                Near(WRawDamage(5, 100.0f, 80.0f), 430.0f),
            "W must use 100 percent bonus AD and 125 percent AP");

    WContext rawW{};
    rawW.Ready = rawW.TargetValid = rawW.PredictionHits = true;
    rawW.FollowupAvailable = true;
    Require(!ShouldCastW(rawW),
            "W must not be thrown raw at a mobile target");
    rawW.TargetDashSpent = rawW.ResetsSpentQ = true;
    Require(ShouldCastW(rawW),
            "W should punish a spent dash and refund the chase quadrant");
    WContext interrupt = rawW;
    interrupt.TargetDashSpent = interrupt.ResetsSpentQ = false;
    interrupt.Interrupt = true;
    Require(ShouldCastW(interrupt),
            "W interrupt must not require a Q refund");

    std::vector<Body> eTargets = {
        Target(20, 250.0f, 0.0f, 400.0f, 1000.0f),
        Target(21, 300.0f, 0.0f, 150.0f, 500.0f),
        Target(22, 700.0f, 0.0f, 1.0f, 1000.0f),
    };
    Require(SelectETargetIndex(origin, eTargets) == 1,
            "E must force the nearest in-range unit with lowest health percentage");
    eTargets[0].Health = 200.0f;
    Require(SelectETargetIndex(origin, eTargets) == 0,
            "E target selection must update when another body falls lower");
    Require(EStrikeCount(0.0f) == 6 && EStrikeCount(33.4f) == 7 &&
                EStrikeCount(100.0f) == 9,
            "E must add one strike per 33.3 percent bonus attack speed");
    Require(Near(EMissingHealthMultiplier(250.0f, 1000.0f), 3.25f) &&
                Near(EOnHitEffectiveness(250.0f, 1000.0f), 0.26f),
            "E missing-health ramp must scale both slash and on-hit effectiveness");
    Require(Near(EStrikeRawDamage(1, 100.0f, 500.0f, 1000.0f), 35.0f) &&
                Near(EStrikeRawDamage(
                    1, 100.0f, 500.0f, 1000.0f, true), 52.5f),
            "E must use 6 plus 8 percent total AD and 150 percent monster damage");
    Require(ESimulatedRawDamage(
                1, 100.0f, 300.0f, 1000.0f, 100.0f) > 300.0f,
            "E simulation must ramp later strikes as the target loses health");
    Require(Near(EDamageReduction(1), 0.35f) &&
                Near(EDamageReduction(5), 0.55f),
            "E must use live 35-55 percent non-true damage reduction");

    EStartContext eExecute{};
    eExecute.Ready = eExecute.CanDeclareAttacks = true;
    eExecute.ForcedTargetValid = eExecute.ForcedTargetIsDesired = true;
    eExecute.ForcedTargetChampion = true;
    eExecute.PositionSafe = true;
    eExecute.Execute = true;
    eExecute.ExpectedDamage = 320.0f;
    eExecute.ForcedTargetHealth = 300.0f;
    eExecute.ForcedTargetHealthPercent = 30.0f;
    Require(ShouldStartE(eExecute),
            "E must finish the actual forced low-health champion");
    eExecute.ForcedTargetIsDesired = false;
    Require(!ShouldStartE(eExecute),
            "a lower-health minion must block a fake champion E execute");
    EStartContext eDefense{};
    eDefense.Ready = eDefense.CanDeclareAttacks = true;
    eDefense.Defensive = eDefense.IncomingReducibleBurst = true;
    eDefense.PlayerHealthPercent = 40.0f;
    Require(ShouldStartE(eDefense),
            "E must function as the real anti-burst button");
    eDefense.IncomingTrueDamageOnly = true;
    Require(!ShouldStartE(eDefense),
            "E must not claim protection from true damage");
    EStartContext runner = eExecute;
    runner.ForcedTargetIsDesired = true;
    runner.Execute = false;
    runner.RecentAbilityCast = true;
    runner.TargetEscaping = true;
    runner.ForcedTargetHealthPercent = 40.0f;
    Require(!ShouldStartE(runner),
            "E must not root Bel'Veth while an unsecured target runs away");
    runner.TargetSlowed = true;
    Require(ShouldStartE(runner),
            "W slow must create a valid late-E finisher window");

    ECancelContext cancel{};
    cancel.Active = true;
    cancel.ElapsedSeconds = 0.50f;
    cancel.IncomingLethalSkillshot = cancel.SafeQEvadeAvailable = true;
    Require(!ShouldCancelE(cancel),
            "E cannot be voluntarily recast before 0.75 seconds");
    cancel.ElapsedSeconds = 0.80f;
    Require(ShouldCancelE(cancel),
            "a safe Q dodge may cancel E after its real recast lock");

    Require(Near(RPassiveProcRawDamage(1, 100.0f, 0, false), 18.0f) &&
                Near(RPassiveProcRawDamage(1, 100.0f, 4, true), 90.0f) &&
                Near(RPassiveProcRawDamage(1, 100.0f, 5, true), 90.0f),
            "R passive must scale each proc and cap epic monsters at five increases");
    RPassiveTracker tracker{};
    Require(Near(tracker.ObserveAttack(30, 1000, 1, 100.0f, false), 0.0f) &&
                Near(tracker.ObserveAttack(30, 1200, 1, 100.0f, false), 18.0f) &&
                Near(tracker.ObserveAttack(30, 1400, 1, 100.0f, false), 0.0f) &&
                Near(tracker.ObserveAttack(30, 1600, 1, 100.0f, false), 36.0f),
            "R passive must proc every second attack on the same marked target");
    Require(Near(tracker.ObserveAttack(31, 1800, 1, 100.0f, false), 0.0f) &&
                tracker.ProcStacks == 0,
            "changing targets must reset R's mark and infinite stack chain");

    Require(Near(RExplosionRawDamage(
                    1, 100.0f, 300.0f, 1000.0f), 425.0f) &&
                Near(RExplosionRawDamage(
                    3, 100.0f, 0.0f, 100000.0f, true), 1500.0f),
            "R explosion must use 25 percent missing health and monster cap");
    Require(Near(RHeal(1, 100.0f, 100.0f), 310.0f),
            "R heal must use 120 percent bonus AD and 90 percent AP");
    Require(Near(UpdatedFormSeconds(40.0f, false, false), 60.0f) &&
                Near(UpdatedFormSeconds(40.0f, false, true), 180.0f) &&
                Near(UpdatedFormSeconds(100.0f, true, false), 160.0f),
            "normal, enhanced and enhanced-plus-normal form refreshes must differ");

    RContext form{};
    form.Ready = form.DestinationSafe = form.CursorAgrees = true;
    form.Heal = 300.0f;
    form.PlayerMissingHealth = 100.0f;
    Require(EvaluateR(form).Cast,
            "a safe first coral should unlock true form");
    form.CurrentlyTrueForm = true;
    form.CurrentFormSeconds = 50.0f;
    Require(!EvaluateR(form).Cast,
            "a healthy long normal form must not be overwritten without payoff");
    form.AnyEnhancedCoral = form.WaveMacroWindow = true;
    Require(EvaluateR(form).Cast,
            "enhanced coral must unlock the remora macro branch");

    RContext execute{};
    execute.Ready = true;
    execute.DestinationSafe = true;
    execute.HitCount = execute.KillCount = 1;
    execute.ExplosionDamage = 600.0f;
    execute.EnemiesAtCoral = 1;
    Require(EvaluateR(execute).Cast,
            "R must consume a safe coral for a real missing-health execute");
    execute.DestinationSafe = false;
    execute.KillCount = 0;
    Require(!EvaluateR(execute).Cast,
            "R must not channel on an unsafe contested coral without a kill/save");
    RContext survival{};
    survival.Ready = true;
    survival.DestinationSafe = false;
    survival.IncomingLethalDamage = survival.PlayerLow = true;
    survival.Heal = 500.0f;
    survival.PlayerMissingHealth = 600.0f;
    Require(EvaluateR(survival).Cast,
            "start-of-cast R heal may save Bel'Veth even before the explosion");

    std::cout << "ALL AIBELVETH GEOMETRY TESTS PASSED\n";
    return 0;
}
