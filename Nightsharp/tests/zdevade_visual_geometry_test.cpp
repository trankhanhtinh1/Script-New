#include "tests/ZDEvadeTestSupport.h"
#include "plugins/ZDEvade/Database/ThreatDatabase.h"
#include "plugins/ZDEvade/Debug/SelfSkillDebugPolicy.h"
#include "plugins/ZDEvade/Detection/ThreatDetectionPolicy.h"
#include "plugins/ZDEvade/Visual/TargetVisualDispatch.h"
#include "plugins/ZDEvade/Visual/ThreatVisualDispatch.h"
#include "plugins/ZDEvade/Visual/ThreatVisualGeometry.h"
#include "plugins/ZDEvade/Visual/ThreatVisualPlan.h"
#include "plugins/ZDEvade/Visual/ThreatVisualStyle.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <limits>

using namespace ZDEvade;
using ZDEvadeTest::ExpectEq;
using ZDEvadeTest::ExpectNear;
using ZDEvadeTest::ExpectTrue;

namespace {

float DistanceToSegment(const Vec2& point,
                        const Vec2& start,
                        const Vec2& end) {
    const Vec2 segment = end - start;
    const float lengthSquared = segment.LengthSqr();
    if (lengthSquared <= 0.00000001f) return point.Distance(start);
    const float t = std::clamp(
        (point - start).Dot(segment) / lengthSquared,
        0.0f,
        1.0f);
    return point.Distance(start + segment * t);
}

void ExpectFinitePath(const char* name, const ThreatVisualPath& path) {
    bool finite = path.count <= path.points.size();
    for (std::size_t index = 0; index < path.count; ++index) {
        finite = finite && path.points[index].IsValid();
    }
    ExpectTrue(name, finite);
}

} // namespace

int main() {
    const Vec2 capsuleStart(0.0f, 0.0f);
    const Vec2 capsuleEnd(100.0f, 0.0f);
    const ThreatVisualPath capsule =
        ThreatVisualGeometry::Capsule(capsuleStart, capsuleEnd, 10.0f);
    ExpectTrue("capsule is closed", capsule.closed);
    ExpectEq("capsule has two sampled superellipse caps",
             static_cast<int>(capsule.count),
             2 * (kThreatVisualCapsuleCapSegments + 1));
    ExpectTrue("capsule count stays bounded",
               capsule.count <= kThreatVisualMaxPoints);
    ExpectFinitePath("capsule points are finite", capsule);
    const float capCircumscription = 1.0f / std::cos(
        kThreatVisualPi /
        (2.0f * static_cast<float>(kThreatVisualCapsuleCapSegments)));
    const float inflatedRadius = 10.0f * capCircumscription;
    const float extentInflation =
        capCircumscription - 1.0f;
    for (std::size_t index = 0; index < capsule.count; ++index) {
        ExpectTrue("capsule never under-draws segment-radius boundary",
                   DistanceToSegment(
                       capsule.points[index],
                       capsuleStart,
                       capsuleEnd) >= 9.998f);
    }
    ExpectNear("capsule starts at end-left x", capsule.points[0].x, 100.0f);
    ExpectNear("capsule starts at end-left y",
               capsule.points[0].y,
               inflatedRadius,
               0.002f);
    ExpectNear("capsule end cap finishes end-right x",
               capsule.points[kThreatVisualCapsuleCapSegments].x,
               100.0f);
    ExpectNear("capsule end cap finishes end-right y",
               capsule.points[kThreatVisualCapsuleCapSegments].y,
               -inflatedRadius,
               0.002f);
    ExpectNear("capsule closes from start-left x",
               capsule.points[capsule.count - 1].x,
               0.0f);
    ExpectNear("capsule closes from start-left y",
               capsule.points[capsule.count - 1].y,
               capsule.points[0].y);
    float capsuleMinY = capsule.points[0].y;
    float capsuleMaxY = capsule.points[0].y;
    for (std::size_t index = 1; index < capsule.count; ++index) {
        capsuleMinY = std::min(capsuleMinY, capsule.points[index].y);
        capsuleMaxY = std::max(capsuleMaxY, capsule.points[index].y);
    }
    ExpectNear("capsule side minimum uses circumscribed radius",
               capsuleMinY,
               -inflatedRadius,
               0.002f);
    ExpectNear("capsule side maximum uses circumscribed radius",
               capsuleMaxY,
               inflatedRadius,
               0.002f);
    ExpectNear("capsule forward extent uses circumscribed radius",
               capsule.points[kThreatVisualCapsuleCapSegments / 2].x,
               100.0f + inflatedRadius,
               0.002f);
    ExpectNear("capsule rear extent uses circumscribed radius",
               capsule.points[
                   kThreatVisualCapsuleCapSegments +
                   1 + kThreatVisualCapsuleCapSegments / 2].x,
               -inflatedRadius,
               0.002f);
    ExpectTrue("default circumscription inflates extents below 0.2 percent",
               extentInflation > 0.0f && extentInflation < 0.002f);

    const std::size_t diagonalIndex = kThreatVisualCapsuleCapSegments / 4;
    const float diagonalRadius =
        capsule.points[diagonalIndex].Distance(capsuleEnd);
    const float maxConservativeCapRadius = 10.0f * std::pow(
        2.0f,
        0.5f - 1.0f / kThreatVisualCapsuleCapExponent) *
        capCircumscription;
    ExpectTrue("diagonal superellipse cap stays outside true circle",
               diagonalRadius >= 10.0f);
    ExpectTrue("diagonal superellipse cap stays within documented bound",
               diagonalRadius <= maxConservativeCapRadius + 0.002f);
    ExpectTrue("superellipse cap is flatter than circular diagonal",
               capsule.points[diagonalIndex].x - capsuleEnd.x >
               std::sqrt(0.5f) * 10.0f);
    constexpr int kChordInterpolationSteps = 64;
    float minimumSampledChordRadius = std::numeric_limits<float>::max();
    bool everyCapChordIsConservative = true;
    for (int index = 0; index <= kThreatVisualCapsuleCapSegments; ++index) {
        ExpectTrue("every end-cap point stays outside true circle",
                   capsule.points[index].Distance(capsuleEnd) >= 9.998f);
        ExpectTrue("every start-cap point stays outside true circle",
                   capsule.points[
                       kThreatVisualCapsuleCapSegments + 1 + index].
                       Distance(capsuleStart) >= 9.998f);
        if (index > 0) {
            ExpectTrue("end cap samples smoothly from left to right",
                       capsule.points[index].y <=
                       capsule.points[index - 1].y + 0.0001f);
            const std::size_t startCapIndex =
                kThreatVisualCapsuleCapSegments + 1 + index;
            ExpectTrue("start cap samples smoothly from right to left",
                       capsule.points[startCapIndex].y >=
                       capsule.points[startCapIndex - 1].y - 0.0001f);
        }
        if (index == kThreatVisualCapsuleCapSegments) continue;

        const std::size_t startCapEdge =
            kThreatVisualCapsuleCapSegments + 1 + index;
        for (int sampleIndex = 0;
             sampleIndex <= kChordInterpolationSteps;
             ++sampleIndex) {
            const float t = static_cast<float>(sampleIndex) /
                static_cast<float>(kChordInterpolationSteps);
            const Vec2 endEdgePoint =
                capsule.points[index] * (1.0f - t) +
                capsule.points[index + 1] * t;
            const Vec2 startEdgePoint =
                capsule.points[startCapEdge] * (1.0f - t) +
                capsule.points[startCapEdge + 1] * t;
            const float endRadius = endEdgePoint.Distance(capsuleEnd);
            const float startRadius = startEdgePoint.Distance(capsuleStart);
            minimumSampledChordRadius = std::min(
                minimumSampledChordRadius,
                std::min(endRadius, startRadius));
            everyCapChordIsConservative =
                everyCapChordIsConservative &&
                endRadius >= 9.999f &&
                startRadius >= 9.999f;
        }
    }
    ExpectTrue("every densely sampled cap chord stays outside true circle",
               everyCapChordIsConservative);
    ExpectTrue("minimum cap chord radius reaches boundary without underdraw",
               minimumSampledChordRadius >= 9.999f);

    const ThreatVisualPath cappedCapsule =
        ThreatVisualGeometry::Capsule(
            capsuleStart,
            capsuleEnd,
            10.0f,
            100000);
    ExpectEq("oversampled capsule is capped",
             static_cast<int>(cappedCapsule.count),
             static_cast<int>(kThreatVisualMaxPoints));
    ExpectTrue("oversampled capsule remains closed", cappedCapsule.closed);

    const Vec2 degenerateCenter(25.0f, -30.0f);
    const ThreatVisualPath degenerateCapsule =
        ThreatVisualGeometry::Capsule(
            degenerateCenter,
            degenerateCenter,
            17.0f);
    ExpectEq("degenerate capsule becomes 64-segment circle",
             static_cast<int>(degenerateCapsule.count),
             kThreatVisualCircleSegments);
    ExpectTrue("degenerate capsule circle is closed",
               degenerateCapsule.closed);
    for (std::size_t index = 0; index < degenerateCapsule.count; ++index) {
        ExpectNear("degenerate capsule keeps circle radius",
                   degenerateCapsule.points[index].Distance(degenerateCenter),
                   17.0f,
                   0.002f);
    }

    const Vec2 circleCenter(-12.0f, 44.0f);
    const ThreatVisualPath circle =
        ThreatVisualGeometry::Circle(circleCenter, 35.0f);
    ExpectEq("circle uses exactly 64 segments",
             static_cast<int>(circle.count),
             kThreatVisualCircleSegments);
    ExpectTrue("circle is closed", circle.closed);
    ExpectFinitePath("circle points are finite", circle);
    for (std::size_t index = 0; index < circle.count; ++index) {
        ExpectNear("circle point lies on radius",
                   circle.points[index].Distance(circleCenter),
                   35.0f,
                   0.002f);
    }

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const float negativeInfinity = -infinity;
    ExpectTrue("circle rejects non-finite center",
               ThreatVisualGeometry::Circle(Vec2(nan, 0.0f), 20.0f).Empty());
    ExpectTrue("circle rejects non-finite radius",
               ThreatVisualGeometry::Circle(Vec2(), infinity).Empty());
    ExpectTrue("circle rejects zero radius",
               ThreatVisualGeometry::Circle(Vec2(), 0.0f).Empty());
    ExpectTrue("circle rejects fewer than three segments",
               ThreatVisualGeometry::Circle(Vec2(), 20.0f, 2).Empty());
    ExpectEq("oversampled circle is capped",
             static_cast<int>(
                 ThreatVisualGeometry::Circle(
                     Vec2(),
                     20.0f,
                     100000).count),
             static_cast<int>(kThreatVisualMaxPoints));

    const float sectorAngle = kThreatVisualPi / 3.0f;
    const float expectedBoundaryX = std::cos(kThreatVisualPi / 6.0f) * 100.0f;
    const float expectedBoundaryY = std::sin(kThreatVisualPi / 6.0f) * 100.0f;
    const ThreatVisualPath sector =
        ThreatVisualGeometry::Sector(
            Vec2(),
            Vec2(1.0f, 0.0f),
            100.0f,
            sectorAngle);
    ExpectTrue("sector is a closed center path", sector.closed);
    ExpectEq("sector contains center plus sampled arc",
             static_cast<int>(sector.count),
             kThreatVisualSectorArcSegments + 2);
    ExpectNear("sector starts at center x", sector.points[0].x, 0.0f);
    ExpectNear("sector starts at center y", sector.points[0].y, 0.0f);
    ExpectNear("sector left endpoint x",
               sector.points[1].x,
               expectedBoundaryX,
               0.002f);
    ExpectNear("sector left endpoint y",
               sector.points[1].y,
               expectedBoundaryY,
               0.002f);
    ExpectNear("sector right endpoint x",
               sector.points[sector.count - 1].x,
               expectedBoundaryX,
               0.002f);
    ExpectNear("sector right endpoint y",
               sector.points[sector.count - 1].y,
               -expectedBoundaryY,
               0.002f);
    for (std::size_t index = 1; index < sector.count; ++index) {
        ExpectNear("sector arc point keeps outer radius",
                   sector.points[index].Length(),
                   100.0f,
                   0.002f);
        if (index + 1 < sector.count) {
            ExpectTrue("sector arc orders left to right",
                       sector.points[index].Cross(
                           sector.points[index + 1]) <= 0.0001f);
        }
    }
    ExpectEq("oversampled sector is capped",
             static_cast<int>(
                 ThreatVisualGeometry::Sector(
                     Vec2(),
                     Vec2(1.0f, 0.0f),
                     100.0f,
                     sectorAngle,
                     100000).count),
             static_cast<int>(kThreatVisualMaxPoints));
    ExpectTrue("sector rejects non-finite angle",
               ThreatVisualGeometry::Sector(
                   Vec2(),
                   Vec2(1.0f, 0.0f),
                   100.0f,
                   nan).Empty());
    ExpectTrue("sector rejects zero direction",
               ThreatVisualGeometry::Sector(
                   Vec2(),
                   Vec2(),
                   100.0f,
                   sectorAngle).Empty());

    std::array<bool, kThreatVisualMaxPoints> visibility = {};
    visibility[0] = true;
    visibility[1] = true;
    visibility[3] = true;
    visibility[4] = true;
    ThreatVisualVisibleRuns runs =
        ThreatVisualGeometry::SegmentVisibleRuns(visibility, 5, false);
    ExpectEq("missing middle point creates two open runs",
             static_cast<int>(runs.count),
             2);
    ExpectEq("missing middle first run starts at zero",
             static_cast<int>(runs[0].first),
             0);
    ExpectEq("missing middle first run has two adjacent points",
             static_cast<int>(runs[0].count),
             2);
    ExpectEq("missing middle second run starts after gap",
             static_cast<int>(runs[1].first),
             3);
    ExpectEq("missing middle second run has two adjacent points",
             static_cast<int>(runs[1].count),
             2);
    ExpectTrue("partial open runs never close",
               !runs[0].closed && !runs[1].closed);

    visibility = {};
    visibility[0] = true;
    visibility[2] = true;
    visibility[4] = true;
    runs = ThreatVisualGeometry::SegmentVisibleRuns(
        visibility,
        6,
        false);
    ExpectEq("alternating visibility creates singleton runs",
             static_cast<int>(runs.count),
             3);
    ExpectTrue("alternating visibility never fabricates adjacency",
               runs[0].count == 1 &&
               runs[1].count == 1 &&
               runs[2].count == 1);

    visibility = {};
    visibility[0] = true;
    visibility[1] = true;
    visibility[4] = true;
    visibility[5] = true;
    runs = ThreatVisualGeometry::SegmentVisibleRuns(
        visibility,
        6,
        true);
    ExpectEq("closed first-last adjacency forms one wrap run",
             static_cast<int>(runs.count),
             1);
    ExpectEq("closed wrap run starts after final gap",
             static_cast<int>(runs[0].first),
             4);
    ExpectEq("closed wrap run preserves four adjacent points",
             static_cast<int>(runs[0].count),
             4);
    ExpectTrue("partial closed wrap run is open and marked wrapping",
               runs[0].wraps && !runs[0].closed);
    ExpectEq("closed wrap order includes last-side first point",
             static_cast<int>(
                 ThreatVisualGeometry::VisibleRunPointIndex(
                     runs[0],
                     0,
                     6)),
             4);
    ExpectEq("closed wrap order crosses to index zero",
             static_cast<int>(
                 ThreatVisualGeometry::VisibleRunPointIndex(
                     runs[0],
                     2,
                     6)),
             0);

    visibility = {};
    for (std::size_t index = 0; index < 5; ++index) {
        visibility[index] = true;
    }
    runs = ThreatVisualGeometry::SegmentVisibleRuns(
        visibility,
        5,
        true);
    ExpectTrue("all-visible closed path remains one smooth closed run",
               runs.allVisible &&
               runs.count == 1 &&
               runs[0].first == 0 &&
               runs[0].count == 5 &&
               runs[0].closed &&
               !runs[0].wraps);

    visibility = {};
    runs = ThreatVisualGeometry::SegmentVisibleRuns(
        visibility,
        8,
        true);
    ExpectTrue("all-invalid path produces no runs",
               runs.Empty() && !runs.allVisible);

    visibility = {};
    for (std::size_t index = 0;
         index < kThreatVisualMaxPoints;
         index += 2) {
        visibility[index] = true;
    }
    runs = ThreatVisualGeometry::SegmentVisibleRuns(
        visibility,
        kThreatVisualMaxPoints,
        false);
    ExpectEq("maximum alternating mask stays within run capacity",
             static_cast<int>(runs.count),
             static_cast<int>(kThreatVisualMaxPoints / 2));
    ExpectTrue("point count beyond fixed capacity fails closed",
               ThreatVisualGeometry::SegmentVisibleRuns(
                   visibility,
                   kThreatVisualMaxPoints + 1,
                   true).Empty());

    const ThreatVisualDispatch lineDispatch =
        GetThreatVisualDispatch(ZDSpellType::Line, false);
    const ThreatVisualDispatch circleDispatch =
        GetThreatVisualDispatch(ZDSpellType::Circular, false);
    const ThreatVisualDispatch ringDispatch =
        GetThreatVisualDispatch(ZDSpellType::Ring, false);
    const ThreatVisualDispatch coneDispatch =
        GetThreatVisualDispatch(ZDSpellType::Cone, false);
    const ThreatVisualDispatch explosionDispatch =
        GetThreatVisualDispatch(ZDSpellType::Line, true);
    const ThreatVisualDispatch arcDispatch =
        GetThreatVisualDispatch(ZDSpellType::Arc, true);
    ExpectEq("line dispatches capsule",
             static_cast<int>(lineDispatch.body),
             static_cast<int>(ThreatVisualBody::Capsule));
    ExpectEq("circle dispatches circle",
             static_cast<int>(circleDispatch.body),
             static_cast<int>(ThreatVisualBody::Circle));
    ExpectEq("ring dispatches paired circles",
             static_cast<int>(ringDispatch.body),
             static_cast<int>(ThreatVisualBody::Ring));
    ExpectEq("cone dispatches sector",
             static_cast<int>(coneDispatch.body),
             static_cast<int>(ThreatVisualBody::Sector));
    ExpectTrue("end explosion dispatch remains enabled",
               explosionDispatch.endExplosion);
    ExpectTrue("Arc suppresses body and end explosion",
               arcDispatch.body == ThreatVisualBody::None &&
               !arcDispatch.endExplosion);

    const std::array<ZDSpellType, 4> castVisualTypes = {{
        ZDSpellType::Line,
        ZDSpellType::Circular,
        ZDSpellType::Ring,
        ZDSpellType::Cone,
    }};
    for (const ZDSpellType type : castVisualTypes) {
        SpellData castData = ZDEvadeTest::MakeSpell(type);
        castData.spellDelay = 500;
        Threat castThreat = ZDEvadeTest::MakeThreat(castData);
        castThreat.authoredEndPos = castThreat.endPos;
        const ThreatVisualPlan preLaunch =
            ResolveThreatVisualPlan(castThreat, 1100);
        ExpectTrue("valid cast-origin body draws during cast animation",
                   preLaunch.drawBody);
    }

    SpellData preLaunchLineData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    preLaunchLineData.spellDelay = 500;
    preLaunchLineData.radius = 50.0f;
    Threat preLaunchLine =
        ZDEvadeTest::MakeThreat(preLaunchLineData);
    preLaunchLine.authoredEndPos = Vec2(1000.0f, 0.0f);
    preLaunchLine.endPos = Vec2(700.0f, 0.0f);
    preLaunchLine.direction = Vec2(1.0f, 0.0f);
    const ThreatVisualPlan preLaunchLinePlan =
        ResolveThreatVisualPlan(preLaunchLine, 1100);
    ExpectTrue("pre-launch line draws full predicted capsule",
               preLaunchLinePlan.drawBody &&
                   preLaunchLinePlan.body ==
                       ThreatVisualBody::Capsule);
    ExpectNear("pre-launch line head starts at cast origin",
               preLaunchLinePlan.line.head.x,
               preLaunchLine.startPos.x);
    ExpectNear("pre-launch line uses authored endpoint",
               preLaunchLinePlan.line.end.x,
               preLaunchLine.AuthoredEnd().x);

    Threat boundLine = preLaunchLine;
    boundLine.missileBound = true;
    boundLine.launchTick = 1200;
    boundLine.observedHead = Vec2(320.0f, 0.0f);
    boundLine.observedTick = 1300;
    const ThreatVisualPlan boundLinePlan =
        ResolveThreatVisualPlan(boundLine, 1300);
    ExpectTrue("missile bind keeps line body visible",
               boundLinePlan.drawBody);
    ExpectNear("missile bind updates visual head",
               boundLinePlan.line.head.x,
               320.0f);

    const ThreatVisualPath effectiveLinePath =
        ThreatVisualGeometry::Capsule(
            preLaunchLinePlan.line.head,
            preLaunchLinePlan.line.end,
            preLaunchLine.Radius());
    float effectiveLineLateralExtent = 0.0f;
    for (std::size_t index = 0;
         index < effectiveLinePath.count;
         ++index) {
        effectiveLineLateralExtent = std::max(
            effectiveLineLateralExtent,
            std::abs(effectiveLinePath.points[index].y));
    }
    ExpectNear("line drawing uses effective radius",
               effectiveLineLateralExtent,
               56.0f * capCircumscription,
               0.01f);
    ExpectNear("line visual radius includes six-unit safety padding",
               preLaunchLine.Radius(),
               56.0f);
    ExpectNear("line authored collision radius remains uninflated",
               preLaunchLine.AuthoredRadius(),
               50.0f);
    SelfSkillDebugStore<4> selfVisualStore;
    SelfSkillProcessObservation selfLineObservation;
    selfLineObservation.localPlayerNetworkId = 77u;
    selfLineObservation.sourceNetworkId = 77u;
    selfLineObservation.data = &preLaunchLineData;
    selfLineObservation.matchDisposition =
        ProcessSpellMatchDisposition::Matched;
    selfLineObservation.tick = 1000;
    selfLineObservation.start = Vec2(100.0f, 100.0f);
    selfLineObservation.end = Vec2(1100.0f, 100.0f);
    ExpectTrue("self debug line creates tested renderer input",
               selfVisualStore.ObserveProcess(
                   selfLineObservation).accepted);
    const auto selfLineVisuals = selfVisualStore.Snapshot();
    ExpectTrue("self debug line visual uses same effective +6 radius",
               selfLineVisuals.size() == 1 &&
                   std::fabs(
                       selfLineVisuals.front().visual.Radius() -
                       56.0f) < 0.001f);

    SpellData effectiveCircleData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    effectiveCircleData.radius = 100.0f;
    Threat effectiveCircle =
        ZDEvadeTest::MakeThreat(effectiveCircleData);
    const ThreatVisualPath effectiveCirclePath =
        ThreatVisualGeometry::Circle(
            effectiveCircle.endPos,
            effectiveCircle.Radius());
    ExpectNear("circle drawing keeps authored radius",
               effectiveCirclePath.points[0].Distance(
                   effectiveCircle.endPos),
               100.0f);
    selfVisualStore.Clear();
    SelfSkillProcessObservation selfCircleObservation =
        selfLineObservation;
    selfCircleObservation.data = &effectiveCircleData;
    ExpectTrue("self debug circle creates tested renderer input",
               selfVisualStore.ObserveProcess(
                   selfCircleObservation).accepted);
    const auto selfCircleVisuals = selfVisualStore.Snapshot();
    ExpectTrue("self debug circle visual keeps authored radius",
               selfCircleVisuals.size() == 1 &&
                   std::fabs(
                       selfCircleVisuals.front().visual.Radius() -
                       100.0f) < 0.001f);

    SpellData effectiveRingData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Ring);
    effectiveRingData.innerRadius = 100.0f;
    effectiveRingData.radius = 200.0f;
    Threat effectiveRing =
        ZDEvadeTest::MakeThreat(effectiveRingData);
    const ThreatVisualPath effectiveRingOuter =
        ThreatVisualGeometry::Circle(
            effectiveRing.endPos,
            effectiveRing.Radius());
    const ThreatVisualPath effectiveRingInner =
        ThreatVisualGeometry::Circle(
            effectiveRing.endPos,
            effectiveRing.InnerRadius());
    ExpectNear("ring drawing keeps authored outer radius",
               effectiveRingOuter.points[0].Distance(
                   effectiveRing.endPos),
               200.0f);
    ExpectNear("ring drawing keeps authored inner radius",
               effectiveRingInner.points[0].Distance(
                   effectiveRing.endPos),
               100.0f);

    SpellData effectiveConeData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    effectiveConeData.range = 100.0f;
    effectiveConeData.coneEdgePadding = 5.0f;
    Threat effectiveCone =
        ZDEvadeTest::MakeThreat(effectiveConeData);
    const ThreatVisualPath effectiveConePath =
        ThreatVisualGeometry::Sector(
            effectiveCone.startPos,
            effectiveCone.direction,
            effectiveCone.Range(),
            effectiveCone.Angle() * kThreatVisualPi / 180.0f,
            kThreatVisualSectorArcSegments,
            effectiveCone.ConeEdgePadding());
    float effectiveConeMaximumRadius = 0.0f;
    for (std::size_t index = 0;
         index < effectiveConePath.count;
         ++index) {
        effectiveConeMaximumRadius = std::max(
            effectiveConeMaximumRadius,
            effectiveConePath.points[index].Distance(
                effectiveCone.startPos));
    }
    ExpectTrue("padded cone drawing remains closed and finite",
               effectiveConePath.closed &&
                   !effectiveConePath.Empty());
    ExpectNear("cone drawing uses effective edge padding",
               effectiveConeMaximumRadius,
               effectiveCone.Range() +
                   effectiveCone.ConeEdgePadding(),
               0.01f);
    ExpectNear("cone drawing keeps authored edge padding",
               effectiveCone.ConeEdgePadding(),
               5.0f);
    ExpectNear("cone range remains authored",
               effectiveCone.Range(),
               100.0f);
    ExpectNear("cone angle remains authored",
               effectiveCone.Angle(),
               60.0f);

    Threat endedCircle =
        ZDEvadeTest::MakeThreat(effectiveCircleData);
    const int endedCircleTick =
        endedCircle.BodyActivationTick() + 101;
    ExpectTrue("body lifetime end suppresses visual body",
               !ResolveThreatVisualPlan(
                    endedCircle,
                    endedCircleTick).drawBody);
    Threat terminatedLine = preLaunchLine;
    terminatedLine.projectileTerminated = true;
    terminatedLine.projectileTerminationTick = 1100;
    ExpectTrue("projectile termination suppresses visual body",
               !ResolveThreatVisualPlan(
                    terminatedLine,
                    1100).drawBody);

    SpellData noProcessData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    noProcessData.noProcess = true;
    Threat noProcessThreat =
        ZDEvadeTest::MakeThreat(noProcessData);
    ExpectTrue("noProcess cast has no visual plan",
               !ResolveThreatVisualPlan(
                    noProcessThreat,
                    1000).drawBody);
    SpellData invalidVisualData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    invalidVisualData.radius =
        std::numeric_limits<float>::quiet_NaN();
    Threat invalidVisualThreat =
        ZDEvadeTest::MakeThreat(invalidVisualData);
    Threat missingVisualThreat;
    ExpectTrue("invalid geometry has no visual plan",
               !ResolveThreatVisualPlan(
                    invalidVisualThreat,
                    1000).drawBody);
    ExpectTrue("missing data has no visual plan",
               !ResolveThreatVisualPlan(
                    missingVisualThreat,
                    1000).drawBody);

    ThreatDatabase::Initialize();
    struct StableEndpointFixture {
        const char* champion;
        const char* spell;
        float expectedRange;
    };
    const std::array<StableEndpointFixture, 3> stableEndpointFixtures = {{
        {"Nautilus", "NautilusAnchorDrag", 1150.0f},
        {"Ezreal", "EzrealQ", 1150.0f},
        {"Veigar", "VeigarBalefulStrike", 1000.0f},
    }};
    for (const auto& fixture : stableEndpointFixtures) {
        const SpellData* data =
            ThreatDatabase::FindAny(fixture.spell, fixture.champion);
        ExpectTrue("stable-endpoint fixture exists in real spell database",
                   data != nullptr);
        if (!data) continue;
        ExpectNear("stable-endpoint fixture keeps expected database range",
                   data->range,
                   fixture.expectedRange);
        ExpectTrue("stable-endpoint fixture is a straight line missile",
                   data->spellType == ZDSpellType::Line &&
                   data->missileRouteMode == MissileRouteMode::Straight);

        Threat live;
        live.data = data;
        live.startPos = Vec2(0.0f, 100.0f);
        live.direction = Vec2(1.0f, 0.0f);
        live.authoredEndPos =
            live.startPos + live.direction * data->range;
        live.endPos = live.authoredEndPos;
        live.startTick = 750;
        live.launchTick = 1000;
        live.endTick = 4000;
        live.missileBound = true;
        live.missileNetworkId = 77u;
        live.missileObjectIdentity = 0xA100u;
        live.observedSpeed = data->projectileSpeed;

        live.missilePositionUnavailable = true;
        const ThreatVisualPlan unavailableBeforeFirstHead =
            ResolveThreatVisualPlan(live, 1100);
        ExpectNear(
            "position unavailable before first trustworthy head freezes at start",
            unavailableBeforeFirstHead.line.head.x,
            live.startPos.x);

        const auto applyStraightObservation =
            [&](float progress, int tick) {
                const Vec2 runtimeHead(progress, 100.0f);
                const Vec2 acceptedHead = MonotonicMissileHead(
                    live.observedHead,
                    runtimeHead,
                    live.direction,
                    true);
                const MissileRouteObservationUpdate routeUpdate =
                    ResolveMissileRouteObservationUpdate(
                        live.RouteMode(),
                        live.direction,
                        live.startPos,
                        live.AuthoredEnd(),
                        live.endPos,
                        acceptedHead,
                        runtimeHead,
                        acceptedHead - live.observedHead,
                        false,
                        live.collisionStopped);
                ExpectTrue(
                    "straight runtime EndPosition=head cannot change route geometry",
                    !routeUpdate.geometryChanged &&
                    routeUpdate.direction.DistanceSqr(live.direction) <=
                        0.0001f &&
                    routeUpdate.authoredEnd.DistanceSqr(
                        live.AuthoredEnd()) <= 1.0f &&
                    routeUpdate.effectiveEnd.DistanceSqr(
                        live.endPos) <= 1.0f);
                const MissileEvidenceStateUpdate evidence =
                    ResolveMissileEvidenceState(
                        true,
                        true,
                        live.missileMissingSinceTick,
                        live.missilePositionUnavailable,
                        tick);
                live.missileMissingSinceTick =
                    evidence.missingSinceTick;
                live.missilePositionUnavailable =
                    evidence.positionUnavailable;
                live.observedHead = acceptedHead;
                live.observedTick = tick;

                const ThreatVisualPlan plan =
                    ResolveThreatVisualPlan(live, tick);
                ExpectTrue(
                    "live straight runtime endpoint anomaly keeps capsule body",
                    plan.drawBody &&
                    plan.body == ThreatVisualBody::Capsule);
                ExpectNear("visual plan head equals observed runtime head",
                           plan.line.head.x,
                           progress);
                ExpectNear("visual plan endpoint remains exact database range",
                           plan.line.end.x,
                           fixture.expectedRange);
                ExpectNear("authored endpoint remains exact database range",
                           live.AuthoredEnd().x,
                           fixture.expectedRange);
                const ThreatVisualPath path =
                    ThreatVisualGeometry::Capsule(
                        plan.line.head,
                        plan.line.end,
                        live.Radius());
                ExpectEq("stable live line capsule contains exactly 66 points",
                         static_cast<int>(path.count),
                         66);
            };

        applyStraightObservation(400.0f, 1400);

        const int trustworthyEndTick = live.endTick;
        for (const int invalidTick : {1500, 1600, 1650}) {
            const MissileEvidenceStateUpdate unavailable =
                ResolveMissileEvidenceState(
                    true,
                    false,
                    live.missileMissingSinceTick,
                    live.missilePositionUnavailable,
                    invalidTick);
            live.missileMissingSinceTick =
                unavailable.missingSinceTick;
            live.missilePositionUnavailable =
                unavailable.positionUnavailable;
            ExpectTrue(
                "live object with invalid position is unavailable, not missing",
                live.missilePositionUnavailable &&
                live.missileMissingSinceTick == -1);
            ExpectEq("invalid live-object frame retains trustworthy endTick",
                     live.endTick,
                     trustworthyEndTick);
            const ThreatVisualPlan unavailablePlan =
                ResolveThreatVisualPlan(live, invalidTick);
            ExpectTrue(
                "invalid live-object frame freezes nondegenerate corridor",
                unavailablePlan.drawBody &&
                unavailablePlan.body == ThreatVisualBody::Capsule);
            ExpectNear("invalid live-object frame freezes exact head at 400",
                       unavailablePlan.line.head.x,
                       400.0f);
            ExpectNear(
                "invalid live-object frame retains exact authored endpoint",
                unavailablePlan.line.end.x,
                fixture.expectedRange);
            ExpectEq(
                "invalid live-object frame retains 66-point corridor",
                static_cast<int>(
                    ThreatVisualGeometry::Capsule(
                        unavailablePlan.line.head,
                        unavailablePlan.line.end,
                        live.Radius()).count),
                66);
        }

        applyStraightObservation(700.0f, 1700);
        ExpectTrue("valid reacquisition clears position-unavailable state",
                   !live.missilePositionUnavailable &&
                   live.missileMissingSinceTick == -1);
        ExpectNear("valid reacquisition resumes exact head at 700",
                   live.HeadAtTick(1700).x,
                   700.0f);

        live.missileMissingSinceTick = 1701;
        const ThreatVisualPlan missingPlan =
            ResolveThreatVisualPlan(live, 2000);
        ExpectTrue("missing evidence freezes head but keeps live corridor",
                   missingPlan.drawBody &&
                   missingPlan.body == ThreatVisualBody::Capsule);
        ExpectNear("missing evidence freezes exact head at 700",
                   missingPlan.line.head.x,
                   700.0f);
        ExpectNear("missing evidence keeps exact authored endpoint",
                   missingPlan.line.end.x,
                   fixture.expectedRange);

        live.missileMissingSinceTick = -1;
        live.collisionStopped = true;
        live.collisionKind = ZDCollisionKind::Terrain;
        live.endPos = Vec2(550.0f, 100.0f);
        const ThreatVisualPlan predictedCollisionPlan =
            ResolveThreatVisualPlan(live, 1700);
        ExpectTrue("predicted collision does not suppress live bound line",
                   predictedCollisionPlan.drawBody);
        ExpectNear("predicted collision cannot truncate live visual endpoint",
                   predictedCollisionPlan.line.end.x,
                   fixture.expectedRange);

        Threat validEndpointArrival = live;
        validEndpointArrival.collisionStopped = false;
        validEndpointArrival.collisionKind = ZDCollisionKind::None;
        validEndpointArrival.endPos =
            validEndpointArrival.AuthoredEnd();
        validEndpointArrival.observedHead =
            validEndpointArrival.AuthoredEnd();
        validEndpointArrival.observedTick = 1800;
        validEndpointArrival.missilePositionUnavailable = false;
        const ThreatVisualPlan validEndpointPlan =
            ResolveThreatVisualPlan(validEndpointArrival, 1800);
        const ThreatVisualPath validEndpointPath =
            ThreatVisualGeometry::Capsule(
                validEndpointPlan.line.head,
                validEndpointPlan.line.end,
                validEndpointArrival.Radius());
        ExpectTrue(
            "valid observation at authored endpoint may retain live body plan",
            validEndpointPlan.drawBody &&
            validEndpointPlan.body == ThreatVisualBody::Capsule);
        ExpectEq(
            "valid observation truly at endpoint may render endpoint circle",
            static_cast<int>(validEndpointPath.count),
            kThreatVisualCircleSegments);

        live.projectileTerminated = true;
        live.projectileTerminationTick = 1700;
        live.missileBound = false;
        live.endPos = Vec2(700.0f, 100.0f);
        const ThreatVisualPlan deletedPlan =
            ResolveThreatVisualPlan(live, 1700);
        ExpectTrue("confirmed missile delete suppresses line body",
                   !deletedPlan.drawBody);
        ExpectNear("confirmed delete keeps authoritative effective end",
                   deletedPlan.line.end.x,
                   700.0f);
    }

    SpellData steeringData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    steeringData.missileRouteMode = MissileRouteMode::Steering;
    const MissileRouteObservationUpdate steeringUpdate =
        ResolveMissileRouteObservationUpdate(
            steeringData.missileRouteMode,
            Vec2(1.0f, 0.0f),
            Vec2(0.0f, 100.0f),
            Vec2(1000.0f, 100.0f),
            Vec2(1000.0f, 100.0f),
            Vec2(400.0f, 200.0f),
            Vec2(900.0f, 500.0f),
            Vec2(100.0f, 100.0f),
            false,
            false);
    ExpectTrue("explicit steering route update remains supported",
               steeringUpdate.geometryChanged &&
               steeringUpdate.direction.DistanceSqr(
                   Vec2(500.0f, 300.0f).Normalized()) <= 0.0001f);
    ExpectNear("steering update accepts validated runtime endpoint x",
               steeringUpdate.authoredEnd.x,
               900.0f);
    ExpectNear("steering update accepts validated runtime endpoint y",
               steeringUpdate.authoredEnd.y,
               500.0f);

    SpellData retainedExplosionData =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    retainedExplosionData.hasEndExplosion = true;
    retainedExplosionData.secondaryRadius = 180.0f;
    retainedExplosionData.endExplosionDuration = 500;
    Threat retainedExplosion =
        ZDEvadeTest::MakeThreat(retainedExplosionData);
    retainedExplosion.projectileTerminated = true;
    retainedExplosion.projectileTerminationTick = 2000;
    retainedExplosion.endPos = Vec2(620.0f, 100.0f);
    retainedExplosion.endTick = 2500;
    const ThreatVisualPlan retainedExplosionPlan =
        ResolveThreatVisualPlan(retainedExplosion, 2000);
    ExpectTrue("terminated projectile suppresses body and retains explosion separately",
               !retainedExplosionPlan.drawBody &&
               retainedExplosionPlan.drawEndExplosion);
    ExpectTrue("retained explosion is absent before active window",
               !ResolveThreatVisualPlan(
                    retainedExplosion,
                    1999).drawEndExplosion);
    ExpectTrue("retained explosion is absent after active window",
               !ResolveThreatVisualPlan(
                    retainedExplosion,
                    retainedExplosion.EndExplosionEndTick() + 1).
                        drawEndExplosion);
    const ThreatVisualPath retainedExplosionPath =
        ThreatVisualGeometry::Circle(
            retainedExplosion.EndExplosionCenter(),
            retainedExplosion.EndExplosionRadius());
    ExpectNear("line explosion drawing keeps authored circular radius",
               retainedExplosionPath.points[0].Distance(
                   retainedExplosion.EndExplosionCenter()),
               retainedExplosion.AuthoredEndExplosionRadius());

    const LockedTargetVisualDispatch safeTarget =
        GetLockedTargetVisualDispatch(true, 65.0f);
    const LockedTargetVisualDispatch fallbackTarget =
        GetLockedTargetVisualDispatch(false, 65.0f);
    const LockedTargetVisualDispatch defaultTarget;
    ExpectTrue("strict-safe target dispatches green footprint",
               safeTarget.color ==
                   kStrictSafeTargetFootprintColor &&
                   ThreatVisualStyle::Green(safeTarget.color) >
                       ThreatVisualStyle::Red(safeTarget.color) &&
                   ThreatVisualStyle::Green(safeTarget.color) >
                       ThreatVisualStyle::Blue(safeTarget.color));
    ExpectTrue("fallback target dispatch keeps fallback color",
               fallbackTarget.color ==
                   kFallbackTargetFootprintColor);
    ExpectNear("target footprint forwards valid runtime radius",
               safeTarget.footprintRadius,
               65.0f);
    ExpectNear("default target dispatch uses centralized fallback radius",
               defaultTarget.footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("fallback target sanitizes invalid runtime radius",
               GetLockedTargetVisualDispatch(
                   false,
                   nan).footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("target footprint forwards large runtime radius",
               GetLockedTargetVisualDispatch(
                   true,
                   4096.0f).footprintRadius,
               4096.0f);
    ExpectNear("target footprint clamps too-small runtime radius",
               GetLockedTargetVisualDispatch(
                   true,
                   9.5f).footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("target footprint defaults zero runtime radius",
               GetLockedTargetVisualDispatch(
                   true,
                   0.0f).footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("target footprint defaults NaN runtime radius",
               GetLockedTargetVisualDispatch(
                   true,
                   nan).footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("target footprint defaults positive infinity radius",
               GetLockedTargetVisualDispatch(
                   true,
                   infinity).footprintRadius,
               kMinimumHeroRadius);
    ExpectNear("target footprint defaults negative infinity radius",
               GetLockedTargetVisualDispatch(
                   true,
                   negativeInfinity).footprintRadius,
               kMinimumHeroRadius);

    ExpectTrue("outer stroke is red family",
               ThreatVisualStyle::IsRedFamily(
                   ThreatVisualStyle::kOuterStrokeColor));
    ExpectTrue("core stroke is red family",
               ThreatVisualStyle::IsRedFamily(
                   ThreatVisualStyle::kCoreStrokeColor));
    ExpectTrue("label is red family",
               ThreatVisualStyle::IsRedFamily(
                   ThreatVisualStyle::kLabelColor));
    ExpectTrue("outer stroke is translucent",
               ThreatVisualStyle::Alpha(
                   ThreatVisualStyle::kOuterStrokeColor) <
               ThreatVisualStyle::Alpha(
                   ThreatVisualStyle::kCoreStrokeColor));
    ExpectTrue("outer stroke is wider than core",
               ThreatVisualStyle::kOuterStrokeThickness >
               ThreatVisualStyle::kCoreStrokeThickness);
    ExpectTrue("stroke thicknesses are positive",
               ThreatVisualStyle::kCoreStrokeThickness > 0.0f);

    // Warm the thread-safe default sample cache before timing steady-state
    // renderer work; this benchmark guards against reintroducing per-cap pow.
    (void)ThreatVisualGeometry::Capsule(
        capsuleStart,
        capsuleEnd,
        10.0f);
    constexpr int kCapsuleBenchmarkIterations = 100000;
    const auto benchmarkStart = std::chrono::steady_clock::now();
    float benchmarkChecksum = 0.0f;
    for (int iteration = 0;
         iteration < kCapsuleBenchmarkIterations;
         ++iteration) {
        const ThreatVisualPath benchmarkCapsule =
            ThreatVisualGeometry::Capsule(
                capsuleStart,
                capsuleEnd,
                10.0f);
        benchmarkChecksum +=
            benchmarkCapsule.points[
                kThreatVisualCapsuleCapSegments / 2].x +
            static_cast<float>(benchmarkCapsule.count);
    }
    const double benchmarkMilliseconds =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - benchmarkStart).count();
    std::printf(
        "INFO: generated %d cached default capsules in %.2f ms\n",
        kCapsuleBenchmarkIterations,
        benchmarkMilliseconds);
    ExpectTrue("default capsule generation checksum remains observable",
               benchmarkChecksum > 0.0f);
    ExpectTrue("default capsule generation stays below regression threshold",
               benchmarkMilliseconds < 2000.0);

    return ZDEvadeTest::Finish("ZDEVADE VISUAL GEOMETRY");
}
