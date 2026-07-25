#include <cmath>
#include <cstdio>
#include <limits>
#include <string_view>
#include <vector>

#include "tests/ZDEvadeTestSupport.h"
#include "../plugins/ZDEvade/Evade/EvadeRoutingPolicy.h"

using namespace ZDEvade;
using ZDEvadeTest::ExpectNear;
using ZDEvadeTest::ExpectTrue;

int main() {
    const SpellData fixtureSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    const Threat fixtureThreat = ZDEvadeTest::MakeThreat(fixtureSpell);
    ExpectTrue("shared spell and threat fixtures preserve type and data",
               fixtureThreat.HasData() &&
               fixtureThreat.Type() == ZDSpellType::Cone &&
               fixtureThreat.data->spellType == fixtureSpell.spellType);

    ExpectTrue("numerical outward epsilon is positive",
               kNumericalOutwardEpsilon > 0.0f);
    ExpectTrue("numerical outward epsilon remains within one unit",
               kNumericalOutwardEpsilon <= 1.0f);
    ExpectNear("default endpoint margin is eighteen",
               kDefaultEndpointMargin,
               18.0f);
    ExpectTrue("menu endpoint default shares the policy constant",
               kEndpointMarginMenuDefault == kDefaultEndpointMargin &&
                   kEndpointMarginMenuMinimum == 0 &&
                   kEndpointMarginMenuMaximum == 30);
    ExpectTrue("endpoint persistence key is versioned for new default",
               std::string_view(kEndpointMarginMenuPersistenceId) ==
                   "exitMarginV4");
    ExpectNear("hero radius sanitizer preserves actual footprint",
               SanitizeHeroRadius(65.0f),
               65.0f);
    ExpectNear("hero radius sanitizer clamps undersized footprint",
               SanitizeHeroRadius(0.0f),
               kMinimumHeroRadius);
    ExpectNear("hero radius sanitizer rejects negative footprint",
               SanitizeHeroRadius(-65.0f),
               kMinimumHeroRadius);
    ExpectNear("hero radius sanitizer rejects nonfinite footprint",
               SanitizeHeroRadius(
                   std::numeric_limits<float>::quiet_NaN()),
               kMinimumHeroRadius);
    ExpectNear("hero radius sanitizer rejects infinite footprint",
               SanitizeHeroRadius(
                   std::numeric_limits<float>::infinity()),
               kMinimumHeroRadius);
    ExpectNear("configured model-edge collision boundary",
               ExitCollisionDistance(80.0f, 65.0f, kDefaultEndpointMargin, 0.0f),
               163.0f);
    ExpectNear("default target clearance is 18.25 plus uncertainty",
               ExitCenterDistance(80.0f, 65.0f, kDefaultEndpointMargin, 0.0f),
               163.25f);
    ExpectNear("default target clearance adds uncertainty once",
               ExitCenterDistance(80.0f, 65.0f, kDefaultEndpointMargin, 7.0f) -
                   80.0f - 65.0f,
               25.25f);
    ExpectNear("uncertainty added once",
               ExitCenterDistance(80.0f, 65.0f, 10.0f, 7.0f),
               162.25f);
    ExpectNear("negative values clamp",
               ExitCenterDistance(-80.0f, 65.0f, -10.0f, -7.0f),
               65.25f);
    const float defaultReachTolerance =
        EndpointReachTolerance(kDefaultEndpointMargin);
    ExpectTrue("default endpoint reach tolerance remains within two to four",
               defaultReachTolerance >= 3.5f &&
                   defaultReachTolerance <= 4.0f);
    const float realizedEdgeClearance =
        kDefaultEndpointMargin + kNumericalOutwardEpsilon -
        defaultReachTolerance;
    ExpectNear("default four-unit shortfall realizes 14.25 edge clearance",
               kDefaultEndpointMargin + kNumericalOutwardEpsilon - 4.0f,
               14.25f);
    ExpectTrue("boundary reach preserves at least 14.25 edge clearance",
               realizedEdgeClearance >= 14.25f);
    ExpectTrue("four units short obeys endpoint boundary policy",
               IsRouteTargetReached(
                   4.0f,
                   kDefaultEndpointMargin,
                   false) ==
                   (4.0f <= defaultReachTolerance));
    ExpectTrue("exact-danger position is never endpoint-reached",
               !IsRouteTargetReached(
                   1.0f,
                   kDefaultEndpointMargin,
                   true));
    const float zeroMarginReachTolerance =
        EndpointReachTolerance(0.0f);
    ExpectNear("zero endpoint margin uses two-unit reach tolerance",
               zeroMarginReachTolerance,
               2.0f);
    ExpectTrue("zero-margin exact danger vetoes early completion",
               !IsRouteTargetReached(
                   zeroMarginReachTolerance,
                   0.0f,
                   true) &&
                   !IsMoveTargetReached(
                       zeroMarginReachTolerance,
                       zeroMarginReachTolerance,
                       true));
    ExpectTrue("zero-margin endpoint completes once exact-safe",
               IsRouteTargetReached(
                   zeroMarginReachTolerance,
                   0.0f,
                   false) &&
                   IsMoveTargetReached(
                       zeroMarginReachTolerance,
                       zeroMarginReachTolerance,
                       false));

    ThreatCoverage dangerFour;
    dangerFour.collisionCount = 1;
    dangerFour.endpointDanger = 4;
    dangerFour.maxDanger = 4;
    dangerFour.pathDanger = 4;
    dangerFour.dangerExposureMs = 120.0f;
    dangerFour.firstCollisionTimeMs = 80.0f;
    ThreatCoverage dangerOne = dangerFour;
    dangerOne.endpointDanger = 1;
    dangerOne.maxDanger = 1;
    dangerOne.pathDanger = 1;
    ExpectTrue("equal-count danger one improves over danger four",
               ImprovesThreatCoverage(dangerOne, dangerFour));

    ThreatCoverage lowerPathDanger = dangerOne;
    lowerPathDanger.pathDanger = 0;
    ExpectTrue("one-to-one lower path danger improves coverage",
               ImprovesThreatCoverage(lowerPathDanger, dangerOne));

    ThreatCoverage lowerExposure = dangerOne;
    lowerExposure.dangerExposureMs = 90.0f;
    ExpectTrue("one-to-one lower exposure improves coverage",
               ImprovesThreatCoverage(lowerExposure, dangerOne));

    ThreatCoverage laterCollision = dangerOne;
    laterCollision.firstCollisionTimeMs = 140.0f;
    ExpectTrue("later first collision is final coverage tie-break",
               ImprovesThreatCoverage(laterCollision, dangerOne));
    ExpectTrue("earlier first collision does not improve coverage",
               !ImprovesThreatCoverage(dangerOne, laterCollision));
    ExpectTrue("equal fallback coverage remains no worse",
               ThreatCoverageNoWorse(dangerOne, dangerOne));
    ExpectTrue("strictly improved fallback coverage remains no worse",
               ThreatCoverageNoWorse(lowerPathDanger, dangerOne));
    ThreatCoverage increasedCollision = dangerOne;
    ++increasedCollision.collisionCount;
    increasedCollision.dangerExposureMs = 20.0f;
    ExpectTrue("increased fallback collision count invalidates lock",
               !ThreatCoverageNoWorse(increasedCollision, dangerOne));
    ThreatCoverage increasedPathDanger = dangerOne;
    ++increasedPathDanger.pathDanger;
    increasedPathDanger.dangerExposureMs = 20.0f;
    ExpectTrue("increased fallback path danger invalidates lock",
               !ThreatCoverageNoWorse(increasedPathDanger, dangerOne));

    const Vec2 hero(100.0f, 100.0f);
    const Vec2 goal(900.0f, 100.0f);
    const DetourEnvelope detours[] = {
        {DetourGeometry::Line, Vec2(500.0f, 0.0f), Vec2(500.0f, 200.0f),
         Vec2(), Vec2(), 0.0f, 0.0f, 100.0f, 0.0f},
        {DetourGeometry::Circular, Vec2(), Vec2(), Vec2(500.0f, 100.0f),
         Vec2(), 0.0f, 0.0f, 100.0f, 0.0f},
        {DetourGeometry::Ring, Vec2(), Vec2(), Vec2(500.0f, 100.0f),
         Vec2(), 0.0f, 45.0f, 100.0f, 0.0f},
        {DetourGeometry::Cone, Vec2(), Vec2(), Vec2(400.0f, 100.0f),
         Vec2(1.0f, 0.0f), 240.0f, 0.0f, 20.0f, 0.45f},
        {DetourGeometry::Arc, Vec2(500.0f, 0.0f), Vec2(500.0f, 200.0f),
         Vec2(), Vec2(), 0.0f, 0.0f, 100.0f, 0.0f},
    };
    const char* detourNames[] = {"line", "circle", "ring", "cone", "arc"};
    for (int index = 0; index < 5; ++index) {
        const DetourEnvelope& envelope = detours[index];
        ExpectTrue(detourNames[index], RouteNeedsDetour(envelope, hero, goal));
        const std::vector<Vec2> candidates =
            BuildDetourCandidates(envelope, hero, goal);
        ExpectTrue("intersecting threat yields detour candidates",
                   !candidates.empty());
        bool hasSafelyOutsideCandidate = false;
        for (const Vec2& candidate : candidates) {
            if (DetourSignedClearance(envelope, candidate) >
                kNumericalOutwardEpsilon * 0.5f) {
                hasSafelyOutsideCandidate = true;
            }
        }
        ExpectTrue("detour includes safely outside candidate",
                   hasSafelyOutsideCandidate);
    }
    ExpectTrue("non-intersecting route does not detour",
               !RouteNeedsDetour(detours[1], hero, Vec2(300.0f, 100.0f)));
    ExpectTrue("hero already inside is ordinary evade",
               !RouteNeedsDetour(detours[1], Vec2(500.0f, 100.0f), goal));

    const DetourEnvelope innerRing = {
        DetourGeometry::Ring,
        Vec2(),
        Vec2(),
        Vec2(500.0f, 100.0f),
        Vec2(),
        0.0f,
        100.0f,
        180.0f,
        0.0f,
    };
    ExpectTrue("ring inner-hole route requests detour",
               RouteNeedsDetour(innerRing, Vec2(500.0f, 100.0f), goal));
    ExpectTrue("ring inner-hole yields validated candidate seeds",
               !BuildDetourCandidates(
                    innerRing, Vec2(500.0f, 100.0f), goal).empty());

    ExpectTrue("walking without usable lock cancels stale movement",
               ShouldCancelUnsafeMovement(true, false));
    ExpectTrue("usable walking lock keeps movement",
               !ShouldCancelUnsafeMovement(true, true));
    ExpectTrue("disabled walking does not issue stop",
               !ShouldCancelUnsafeMovement(false, false));

    const SweptCircleGridGeometry testGrid = {
        0.0f,
        0.0f,
        100.0f,
        100.0f,
        10.0f,
        10,
        10,
    };
    const auto testCellFlags = [](int x, int y) {
        return x == 4 && y == 4
            ? CoreNavGrid::kInvalidRawFlags
            : static_cast<std::uint16_t>(0);
    };
    const std::vector<Vec2> blockedStraight = {
        Vec2(10.0f, 45.0f),
        Vec2(90.0f, 45.0f),
    };
    ExpectTrue("blocked straight candidate is rejected",
               !SweptCirclePathWalkable(
                   blockedStraight,
                   2.0f,
                   testGrid,
                   testCellFlags));
    const std::vector<Vec2> bendingAroundCell = {
        Vec2(10.0f, 45.0f),
        Vec2(35.0f, 25.0f),
        Vec2(65.0f, 25.0f),
        Vec2(90.0f, 45.0f),
    };
    ExpectTrue("supplied bending polyline around blocked cell passes",
               SweptCirclePathWalkable(
                   bendingAroundCell,
                   2.0f,
                   testGrid,
                   testCellFlags));
    const auto adjacentWallFlags = [](int x, int y) {
        return x == 4 && y == 4
            ? static_cast<std::uint16_t>(
                  Offset::NavGridCellLayout::CELL_WALL)
            : static_cast<std::uint16_t>(0);
    };
    ExpectTrue("point disk rejects adjacent overlapping wall",
               !SweptCirclePointWalkable(
                   Vec2(39.0f, 45.0f),
                   2.0f,
                   testGrid,
                   adjacentWallFlags));
    ExpectTrue("point disk rejects adjacent invalid cell",
               !SweptCirclePointWalkable(
                   Vec2(39.0f, 45.0f),
                   2.0f,
                   testGrid,
                   testCellFlags));
    ExpectTrue("point disk accepts enough adjacent wall clearance",
               SweptCirclePointWalkable(
                   Vec2(37.0f, 45.0f),
                   2.0f,
                   testGrid,
                   adjacentWallFlags));
    ExpectTrue("point disk rejects tangent wall boundary",
               !SweptCirclePointWalkable(
                   Vec2(38.0f, 45.0f),
                   2.0f,
                   testGrid,
                   adjacentWallFlags));
    ExpectTrue("point disk fails closed outside grid",
               !SweptCirclePointWalkable(
                   Vec2(1.0f, 50.0f),
                   2.0f,
                   testGrid,
                   adjacentWallFlags));
    SweptCircleGridGeometry invalidPointGrid = testGrid;
    invalidPointGrid.cellSize = 0.0f;
    ExpectTrue("point disk fails closed for invalid grid",
               !SweptCirclePointWalkable(
                   Vec2(37.0f, 45.0f),
                   2.0f,
                   invalidPointGrid,
                   adjacentWallFlags));
    ExpectTrue("point disk requires a positive radius",
               !SweptCirclePointWalkable(
                   Vec2(35.0f, 45.0f),
                   0.0f,
                   testGrid,
                   adjacentWallFlags));
    ExpectTrue("test epsilon retains zero-radius nav semantics",
               SweptCirclePointWalkable(
                   Vec2(35.0f, 45.0f),
                   kZeroRadiusNavValidationEpsilon,
                   testGrid,
                   adjacentWallFlags));

    StrictRouteRank nearExit;
    nearExit.exitDistance = 90.0f;
    nearExit.travelDistance = 100.0f;
    nearExit.timeMarginMs = 40.0f;
    nearExit.minimumClearance = 2.0f;

    StrictRouteRank farExit = nearExit;
    farExit.exitDistance = 180.0f;
    farExit.travelDistance = 190.0f;
    farExit.timeMarginMs = 300.0f;
    farExit.minimumClearance = 80.0f;
    ExpectTrue("near strict exit wins over extra clearance",
               PreferStrictRoute(nearExit, farExit));

    StrictRouteRank turretExit = nearExit;
    turretExit.exitDistance = 40.0f;
    turretExit.turretPenalty = 100.0f;
    ExpectTrue("turret penalty remains first",
               PreferStrictRoute(nearExit, turretExit));

    FallbackRouteRank collisionAt40;
    collisionAt40.endpointDanger = 1;
    collisionAt40.maxDanger = 2;
    collisionAt40.collisionCount = 1;
    collisionAt40.dangerExposureMs = 80.0f;
    collisionAt40.pathDanger = 2;
    collisionAt40.firstCollisionTimeMs = 40.0f;
    collisionAt40.timeMarginMs = 15.0f;
    collisionAt40.requiresStartEnvelopeExit = true;
    collisionAt40.exitedStartEnvelope = true;
    collisionAt40.exitDistance = 40.0f;
    collisionAt40.travelDistance = 45.0f;

    FallbackRouteRank collisionAt400 = collisionAt40;
    collisionAt400.firstCollisionTimeMs = 400.0f;
    collisionAt400.timeMarginMs = 375.0f;
    collisionAt400.exitDistance = 220.0f;
    collisionAt400.travelDistance = 240.0f;
    ExpectTrue("shorter true fallback exit beats later collision",
               PreferFallbackRoute(collisionAt40, collisionAt400));
    ExpectTrue("later collision cannot beat shorter true exit",
               !PreferFallbackRoute(collisionAt400, collisionAt40));

    FallbackRouteRank largerTimeMargin = collisionAt400;
    largerTimeMargin.timeMarginMs = 420.0f;
    largerTimeMargin.exitDistance = 300.0f;
    ExpectTrue("true exit distance precedes fallback time margin",
               PreferFallbackRoute(collisionAt400, largerTimeMargin));

    FallbackRouteRank noExitWithLateCollision = collisionAt40;
    noExitWithLateCollision.exitedStartEnvelope = false;
    noExitWithLateCollision.exitDistance =
        std::numeric_limits<float>::infinity();
    noExitWithLateCollision.travelDistance = 300.0f;
    noExitWithLateCollision.firstCollisionTimeMs = 900.0f;
    noExitWithLateCollision.timeMarginMs = 900.0f;
    ExpectTrue("genuine start-envelope exit outranks no-exit timing",
               PreferFallbackRoute(
                   collisionAt40,
                   noExitWithLateCollision) &&
                   !PreferFallbackRoute(
                       noExitWithLateCollision,
                       collisionAt40));

    FallbackRouteRank entersNewThreat = collisionAt40;
    entersNewThreat.enteredNewThreat = true;
    entersNewThreat.exitDistance = 10.0f;
    entersNewThreat.travelDistance = 15.0f;
    ExpectTrue("new-threat entry precedes egress distance ranking",
               PreferFallbackRoute(
                   collisionAt40,
                   entersNewThreat) &&
                   !PreferFallbackRoute(
                       entersNewThreat,
                       collisionAt40));

    FallbackRouteRank startsOutsideShort = collisionAt40;
    startsOutsideShort.requiresStartEnvelopeExit = false;
    startsOutsideShort.exitedStartEnvelope = false;
    startsOutsideShort.exitDistance = 0.0f;
    FallbackRouteRank startsOutsideLate = collisionAt400;
    startsOutsideLate.requiresStartEnvelopeExit = false;
    startsOutsideLate.exitedStartEnvelope = false;
    startsOutsideLate.exitDistance = 0.0f;
    ExpectTrue("starts-outside preserves later-collision priority",
               PreferFallbackRoute(
                   startsOutsideLate,
                   startsOutsideShort) &&
                   !PreferFallbackRoute(
                       startsOutsideShort,
                       startsOutsideLate));
    FallbackRouteRank mixedStartContext = collisionAt40;
    mixedStartContext.requiresStartEnvelopeExit = false;
    mixedStartContext.exitedStartEnvelope = false;
    ExpectTrue("mixed start contexts have deterministic ordering",
               PreferFallbackRoute(
                   mixedStartContext,
                   collisionAt40) !=
                   PreferFallbackRoute(
                       collisionAt40,
                       mixedStartContext));

    FallbackRouteRank twoThreatCoverage = collisionAt40;
    twoThreatCoverage.collisionCount = 2;
    twoThreatCoverage.maxDanger = 4;
    twoThreatCoverage.pathDanger = 4;
    FallbackRouteRank betterMultiThreatCoverage =
        noExitWithLateCollision;
    betterMultiThreatCoverage.collisionCount = 1;
    betterMultiThreatCoverage.maxDanger = 2;
    betterMultiThreatCoverage.pathDanger = 2;
    ExpectTrue("multi-threat coverage dominance precedes exit evidence",
               PreferFallbackRoute(
                   betterMultiThreatCoverage,
                   twoThreatCoverage));

    FallbackRouteRank saferPath = collisionAt400;
    saferPath.pathDanger = 1;
    saferPath.dangerExposureMs = 180.0f;
    FallbackRouteRank lowerExposureButHigherPath = collisionAt400;
    lowerExposureButHigherPath.pathDanger = 3;
    lowerExposureButHigherPath.dangerExposureMs = 10.0f;
    ExpectTrue("higher path danger loses despite lower exposure",
               PreferFallbackRoute(saferPath, lowerExposureButHigherPath));
    ExpectTrue("lower exposure cannot overcome higher path danger",
               !PreferFallbackRoute(lowerExposureButHigherPath, saferPath));

    LockedRouteStatus stable;
    stable.hasLock = true;
    stable.valid = true;
    stable.walkable = true;
    stable.pathSafe = true;
    stable.endpointSafe = true;
    ExpectTrue("keep safe strict route", KeepStrictRoute(stable));

    LockedRouteStatus newThreatBlocked = stable;
    newThreatBlocked.pathSafe = false;
    ExpectTrue("replace route blocked by new threat",
               !KeepStrictRoute(newThreatBlocked));

    LockedRouteStatus badEndpoint = stable;
    badEndpoint.endpointSafe = false;
    ExpectTrue("replace unsafe endpoint", !KeepStrictRoute(badEndpoint));

    LockedRouteStatus reached = stable;
    reached.reachedTarget = true;
    ExpectTrue("replace reached target", !KeepStrictRoute(reached));

    DeferredDestination deferred;
    ExpectTrue("deferred starts empty", !deferred.HasValue());
    deferred.Record(Vec2(500.0f, 100.0f), 1000);
    ExpectTrue("unsafe command remembered", deferred.HasValue());
    ExpectTrue("remember destination",
               deferred.Position().Distance(Vec2(500.0f, 100.0f)) < 0.001f);
    ExpectTrue("remember tick", deferred.Tick() == 1000);
    deferred.Record(Vec2(700.0f, 200.0f), 1100);
    ExpectTrue("new unsafe command replaces old",
               deferred.Position().Distance(Vec2(700.0f, 200.0f)) < 0.001f);
    ExpectTrue("unsafe direct route requests detour",
               DecideDeferredRoute(true, false) == DeferredRouteAction::Detour);
    ExpectTrue("safe direct route requests resume",
               DecideDeferredRoute(true, true) == DeferredRouteAction::Resume);
    deferred.Clear();
    ExpectTrue("clear deferred", !deferred.HasValue());

    return ZDEvadeTest::Finish("ZDEVADE ROUTING POLICY");
}
