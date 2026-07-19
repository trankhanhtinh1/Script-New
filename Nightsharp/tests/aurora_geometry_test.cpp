#include "../plugins/Champion/KuroAIO/AI/Controllers/AIAuroraGeometry.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <vector>

using namespace Plugins::KuroAIO::AI::Controllers::Aurora::Geometry;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

bool Near(float left, float right, float epsilon = 0.01f) {
    return std::fabs(left - right) <= epsilon;
}

} // namespace

int main() {
    Require(Near(PassiveMaximumHealthPercent(0.0f), 0.01f) &&
                Near(PassiveMaximumHealthPercent(100.0f), 0.037f),
            "passive must use one percent plus 2.7 percent per 100 AP");
    Require(Near(PassiveRawDamage(1000.0f, 100.0f), 37.0f) &&
                Near(PassiveRawDamage(1000.0f, 100.0f, true, 1), 10.0f),
            "passive must retain champion max-health damage and its level-scaled monster cap");
    Require(Near(SpiritHealPerSecond(18, 100.0f), 22.0f) &&
                Near(TotalSpiritHealPerSecond(4, 18, 100.0f), 88.0f),
            "four spirits must independently heal at current 3-20 plus two percent AP");
    Require(NormalizePassiveStacks(2, false) == 2 &&
                NormalizePassiveStacks(4, true) == 2 &&
                NormalizePassiveStacks(6, true) == 0,
            "passive telemetry must normalize both 1/2/3 and 2/4/6 bridges");

    PassiveState passive{};
    passive = AdvancePassive(passive, 2, 0.0f);
    Require(passive.Stacks == 2 && passive.Procs == 0,
            "two applications must arm but not proc Across the Veil");
    passive = AdvancePassive(passive, 1, 0.20f);
    Require(passive.Stacks == 0 && passive.Procs == 1,
            "the third attack or spell must proc the passive exactly once");
    passive = AdvancePassive(passive, 2, 4.01f);
    Require(passive.Stacks == 2 && passive.Procs == 1,
            "expired passive stacks must clear before new applications arrive");

    Require(Near(QBaseDamage(5, 100.0f), 185.0f),
            "Q1 and base Q2 must use 145 plus forty percent AP at rank five");
    Require(Near(Q2MissingHealthMultiplier(0.0f), 1.0f) &&
                Near(Q2MissingHealthMultiplier(1.0f), 1.5f),
            "Q2 must scale continuously up to 1.5x at zero target health");
    Require(Near(Q2BoltRawDamage(5, 100.0f, 1.0f), 277.5f) &&
                Near(Q2BoltRawDamage(5, 100.0f, 1.0f, true, true), 22.2f),
            "Q2 must separate missing-health, additional-bolt and minion modifiers");

    const Vec3 origin{ 0.0f, 0.0f, 0.0f };
    const Vec3 qEnd{ 900.0f, 0.0f, 0.0f };
    LineUnit target{ Vec3{ 500.0f, 0.0f, 0.0f }, 65.0f,
                     10, true, false, true };
    LineUnit edge{ Vec3{ 500.0f, 0.0f, 108.0f }, 65.0f,
                   11, true, false, true };
    LineUnit miss{ Vec3{ 500.0f, 0.0f, 120.0f }, 65.0f,
                   12, true, false, true };
    Require(QLineHits(origin, qEnd, target) && QLineHits(origin, qEnd, edge) &&
                !QLineHits(origin, qEnd, miss),
            "Q outbound must use missile half-width plus target radius");
    const auto lineIds = QLineHitIds(origin, qEnd, { target, edge, miss });
    Require(lineIds.size() == 2 && lineIds[0] == 10 && lineIds[1] == 11,
            "Q1 must pierce and mark every body actually intersecting its line");

    std::vector<QMark> marks = {
        { target.Position, 65.0f, 10, 2.0f, true, false, true },
        { Vec3{ 720.0f, 0.0f, 0.0f }, 35.0f,
          20, 2.0f, false, true, true },
        { Vec3{ 700.0f, 0.0f, 330.0f }, 35.0f,
          21, 2.0f, false, true, true },
    };
    const auto aligned = EvaluateQReturn(
        target, marks, Vec3{ 0.0f, 0.0f, 0.0f });
    Require(aligned.Bolts == 2 && aligned.CrossingBolts == 1 &&
                aligned.OwnBolt && Near(aligned.DamageUnits, 1.2f),
            "aligned pullback must award one full bolt and twenty percent for the crossing bolt");
    const auto moved = EvaluateQReturn(
        target, marks, Vec3{ 500.0f, 0.0f, 600.0f });
    Require(moved.Bolts >= 1 && moved.OwnBolt,
            "the originally marked target's own Q2 bolt must remain guaranteed after repositioning");

    QRecastContext recast{};
    recast.MarkActive = recast.ControllerOwned = recast.TargetValid = true;
    recast.AutoAttackWindup = true;
    recast.RemainingSeconds = 0.60f;
    Require(!ShouldRecastQ(recast),
            "Q2 must not cancel a valuable player auto windup without urgency");
    recast.LethalNow = true;
    Require(ShouldRecastQ(recast),
            "lethal Q2 may override the protected auto windup");
    recast = {};
    recast.MarkActive = recast.ControllerOwned = recast.TargetValid = true;
    recast.EReady = recast.EWouldHit = true;
    recast.RemainingSeconds = 0.80f;
    Require(!ShouldRecastQ(recast),
            "single-target Q2 must wait for E damage when the mark has time");
    recast.WaveSequence = recast.EWouldKillMarkedWave = true;
    Require(ShouldRecastQ(recast),
            "wave Q2 must fire before E deletes marked minions and their extra bolts");
    Require(PreferQ2BeforeE(true, 4, true, false) &&
                !PreferQ2BeforeE(false, 0, true, true),
            "Q2-before-E is a marked-wave exception, not the ordinary champion combo");

    Require(Near(WInvisibilitySeconds(1), 1.0f) &&
                Near(WInvisibilitySeconds(5), 1.6f) &&
                Near(WMovementSpeedPercent(5), 0.40f),
            "W must use current rank-scaled invisibility and Realm Hopper speed");
    Require(Near(WBaseEndpoint(origin, Vec3{ 600.0f, 0.0f, 0.0f }).x,
                 300.0f),
            "ordinary W must dash exactly 300 units");
    Require(Near(WResolvedEndpoint(
                     origin, Vec3{ 600.0f, 0.0f, 0.0f },
                     Vec3{ 430.0f, 0.0f, 0.0f }, true).x,
                 430.0f),
            "W may extend to a validated terrain exit inside 450 units");
    Require(Near(WResolvedEndpoint(
                     origin, Vec3{ 600.0f, 0.0f, 0.0f },
                     Vec3{ 520.0f, 0.0f, 0.0f }, true).x,
                 300.0f),
            "W must reject an impossible wall exit beyond its forgiveness");

    WRouteContext safeW{};
    safeW.EndpointValid = safeW.CursorAgrees = safeW.TerrainReachable = true;
    safeW.CreatesSpellAngle = safeW.ConcealsTurn = true;
    safeW.TakedownLikely = safeW.DamagedChampionRecently = true;
    safeW.AlliesAtEndpoint = 1;
    safeW.DistanceFromThreat = 400.0f;
    Require(ShouldSpendW(safeW),
            "W may be spent before a likely takedown when it creates a real spell angle");
    safeW.EnemyTurret = true;
    Require(!ShouldSpendW(safeW),
            "W reset optimism must never excuse an enemy-turret endpoint");
    safeW.EnemyTurret = false;
    safeW.PlayerWindingUp = true;
    Require(!ShouldSpendW(safeW),
            "offensive W must preserve the player's attack windup");
    safeW.Defensive = safeW.EscapesThreat = true;
    Require(ShouldSpendW(safeW),
            "a genuinely defensive W may override attack preservation");

    Require(Near(ERawDamage(5, 100.0f), 300.0f),
            "E must use current 230 plus seventy percent AP at rank five");
    Require(Near(ERecoilEndpoint(
                     origin, Vec3{ 800.0f, 0.0f, 0.0f }, false).x,
                 -250.0f) &&
                Near(ERecoilEndpoint(
                     origin, Vec3{ 800.0f, 0.0f, 0.0f }, true).x,
                 0.0f),
            "E must recoil 250 opposite its cast unless grounded or rooted");
    LineUnit eMiss = miss;
    eMiss.Position.z = 160.0f;
    Require(ELineHits(origin, Vec3{ 825.0f, 0.0f, 0.0f }, target) &&
                !ELineHits(origin, Vec3{ 825.0f, 0.0f, 0.0f }, eMiss),
            "E must use its non-projectile 825 by 175 line hitbox");

    ECastContext eCast{};
    eCast.TargetHit = eCast.RecoilEndpointValid = eCast.AllIn = true;
    Require(ShouldCastE(eCast),
            "all-in E may finish the sequence when its recoil endpoint is safe");
    eCast.EndpointTurret = true;
    Require(!ShouldCastE(eCast),
            "E must reject a recoil endpoint inside an enemy turret");
    eCast.EndpointTurret = false;
    eCast.IncomingDisplacement = eCast.CanBufferDisplacement = true;
    Require(ShouldCastE(eCast),
            "E may deliberately buffer a displacement when the landing remains safe");
    eCast = {};
    eCast.TargetHit = eCast.RecoilEndpointValid = eCast.AllIn = true;
    eCast.FinalMobilityResource = true;
    eCast.TargetCanBeChasedAfter = false;
    Require(!ShouldCastE(eCast),
            "E must be held when early recoil would end an otherwise winnable chase");

    Require(Near(RRawDamage(3, 100.0f), 445.0f) &&
                Near(RArenaDuration(1), 2.5f) &&
                Near(RArenaDuration(3), 4.0f) &&
                Near(RBoundarySlowSeconds(2), 1.75f),
            "R must use current 70 AP ratio, arena duration and boundary slow ranks");
    const RPlacement placement = ResolveRPlacement(
        origin, Vec3{ 700.0f, 0.0f, 0.0f });
    Require(placement.Valid && Near(placement.LeapEndpoint.x, 250.0f) &&
                Near(placement.ArenaCenter.x, 425.0f),
            "R must distinguish its 250 leap from the approximately 425-ahead arena center");
    const Vec3 boundary = BoundaryContactForRay(
        origin, origin, Vec3{ 1000.0f, 0.0f, 0.0f });
    const Vec3 portal = PortalDestination(origin, boundary);
    Require(Near(boundary.x, 700.0f) && Near(portal.x, -700.0f),
            "touching the eastern R edge must teleport Aurora to the opposite western edge");
    Require(NearPortalBoundary(Vec3{ 670.0f, 0.0f, 0.0f }, origin) &&
                !NearPortalBoundary(Vec3{ 500.0f, 0.0f, 0.0f }, origin),
            "portal activation must require actual boundary proximity");

    PortalContext portalContext{};
    portalContext.ArenaActive = portalContext.PortalReady =
        portalContext.NearBoundary = portalContext.DestinationSafe = true;
    portalContext.RemainingSeconds = 1.0f;
    portalContext.TurretShotPending = true;
    Require(ShouldUsePortal(portalContext),
            "R portal may intentionally drop a pending turret shot");
    portalContext.TurretShotPending = false;
    portalContext.IncomingOneInstanceCrowdControl = true;
    Require(ShouldUsePortal(portalContext),
            "R portal may buffer one-instance crowd control through untargetability");
    portalContext.IncomingSuppressionOrLongCrowdControl = true;
    Require(!ShouldUsePortal(portalContext),
            "R portal must not pretend to erase suppression or long-duration crowd control");
    portalContext.IncomingSuppressionOrLongCrowdControl = false;
    portalContext.IncomingOneInstanceCrowdControl = false;
    portalContext.WReady = true;
    Require(!ShouldUsePortal(portalContext),
            "W should remain an independent resource when portal combination adds no value");
    portalContext.CombiningMobilityAddsValue = true;
    Require(ShouldUsePortal(portalContext),
            "W plus portal is allowed only for an explicitly valuable hidden bounce");

    REarlyEndContext endR{};
    endR.ArenaActive = endR.RecastReady = true;
    endR.ForcedUnsafePortal = true;
    endR.RemainingSeconds = 1.2f;
    Require(ShouldEndR(endR),
            "R should end before a forced portal sends Aurora into danger");
    endR.ForcedUnsafePortal = false;
    endR.TargetEscaped = true;
    endR.IncomingThreatCanBePortaled = false;
    Require(ShouldEndR(endR),
            "R may end after the target escapes and no defensive portal value remains");

    std::vector<RUnit> rUnits = {
        { Vec3{ 425.0f, 0.0f, 0.0f }, 65.0f, 2.0f,
          true, true, false, false, true },
        { Vec3{ 700.0f, 0.0f, 100.0f }, 65.0f, 1.7f,
          false, false, true, false, true },
        { Vec3{ 1300.0f, 0.0f, 0.0f }, 65.0f, 2.0f,
          false, false, false, false, true },
    };
    const auto rEval = EvaluateR(
        placement.ArenaCenter, rUnits, 2, 2, true);
    Require(rEval.Hits == 2 && rEval.PriorityHits == 2 &&
                rEval.PrimaryHit && rEval.Score > 600.0f,
            "R planner must value priority coverage, allied follow-up and two passive procs");
    const auto impossibleR = EvaluateR(
        placement.ArenaCenter, rUnits, 2, 2, false);
    Require(impossibleR.Score <= -FLT_MAX * 0.5f,
            "R must reject terrain-corrupted placements before scoring hits");
    RCastContext rCast{};
    rCast.TerrainFeasible = rCast.LeapEndpointSafe =
        rCast.FollowupReady = true;
    rCast.MinimumHits = 2;
    rCast.Evaluation = rEval;
    Require(ShouldCastR(rCast),
            "offensive R requires a real primary, multi-hit and follow-up package");
    rCast.PlayerWindingUp = true;
    Require(!ShouldCastR(rCast),
            "ordinary offensive R must preserve the player's attack windup");
    rCast.PlayerWindingUp = false;
    rCast.DefensiveBuffer = rCast.IncomingOneInstanceCrowdControl = true;
    Require(ShouldCastR(rCast),
            "R leap may intentionally buffer one-instance crowd control");
    rCast.IncomingSuppressionOrLongCrowdControl = true;
    Require(!ShouldCastR(rCast),
            "R leap must not be spent as a fake answer to suppression or persistent CC");

    ComboContext combo{};
    combo.QReady = true;
    Require(NextComboAction(combo) == ComboAction::Q1,
            "Aurora's ordinary trade must begin with Q before the auto");
    combo.QReady = false;
    combo.QMarkActive = combo.AutoAvailable = combo.AutoSafe = true;
    combo.QRemainingSeconds = 1.5f;
    Require(NextComboAction(combo) == ComboAction::AutoAttackWindow,
            "Q1 must yield a player-owned auto window before Q2 when safe");
    combo.PassiveStacks = 2;
    combo.AutoAvailable = false;
    combo.EReady = true;
    Require(NextComboAction(combo) == ComboAction::E,
            "E should proc two armed passive stacks before the delayed Q2");
    combo.WaveHasMarkedMinions = combo.EWouldKillMarkedMinions = true;
    Require(NextComboAction(combo) == ComboAction::Q2,
            "marked-wave clear must use Q1-Q2-E ordering");
    combo = {};
    combo.AllIn = combo.RReady = combo.RWindow = true;
    Require(NextComboAction(combo) == ComboAction::R,
            "a verified all-in R window may precede Q to create the extended arena fight");

    std::cout << "ALL AURORA GEOMETRY TESTS PASSED\n";
    return 0;
}
