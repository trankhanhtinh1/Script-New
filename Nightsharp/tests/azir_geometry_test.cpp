#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAzirGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Azir::Geometry;

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

} // namespace

int main() {
    Require(Near(QRawDamage(1, 100.0f), 95.0f) &&
                Near(QRawDamage(5, 100.0f), 195.0f),
            "Q must use current rank-scaled 35 to 55 percent AP ratios");
    Require(Near(WRawDamage(18, 5, 100.0f, 1), 247.0f) &&
                Near(WRawDamage(18, 5, 100.0f, 3), 370.5f),
            "W must add 72 level damage and 25 percent per extra soldier");
    Require(Near(ERawDamage(5, 100.0f), 290.0f) &&
                Near(EShield(5, 100.0f), 290.0f),
            "current E damage and shield must both use 230 plus 60 percent AP");
    Require(Near(RRawDamage(3, 100.0f), 675.0f) &&
                RSoldierCount(1) == 6 && RSoldierCount(3) == 8 &&
                Near(RWallLength(2), 810.0f),
            "R must retain current damage and 6/7/8-soldier wall lengths");
    Require(Near(SunDiscRawDamage(1, 100.0f), 270.0f) &&
                Near(SunDiscRawDamage(18, 100.0f), 450.0f) &&
                Near(SunDiscBonusResists(18), 90.0f),
            "Sun Disc must use level breakpoints from live passive data");

    const Vec3 azir{ 0.0f, 0.0f, 0.0f };
    Soldier edge{ Vec3{ 660.0f, 0.0f, 0.0f }, 1, 1.0f, 4.0f,
                  true, true };
    Unit target{ Vec3{ 1035.0f, 0.0f, 0.0f }, {}, 0.0f, 10,
                 1000.0f, 1.5f, true, false, false, false, false,
                 false, true };
    Require(Commandable(azir, edge) && SoldierCanAttack(azir, edge, target),
            "a soldier on the 660 tether edge must command a 375-range stab");
    edge.Position.x = 660.1f;
    Require(!Commandable(azir, edge),
            "a soldier beyond the tether must deactivate immediately");
    edge.Position.x = 650.0f;
    target.Structure = true;
    Require(!SoldierCanAttack(azir, edge, target),
            "soldiers must never attack structures");
    target.Structure = false;
    target.WardOrTrap = true;
    Require(!SoldierCanAttack(azir, edge, target),
            "soldiers must never attack wards or traps");
    target.WardOrTrap = false;

    const auto formation = QFormation(
        azir, Vec3{ 900.0f, 0.0f, 0.0f }, 3);
    Require(formation.size() == 3 && Near(formation[1].x, 770.0f) &&
                Near(formation[0].z, -105.0f) &&
                Near(formation[2].z, 105.0f),
            "Q must clamp at 720, overshoot 50 and spread three soldiers laterally");

    std::vector<Soldier> soldiers = {
        { Vec3{ 300.0f, 0.0f, -80.0f }, 1, 1.0f, 8.0f, true, true },
        { Vec3{ 320.0f, 0.0f, 80.0f }, 2, 1.0f, 8.0f, true, true },
    };
    Unit primary{};
    primary.Position = Vec3{ 500.0f, 0.0f, 0.0f };
    primary.PredictedPosition = Vec3{ 590.0f, 0.0f, 0.0f };
    primary.Radius = 65.0f;
    primary.Id = 20;
    primary.Health = 600.0f;
    primary.Priority = 1.6f;
    primary.Champion = primary.Valid = true;
    Unit minion{};
    minion.Position = Vec3{ 490.0f, 0.0f, 25.0f };
    minion.PredictedPosition = minion.Position;
    minion.Radius = 35.0f;
    minion.Id = 21;
    minion.Minion = minion.Valid = true;
    const QEvaluation q = EvaluateQ(
        azir, Vec3{ 590.0f, 0.0f, 0.0f }, soldiers,
        { primary, minion }, primary.Id, Vec3{ 1.0f, 0.0f, 0.0f });
    Require(q.Valid && q.PrimaryHit && q.ChampionHits == 1 &&
                q.FuturePrimaryAttackers >= 1 && q.ExtendsAlongRetreat,
            "late Q must hit once while carrying soldier coverage along retreat");
    Require(std::count(q.HitIds.begin(), q.HitIds.end(), primary.Id) == 1,
            "multiple Q paths must never credit duplicate champion damage");

    LateQContext late{};
    late.QReady = late.TargetValid = late.TargetLeavingCoverage = true;
    late.CurrentCoverage = late.FutureCoverage = late.QHitsTarget = true;
    late.EnoughMana = late.DirectionAgrees = true;
    late.CurrentSoldierAttackers = late.FutureSoldierAttackers = 1;
    late.PlayerAttackWindingUp = true;
    Require(!ShouldCastLateQ(late),
            "Q must preserve a soldier attack already winding up");
    late.PlayerAttackWindingUp = false;
    late.AttackJustCompleted = true;
    Require(ShouldCastLateQ(late),
            "Q should extend the trade after the player's attack completes");
    late.EscapeAnchorWouldBeLost = true;
    Require(!ShouldCastLateQ(late),
            "Q must not move the only required escape anchor offensively");
    late.EscapeAnchorWouldBeLost = false;
    late.PlayerAttackWindingUp = true;
    late.QLethal = true;
    Require(ShouldCastLateQ(late),
            "a lethal Q may override protected windup timing");

    WPlacementContext w{};
    w.CastPositionValid = w.Offensive = w.CursorAgrees = true;
    w.CreatesTargetCoverage = true;
    w.EReady = true;
    w.Charges = 1;
    w.MinimumReserve = 1;
    Require(!ShouldPlaceW(w),
            "the last charge must remain an E escape anchor when none exists");
    w.ExistingEscapeAnchor = true;
    Require(ShouldPlaceW(w),
            "an existing escape soldier allows the offensive charge");
    w.PlayerAttackWindingUp = true;
    Require(!ShouldPlaceW(w),
            "offensive W must not cancel a valuable attack windup");
    w.Offensive = false;
    w.Defensive = true;
    Require(ShouldPlaceW(w),
            "defensive W may override attack preservation");

    std::vector<CollisionBody> bodies = {
        { Vec3{ 350.0f, 0.0f, 0.0f }, 65.0f, 31, true, true, true },
        { Vec3{ 700.0f, 0.0f, 0.0f }, 65.0f, 32, true, false, true },
    };
    const DashResult dash = ResolveDashSegment(
        azir, Vec3{ 900.0f, 0.0f, 0.0f }, bodies);
    Require(dash.Valid && dash.HitChampion && dash.CollisionId == 31 &&
                !dash.ReachedSoldier && dash.CollisionT < 0.5f,
            "E must stop on the first champion rather than pass through to its soldier");

    Soldier anchor{ Vec3{ 500.0f, 0.0f, 0.0f }, 40, 1.0f, 8.0f,
                    true, true };
    const DriftResult collisionDrift = ResolveDrift(
        azir, anchor, Vec3{ 720.0f, 0.0f, 280.0f }, bodies);
    Require(collisionDrift.Valid && collisionDrift.HitChampion &&
                collisionDrift.CollisionId == 31 &&
                !collisionDrift.QBuffered,
            "an early champion collision must cancel the planned Q extension");
    const DriftResult cleanDrift = ResolveDrift(
        azir, anchor, Vec3{ 720.0f, 0.0f, 280.0f }, {});
    Require(cleanDrift.Valid && cleanDrift.QBuffered &&
                cleanDrift.ReachedRedirect &&
                cleanDrift.TravelDistance > 700.0f,
            "a clean E-Q must follow the moved soldier for extended drift distance");

    ECommitContext e{};
    e.EReady = e.AnchorValid = e.EndpointNavigable = e.CursorAgrees = true;
    e.TargetCollisionDesired = e.TargetCollisionConfirmed = e.Killable = true;
    Require(ShouldCommitE(e),
            "E may collide for a verified kill and W-charge refund");
    e.TargetCollisionConfirmed = false;
    Require(!ShouldCommitE(e),
            "offensive E must not assume a collision that geometry rejects");
    e = {};
    e.EReady = e.AnchorValid = e.EndpointNavigable = e.CursorAgrees = true;
    e.Shuffle = e.QReadyForRedirect = e.HasRExit = e.HasAlliedFollowup = true;
    e.EnemiesAtEndpoint = 2;
    e.AlliesAtEndpoint = 1;
    Require(ShouldCommitE(e),
            "a shuffle may commit with Q, R and allied follow-up intact");
    e.EndpointPointClickThreat = true;
    Require(!ShouldCommitE(e),
            "offensive drift must reject ready point-click lockdown");

    Unit rPrimary = primary;
    rPrimary.Position = Vec3{ 180.0f, 0.0f, 50.0f };
    Unit rEdge = primary;
    rEdge.Id = 51;
    rEdge.Position = Vec3{ 100.0f, 0.0f, 430.0f };
    Require(RHits(azir, Vec3{ 1.0f, 0.0f, 0.0f }, 1, rPrimary) &&
                !RHits(azir, Vec3{ 1.0f, 0.0f, 0.0f }, 1, rEdge) &&
                RHits(azir, Vec3{ 1.0f, 0.0f, 0.0f }, 3, rEdge),
            "R lateral coverage must grow with its 6/7/8-soldier wall");
    const Vec3 landing = RLandingPosition(
        azir, Vec3{ 1.0f, 0.0f, 0.0f }, rPrimary);
    Require(Near(landing.x, 650.0f) && Near(landing.z, 50.0f),
            "R must preserve lateral offset while pushing forward");
    const REvaluation r = EvaluateR(
        azir, Vec3{ 250.0f, 0.0f, 0.0f }, 3,
        { rPrimary, rEdge }, rPrimary.Id,
        Vec3{ 800.0f, 0.0f, 0.0f }, 2, true);
    Require(r.Valid && r.Hits == 2 && r.PrimaryHit &&
                r.PushesPrimaryTowardAllies && r.Score > 1000.0f,
            "R planner must value multi-hit displacement toward allied follow-up");

    RCastContext rCast{};
    rCast.RReady = rCast.CursorAgrees = rCast.HasStasisOrExit = true;
    rCast.KeyCrowdControlSpent = true;
    rCast.Evaluation = r;
    rCast.Purpose = RPurpose::Shuffle;
    rCast.MinimumHits = 2;
    Require(ShouldCastR(rCast),
            "verified two-target shuffle with follow-up and exit should cast");
    rCast.FrontToBackDpsAvailable = true;
    Require(!ShouldCastR(rCast),
            "front-to-back DPS should beat a merely two-target highlight shuffle");
    rCast = {};
    rCast.RReady = rCast.ThreatCommitted = true;
    rCast.Evaluation = r;
    rCast.Purpose = RPurpose::Peel;
    Require(ShouldCastR(rCast),
            "committed divers should be peeled without offensive cursor gates");

    ShuffleGate shuffle{};
    shuffle.AutomaticEnabled = shuffle.TargetValid =
        shuffle.WReadyOrAnchor = shuffle.EReady = shuffle.QReady =
        shuffle.RReady = shuffle.CursorAgrees = shuffle.AlliedFollowup =
        shuffle.ExitAvailable = shuffle.KeyCrowdControlSpent = true;
    Require(MayStartShuffle(shuffle),
            "full safe resource gate should permit an automatic shuffle");
    shuffle.FrontToBackDpsAvailable = true;
    Require(!MayStartShuffle(shuffle),
            "automatic shuffle must yield to valuable front-to-back damage");
    shuffle.ManualKey = true;
    Require(MayStartShuffle(shuffle),
            "manual player intent may authorize the otherwise safe route");
    shuffle.TurretRisk = true;
    Require(!MayStartShuffle(shuffle),
            "manual intent still cannot authorize a known lethal turret route");

    SunDiscContext disc{};
    disc.PassiveReady = disc.RuinInRange = disc.PlayerChannelSafe = true;
    disc.ObjectiveSoon = disc.TeamCanUseZone = true;
    disc.AlliedFollowup = 2;
    Require(ShouldSuggestSunDisc(disc),
            "coach should suggest a safe objective Sun Disc with teammates");
    disc.PlayerLeavingArea = true;
    Require(!ShouldSuggestSunDisc(disc),
            "Sun Disc should not be suggested while Azir is leaving its zone");
    disc.PlayerLeavingArea = false;
    disc.EnemyCanImmediatelyDestroy = true;
    Require(!ShouldSuggestSunDisc(disc),
            "a disposable enemy-controlled Disc is not a strategic cast");

    SequenceState sequence{};
    sequence.Kind = Sequence::WAutoQAuto;
    sequence.Phase = SequencePhase::AwaitFirstAttack;
    sequence.StartedTick = 1000;
    sequence.LastTransitionTick = 1100;
    sequence.ExpireTick = 2500;
    Require(!CanAdvanceSequence(sequence, 1150, 80) &&
                CanAdvanceSequence(sequence, 1200, 80) &&
                SequenceExpired(sequence, 2501),
            "combo phases must respect their timing gate and hard expiry");

    std::cout << "ALL AIAZIR GEOMETRY TESTS PASSED\n";
    return 0;
}
