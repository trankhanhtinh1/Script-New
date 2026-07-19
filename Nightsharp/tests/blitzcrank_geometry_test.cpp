#include "../plugins/Champion/KuroAIO/AI/Controllers/AIBlitzcrankGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Blitzcrank::Geometry;

namespace {

int ScenarioCount = 0;

void Require(bool condition, const char* message) {
    ++ScenarioCount;
    if (!condition) {
        std::cerr << "FAILED [" << ScenarioCount << "]: "
                  << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.04f) {
    return std::fabs(left - right) <= epsilon;
}

HookBody Body(int id,
              float x,
              float z = 0.0f,
              float radius = 40.0f,
              bool champion = true) {
    HookBody body{};
    body.Id = id;
    body.Position = Vec3{ x, 0.0f, z };
    body.Radius = radius;
    body.Champion = champion;
    body.Minion = !champion;
    return body;
}

HookContext PremiumHook() {
    HookContext context{};
    context.Ready = context.TargetValid = context.IntendedFirstBody = true;
    context.HighConfidence = context.SelectedTarget = true;
    context.FollowupAvailable = true;
    context.CursorAgrees = true;
    context.TargetPriority = 2.0f;
    context.CollisionConfidence = 0.95f;
    context.AlliesAtLanding = 2;
    context.EnemiesAtLanding = 1;
    context.Archetype = PullArchetype::Carry;
    context.Purpose = HookPurpose::SelectedPick;
    return context;
}

WContext WalkUpW() {
    WContext context{};
    context.Ready = context.HasMana = context.PathSafe = true;
    context.TargetValid = context.TargetEscaping = true;
    context.EReady = context.EWillBeInRange = true;
    context.CursorAgrees = true;
    context.TargetDistance = 260.0f;
    context.DistanceClosedBeforeDecay = 280.0f;
    context.AlliesAtDestination = context.EnemiesAtDestination = 1;
    context.Purpose = WPurpose::WalkUpE;
    return context;
}

RContext ValuableR() {
    RContext context{};
    context.Ready = context.HasMana = context.TargetInRange = true;
    context.EnemyHitCount = 2;
    context.PriorityEnemyHitCount = 1;
    context.Purpose = RPurpose::MultiTarget;
    return context;
}

} // namespace

int main() {
    // Live 26.14 data and post-25.08/25.22 reconciliation.
    Require(Near(ManaBarrierShield(1000.0f), 350.0f),
            "Mana Barrier must shield for 35 percent maximum mana");
    Require(Near(ManaBarrierShield(0.0f), 0.0f),
            "Mana Barrier must clamp malformed mana");
    Require(ManaBarrierWillTrigger(400.0f, 1000.0f, 120.0f, true),
            "damage crossing 30 percent health must trigger ready barrier");
    Require(!ManaBarrierWillTrigger(250.0f, 1000.0f, 50.0f, true),
            "already-low health must not fabricate a new threshold crossing");
    Require(!ManaBarrierWillTrigger(400.0f, 1000.0f, 120.0f, false),
            "barrier cooldown must be respected");
    Require(Near(QRawDamage(1, 100.0f), 230.0f) &&
                Near(QRawDamage(5, 100.0f), 430.0f),
            "Q must use Riot 25.08 base values and 120 percent AP");
    Require(Near(QRawDamage(0, 500.0f), 0.0f),
            "unlearned Q must deal no simulated damage");
    Require(Near(WInitialMoveSpeedPercent(1), 60.0f) &&
                Near(WInitialMoveSpeedPercent(5), 80.0f),
            "W initial movement speed must scale 60 to 80 percent");
    Require(Near(WAttackSpeedPercent(1), 30.0f) &&
                Near(WAttackSpeedPercent(5), 70.0f),
            "W attack speed must scale 30 to 70 percent");
    Require(Near(WMoveSpeedPercentAt(5, 0.0f), 80.0f) &&
                Near(WMoveSpeedPercentAt(5, 2.9f), 10.0f) &&
                Near(WMoveSpeedPercentAt(5, 4.9f), 10.0f),
            "W movement must decay to ten percent by 2.9 seconds");
    Require(Near(WMoveSpeedPercentAt(5, 5.0f), 0.0f),
            "W speed must end at five seconds before the self-slow");
    Require(WBonusTravelDistance(325.0f, 5, 2.9f) > 400.0f &&
                WBonusTravelDistance(325.0f, 5, 5.0f) < 550.0f,
            "W travel estimate must integrate decay instead of full tooltip speed");
    Require(Near(EEmpoweredAttackRawDamage(100.0f, 80.0f), 220.0f) &&
                Near(EAdditionalRawDamage(100.0f, 80.0f), 120.0f),
            "E must be a 200 percent total-AD attack plus 25 percent AP");
    Require(Near(RPassiveRawDamage(1, 100.0f, 1000.0f), 100.0f) &&
                Near(RPassiveRawDamage(3, 100.0f, 1000.0f), 220.0f),
            "R passive must include live AP ratio and two percent max mana");
    Require(Near(RActiveRawDamage(1, 100.0f), 375.0f) &&
                Near(RActiveRawDamage(3, 100.0f), 625.0f),
            "R active must use 275/400/525 and 100 percent AP");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 aim{ 1200.0f, 0.0f, 0.0f };
    HookBody target = Body(10, 700.0f);
    HookContact contact = ContactWithBody(origin, aim, target);
    Require(contact.Hit && contact.BodyId == 10,
            "stationary target inside Q corridor must be contacted");
    Require(contact.ProjectileSeconds > 0.25f &&
                contact.CastElapsedSeconds > contact.ProjectileSeconds,
            "contact time must include cast delay separately from flight");
    Require(contact.Kind == HookContactKind::MissileBody,
            "ordinary target must be a missile-body contact");
    target.Position.z = 120.0f;
    Require(!ContactWithBody(origin, aim, target).Hit,
            "target outside Q plus bounding radius must miss");
    target.Position.z = 109.0f;
    Require(ContactWithBody(origin, aim, target).Hit,
            "target edge inside Q combined radii must count");

    HookBody entering = Body(11, 650.0f, 250.0f, 35.0f, false);
    entering.Velocity = Vec3{ 0.0f, 0.0f, -400.0f };
    Require(ContactWithBody(origin, aim, entering).Hit,
            "moving blocker entering the corridor at intercept time must block");
    HookBody leaving = Body(12, 650.0f, 0.0f, 35.0f, false);
    leaving.Velocity = Vec3{ 0.0f, 0.0f, 900.0f };
    Require(!ContactWithBody(origin, aim, leaving).Hit,
            "blocker leaving during cast delay must not be treated as static collision");
    HookBody endpoint = Body(13, 1115.0f, 0.0f, 0.0f);
    HookContact endpointContact = ContactWithBody(origin, aim, endpoint);
    Require(endpointContact.Hit &&
                endpointContact.Kind == HookContactKind::EndpointLollipop,
            "Q must model its center-only endpoint lollipop through 1115");
    endpoint.Position.x = 1115.2f;
    Require(!ContactWithBody(origin, aim, endpoint).Hit,
            "Q must not inflate center range beyond the live 1115 endpoint");
    HookBody behind = Body(14, -150.0f);
    Require(!ContactWithBody(origin, aim, behind).Hit,
            "Q must not collide with bodies behind Blitzcrank");
    HookBody invalid = Body(15, 500.0f);
    invalid.Targetable = false;
    Require(!ContactWithBody(origin, aim, invalid).Hit,
            "untargetable bodies must not block Q");

    HookBody minion = Body(20, 400.0f, 0.0f, 35.0f, false);
    target = Body(21, 800.0f);
    HookContact first = FirstHookContact(origin, aim, { target, minion });
    Require(first.Hit && first.BodyId == minion.Id,
            "near minion must be the first Q body regardless of vector order");
    Require(!HookHitsIntendedFirst(origin, aim, { target, minion }, target.Id),
            "blocked intended champion must not be authorized");
    Require(HookHitsIntendedFirst(origin, aim, { target }, target.Id, &first) &&
                first.BodyIndex == 0,
            "clean intended target must be returned with body index");
    HookBody crossing = Body(22, 500.0f, 300.0f, 30.0f, false);
    crossing.Velocity = Vec3{ 0.0f, 0.0f, -550.0f };
    first = FirstHookContact(origin, aim, { target, crossing });
    Require(first.Hit && first.BodyId == crossing.Id,
            "moving first body must beat a later stationary target");
    HookBody tieHigh = Body(31, 500.0f);
    HookBody tieLow = Body(30, 500.0f);
    first = FirstHookContact(origin, aim, { tieHigh, tieLow });
    Require(first.BodyId == 30,
            "collision tie must be deterministic by network id");
    Require(Near(PullLandingPosition(origin, Vec3{ 500.0f, 0.0f, 0.0f }).x,
                 75.0f),
            "hooked target destination must be 75 units in front of Blitz");
    Require(PullLandingPosition(origin, origin) == origin,
            "degenerate pull direction must stay at Blitz position");

    HookContext hook = PremiumHook();
    Require(EvaluateHook(hook).Cast,
            "clean selected carry pick with follow-up must cast Q");
    HookContext noFirst = hook;
    noFirst.IntendedFirstBody = false;
    Require(!EvaluateHook(noFirst).Cast,
            "Q must reject a different first collision body");
    HookContext wall = hook;
    wall.ProjectileWallBlocked = true;
    Require(!EvaluateHook(wall).Cast,
            "projectile wall must block Q authorization");
    HookContext unstoppable = hook;
    unstoppable.TargetUnstoppable = true;
    Require(!EvaluateHook(unstoppable).Cast,
            "unstoppable target must not consume Q");
    HookContext shield = hook;
    shield.TargetSpellShield = true;
    Require(!EvaluateHook(shield).Cast,
            "ordinary spell shield must preserve hook");
    shield.InterruptUrgent = true;
    shield.Purpose = HookPurpose::Interrupt;
    Require(!EvaluateHook(shield).Cast,
            "Q cannot interrupt a channel through spell shield");
    HookContext noFollow = hook;
    noFollow.FollowupAvailable = false;
    Require(!EvaluateHook(noFollow).Cast,
            "unsupported lane hook must be held");
    noFollow.TargetKillable = true;
    noFollow.Purpose = HookPurpose::Kill;
    Require(EvaluateHook(noFollow).Cast,
            "real Q lethal may override missing follow-up");

    HookContext bomb = hook;
    bomb.Archetype = PullArchetype::EngageBomb;
    bomb.PullsOntoProtectedCarry = true;
    Require(!EvaluateHook(bomb).Cast,
            "healthy engage tank must never be delivered onto carry");
    bomb.TargetIsolated = true;
    bomb.TargetKeyCooldownsSpent = true;
    bomb.TargetImmobile = true;
    bomb.PullsTowardAlliedTurret = true;
    bomb.Purpose = HookPurpose::AllyTurret;
    Require(EvaluateHook(bomb).Cast,
            "isolated cooldown-less tank can be displaced into allied turret");
    HookContext diverPeel = hook;
    diverPeel.Archetype = PullArchetype::Diver;
    diverPeel.PullsOntoProtectedCarry = true;
    diverPeel.ProtectedCarryThreatened = true;
    diverPeel.PeelDisplacement = true;
    diverPeel.Purpose = HookPurpose::Peel;
    Require(EvaluateHook(diverPeel).Cast,
            "peel hook may displace a diver already threatening the carry");
    HookContext numbers = hook;
    numbers.AlliesAtLanding = 0;
    numbers.EnemiesAtLanding = 3;
    Require(!EvaluateHook(numbers).Cast,
            "Q must reject a pull into losing numbers");
    numbers.PullsTowardAlliedTurret = true;
    numbers.TargetKillable = true;
    numbers.Purpose = HookPurpose::Kill;
    Require(EvaluateHook(numbers).Cast,
            "confirmed turret-side lethal may override local numbers");
    HookContext holdPressure = hook;
    holdPressure.WEReliableWalkup = true;
    Require(!EvaluateHook(holdPressure).Cast,
            "one-trick hook pressure must prefer guaranteed W-E setup");
    holdPressure.TargetEscapeSpent = true;
    Require(EvaluateHook(holdPressure).Cast,
            "spent escape removes the need to preserve W-E hook pressure");
    HookContext eAttack = hook;
    eAttack.PlayerAttackWindingUp = eAttack.EPrimedInAttackRange = true;
    Require(!EvaluateHook(eAttack).Cast,
            "Q must not cancel a guaranteed Power Fist windup");
    eAttack.PeelDisplacement = true;
    eAttack.Purpose = HookPurpose::Peel;
    Require(EvaluateHook(eAttack).Cast,
            "urgent displacement may override E windup preservation");
    HookContext objective = hook;
    objective.FollowupAvailable = false;
    objective.ObjectiveContest = true;
    objective.Purpose = HookPurpose::ObjectiveJungler;
    Require(EvaluateHook(objective).Cast,
            "clean objective-jungler hook must remain available without lane follow-up");
    HookContext instantEscape = hook;
    instantEscape.TargetCanInstantEscape = true;
    instantEscape.HighConfidence = false;
    Require(EvaluateHook(instantEscape).Score < EvaluateHook(hook).Score,
            "raw hook score must respect available instant escape");
    HookContext dashEnd = instantEscape;
    dashEnd.TargetDashEnding = true;
    dashEnd.Purpose = HookPurpose::DashEndpoint;
    Require(EvaluateHook(dashEnd).Cast &&
                EvaluateHook(dashEnd).Score > EvaluateHook(instantEscape).Score,
            "dash endpoint must restore a premium hook window");

    WContext w = WalkUpW();
    Require(EvaluateW(w).Cast,
            "safe W that reaches guaranteed E must cast");
    WContext windup = w;
    windup.PlayerAttackWindingUp = true;
    Require(!EvaluateW(windup).Cast,
            "W must not cancel an ordinary attack windup");
    windup.IncomingLethal = true;
    windup.Purpose = WPurpose::Flee;
    Require(EvaluateW(windup).Cast,
            "lethal flee may override attack-windup preservation");
    WContext unsafe = w;
    unsafe.PathSafe = false;
    Require(!EvaluateW(unsafe).Cast,
            "W walk-up must reject unsafe path");
    unsafe.Purpose = WPurpose::Flee;
    unsafe.IncomingLethal = true;
    Require(EvaluateW(unsafe).Cast,
            "survival W can leave a locally unsafe position");
    WContext turret = w;
    turret.DestinationUnderEnemyTurret = true;
    Require(!EvaluateW(turret).Cast,
            "W must not manufacture an unapproved turret commitment");
    WContext postHook = w;
    postHook.HookLanded = true;
    postHook.Purpose = WPurpose::PostHook;
    Require(EvaluateW(postHook).Cast,
            "post-hook W must amplify attacks and preserve contact");
    WContext angle = w;
    angle.EWillBeInRange = false;
    angle.BetterHookAngleCreated = angle.HookReady = true;
    angle.Purpose = WPurpose::HookAngle;
    Require(EvaluateW(angle).Cast,
            "W may create a materially better hook angle");
    WContext slowTrap = angle;
    slowTrap.FutureSelfSlowUnsafe = true;
    slowTrap.BetterHookAngleCreated = false;
    slowTrap.TargetEscaping = false;
    Require(!EvaluateW(slowTrap).Cast,
            "marginal W must be held when its future self-slow is punishable");
    WContext badNumbers = w;
    badNumbers.EnemiesAtDestination = 4;
    badNumbers.AlliesAtDestination = 1;
    Require(!EvaluateW(badNumbers).Cast,
            "W must not run Blitz into losing numbers");
    badNumbers.Purpose = WPurpose::Peel;
    badNumbers.AllyNeedsPeel = true;
    Require(EvaluateW(badNumbers).Cast,
            "peel W may answer numbers when an ally is already threatened");

    EContext e{};
    e.Ready = e.TargetValid = e.TargetInEmpoweredAttackRange = true;
    e.ExactAttackTarget = true;
    e.AttackJustCompleted = e.TargetCannotEscape = true;
    EDecision eDecision = EvaluateE(e);
    Require(eDecision.ArmNow && eDecision.Timing == ETiming::ResetAfterAttack &&
                !eDecision.PreserveAttack,
            "safe target must use the real AA-E-AA reset");
    EContext escape = e;
    escape.AttackJustCompleted = escape.TargetCannotEscape = false;
    escape.TargetCanInstantEscape = true;
    eDecision = EvaluateE(escape);
    Require(eDecision.ArmNow &&
                eDecision.Timing == ETiming::ImmediateOnArrival,
            "instant escape target must not receive a free AA-before-E window");
    EContext hookFlight = escape;
    hookFlight.TargetInEmpoweredAttackRange = false;
    hookFlight.QInFlightToTarget = hookFlight.HookWillLand = true;
    hookFlight.HookArrivalSeconds = 0.45f;
    eDecision = EvaluateE(hookFlight);
    Require(eDecision.ArmNow && eDecision.Timing == ETiming::PreArmDuringHook,
            "E must be armed while Q flies toward an escape-ready target");
    hookFlight.HookArrivalSeconds = 5.1f;
    Require(!EvaluateE(hookFlight).ArmNow,
            "E must not expire before a fictional late hook arrival");
    EContext delayed = escape;
    delayed.TargetCanInstantEscape = false;
    delayed.EscapeHasInterruptibleStartup = delayed.EscapeCastStarted = true;
    eDecision = EvaluateE(delayed);
    Require(eDecision.ArmNow &&
                eDecision.Timing == ETiming::DelayForEscapeCast,
            "Power Fist must catch a committed interruptible escape startup");
    EContext peel = escape;
    peel.PeelUrgent = true;
    peel.TargetSpellShield = true;
    eDecision = EvaluateE(peel);
    Require(eDecision.ArmNow && eDecision.Timing == ETiming::PeelNow,
            "urgent E peel still contributes damage through spell shield");
    EContext blocked = peel;
    blocked.AttackBlockedByZone = true;
    Require(!EvaluateE(blocked).ArmNow,
            "Shen-like attack blocker must prevent fake E knock-up");
    EContext normalLethal = e;
    normalLethal.TargetKillableByNormalAttack = true;
    Require(!EvaluateE(normalLethal).ArmNow,
            "E must not be wasted when ordinary attack safely kills");
    EContext winding = escape;
    winding.PlayerAttackWindingUp = true;
    Require(!EvaluateE(winding).ArmNow,
            "E must preserve an already-started attack before reset");
    EContext exactGate = escape;
    exactGate.ExactAttackTarget = false;
    exactGate.WrongUnitAttackPending = true;
    Require(!EvaluateE(exactGate).ArmNow,
            "E must stay reserved when orbwalker targets the wrong unit");
    Require(ShouldBlockWrongAttackWhileEArmed(
                true, 100, 101, true, 0.4f, 2.0f),
            "armed E may narrowly block a minion attack before hooked target arrives");
    Require(!ShouldBlockWrongAttackWhileEArmed(
                true, 100, 101, true, 2.0f, 1.0f),
            "orbwalker must not freeze when desired target arrives after E expires");
    Require(!ShouldBlockWrongAttackWhileEArmed(
                true, 100, 100, true, 0.2f, 2.0f),
            "exact desired E target must never be blocked");

    RMarkTracker marks{};
    marks.RecordAttack(1, 1000);
    marks.RecordAttack(1, 1200);
    marks.RecordAttack(1, 1400);
    Require(marks.Pending(1) == 3 && Near(marks.SecondsToNext(1, 1500), 0.5f),
            "R passive must backlog unlimited attacks on one-second queue");
    RMarkAdvance advance = marks.Advance(1, 1999);
    Require(advance.DetonatedStacks == 0 && advance.PendingStacks == 3,
            "R mark must not detonate early");
    advance = marks.Advance(1, 2000);
    Require(advance.DetonatedStacks == 1 && advance.PendingStacks == 2,
            "exactly one R stack must detonate at first second");
    advance = marks.Advance(1, 4500);
    Require(advance.DetonatedStacks == 2 && advance.PendingStacks == 0,
            "late update must consume backlog one per elapsed second");
    marks.RecordAttack(2, 5000);
    marks.RecordAttack(3, 5200);
    Require(marks.Pending(2) == 1 && marks.Pending(3) == 1,
            "R marks must be tracked independently per target");
    marks.Synchronize(2, 4, 5400);
    Require(marks.Pending(2) == 4,
            "visible buff count must resynchronize missed attack events");
    marks.Synchronize(2, 0, 5600);
    Require(marks.Pending(2) == 0 &&
                marks.SecondsToNext(2, 5600) == FLT_MAX,
            "zero visible marks must clear detonation timer");
    Require(Near(RPassiveOpportunityCost(100.0f, 5), 325.0f),
            "active R opportunity cost must account for lost ready-passive attacks");

    RContext r = ValuableR();
    Require(EvaluateR(r).Cast,
            "two-target R with priority target must be allowed");
    RContext pending = r;
    pending.PendingPassiveLethal = pending.PendingTickSoon = true;
    Require(!EvaluateR(pending).Cast,
            "R must wait when pending passive lightning already kills");
    pending.ChannelInterruptUrgent = true;
    pending.Purpose = RPurpose::Interrupt;
    Require(EvaluateR(pending).Cast,
            "urgent silence must override passive lethal delay");
    RContext spellShield = ValuableR();
    spellShield.EnemyHitCount = 1;
    spellShield.PriorityEnemyHitCount = 0;
    spellShield.TargetSpellShield = true;
    Require(!EvaluateR(spellShield).Cast,
            "active R must not be donated to bare spell shield");
    spellShield.TargetHasDamageShield = spellShield.CriticalShieldBreak = true;
    spellShield.TotalShields = 700.0f;
    spellShield.Purpose = RPurpose::ShieldBreak;
    Require(EvaluateR(spellShield).Cast,
            "R must destroy critical damage shields even through spell shield");
    RContext midPull{};
    midPull.Ready = midPull.HasMana = midPull.TargetInRange = true;
    midPull.HookInFlight = midPull.HookWillLand = true;
    midPull.HookArrivalSeconds = 0.4f;
    midPull.EscapeCastMustBeSilenced = true;
    midPull.Purpose = RPurpose::MidPullSilence;
    Require(EvaluateR(midPull).Cast,
            "R must silence escape-ready caster before hook arrival");
    RContext preHook{};
    preHook.Ready = preHook.HasMana = preHook.TargetInRange = true;
    preHook.QReady = preHook.QLineClear = preHook.MobilePriorityTarget = true;
    preHook.Purpose = RPurpose::PreHookSilence;
    Require(EvaluateR(preHook).Cast,
            "R-Q may silence a mobile priority target before guaranteed hook");
    RContext expensive = r;
    expensive.EnemyHitCount = 1;
    expensive.PriorityEnemyHitCount = 0;
    expensive.PassiveOpportunityCost = 1000.0f;
    Require(!EvaluateR(expensive).Cast,
            "low-payoff active must preserve future R passive attacks");
    RContext lethal = expensive;
    lethal.ActiveDamageLethal = true;
    lethal.Purpose = RPurpose::Lethal;
    Require(EvaluateR(lethal).Cast,
            "confirmed active lethal must override passive opportunity cost");
    RContext windingR = ValuableR();
    windingR.EnemyHitCount = 1;
    windingR.PriorityEnemyHitCount = 0;
    windingR.PlayerAttackWindingUp = true;
    Require(!EvaluateR(windingR).Cast,
            "R must preserve an ordinary attack and its passive stack");
    windingR.PeelUrgent = true;
    windingR.Purpose = RPurpose::Peel;
    Require(EvaluateR(windingR).Cast,
            "urgent AoE silence may override attack preservation");

    ManaCosts costs{};
    costs.R = 100.0f;
    Require(Near(SequenceCost(costs, ManaSequence::HookE), 125.0f) &&
                Near(SequenceCost(costs, ManaSequence::WalkUpEHook), 200.0f) &&
                Near(SequenceCost(costs, ManaSequence::FullCatch), 300.0f),
            "mana planner must price each real Blitz sequence");
    Require(CanAffordSequence(
                225.0f, costs, ManaSequence::WalkUpEHook, 25.0f),
            "walk-up sequence must preserve explicit peel reserve");
    Require(!CanAffordSequence(
                224.0f, costs, ManaSequence::WalkUpEHook, 25.0f),
            "one mana short must reject full planned sequence");
    Require(CanAffordSequence(
                100.0f, costs, ManaSequence::EmergencyR, 200.0f, true),
            "emergency R may spend the reserved mana itself");
    Require(Near(ManaBarrierShield(1500.0f), 525.0f),
            "barrier strength must depend on maximum, never current, mana");

    std::cout << "ALL AIBLITZCRANK GEOMETRY TESTS PASSED ("
              << ScenarioCount << " scenarios)\n";
    return 0;
}
