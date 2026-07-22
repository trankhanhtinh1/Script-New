#include "tests/ZDEvadeTestSupport.h"
#include "plugins/ZDEvade/Database/SpellDatabase.h"
#include "plugins/ZDEvade/Evade/EvadeGeometry.h"
#define ZDEVADE_PLANNER_SEED_ONLY
#include "plugins/ZDEvade/Evade/EvadePlanner.h"
#undef ZDEVADE_PLANNER_SEED_ONLY

#include <array>
#include <chrono>
#include <climits>
#include <cstring>
#include <type_traits>

namespace ImGui {
ImGuiContext* GetCurrentContext() { return nullptr; }
int GetFrameCount() { return 0; }
} // namespace ImGui

using namespace ZDEvade;
using ZDEvadeTest::ExpectEq;
using ZDEvadeTest::ExpectNear;
using ZDEvadeTest::ExpectTrue;

namespace {

const SpellData* FindSpell(const char* spellName) {
    for (const SpellData& spell : SpellDatabase::Spells) {
        if (spell.spellName == spellName) return &spell;
    }
    return nullptr;
}

Vec2 PolarPoint(float distance, float angleDegrees) {
    constexpr float kPi = 3.14159265358979323846f;
    const float radians = angleDegrees * kPi / 180.0f;
    return Vec2(std::cos(radians) * distance, std::sin(radians) * distance);
}

void InstallWalkableTestGrid() {
    alignas(16) static std::array<std::uint8_t, 0x20> navGrid = {};
    alignas(16) static std::array<std::uint8_t, 0x800> manager = {};
    alignas(16) static std::array<std::uint8_t, 10 * 10 * 16> cells = {};
    std::memset(navGrid.data(), 0, navGrid.size());
    std::memset(manager.data(), 0, manager.size());
    std::memset(cells.data(), 0, cells.size());

    *reinterpret_cast<std::uintptr_t*>(
        navGrid.data() + Offset::NavGridLayout::NavGridMgr) =
        reinterpret_cast<std::uintptr_t>(manager.data());
    *reinterpret_cast<std::uintptr_t*>(
        manager.data() + Offset::NavGridLayout::Data) =
        reinterpret_cast<std::uintptr_t>(cells.data());
    *reinterpret_cast<int*>(manager.data() + Offset::NavGridLayout::Width) = 10;
    *reinterpret_cast<int*>(manager.data() + Offset::NavGridLayout::Height) = 10;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::MinX) = -1000.0f;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::MinZ) = -1000.0f;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::MaxX) = 2000.0f;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::MaxZ) = 2000.0f;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::Scale) = 300.0f;
    *reinterpret_cast<float*>(manager.data() + Offset::NavGridLayout::InverseScale) =
        1.0f / 300.0f;

    CoreRuntime::g_ctx.navGrid = reinterpret_cast<std::uintptr_t>(navGrid.data());
    CoreRuntime::g_ctx.statusMask = CoreRuntime::BuildRequiredInitMask();
    ++CoreRuntime::g_ctx.refreshGeneration;
}

void ExpectConeBoundary(const char* spellName,
                        float expectedAngle,
                        float insideDegrees,
                        float outsideDegrees) {
    const SpellData* spell = FindSpell(spellName);
    ExpectTrue("cone spell exists", spell != nullptr);
    if (!spell) return;

    ExpectNear("cone full angle", spell->coneAngleDegrees, expectedAngle);
    Threat threat = ZDEvadeTest::MakeThreat(*spell);
    const int activeTick = threat.startTick + threat.Delay();
    const float testDistance = std::min(400.0f, spell->range * 0.75f);
    ExpectTrue("cone includes point inside angular boundary",
               EvadeGeometry::ContainsAt(
                   threat, PolarPoint(testDistance, insideDegrees), 0.0f, 0.0f, activeTick));
    ExpectTrue("cone excludes point outside angular boundary",
               !EvadeGeometry::ContainsAt(
                   threat, PolarPoint(testDistance, outsideDegrees), 0.0f, 0.0f, activeTick));
}

bool HasSeedNear(const std::vector<CandidateSeed>& seeds,
                 const Vec2& expected,
                 float epsilon = 0.2f) {
    for (const CandidateSeed& seed : seeds) {
        if (seed.position.Distance(expected) <= epsilon) return true;
    }
    return false;
}

int CountSeedSource(const std::vector<CandidateSeed>& seeds,
                    PlannerCandidateSource source) {
    int count = 0;
    for (const CandidateSeed& seed : seeds) {
        if (seed.source == source) ++count;
    }
    return count;
}

bool SameSeeds(const std::vector<CandidateSeed>& left,
               const std::vector<CandidateSeed>& right,
               float epsilon = 0.01f) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].source != right[index].source ||
            left[index].threatId != right[index].threatId ||
            left[index].stabilityBranchKey !=
                right[index].stabilityBranchKey ||
            left[index].position.Distance(right[index].position) > epsilon) {
            return false;
        }
    }
    return true;
}

int PositiveBudgetClassCount(const CandidateBudget& budget) {
    return (budget.analytical > 0 ? 1 : 0) +
           (budget.cursor > 0 ? 1 : 0) +
           (budget.singleThreatDetour > 0 ? 1 : 0) +
           (budget.exactIntersections > 0 ? 1 : 0) +
           (budget.radialFallback > 0 ? 1 : 0);
}

using PublicCandidateSignature = CandidateEvaluation (*)(
    const Vec2&,
    PlannerCandidateSource,
    int,
    const Vec2&,
    const Vec2&,
    float,
    float,
    float,
    int,
    const EvadeSettings&,
    const std::vector<Threat>&,
    CollisionIdentitySet*,
    bool);
static_assert(
    std::is_same_v<
        decltype(&EvadeGeometry::EvaluateCandidate),
        PublicCandidateSignature>,
    "public EvaluateCandidate must not expose a nav-validation bypass");

} // namespace

int main() {
    SpellData lineSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    lineSpell.spellDelay = 200;
    lineSpell.projectileSpeed = 1000.0f;
    lineSpell.radius = 50.0f;
    lineSpell.range = 1000.0f;
    Threat lineThreat = ZDEvadeTest::MakeThreat(lineSpell);
    lineThreat.endPos = Vec2(1000.0f, 0.0f);
    lineThreat.direction = (lineThreat.endPos - lineThreat.startPos).Normalized();

    bool onSegment = false;
    Vec2 projection;
    ExpectNear("distance to line segment",
               EvadeGeometry::DistanceToSegment(
                   Vec2(500.0f, 75.0f),
                   lineThreat.startPos,
                   lineThreat.endPos,
                   &onSegment,
                   &projection),
               75.0f);
    ExpectTrue("line projection is on segment", onSegment);
    ExpectNear("line projection x", projection.x, 500.0f);
    ExpectNear("line impact timing",
               static_cast<float>(
                   EvadeGeometry::ImpactTickAt(lineThreat, Vec2(500.0f, 0.0f))),
               1700.0f);

    SpellData extremeMarginSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    extremeMarginSpell.spellDelay = 0;
    extremeMarginSpell.projectileSpeed = 0.0f;
    Threat maximumImpactMargin =
        ZDEvadeTest::MakeThreat(extremeMarginSpell);
    maximumImpactMargin.startTick = INT_MAX;
    ExpectTrue("time margin preserves INT_MAX minus INT_MIN",
               EvadeGeometry::TimeMarginAt(
                   maximumImpactMargin,
                   maximumImpactMargin.endPos,
                   INT_MIN) >
                   static_cast<float>(INT_MAX));
    Threat minimumImpactMargin = maximumImpactMargin;
    minimumImpactMargin.startTick = INT_MIN;
    ExpectTrue("time margin preserves INT_MIN minus INT_MAX",
               EvadeGeometry::TimeMarginAt(
                   minimumImpactMargin,
                   minimumImpactMargin.endPos,
                   INT_MAX) <
                   static_cast<float>(INT_MIN));

    constexpr float heroRadius = 35.0f;
    constexpr float extraBuffer = 10.0f;
    lineThreat.positionUncertainty = 5.0f;
    const float expectedExitRadius = ExitCenterDistance(
        lineThreat.Radius(),
        heroRadius,
        extraBuffer,
        lineThreat.PositionUncertainty());

    const Vec2 preLaunchHero(0.0f, 0.0f);
    const Vec2 preLaunchLeft = EvadeGeometry::ClosestLineExit(
        lineThreat, preLaunchHero, heroRadius, extraBuffer, true, 1100);
    const Vec2 preLaunchRight = EvadeGeometry::ClosestLineExit(
        lineThreat, preLaunchHero, heroRadius, extraBuffer, false, 1100);
    ExpectNear("pre-launch exit projects from start", preLaunchLeft.x, 0.0f);
    ExpectNear("pre-launch left exit radius", preLaunchLeft.y, expectedExitRadius);
    ExpectNear("pre-launch right exit radius", preLaunchRight.y, -expectedExitRadius);
    ExpectNear("pre-launch left distance from projection",
               preLaunchLeft.Distance(lineThreat.startPos),
               expectedExitRadius);
    ExpectNear("pre-launch right distance from projection",
               preLaunchRight.Distance(lineThreat.startPos),
               expectedExitRadius);
    ExpectTrue("pre-launch exits lie on opposite sides",
               preLaunchLeft.y > 0.0f && preLaunchRight.y < 0.0f);

    const Vec2 inFlightHero(300.0f, 0.0f);
    const Vec2 inFlightLeft = EvadeGeometry::ClosestLineExit(
        lineThreat, inFlightHero, heroRadius, extraBuffer, true, 1500);
    const Vec2 inFlightRight = EvadeGeometry::ClosestLineExit(
        lineThreat, inFlightHero, heroRadius, extraBuffer, false, 1500);
    ExpectNear("in-flight exit follows advanced head", inFlightLeft.x, 300.0f);
    ExpectNear("in-flight left exit radius", inFlightLeft.y, expectedExitRadius);
    ExpectNear("in-flight right exit radius", inFlightRight.y, -expectedExitRadius);
    ExpectNear("in-flight left distance from projection",
               inFlightLeft.Distance(Vec2(300.0f, 0.0f)),
               expectedExitRadius);
    ExpectNear("in-flight right distance from projection",
               inFlightRight.Distance(Vec2(300.0f, 0.0f)),
               expectedExitRadius);
    ExpectTrue("in-flight exits lie on opposite sides",
               inFlightLeft.y > 0.0f && inFlightRight.y < 0.0f);

    Threat observedThreat = lineThreat;
    observedThreat.missileBound = true;
    observedThreat.launchTick = 2000;
    observedThreat.observedHead = Vec2(450.0f, 0.0f);
    observedThreat.observedTick = 2600;
    const Vec2 observedHero(550.0f, 0.0f);
    const Vec2 observedLeft = EvadeGeometry::ClosestLineExit(
        observedThreat, observedHero, heroRadius, extraBuffer, true, 2700);
    const Vec2 observedRight = EvadeGeometry::ClosestLineExit(
        observedThreat, observedHero, heroRadius, extraBuffer, false, 2700);
    ExpectNear("observed exit advances from observed head", observedLeft.x, 550.0f);
    ExpectNear("observed left exit radius", observedLeft.y, expectedExitRadius);
    ExpectNear("observed right exit radius", observedRight.y, -expectedExitRadius);
    ExpectNear("observed left distance from projection",
               observedLeft.Distance(Vec2(550.0f, 0.0f)),
               expectedExitRadius);
    ExpectNear("observed right distance from projection",
               observedRight.Distance(Vec2(550.0f, 0.0f)),
               expectedExitRadius);
    ExpectTrue("observed exits lie on opposite sides",
               observedLeft.y > 0.0f && observedRight.y < 0.0f);
    ExpectTrue("moving missile endpoint is exact-body safe",
               !EvadeGeometry::HeroThreatenedNow(
                   {observedThreat},
                   observedLeft,
                   heroRadius,
                   0.0f,
                   2700,
                   1800.0f));
    ExpectTrue("moving missile endpoint remains in release margin",
               EvadeGeometry::HeroThreatenedNow(
                   {observedThreat},
                   observedLeft,
                   heroRadius,
                   48.0f,
                   2700,
                   1800.0f));

    ExpectTrue("missile route mode defaults straight",
               lineSpell.missileRouteMode == MissileRouteMode::Straight);
    Threat straightObserved = lineThreat;
    straightObserved.observedHead = Vec2(400.0f, 30.0f);
    straightObserved.observedTick = 2000;
    straightObserved.observedSpeed = 1000.0f;
    ExpectNear("straight observed head projects lateral jitter x",
               straightObserved.HeadAtTick(2000).x,
               400.0f);
    ExpectNear("straight observed head filters lateral jitter y",
               straightObserved.HeadAtTick(2000).y,
               0.0f);
    ExpectNear("straight future head follows authored route x",
               straightObserved.HeadAtTick(2100).x,
               500.0f);
    ExpectNear("straight future head follows authored route y",
               straightObserved.HeadAtTick(2100).y,
               0.0f);

    SpellData steeringSpell = lineSpell;
    steeringSpell.missileRouteMode = MissileRouteMode::Steering;
    Threat steeringObserved = ZDEvadeTest::MakeThreat(steeringSpell);
    steeringObserved.observedHead = Vec2(400.0f, 30.0f);
    steeringObserved.observedTick = 2000;
    steeringObserved.observedSpeed = 1000.0f;
    ExpectNear("steering observed head remains exact x",
               steeringObserved.HeadAtTick(2000).x,
               400.0f);
    ExpectNear("steering observed head remains exact y",
               steeringObserved.HeadAtTick(2000).y,
               30.0f);
    ExpectNear("steering future head advances from observed anchor x",
               steeringObserved.HeadAtTick(2100).x,
               500.0f);
    ExpectNear("steering future head keeps observed route anchor y",
               steeringObserved.HeadAtTick(2100).y,
               30.0f);
    ExpectNear("steering future head clamps at route end progress",
               steeringObserved.HeadAtTick(3000).x,
               1000.0f);
    ExpectNear("steering terminal clamp uses exact endpoint y",
               steeringObserved.HeadAtTick(3000).y,
               0.0f);
    ExpectNear("straight negative elapsed does not rewind",
               straightObserved.HeadAtTick(1900).x,
               400.0f);
    ExpectNear("steering negative elapsed does not rewind x",
               steeringObserved.HeadAtTick(1900).x,
               400.0f);
    ExpectNear("steering negative elapsed does not rewind y",
               steeringObserved.HeadAtTick(1900).y,
               30.0f);

    Threat zeroDirectionSteering = steeringObserved;
    zeroDirectionSteering.direction = {};
    ExpectNear("zero steering route holds observed x",
               zeroDirectionSteering.HeadAtTick(2500).x,
               400.0f);
    ExpectNear("zero steering route holds observed y",
               zeroDirectionSteering.HeadAtTick(2500).y,
               30.0f);

    Threat beyondStraight = straightObserved;
    beyondStraight.observedHead = Vec2(1200.0f, 25.0f);
    ExpectNear("straight beyond-route observation clamps endpoint x",
               beyondStraight.HeadAtTick(2000).x,
               1000.0f);
    ExpectNear("straight beyond-route observation clamps endpoint y",
               beyondStraight.HeadAtTick(2000).y,
               0.0f);
    ExpectNear("straight beyond-route future stays endpoint",
               beyondStraight.HeadAtTick(2200).x,
               1000.0f);

    Threat beyondSteering = steeringObserved;
    beyondSteering.observedHead = Vec2(1200.0f, 0.0f);
    ExpectNear("steering beyond-route observation clamps endpoint",
               beyondSteering.HeadAtTick(2000).x,
               1000.0f);
    ExpectNear("steering beyond-route future stays endpoint",
               beyondSteering.HeadAtTick(2200).x,
               1000.0f);

    SpellData circleSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    circleSpell.spellDelay = 250;
    circleSpell.radius = 100.0f;
    Threat circleThreat = ZDEvadeTest::MakeThreat(circleSpell);
    circleThreat.endPos = Vec2(400.0f, 0.0f);

    ExpectNear("circle impact timing",
               static_cast<float>(
                   EvadeGeometry::ImpactTickAt(circleThreat, circleThreat.endPos)),
               1250.0f);
    ExpectTrue("circle contains center at impact",
               EvadeGeometry::ContainsAt(
                   circleThreat, circleThreat.endPos, 0.0f, 0.0f, 1250));
    ExpectTrue("circle excludes point outside radius",
               !EvadeGeometry::ContainsAt(
                   circleThreat, Vec2(501.0f, 0.0f), 0.0f, 0.0f, 1250));

    ExpectTrue("tick addition clamps positive overflow",
               SaturatingTickAdd(INT_MAX, 1) == INT_MAX);
    ExpectTrue("tick addition clamps negative overflow",
               SaturatingTickAdd(INT_MIN, -1) == INT_MIN);
    ExpectTrue("tick addition accepts large positive durations",
               SaturatingTickAdd(INT_MAX - 10, 1000000) == INT_MAX);
    ExpectTrue("tick addition preserves ordinary sums",
               SaturatingTickAdd(1000, 250) == 1250);

    SpellData persistentSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    persistentSpell.spellDelay = 0;
    persistentSpell.projectileSpeed = 0.0f;
    persistentSpell.radius = 100.0f;
    persistentSpell.hasEndExplosion = true;
    persistentSpell.secondaryRadius = 75.0f;
    persistentSpell.endExplosionDuration = 100;
    Threat persistentThreat = ZDEvadeTest::MakeThreat(persistentSpell);
    persistentThreat.startTick = 1000;
    persistentThreat.endTick = INT_MAX;
    persistentThreat.persistent = true;
    persistentThreat.endPos = Vec2(400.0f, 0.0f);
    ExpectTrue("persistent threat remains unexpired at now",
               !persistentThreat.IsExpiredAt(1000));
    ExpectTrue("persistent threat remains unexpired at horizon",
               !persistentThreat.IsExpiredAt(1000000));
    ExpectTrue("persistent threat body remains active at now",
               EvadeGeometry::ContainsAt(
                   persistentThreat, persistentThreat.endPos, 0.0f, 0.0f, 1000));
    ExpectTrue("persistent threat body remains active at horizon",
               EvadeGeometry::ContainsAt(
                   persistentThreat, persistentThreat.endPos, 0.0f, 0.0f, 1000000));
    ExpectTrue("persistent threat analysis remains active through horizon",
               EvadeGeometry::HeroThreatenedNow(
                   {persistentThreat},
                   persistentThreat.endPos,
                   0.0f,
                   0.0f,
                   1000000,
                   2000.0f));
    Threat overflowPersistentExplosion = persistentThreat;
    overflowPersistentExplosion.startTick = INT_MAX;
    overflowPersistentExplosion.endTick = INT_MAX;
    overflowPersistentExplosion.projectileTerminated = true;
    overflowPersistentExplosion.projectileTerminationTick = INT_MAX;
    ExpectTrue("persistent explosion timing saturates instead of wrapping",
               overflowPersistentExplosion.EndExplosionEndTick() == INT_MAX);
    ExpectTrue("persistent saturated explosion remains active",
               EvadeGeometry::ContainsAt(
                   overflowPersistentExplosion,
                   overflowPersistentExplosion.endPos,
                   0.0f,
                   0.0f,
                   INT_MAX));

    Threat expiringThreat = persistentThreat;
    expiringThreat.persistent = false;
    expiringThreat.data = persistentThreat.data;
    expiringThreat.endTick = 2000;
    ExpectTrue("ordinary threat remains active at expiration boundary",
               !expiringThreat.IsExpiredAt(2250));
    ExpectTrue("ordinary threat expires after expiration boundary",
               expiringThreat.IsExpiredAt(2251));

    SpellDatabase::Initialize();
    ExpectTrue("spell database has no invalid cones",
               SpellDatabase::InvalidConeSpellCount() == 0);
    ExpectTrue("spell database has no active Arc entries",
               SpellDatabase::SupportedArcSpellCount() == 0);
    const SpellData* yuumiQ = FindSpell("YuumiQCast");
    ExpectTrue("Yuumi routed missile is steering",
               yuumiQ &&
                   yuumiQ->missileRouteMode == MissileRouteMode::Steering);
    const SpellData* ahriQ = FindSpell("AhriQ");
    ExpectTrue("ordinary missile remains straight",
               ahriQ &&
                   ahriQ->missileRouteMode == MissileRouteMode::Straight);
    int coneCount = 0;
    for (const SpellData& spell : SpellDatabase::Spells) {
        if (spell.spellType != ZDSpellType::Cone) continue;
        ++coneCount;
        ExpectTrue("database cone angle is valid",
                   spell.coneAngleDegrees > 0.0f &&
                       spell.coneAngleDegrees <= 360.0f);
        ExpectNear("database cone does not use multipleAngle",
                   spell.multipleAngle,
                   0.0f);
    }
    ExpectTrue("spell database contains cones", coneCount > 0);

    const SpellData* wildCards = FindSpell("WildCards");
    ExpectTrue("Wild Cards line entry exists", wildCards != nullptr);
    if (wildCards) {
        ExpectTrue("Wild Cards remains a line spell",
                   wildCards->spellType == ZDSpellType::Line);
        ExpectTrue("Wild Cards remains multi-projectile",
                   wildCards->multipleNumber > 1);
        ExpectNear("Wild Cards keeps projectile spread angle",
                   wildCards->multipleAngle,
                   28.0f);
    }

    ExpectConeBoundary("AatroxQ2", 60.0f, 29.0f, 31.0f);
    ExpectConeBoundary("Incinerate", 25.0f, 12.0f, 13.0f);
    ExpectConeBoundary("CassiopeiaR", 80.0f, 39.0f, 41.0f);
    ExpectConeBoundary("SwainQ", 45.0f, 22.0f, 23.0f);

    const SpellData* swainSpell = FindSpell("SwainQ");
    ExpectTrue("Swain Q exists", swainSpell != nullptr);
    if (swainSpell) {
        Threat swainThreat = ZDEvadeTest::MakeThreat(*swainSpell);
        const int activeTick = swainThreat.startTick + swainThreat.Delay();
        ExpectTrue("Swain Q radius remains database metadata",
                   swainThreat.Radius() == 725.0f);
        ExpectTrue("Swain Q excludes behind-caster point",
                   !EvadeGeometry::ContainsAt(
                       swainThreat, Vec2(-100.0f, 0.0f), 0.0f, 0.0f, activeTick));
        ExpectTrue("Swain Q excludes far-side point",
                   !EvadeGeometry::ContainsAt(
                       swainThreat, Vec2(0.0f, 700.0f), 0.0f, 0.0f, activeTick));
    }

    SpellData paddedCone = ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    paddedCone.range = 100.0f;
    paddedCone.radius = 725.0f;
    paddedCone.coneEdgePadding = 5.0f;
    Threat paddedConeThreat = ZDEvadeTest::MakeThreat(paddedCone);
    const int paddedConeTick = paddedConeThreat.startTick + paddedConeThreat.Delay();
    ExpectTrue("cone edge padding expands sector boundary",
               EvadeGeometry::ContainsAt(
                   paddedConeThreat, Vec2(104.0f, 0.0f), 0.0f, 0.0f, paddedConeTick));
    ExpectTrue("cone radius does not expand sector boundary",
               !EvadeGeometry::ContainsAt(
                   paddedConeThreat, Vec2(106.0f, 0.0f), 0.0f, 0.0f, paddedConeTick));

    SpellData invalidCone = ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    invalidCone.coneAngleDegrees = 0.0f;
    Threat invalidConeThreat = ZDEvadeTest::MakeThreat(invalidCone);
    const int invalidConeTick = invalidConeThreat.startTick + invalidConeThreat.Delay();
    ExpectTrue("invalid cone remains conservatively dangerous",
               EvadeGeometry::ContainsAt(
                   invalidConeThreat, Vec2(-5000.0f, 0.0f), 0.0f, 0.0f, invalidConeTick));
    std::vector<Vec2> invalidConeExits;
    EvadeGeometry::AddConeExits(
        invalidConeThreat, Vec2(100.0f, 0.0f), 35.0f, 10.0f, invalidConeExits);
    ExpectTrue("invalid cone has no supported analytical exits",
               invalidConeExits.empty());

    const SweptCircleGridGeometry fineGrid = {
        0.0f, 0.0f, 100.0f, 100.0f, 1.0f, 100, 100
    };
    const std::vector<Vec2> straightSweep = {
        Vec2(20.0f, 50.0f),
        Vec2(80.0f, 50.0f),
    };
    const auto openCells = [](int, int) {
        return static_cast<std::uint16_t>(0);
    };
    const auto tinyBetweenRays = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 49 && y == 51
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    ExpectTrue("tiny wall cell between old rays rejects exact sweep",
               !SweptCirclePathWalkable(
                   straightSweep,
                   10.0f,
                   fineGrid,
                   tinyBetweenRays));

    const auto wallInsideDisk = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 54 && y == 54
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    ExpectTrue("wall fully inside hero disk rejects exact sweep",
               !SweptCirclePathWalkable(
                   std::array<Vec2, 2>{
                       Vec2(50.0f, 50.0f),
                       Vec2(70.0f, 50.0f),
                   },
                   10.0f,
                   fineGrid,
                   wallInsideDisk));

    const SweptCircleGridGeometry tenUnitGrid = {
        0.0f, 0.0f, 100.0f, 100.0f, 10.0f, 10, 10
    };
    const auto tangentCornerWall = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 5 && y == 5
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    ExpectTrue("corner tangency rejects at exact hero radius",
               !SweptCirclePathWalkable(
                   std::array<Vec2, 2>{
                       Vec2(20.0f, 40.0f),
                       Vec2(40.0f, 40.0f),
                   },
                   std::sqrt(200.0f),
                   tenUnitGrid,
                   tangentCornerWall));

    const auto wallOnly = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 5 && y == 5
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    const auto brushWall = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 5 && y == 5
                ? Offset::NavGridCellLayout::CELL_WALL |
                      Offset::NavGridFlags::FlagBrush
                : 0);
    };
    const std::array<Vec2, 2> brushSemanticPath = {
        Vec2(20.0f, 55.0f),
        Vec2(80.0f, 55.0f),
    };
    ExpectTrue("plain wall cell is non-walkable",
               !SweptCirclePathWalkable(
                   brushSemanticPath,
                   2.0f,
                   tenUnitGrid,
                   wallOnly));
    ExpectTrue("brush wall cell matches GridRef walkable semantics",
               SweptCirclePathWalkable(
                   brushSemanticPath,
                   2.0f,
                   tenUnitGrid,
                   brushWall));

    const auto wideCorridorCells = [](int, int y) {
        return static_cast<std::uint16_t>(
            y <= 3 || y >= 6
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    const auto narrowCorridorCells = [](int, int y) {
        return static_cast<std::uint16_t>(
            y == 5
                ? 0
                : Offset::NavGridCellLayout::CELL_WALL);
    };
    ExpectTrue("corridor wider than hero diameter accepts exact sweep",
               SweptCirclePathWalkable(
                   straightSweep,
                   9.0f,
                   tenUnitGrid,
                   wideCorridorCells));
    ExpectTrue("corridor narrower than hero diameter rejects exact sweep",
               !SweptCirclePathWalkable(
                   straightSweep,
                   9.0f,
                   tenUnitGrid,
                   narrowCorridorCells));

    ExpectTrue("outside-grid hero disk fails closed",
               !SweptCirclePathWalkable(
                   std::array<Vec2, 2>{
                       Vec2(5.0f, 50.0f),
                       Vec2(20.0f, 50.0f),
                   },
                   6.0f,
                   fineGrid,
                   openCells));
    ExpectTrue("invalid nav cell fails closed",
               !SweptCirclePathWalkable(
                   straightSweep,
                   10.0f,
                   fineGrid,
                   [](int, int) {
                       return CoreNavGrid::kInvalidRawFlags;
                   }));
    ExpectTrue("zero hero radius fails closed",
               !SweptCirclePathWalkable(
                   straightSweep,
                   0.0f,
                   fineGrid,
                   openCells));
    ExpectTrue("negative hero radius fails closed",
               !SweptCirclePathWalkable(
                   straightSweep,
                   -1.0f,
                   fineGrid,
                   openCells));
    ExpectTrue("invalid hero radius fails closed",
               !SweptCirclePathWalkable(
                   straightSweep,
                   std::numeric_limits<float>::quiet_NaN(),
                   fineGrid,
                   openCells));
    ExpectTrue("invalid path vertex fails closed",
               !SweptCirclePathWalkable(
                   std::array<Vec2, 2>{
                       Vec2(20.0f, 50.0f),
                       Vec2(
                           std::numeric_limits<float>::quiet_NaN(),
                           50.0f),
                   },
                   10.0f,
                   fineGrid,
                   openCells));
    ExpectTrue("zero-length sweep segment fails closed",
               !SweptCirclePathWalkable(
                   std::array<Vec2, 2>{
                       Vec2(20.0f, 50.0f),
                       Vec2(20.0f, 50.0f),
                   },
                   10.0f,
                   fineGrid,
                   openCells));

    const std::array<Vec2, 3> joinedSweep = {
        Vec2(20.0f, 50.0f),
        Vec2(50.0f, 50.0f),
        Vec2(50.0f, 80.0f),
    };
    ExpectTrue("open multi-segment join accepts exact sweep",
               SweptCirclePathWalkable(
                   joinedSweep,
                   8.0f,
                   fineGrid,
                   openCells));
    const auto joinCornerWall = [](int x, int y) {
        return static_cast<std::uint16_t>(
            x == 55 && y == 44
                ? Offset::NavGridCellLayout::CELL_WALL
                : 0);
    };
    ExpectTrue("multi-segment join disk rejects corner wall",
               !SweptCirclePathWalkable(
                   joinedSweep,
                   8.0f,
                   fineGrid,
                   joinCornerWall));

    const SweptCircleGridGeometry realisticGrid = {
        0.0f, 0.0f, 2500.0f, 2500.0f, 25.0f, 100, 100
    };
    const std::array<Vec2, 2> maximumRuntimeSweep = {
        Vec2(500.0f, 1000.0f),
        Vec2(1260.0f, 1000.0f),
    };
    constexpr float maximumRuntimeHeroRadius = 65.0f;
    constexpr int candidateBatchSize = 320;
    constexpr int navBenchmarkIterations = 200;
    std::size_t navBenchmarkCalls = 0;
    bool navBenchmarkWalkable = true;
    const auto navBenchmarkStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < navBenchmarkIterations; ++iteration) {
        for (int candidate = 0; candidate < candidateBatchSize; ++candidate) {
            navBenchmarkWalkable =
                SweptCirclePathWalkable(
                    maximumRuntimeSweep,
                    maximumRuntimeHeroRadius,
                    realisticGrid,
                    [&](int, int) {
                        ++navBenchmarkCalls;
                        return static_cast<std::uint16_t>(0);
                    }) &&
                navBenchmarkWalkable;
        }
    }
    const double navBenchmarkBatchMicroseconds =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - navBenchmarkStart).count() /
        static_cast<double>(navBenchmarkIterations);
    const std::size_t projectedCandidateBatchCalls =
        navBenchmarkCalls /
        static_cast<std::size_t>(navBenchmarkIterations);
    ExpectTrue("realistic fake-nav sweep remains walkable",
               navBenchmarkWalkable);
    ExpectTrue("320-candidate exact cell probes stay bounded",
               projectedCandidateBatchCalls <= 320u * 300u);
    std::printf(
        "INFO: exact nav sweep 320 candidates %.2f us (%zu cell probes)\n",
        navBenchmarkBatchMicroseconds,
        projectedCandidateBatchCalls);

    InstallWalkableTestGrid();
    constexpr int blinkNow = 1000;
    constexpr float blinkCastMs = 300.0f;
    constexpr float blinkHeroRadius = 20.0f;
    const Vec2 blinkOrigin(200.0f, 500.0f);
    const Vec2 blinkEndpoint(800.0f, 500.0f);
    EvadeSettings blinkSettings;
    blinkSettings.endpointBuffer = 0.0f;
    blinkSettings.pathBuffer = 0.0f;
    blinkSettings.inputDelayMs = 0.0f;
    blinkSettings.minimumTimeMarginMs = 0.0f;
    blinkSettings.maxThreatHorizonMs = 2000.0f;

    const Vec2 adjacentWallBlinkEndpoint(775.0f, 650.0f);
    const Vec2 adjacentWallBlinkOrigin(200.0f, 650.0f);
    ExpectTrue("runtime fixture blocks adjacent blink wall",
               CoreNavGrid::Get().SetCollisionFlags(
                   6,
                   5,
                   CoreNavGrid::Collision_Wall));
    ExpectTrue("blink endpoint center cell remains walkable",
               CoreNavGrid::IsWalkable(
                   Vec3::From2D(adjacentWallBlinkEndpoint, 0.0f)));
    const CandidateEvaluation overlappingWallBlink =
        EvadeGeometry::EvaluateBlinkCandidate(
            adjacentWallBlinkEndpoint,
            adjacentWallBlinkOrigin,
            adjacentWallBlinkEndpoint,
            0.0f,
            35.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("blink rejects adjacent wall overlapping hero disk",
               overlappingWallBlink.valid &&
                   !overlappingWallBlink.walkable &&
                   overlappingWallBlink.rejectReason ==
                       PlannerRejectReason::Wall);
    const CandidateEvaluation clearWallBlink =
        EvadeGeometry::EvaluateBlinkCandidate(
            adjacentWallBlinkEndpoint,
            adjacentWallBlinkOrigin,
            adjacentWallBlinkEndpoint,
            0.0f,
            24.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("blink accepts endpoint with enough wall clearance",
               clearWallBlink.valid &&
                   clearWallBlink.walkable &&
                   clearWallBlink.strictSafe);
    const CandidateEvaluation tangentWallBlink =
        EvadeGeometry::EvaluateBlinkCandidate(
            adjacentWallBlinkEndpoint,
            adjacentWallBlinkOrigin,
            adjacentWallBlinkEndpoint,
            0.0f,
            25.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("blink conservatively rejects tangent wall boundary",
               tangentWallBlink.valid &&
                   !tangentWallBlink.walkable &&
                   tangentWallBlink.rejectReason ==
                       PlannerRejectReason::Wall);
    ExpectTrue("runtime fixture restores adjacent blink wall",
               CoreNavGrid::Get().SetCollisionFlags(
                   6,
                   5,
                   CoreNavGrid::Collision_None));
    ExpectTrue("runtime fixture blocks wall crossed only by blink",
               CoreNavGrid::Get().SetCollisionFlags(
                   4,
                   5,
                   CoreNavGrid::Collision_Wall));
    const CandidateEvaluation blinkAcrossNavWall =
        EvadeGeometry::EvaluateBlinkCandidate(
            adjacentWallBlinkEndpoint,
            Vec2(-200.0f, 650.0f),
            adjacentWallBlinkEndpoint,
            0.0f,
            24.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("blink nav validation checks endpoint disk only",
               blinkAcrossNavWall.valid &&
                   blinkAcrossNavWall.walkable &&
                   blinkAcrossNavWall.strictSafe);
    ExpectTrue("runtime fixture restores crossed blink wall",
               CoreNavGrid::Get().SetCollisionFlags(
                   4,
                   5,
                   CoreNavGrid::Collision_None));
    const CandidateEvaluation zeroRadiusBlink =
        EvadeGeometry::EvaluateBlinkCandidate(
            adjacentWallBlinkEndpoint,
            adjacentWallBlinkOrigin,
            adjacentWallBlinkEndpoint,
            0.0f,
            0.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("zero-radius blink retains nav epsilon semantics",
               zeroRadiusBlink.valid &&
                   zeroRadiusBlink.walkable &&
                   zeroRadiusBlink.strictSafe);
    const CandidateEvaluation outsideGridBlink =
        EvadeGeometry::EvaluateBlinkCandidate(
            Vec2(-990.0f, 650.0f),
            adjacentWallBlinkOrigin,
            Vec2(-990.0f, 650.0f),
            0.0f,
            20.0f,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {});
    ExpectTrue("blink endpoint disk fails closed outside grid",
               outsideGridBlink.valid &&
                   !outsideGridBlink.walkable &&
                   outsideGridBlink.rejectReason ==
                       PlannerRejectReason::Wall);
    CoreRuntime::g_ctx.navGrid = 0;
    ++CoreRuntime::g_ctx.refreshGeneration;
    ExpectTrue("point walkability wrapper fails closed for invalid grid",
               !EvadeGeometry::PointWalkable(
                   adjacentWallBlinkEndpoint,
                   0.0f,
                   24.0f));
    InstallWalkableTestGrid();

    const Vec3 blockedRuntimeCell =
        Vec3::From2D(Vec2(650.0f, 500.0f), 0.0f);
    ExpectTrue("runtime fixture installs blocked nav cell",
               CoreNavGrid::SetCollisionFlags(
                   blockedRuntimeCell,
                   CoreNavGrid::Collision_Wall));
    const CandidateEvaluation blockedPublicCandidate =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::Cursor,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {});
    ExpectTrue("public candidate cannot bypass blocked nav path",
               blockedPublicCandidate.valid &&
                   !blockedPublicCandidate.walkable &&
                   blockedPublicCandidate.rejectReason ==
                       PlannerRejectReason::Wall);
    ExpectTrue("runtime fixture restores walkable nav cell",
               CoreNavGrid::SetCollisionFlags(
                   blockedRuntimeCell,
                   CoreNavGrid::Collision_None));
    const CandidateEvaluation validatedOpenPolyline =
        EvadeGeometry::EvaluatePathCandidate(
            {
                blinkOrigin,
                Vec2(500.0f, 700.0f),
                blinkEndpoint,
            },
            PlannerCandidateSource::Cursor,
            -1,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {});
    ExpectTrue("path evaluator remains functional after one nav validation",
               validatedOpenPolyline.valid &&
                   validatedOpenPolyline.walkable &&
                   validatedOpenPolyline.strictSafe);

    SpellData extremeFutureCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    extremeFutureCircleSpell.spellDelay = 0;
    extremeFutureCircleSpell.projectileSpeed = 0.0f;
    extremeFutureCircleSpell.radius = 80.0f;
    extremeFutureCircleSpell.extraEndTime = 100;
    Threat extremeFutureCircle =
        ZDEvadeTest::MakeThreat(extremeFutureCircleSpell);
    extremeFutureCircle.startTick = INT_MAX;
    extremeFutureCircle.endTick = INT_MAX;
    extremeFutureCircle.endPos = blinkEndpoint;
    const CandidateEvaluation extremeFutureBody =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            INT_MIN,
            blinkCastMs,
            blinkSettings,
            {extremeFutureCircle});
    ExpectTrue("geometry ignores body beyond INT_MIN analysis horizon",
               extremeFutureBody.strictSafe);
    ExpectTrue("geometry future impact subtraction does not wrap",
               !EvadeGeometry::HeroThreatenedNow(
                   {extremeFutureCircle},
                   blinkEndpoint,
                   blinkHeroRadius,
                   0.0f,
                   INT_MIN,
                   2000.0f));

    SpellData extremeExplosionSpell = extremeFutureCircleSpell;
    extremeExplosionSpell.hasEndExplosion = true;
    extremeExplosionSpell.secondaryRadius = 100.0f;
    extremeExplosionSpell.endExplosionDuration = INT_MAX;
    Threat extremeFutureExplosion =
        ZDEvadeTest::MakeThreat(extremeExplosionSpell);
    extremeFutureExplosion.startTick = INT_MAX;
    extremeFutureExplosion.endTick = INT_MAX;
    extremeFutureExplosion.endPos = Vec2(950.0f, 950.0f);
    extremeFutureExplosion.collisionExplosionCenter = blinkEndpoint;
    extremeFutureExplosion.projectileTerminated = true;
    extremeFutureExplosion.projectileTerminationTick = INT_MAX;
    const CandidateEvaluation extremeExplosionCandidate =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            INT_MIN,
            blinkCastMs,
            blinkSettings,
            {extremeFutureExplosion});
    ExpectTrue("geometry ignores explosion beyond INT_MIN analysis horizon",
               extremeExplosionCandidate.strictSafe);
    ExpectTrue("geometry explosion end composes at INT_MAX",
               extremeFutureExplosion.EndExplosionEndTick() == INT_MAX);

    SpellData middleCircleSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    middleCircleSpell.spellDelay = 0;
    middleCircleSpell.projectileSpeed = 0.0f;
    middleCircleSpell.radius = 60.0f;
    middleCircleSpell.extraEndTime = 1000;
    Threat middleCircle = ZDEvadeTest::MakeThreat(middleCircleSpell);
    middleCircle.startTick = 900;
    middleCircle.endTick = 2500;
    middleCircle.endPos = Vec2(500.0f, 500.0f);
    const std::vector<Threat> middleHazard = {middleCircle};
    const CandidateEvaluation blinkAcrossMiddle =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            middleHazard);
    const CandidateEvaluation dashAcrossMiddle =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::EvadeSpell,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            middleHazard);
    ExpectTrue("blink ignores active hazard crossed only instantaneously",
               blinkAcrossMiddle.strictSafe);
    ExpectTrue("equivalent continuous dash crosses middle hazard",
               !dashAcrossMiddle.strictSafe && dashAcrossMiddle.pathDanger > 0);

    Threat bendThreat = middleCircle;
    bendThreat.id = 101;
    bendThreat.endPos = Vec2(500.0f, 500.0f);
    const CandidateEvaluation pathAcrossSameThreat =
        EvadeGeometry::EvaluatePathCandidate(
            {blinkOrigin, Vec2(500.0f, 500.0f), blinkEndpoint},
            PlannerCandidateSource::Cursor,
            -1,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {bendThreat});
    ExpectTrue("same threat crossing two polyline segments counts once",
               pathAcrossSameThreat.collisionCount == 1);

    Threat positiveIdentityA = middleCircle;
    positiveIdentityA.id = 303;
    Threat positiveIdentityB = positiveIdentityA;
    const std::vector<Threat> repeatedPositiveIdentity = {
        positiveIdentityA,
        positiveIdentityB,
    };
    const CandidateEvaluation singleAcrossRepeatedPositive =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::Cursor,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            repeatedPositiveIdentity);
    const CandidateEvaluation pathAcrossRepeatedPositive =
        EvadeGeometry::EvaluatePathCandidate(
            {blinkOrigin, Vec2(500.0f, 500.0f), blinkEndpoint},
            PlannerCandidateSource::Cursor,
            -1,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            repeatedPositiveIdentity);
    ExpectTrue("same positive ID counts once in single candidate",
               singleAcrossRepeatedPositive.collisionCount == 1 &&
                   singleAcrossRepeatedPositive.summedExposureDanger ==
                       positiveIdentityA.Danger());
    ExpectTrue("same positive ID counts once over polyline",
               pathAcrossRepeatedPositive.collisionCount == 1 &&
                   pathAcrossRepeatedPositive.summedExposureDanger ==
                       positiveIdentityA.Danger());

    Threat negativeIdentityA = middleCircle;
    negativeIdentityA.id = -1;
    Threat negativeIdentityB = negativeIdentityA;
    const CandidateEvaluation singleAcrossSeparateNegativeRecords =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::Cursor,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {negativeIdentityA, negativeIdentityB});
    const CandidateEvaluation pathAcrossSeparateNegativeRecords =
        EvadeGeometry::EvaluatePathCandidate(
            {blinkOrigin, Vec2(500.0f, 500.0f), blinkEndpoint},
            PlannerCandidateSource::Cursor,
            -1,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {negativeIdentityA, negativeIdentityB});
    const CandidateEvaluation pathAcrossOneNegativeRecord =
        EvadeGeometry::EvaluatePathCandidate(
            {blinkOrigin, Vec2(500.0f, 500.0f), blinkEndpoint},
            PlannerCandidateSource::Cursor,
            -1,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {negativeIdentityA});
    ExpectTrue("separate negative-ID records count separately",
               singleAcrossSeparateNegativeRecords.collisionCount == 2);
    ExpectTrue("separate negative-ID records remain distinct over polyline",
               pathAcrossSeparateNegativeRecords.collisionCount == 2);
    ExpectTrue("same negative-ID record across segments counts once",
               pathAcrossOneNegativeRecord.collisionCount == 1);

    SpellData delayedCircleSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    delayedCircleSpell.spellDelay = 250;
    delayedCircleSpell.projectileSpeed = 0.0f;
    delayedCircleSpell.radius = 60.0f;
    delayedCircleSpell.extraEndTime = 100;
    Threat delayedCircle = ZDEvadeTest::MakeThreat(delayedCircleSpell);
    delayedCircle.startTick = blinkNow;
    delayedCircle.endTick = 2000;
    delayedCircle.endPos = Vec2(500.0f, 500.0f);
    const CandidateEvaluation dashLeavesBeforeActivation =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::EvadeSpell,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {delayedCircle});
    ExpectTrue("static body is inactive immediately before activation",
               !EvadeGeometry::ContainsAt(
                   delayedCircle,
                   delayedCircle.endPos,
                   0.0f,
                   0.0f,
                   blinkNow + delayedCircle.Delay() - 1));
    ExpectTrue("dash crossing and leaving before activation stays path-safe",
               dashLeavesBeforeActivation.pathSafe &&
                   dashLeavesBeforeActivation.pathDanger == 0);
    ExpectTrue("future static impact remains in anticipatory envelope",
               dashLeavesBeforeActivation.reenteredDanger);

    const auto makeExplosion = [&](const Vec2& center, int explosionTick) {
        SpellData spell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
        spell.spellDelay = 0;
        spell.projectileSpeed = 0.0f;
        spell.radius = 20.0f;
        spell.hasEndExplosion = true;
        spell.secondaryRadius = 80.0f;
        spell.endExplosionDuration = 100;
        Threat threat = ZDEvadeTest::MakeThreat(spell);
        threat.startTick = explosionTick;
        threat.endTick = explosionTick + 1000;
        threat.endPos = Vec2(950.0f, 950.0f);
        threat.collisionExplosionCenter = center;
        return threat;
    };

    const Threat explosionAtNowThreat = makeExplosion(blinkOrigin, blinkNow);
    ExpectTrue("end explosion is inactive immediately before activation",
               !EvadeGeometry::ContainsAt(
                   makeExplosion(blinkOrigin, blinkNow + 100),
                   blinkOrigin,
                   blinkHeroRadius,
                   0.0f,
                   blinkNow + 99));
    ExpectTrue("end explosion is active exactly at now",
               EvadeGeometry::ContainsAt(
                   explosionAtNowThreat,
                   blinkOrigin,
                   blinkHeroRadius,
                   0.0f,
                   blinkNow));
    const CandidateEvaluation originExplosionAtNow =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {explosionAtNowThreat});
    const CandidateEvaluation originExplosionBeforeCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkOrigin, 1100)});
    const CandidateEvaluation originExplosionJustBeforeCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkOrigin, 1299)});
    ExpectTrue("origin explosion at now is unsafe",
               !originExplosionAtNow.strictSafe &&
                   originExplosionAtNow.pathDanger > 0 &&
                   originExplosionAtNow.dangerExposureMs > 0.0f &&
                   originExplosionAtNow.summedExposureDanger ==
                       explosionAtNowThreat.Danger());
    ExpectTrue("origin explosion before blink completion is unsafe",
               !originExplosionBeforeCompletion.strictSafe &&
                   originExplosionBeforeCompletion.pathDanger > 0);
    ExpectTrue("origin explosion just before completion is unsafe",
               !originExplosionJustBeforeCompletion.strictSafe &&
                   originExplosionJustBeforeCompletion.pathDanger > 0);

    const CandidateEvaluation originExplosionAtCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkOrigin, 1300)});
    const CandidateEvaluation originExplosionAfterCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkOrigin, 1401)});
    const CandidateEvaluation endpointExplosionAfterCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkEndpoint, 1401)});
    const CandidateEvaluation endpointExplosionAtCompletion =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            blinkSettings,
            {makeExplosion(blinkEndpoint, 1300)});
    ExpectTrue("origin explosion at transition uses endpoint position",
               originExplosionAtCompletion.strictSafe);
    ExpectTrue("origin explosion after completion does not invalidate endpoint",
               originExplosionAfterCompletion.strictSafe);
    ExpectTrue("endpoint explosion at transition is unsafe",
               !endpointExplosionAtCompletion.strictSafe &&
                   endpointExplosionAtCompletion.endpointDanger > 0);
    ExpectTrue("post-completion explosion is evaluated at endpoint",
               !endpointExplosionAfterCompletion.strictSafe &&
                   endpointExplosionAfterCompletion.endpointDanger > 0 &&
                   endpointExplosionAfterCompletion.dangerExposureMs >
                       0.0f &&
                   endpointExplosionAfterCompletion
                           .summedExposureDanger ==
                       explosionAtNowThreat.Danger());

    SpellData sweptExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    sweptExplosionSpell.spellDelay = 0;
    sweptExplosionSpell.projectileSpeed = 0.0f;
    sweptExplosionSpell.radius = 5.0f;
    sweptExplosionSpell.hasEndExplosion = true;
    sweptExplosionSpell.secondaryRadius = 50.0f;
    sweptExplosionSpell.endExplosionDuration = 100;
    Threat sweptExplosion =
        ZDEvadeTest::MakeThreat(sweptExplosionSpell);
    sweptExplosion.id = 707;
    sweptExplosion.startTick = blinkNow + 49;
    sweptExplosion.endTick = 3000;
    sweptExplosion.endPos = Vec2(1800.0f, 1200.0f);
    sweptExplosion.collisionExplosionCenter = Vec2(705.0f, 549.0f);
    EvadeSettings sweptExplosionSettings = blinkSettings;
    sweptExplosionSettings.inputDelayMs = 0.0f;
    sweptExplosionSettings.pathStep = 4.0f;
    sweptExplosionSettings.temporalStepMs = 6.0f;
    const CandidateEvaluation sweptExplosionCrossing =
        EvadeGeometry::EvaluateCandidate(
            Vec2(1200.0f, 500.0f),
            PlannerCandidateSource::Cursor,
            -1,
            Vec2(200.0f, 500.0f),
            Vec2(1200.0f, 500.0f),
            0.0f,
            10000.0f,
            0.0f,
            blinkNow,
            sweptExplosionSettings,
            {sweptExplosion, sweptExplosion});
    ExpectTrue("between-sample explosion crossing is path-dangerous",
               !sweptExplosionCrossing.pathSafe &&
                   sweptExplosionCrossing.pathDanger ==
                       sweptExplosion.Danger());
    ExpectTrue("repeated explosion identity counts once",
               sweptExplosionCrossing.collisionCount == 1);
    ExpectTrue("continuous explosion preserves endpoint safety",
               sweptExplosionCrossing.endpointSafe &&
                   sweptExplosionCrossing.endpointDanger == 0);
    ExpectTrue("continuous explosion reports negative clearance",
               sweptExplosionCrossing.minimumClearance < 0.0f);
    ExpectNear("continuous explosion reports analytical first contact",
               sweptExplosionCrossing.firstCollisionTimeMs,
               49.5f,
               0.2f);
    ExpectTrue("continuous explosion contributes exact exposure",
               sweptExplosionCrossing.dangerExposureMs > 4.0f);

    Threat tangentExplosion = sweptExplosion;
    tangentExplosion.id = 708;
    tangentExplosion.collisionExplosionCenter = Vec2(705.0f, 550.0f);
    const CandidateEvaluation tangentExplosionCrossing =
        EvadeGeometry::EvaluateCandidate(
            Vec2(1200.0f, 500.0f),
            PlannerCandidateSource::Cursor,
            -1,
            Vec2(200.0f, 500.0f),
            Vec2(1200.0f, 500.0f),
            0.0f,
            10000.0f,
            0.0f,
            blinkNow,
            sweptExplosionSettings,
            {tangentExplosion});
    ExpectTrue("tangent explosion contact is not sample-false-safe",
               !tangentExplosionCrossing.pathSafe);
    ExpectNear("tangent explosion clearance is exact",
               tangentExplosionCrossing.minimumClearance,
               0.0f,
               0.02f);

    Threat walkingEndpointExplosion = makeExplosion(blinkEndpoint, 1401);
    walkingEndpointExplosion.id = 202;
    const CandidateEvaluation futureEndpointOnly =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::Cursor,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {walkingEndpointExplosion});
    ExpectTrue("future endpoint-only explosion contributes unique coverage",
               futureEndpointOnly.endpointDanger > 0 &&
                   futureEndpointOnly.collisionCount == 1 &&
                   futureEndpointOnly.firstCollisionTimeMs != FLT_MAX);
    Threat repeatedEndpointExplosionA =
        makeExplosion(blinkEndpoint, 1401);
    repeatedEndpointExplosionA.id = 404;
    Threat repeatedEndpointExplosionB = repeatedEndpointExplosionA;
    const CandidateEvaluation repeatedEndpointExplosion =
        EvadeGeometry::EvaluateCandidate(
            blinkEndpoint,
            PlannerCandidateSource::Cursor,
            -1,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            2000.0f,
            blinkHeroRadius,
            blinkNow,
            blinkSettings,
            {repeatedEndpointExplosionA, repeatedEndpointExplosionB});
    ExpectTrue("same positive ID endpoint explosion counts once",
               repeatedEndpointExplosion.collisionCount == 1);

    EvadeSettings tightHorizonSettings = blinkSettings;
    tightHorizonSettings.maxThreatHorizonMs = 100.0f;
    const CandidateEvaluation explosionAtHorizon =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            tightHorizonSettings,
            {makeExplosion(blinkOrigin, 1100)});
    const CandidateEvaluation explosionAfterHorizon =
        EvadeGeometry::EvaluateBlinkCandidate(
            blinkEndpoint,
            blinkOrigin,
            blinkEndpoint,
            0.0f,
            blinkHeroRadius,
            blinkNow,
            blinkCastMs,
            tightHorizonSettings,
            {makeExplosion(blinkOrigin, 1101)});
    ExpectTrue("origin explosion exactly at horizon is evaluated",
               !explosionAtHorizon.strictSafe &&
                   explosionAtHorizon.pathDanger > 0);
    ExpectTrue("origin explosion beyond horizon is ignored",
               explosionAfterHorizon.strictSafe);

    SpellData terminalLineSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    terminalLineSpell.spellDelay = 0;
    terminalLineSpell.projectileSpeed = 1000.0f;
    terminalLineSpell.radius = 20.0f;
    terminalLineSpell.extraEndTime = 0;
    Threat terminalLine = ZDEvadeTest::MakeThreat(terminalLineSpell);
    terminalLine.startPos = Vec2(0.0f, 0.0f);
    terminalLine.endPos = Vec2(1000.0f, 0.0f);
    terminalLine.direction = Vec2(1.0f, 0.0f);
    terminalLine.startTick = 1000;
    terminalLine.endTick = 3000;
    ExpectTrue("moving line occupies endpoint through canonical linger",
               EvadeGeometry::OccupiesAt(
                   terminalLine, terminalLine.endPos, 0.0f, 0.0f, 2080));
    ExpectTrue("moving line releases after canonical linger",
               !EvadeGeometry::OccupiesAt(
                   terminalLine, terminalLine.endPos, 0.0f, 0.0f, 2081));
    ExpectTrue("hero query uses canonical moving-line terminal tick",
               EvadeGeometry::HeroThreatenedNow(
                   {terminalLine},
                   terminalLine.endPos,
                   0.0f,
                   0.0f,
                   2050,
                   100.0f));

    SpellData fractionalLineSpell = terminalLineSpell;
    fractionalLineSpell.projectileSpeed = 50000.0f;
    Threat fractionalLine = ZDEvadeTest::MakeThreat(fractionalLineSpell);
    fractionalLine.startPos = Vec2(0.0f, 0.0f);
    fractionalLine.endPos = Vec2(1000.0f, 0.0f);
    fractionalLine.direction = Vec2(1.0f, 0.0f);
    fractionalLine.startTick = blinkNow;
    fractionalLine.endTick = 3000;
    const CandidateEvaluation fractionalTangent =
        EvadeGeometry::EvaluateBlinkCandidate(
            Vec2(525.0f, 20.0f),
            Vec2(200.0f, 400.0f),
            Vec2(525.0f, 20.0f),
            0.0f,
            0.0f,
            blinkNow,
            10.5f,
            blinkSettings,
            {fractionalLine});
    ExpectTrue("fractional high-speed tangent is conservatively unsafe",
               !fractionalTangent.strictSafe);
    ExpectNear("fractional high-speed tangent keeps exact radius",
               fractionalTangent.minimumClearance,
               0.0f,
               0.02f);

    SpellData graceCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    graceCircleSpell.spellDelay = 0;
    graceCircleSpell.projectileSpeed = 0.0f;
    graceCircleSpell.radius = 80.0f;
    graceCircleSpell.extraEndTime = 200;
    Threat graceCircle = ZDEvadeTest::MakeThreat(graceCircleSpell);
    graceCircle.startTick = 1000;
    graceCircle.endTick = 3000;
    graceCircle.endPos = Vec2(500.0f, 500.0f);
    ExpectTrue("past impact remains occupied during grace",
               EvadeGeometry::OccupiesAt(
                   graceCircle, graceCircle.endPos, 0.0f, 0.0f, 1199));
    ExpectTrue("past impact releases after occupancy grace",
               !EvadeGeometry::HeroThreatenedNow(
                   {graceCircle},
                   graceCircle.endPos,
                   0.0f,
                   0.0f,
                   1201,
                   1800.0f));

    SpellData longExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    longExplosionSpell.spellDelay = 0;
    longExplosionSpell.projectileSpeed = 0.0f;
    longExplosionSpell.radius = 20.0f;
    longExplosionSpell.hasEndExplosion = true;
    longExplosionSpell.secondaryRadius = 80.0f;
    longExplosionSpell.endExplosionDelay = 100;
    longExplosionSpell.endExplosionDuration = 1000;
    Threat longExplosion = ZDEvadeTest::MakeThreat(longExplosionSpell);
    longExplosion.startTick = 1000;
    longExplosion.endTick = 3000;
    longExplosion.endPos = Vec2(900.0f, 900.0f);
    longExplosion.collisionExplosionCenter = Vec2(500.0f, 500.0f);
    ExpectTrue("long currently-active end explosion threatens now",
               EvadeGeometry::HeroThreatenedNow(
                   {longExplosion},
                   longExplosion.collisionExplosionCenter,
                   0.0f,
                   0.0f,
                   1500,
                   0.0f));

    Threat futureExplosion = longExplosion;
    futureExplosion.startTick = 2000;
    futureExplosion.endTick = 4000;
    ExpectTrue("future end explosion starts exactly at horizon",
               EvadeGeometry::ThreatensPointNowOrAtFutureImpact(
                   futureExplosion,
                   futureExplosion.collisionExplosionCenter,
                   0.0f,
                   0.0f,
                   2000,
                   100.0f));
    ExpectTrue("future end explosion beyond horizon is ignored",
               !EvadeGeometry::ThreatensPointNowOrAtFutureImpact(
                   futureExplosion,
                   futureExplosion.collisionExplosionCenter,
                   0.0f,
                   0.0f,
                   2000,
                   99.0f));
    ExpectTrue("past end explosion is not resurrected",
               !EvadeGeometry::ThreatensPointNowOrAtFutureImpact(
                   longExplosion,
                   longExplosion.collisionExplosionCenter,
                   0.0f,
                   0.0f,
                   2201,
                   1000.0f));

    SpellData arrivalSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    arrivalSpell.spellDelay = 200;
    arrivalSpell.projectileSpeed = 1000.0f;
    arrivalSpell.hasEndExplosion = true;
    arrivalSpell.secondaryRadius = 100.0f;
    arrivalSpell.endExplosionDelay = 75;
    Threat arrivalThreat = ZDEvadeTest::MakeThreat(arrivalSpell);
    arrivalThreat.startPos = Vec2(100.0f, 500.0f);
    arrivalThreat.endPos = Vec2(1100.0f, 500.0f);
    arrivalThreat.authoredEndPos = arrivalThreat.endPos;
    arrivalThreat.direction = Vec2(1.0f, 0.0f);
    arrivalThreat.startTick = 1800;
    arrivalThreat.launchTick = 2000;
    arrivalThreat.missileBound = true;
    arrivalThreat.observedHead = Vec2(500.0f, 600.0f);
    arrivalThreat.observedTick = 2500;
    arrivalThreat.observedSpeed = 1000.0f;
    arrivalThreat.endTick = 4000;
    const int observedArrival =
        EvadeGeometry::ImpactTickAt(arrivalThreat, arrivalThreat.endPos);
    ExpectTrue("observed arrival uses projected remaining route distance",
               observedArrival == 3100);
    ExpectTrue("observed explosion adds delay to observed arrival",
               arrivalThreat.EndExplosionStartTick() == 3175);
    ExpectTrue("observed travel-end and endpoint impact timing agree",
               observedArrival + arrivalThreat.EndExplosionDelay() ==
                   arrivalThreat.EndExplosionStartTick());

    SpellData steeringArrivalSpell = arrivalSpell;
    steeringArrivalSpell.missileRouteMode = MissileRouteMode::Steering;
    Threat steeringArrivalThreat =
        ZDEvadeTest::MakeThreat(steeringArrivalSpell);
    steeringArrivalThreat.startPos = arrivalThreat.startPos;
    steeringArrivalThreat.endPos = arrivalThreat.endPos;
    steeringArrivalThreat.authoredEndPos = arrivalThreat.authoredEndPos;
    steeringArrivalThreat.direction = arrivalThreat.direction;
    steeringArrivalThreat.startTick = arrivalThreat.startTick;
    steeringArrivalThreat.launchTick = arrivalThreat.launchTick;
    steeringArrivalThreat.missileBound = true;
    steeringArrivalThreat.observedHead = arrivalThreat.observedHead;
    steeringArrivalThreat.observedTick = arrivalThreat.observedTick;
    steeringArrivalThreat.observedSpeed = arrivalThreat.observedSpeed;
    ExpectNear("steering arrival future head uses exact anchor x",
               steeringArrivalThreat.HeadAtTick(2600).x,
               600.0f);
    ExpectNear("steering arrival future head uses exact anchor y",
               steeringArrivalThreat.HeadAtTick(2600).y,
               600.0f);
    ExpectTrue("steering endpoint arrival uses observed anchor",
               steeringArrivalThreat.ArrivalTick() == 3100);
    ExpectTrue("steering explosion uses selected route arrival",
               steeringArrivalThreat.EndExplosionStartTick() == 3175);

    Threat beyondArrivalThreat = steeringArrivalThreat;
    beyondArrivalThreat.observedHead = Vec2(1200.0f, 500.0f);
    ExpectNear("beyond-route arrival head clamps endpoint x",
               beyondArrivalThreat.HeadAtTick(2500).x,
               1100.0f);
    ExpectNear("beyond-route arrival head clamps endpoint y",
               beyondArrivalThreat.HeadAtTick(2500).y,
               500.0f);
    ExpectTrue("beyond-route arrival clamps to observation tick",
               beyondArrivalThreat.ArrivalTick() == 2500);
    ExpectTrue("beyond-route explosion follows clamped arrival",
               beyondArrivalThreat.EndExplosionStartTick() == 2575);

    Threat terminatedArrivalThreat = arrivalThreat;
    terminatedArrivalThreat.projectileTerminated = true;
    terminatedArrivalThreat.projectileTerminationTick = 2600;
    ExpectTrue("terminated missile explosion uses termination tick plus delay",
               terminatedArrivalThreat.EndExplosionStartTick() == 2675);

    SpellData terminatedCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    terminatedCircleSpell.spellDelay = 0;
    terminatedCircleSpell.projectileSpeed = 1300.0f;
    terminatedCircleSpell.extraEndTime = 400;
    Threat terminatedCircle =
        ZDEvadeTest::MakeThreat(terminatedCircleSpell);
    terminatedCircle.endPos = Vec2(450.0f, 500.0f);
    terminatedCircle.projectileTerminated = true;
    terminatedCircle.projectileTerminationTick = 2600;
    terminatedCircle.endTick = 3000;
    ExpectTrue("terminated Circular retains configured impact persistence",
               EvadeGeometry::ContainsAt(
                   terminatedCircle,
                   terminatedCircle.endPos,
                   0.0f,
                   0.0f,
                   2800));
    ExpectTrue("terminated Circular persistence ends normally",
               !EvadeGeometry::ContainsAt(
                   terminatedCircle,
                   terminatedCircle.endPos,
                   0.0f,
                   0.0f,
                   3001));

    const CandidateBudget minimumBudget = CandidateBudget::ForMax(32);
    ExpectTrue("candidate quota sum does not exceed cap",
               minimumBudget.Total() <= 32);
    ExpectTrue("candidate policy reserves cursor capacity at minimum cap",
               minimumBudget.cursor >= 1);
    ExpectTrue("candidate policy reserves radial capacity at minimum cap",
               minimumBudget.radialFallback >= 8);
    ExpectTrue("minimum-cap candidate quotas are explicit",
               minimumBudget.analytical == 8 &&
                   minimumBudget.cursor == 1 &&
                   minimumBudget.singleThreatDetour == 4 &&
                   minimumBudget.exactIntersections == 11 &&
                   minimumBudget.radialFallback == 8);
    const CandidateBudget fullBudget = CandidateBudget::ForMax(320);
    ExpectTrue("full-cap candidate quotas sum exactly to cap",
               fullBudget.Total() == 320);
    ExpectTrue("full-cap candidate quotas are explicit",
               fullBudget.analytical == 80 &&
                   fullBudget.cursor == 1 &&
                   fullBudget.singleThreatDetour == 40 &&
                   fullBudget.exactIntersections == 119 &&
                   fullBudget.radialFallback == 80);
    for (int capacity = 0; capacity <= 64; ++capacity) {
        const CandidateBudget budget = CandidateBudget::ForMax(capacity);
        ExpectTrue("budget sweep remains nonnegative",
                   budget.analytical >= 0 &&
                       budget.cursor >= 0 &&
                       budget.singleThreatDetour >= 0 &&
                       budget.exactIntersections >= 0 &&
                       budget.radialFallback >= 0);
        ExpectTrue("budget sweep never exceeds requested cap",
                   budget.Total() <= capacity);
        if (capacity < 5) {
            ExpectTrue("sub-five budget does not reserve impossible classes",
                       PositiveBudgetClassCount(budget) <= capacity);
        }
        if (capacity >= 32) {
            ExpectTrue("supported budget reserves every source class",
                       PositiveBudgetClassCount(budget) == 5);
            ExpectTrue("supported budget preserves cursor/radial minima",
                       budget.cursor >= 1 &&
                           budget.radialFallback >= 8);
        }
    }
    const CandidateBudget maximumIntegerBudget =
        CandidateBudget::ForMax(INT_MAX);
    ExpectTrue("INT_MAX budget sums without overflow",
               maximumIntegerBudget.Total() == INT_MAX);

    EvadeSettings seedSettings;
    seedSettings.maxCandidates = 32;
    seedSettings.maxSearchRadius = 760.0f;
    seedSettings.ringStep = 35.0f;
    seedSettings.endpointBuffer = 0.0f;
    seedSettings.pathBuffer = 0.0f;

    SpellData releaseCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    releaseCircleSpell.spellDelay = 0;
    releaseCircleSpell.radius = 100.0f;
    Threat releaseCircle = ZDEvadeTest::MakeThreat(releaseCircleSpell);
    releaseCircle.id = 990;
    releaseCircle.endPos = Vec2(500.0f, 500.0f);
    releaseCircle.startTick = 900;
    releaseCircle.endTick = 3000;
    EvadeSettings lowReleaseSettings = seedSettings;
    lowReleaseSettings.endpointBuffer = kDefaultEndpointMargin;
    lowReleaseSettings.releaseBuffer = 4.0f;
    EvadeSettings highReleaseSettings = lowReleaseSettings;
    highReleaseSettings.releaseBuffer = 96.0f;
    const std::vector<CandidateSeed> lowReleaseSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {releaseCircle},
            releaseCircle.endPos,
            Vec2(900.0f, 500.0f),
            0.0f,
            lowReleaseSettings,
            1000);
    const std::vector<CandidateSeed> highReleaseSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {releaseCircle},
            releaseCircle.endPos,
            Vec2(900.0f, 500.0f),
            0.0f,
            highReleaseSettings,
            1000);
    const Vec2 expectedNearEdgeTarget =
        releaseCircle.endPos +
        (releaseCircle.endPos - releaseCircle.startPos).Normalized() *
            ExitCenterDistance(
                releaseCircle.Radius(),
                0.0f,
                kDefaultEndpointMargin,
                0.0f);
    ExpectTrue("release margin does not move analytical endpoint",
               SameSeeds(lowReleaseSeeds, highReleaseSeeds));
    ExpectTrue("analytical endpoint preserves 10.25 edge margin",
               HasSeedNear(lowReleaseSeeds, expectedNearEdgeTarget));
    bool hasCircleClockwise = false;
    bool hasCircleCounterClockwise = false;
    const std::vector<CandidateSeed> circleBranchSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {releaseCircle},
            Vec2(200.0f, 500.0f),
            Vec2(800.0f, 500.0f),
            0.0f,
            seedSettings,
            1000);
    for (const CandidateSeed& seed : circleBranchSeeds) {
        if (seed.source != PlannerCandidateSource::CircleExit ||
            seed.threatId != releaseCircle.id) {
            continue;
        }
        hasCircleClockwise =
            hasCircleClockwise ||
            seed.stabilityBranchKey ==
                StabilityBranch::CircleClockwise;
        hasCircleCounterClockwise =
            hasCircleCounterClockwise ||
            seed.stabilityBranchKey ==
                StabilityBranch::CircleCounterClockwise;
    }
    ExpectTrue("circle detour tangents receive opposite semantic keys",
               hasCircleClockwise &&
                   hasCircleCounterClockwise);

    SpellData branchConeSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    branchConeSpell.spellDelay = 0;
    branchConeSpell.range = 600.0f;
    branchConeSpell.coneAngleDegrees = 30.0f;
    Threat branchCone = ZDEvadeTest::MakeThreat(branchConeSpell);
    branchCone.id = 991;
    branchCone.startPos = Vec2(100.0f, 500.0f);
    branchCone.endPos = Vec2(700.0f, 500.0f);
    branchCone.direction = Vec2(1.0f, 0.0f);
    branchCone.startTick = 900;
    branchCone.endTick = 3000;
    const std::vector<CandidateSeed> coneBranchSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {branchCone},
            Vec2(300.0f, 500.0f),
            Vec2(800.0f, 500.0f),
            0.0f,
            seedSettings,
            1000);
    bool hasConeLeft = false;
    bool hasConeRight = false;
    for (const CandidateSeed& seed : coneBranchSeeds) {
        if (seed.source != PlannerCandidateSource::ConeSide ||
            seed.threatId != branchCone.id) {
            continue;
        }
        hasConeLeft = hasConeLeft ||
            seed.stabilityBranchKey ==
                StabilityBranch::ConeLeft;
        hasConeRight = hasConeRight ||
            seed.stabilityBranchKey ==
                StabilityBranch::ConeRight;
    }
    ExpectTrue("narrow cone exits receive intrinsic side keys",
               hasConeLeft && hasConeRight);

    SpellData branchLineSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    branchLineSpell.spellDelay = 0;
    branchLineSpell.projectileSpeed = 0.0f;
    branchLineSpell.radius = 40.0f;
    Threat branchLine = ZDEvadeTest::MakeThreat(branchLineSpell);
    branchLine.id = 992;
    branchLine.startPos = Vec2(100.0f, 500.0f);
    branchLine.endPos = Vec2(900.0f, 500.0f);
    branchLine.direction = Vec2(1.0f, 0.0f);
    branchLine.startTick = 900;
    branchLine.endTick = 3000;
    const std::vector<CandidateSeed> lineBranchSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {branchLine},
            Vec2(500.0f, 500.0f),
            Vec2(800.0f, 500.0f),
            0.0f,
            seedSettings,
            1000);
    bool hasLineLeft = false;
    bool hasLineRight = false;
    for (const CandidateSeed& seed : lineBranchSeeds) {
        hasLineLeft = hasLineLeft ||
            (seed.source == PlannerCandidateSource::LineLeft &&
             seed.stabilityBranchKey ==
                 StabilityBranch::LineAnalyticalLeft);
        hasLineRight = hasLineRight ||
            (seed.source == PlannerCandidateSource::LineRight &&
             seed.stabilityBranchKey ==
                 StabilityBranch::LineAnalyticalRight);
    }
    ExpectTrue("analytical line sides have fixed distinct keys",
               hasLineLeft && hasLineRight);
    ExpectNear("analytical model-edge gap remains 10.25",
               expectedNearEdgeTarget.Distance(releaseCircle.endPos) -
                   releaseCircle.Radius(),
               10.25f);
    ExpectTrue("static endpoint clears lower release margin",
               !EvadeGeometry::HeroThreatenedNow(
                   {releaseCircle},
                   expectedNearEdgeTarget,
                   0.0f,
                   4.0f,
                   1000,
                   1800.0f));
    ExpectTrue("static endpoint remains in higher release margin",
               EvadeGeometry::HeroThreatenedNow(
                   {releaseCircle},
                   expectedNearEdgeTarget,
                   0.0f,
                   96.0f,
                   1000,
                   1800.0f));

    std::vector<Threat> denseThreats;
    denseThreats.reserve(16);
    for (int index = 0; index < 16; ++index) {
        SpellData denseLineSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
        denseLineSpell.spellDelay = 0;
        denseLineSpell.projectileSpeed = 0.0f;
        denseLineSpell.radius = 5.0f;
        denseLineSpell.range = 600.0f;
        Threat denseLine = ZDEvadeTest::MakeThreat(denseLineSpell);
        denseLine.id = 1000 + index;
        if (index < 8) {
            const float y = 620.0f + static_cast<float>(index) * 20.0f;
            denseLine.startPos = Vec2(300.0f, y);
            denseLine.endPos = Vec2(900.0f, y);
            denseLine.direction = Vec2(1.0f, 0.0f);
        } else {
            const float x =
                620.0f + static_cast<float>(index - 8) * 20.0f;
            denseLine.startPos = Vec2(x, 300.0f);
            denseLine.endPos = Vec2(x, 900.0f);
            denseLine.direction = Vec2(0.0f, 1.0f);
        }
        denseLine.startTick = 900;
        denseLine.endTick = 3000;
        denseThreats.push_back(denseLine);
    }
    const std::vector<CandidateSeed> denseSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            denseThreats,
            Vec2(500.0f, 500.0f),
            Vec2(200.0f, 200.0f),
            0.0f,
            seedSettings,
            1000);
    ExpectTrue("dense fixture respects max candidate cap",
               denseSeeds.size() <= 32);
    ExpectTrue("dense 16-threat fixture retains cursor class",
               CountSeedSource(denseSeeds, PlannerCandidateSource::Cursor) >= 1);
    ExpectTrue("dense 16-threat fixture retains radial class",
               CountSeedSource(denseSeeds, PlannerCandidateSource::Ring) >= 8);
    ExpectTrue("dense 16-threat fixture exercises reserved intersections",
               CountSeedSource(
                   denseSeeds,
                   PlannerCandidateSource::Intersection) >=
                   minimumBudget.exactIntersections);
    for (int capacity = 0; capacity <= 64; ++capacity) {
        EvadeSettings cappedSettings = seedSettings;
        cappedSettings.maxCandidates = capacity;
        const std::vector<CandidateSeed> cappedSeeds =
            EvadePlanner::GenerateCandidateSeeds(
                denseThreats,
                Vec2(500.0f, 500.0f),
                Vec2(200.0f, 200.0f),
                0.0f,
                cappedSettings,
                1000);
        ExpectTrue("public generator honors every small hard cap",
                   cappedSeeds.size() ==
                       static_cast<std::size_t>(capacity));
    }

    EvadeSettings duplicateSettings = seedSettings;
    duplicateSettings.ringStep = 30.0f;
    const std::vector<CandidateSeed> duplicateSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            denseThreats,
            Vec2(500.0f, 500.0f),
            Vec2(900.0f, 500.0f),
            0.0f,
            duplicateSettings,
            1000);
    int duplicateCursorPositionCount = 0;
    PlannerCandidateSource duplicateCursorPositionSource =
        PlannerCandidateSource::Unknown;
    for (const CandidateSeed& seed : duplicateSeeds) {
        if (seed.position.Distance(Vec2(800.0f, 500.0f)) > 0.2f) continue;
        ++duplicateCursorPositionCount;
        duplicateCursorPositionSource = seed.source;
    }
    ExpectTrue("cross-class dedupe keeps one coincident cursor seed",
               duplicateCursorPositionCount == 1);
    ExpectTrue("cross-class dedupe preserves cursor source ownership",
               duplicateCursorPositionSource == PlannerCandidateSource::Cursor);

    SpellData bridgeCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    bridgeCircleSpell.spellDelay = 0;
    bridgeCircleSpell.radius = 450.0f;
    Threat bridgeUpper = ZDEvadeTest::MakeThreat(bridgeCircleSpell);
    bridgeUpper.id = 1501;
    bridgeUpper.endPos = Vec2(350.0f, 494.0f);
    bridgeUpper.startTick = 900;
    bridgeUpper.endTick = 3000;
    Threat bridgeLower = ZDEvadeTest::MakeThreat(bridgeCircleSpell);
    bridgeLower.id = 1502;
    bridgeLower.endPos = Vec2(350.0f, 506.0f);
    bridgeLower.startTick = 900;
    bridgeLower.endTick = 3000;
    const Vec2 bridgeCursorSeed(800.0f, 500.0f);
    const std::vector<CandidateSeed> bridgeSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {bridgeUpper, bridgeLower},
            Vec2(500.0f, 500.0f),
            Vec2(900.0f, 500.0f),
            0.0f,
            duplicateSettings,
            1000);
    int bridgeNearCursorCount = 0;
    int bridgeCursorOwnedCount = 0;
    for (const CandidateSeed& seed : bridgeSeeds) {
        if (seed.position.Distance(bridgeCursorSeed) > 14.0f) continue;
        ++bridgeNearCursorCount;
        if (seed.source == PlannerCandidateSource::Cursor)
            ++bridgeCursorOwnedCount;
    }
    ExpectTrue("cursor bridge removes every near duplicate",
               bridgeNearCursorCount == 1);
    ExpectTrue("cursor bridge leaves one cursor-owned seed",
               bridgeCursorOwnedCount == 1);

    EvadeSettings intersectionSettings = seedSettings;
    intersectionSettings.maxCandidates = 128;
    intersectionSettings.maxSearchRadius = 2000.0f;
    EvadeSettings zeroIntersectionSettings = intersectionSettings;
    zeroIntersectionSettings.maxCandidates = 0;
    ExpectTrue("public intersection generator honors zero cap",
               EvadePlanner::GenerateExactIntersectionSeeds(
                   denseThreats,
                   Vec2(500.0f, 500.0f),
                   0.0f,
                   zeroIntersectionSettings,
                   1000).empty());

    SpellData exactLineSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    exactLineSpell.spellDelay = 0;
    exactLineSpell.projectileSpeed = 0.0f;
    exactLineSpell.radius = 20.0f;
    exactLineSpell.range = 1000.0f;
    Threat exactLine = ZDEvadeTest::MakeThreat(exactLineSpell);
    exactLine.startPos = Vec2(0.0f, 0.0f);
    exactLine.endPos = Vec2(1000.0f, 0.0f);
    exactLine.direction = Vec2(1.0f, 0.0f);
    exactLine.startTick = 900;
    exactLine.endTick = 3000;

    SpellData exactCircleSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    exactCircleSpell.spellDelay = 0;
    exactCircleSpell.radius = 100.0f;
    Threat exactCircle = ZDEvadeTest::MakeThreat(exactCircleSpell);
    exactCircle.endPos = Vec2(500.0f, 0.0f);
    exactCircle.startTick = 900;
    exactCircle.endTick = 3000;
    const std::vector<CandidateSeed> lineCircleSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {exactLine, exactCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float exactLineRadius = ExitCenterDistance(20.0f, 0.0f, 0.0f, 0.0f);
    const float exactCircleRadius = ExitCenterDistance(100.0f, 0.0f, 0.0f, 0.0f);
    const float lineCircleOffset = std::sqrt(
        exactCircleRadius * exactCircleRadius -
        exactLineRadius * exactLineRadius);
    ExpectTrue("line/circle exact upper-left side intersection",
               HasSeedNear(
                   lineCircleSeeds,
                   Vec2(500.0f - lineCircleOffset, exactLineRadius)));
    ExpectTrue("line/circle exact lower-right side intersection",
               HasSeedNear(
                   lineCircleSeeds,
                   Vec2(500.0f + lineCircleOffset, -exactLineRadius)));

    Threat secondCircle = exactCircle;
    secondCircle.id += 100;
    secondCircle.endPos = Vec2(620.0f, 0.0f);
    const std::vector<CandidateSeed> circleCircleSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {exactCircle, secondCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float circleCircleHeight = std::sqrt(
        exactCircleRadius * exactCircleRadius - 60.0f * 60.0f);
    ExpectTrue("circle/circle exact first intersection",
               HasSeedNear(
                   circleCircleSeeds,
                   Vec2(560.0f, circleCircleHeight)));
    ExpectTrue("circle/circle exact second intersection",
               HasSeedNear(
                   circleCircleSeeds,
                   Vec2(560.0f, -circleCircleHeight)));

    SpellData ringBoundarySpell = ZDEvadeTest::MakeSpell(ZDSpellType::Ring);
    ringBoundarySpell.spellDelay = 0;
    ringBoundarySpell.innerRadius = 60.0f;
    ringBoundarySpell.radius = 120.0f;
    Threat ringBoundary = ZDEvadeTest::MakeThreat(ringBoundarySpell);
    ringBoundary.endPos = Vec2(500.0f, 0.0f);
    ringBoundary.startTick = 900;
    ringBoundary.endTick = 3000;
    SpellData centerLineSpell = exactLineSpell;
    centerLineSpell.radius = 0.0f;
    Threat centerLine = ZDEvadeTest::MakeThreat(centerLineSpell);
    centerLine.startPos = Vec2(0.0f, 0.0f);
    centerLine.endPos = Vec2(1000.0f, 0.0f);
    centerLine.direction = Vec2(1.0f, 0.0f);
    centerLine.startTick = 900;
    centerLine.endTick = 3000;
    const std::vector<CandidateSeed> lineRingSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {centerLine, ringBoundary},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float centerLineRadius = ExitCenterDistance(0.0f, 0.0f, 0.0f, 0.0f);
    const float ringSafeInnerRadius =
        60.0f - ExitCenterDistance(0.0f, 0.0f, 0.0f, 0.0f);
    const float ringInnerLineOffset = std::sqrt(
        ringSafeInnerRadius * ringSafeInnerRadius -
        centerLineRadius * centerLineRadius);
    const float ringOuterRadius = ExitCenterDistance(120.0f, 0.0f, 0.0f, 0.0f);
    const float ringOuterLineOffset = std::sqrt(
        ringOuterRadius * ringOuterRadius -
        centerLineRadius * centerLineRadius);
    ExpectTrue("line/ring includes exact inner boundary",
               HasSeedNear(
                   lineRingSeeds,
                   Vec2(500.0f - ringInnerLineOffset, centerLineRadius)));
    ExpectTrue("line/ring includes exact outer boundary",
               HasSeedNear(
                   lineRingSeeds,
                   Vec2(500.0f - ringOuterLineOffset, centerLineRadius)));
    const Vec2 expectedInnerRingIntersection(
        500.0f - ringInnerLineOffset,
        centerLineRadius);
    ExpectTrue("ring inner intersection is strictly inside safe hole",
               !EvadeGeometry::ContainsAt(
                   ringBoundary,
                   expectedInnerRingIntersection,
                   0.0f,
                   0.0f,
                   1000));
    const Vec2 closestSafeRingExit = EvadeGeometry::ClosestRingExit(
        ringBoundary,
        Vec2(580.0f, 0.0f),
        0.0f,
        0.0f);
    ExpectTrue("analytical ring inner exit is strictly safe",
               !EvadeGeometry::ContainsAt(
                   ringBoundary,
                   closestSafeRingExit,
                   0.0f,
                   0.0f,
                   1000));

    Threat ringCircle = exactCircle;
    ringCircle.id += 200;
    ringCircle.endPos = Vec2(580.0f, 0.0f);
    ringCircle.data = exactCircle.data;
    const std::vector<CandidateSeed> circleRingSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {ringBoundary, ringCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const auto circleIntersectionFromRingCenter = [](
        float ringRadius,
        float circleRadius,
        float centerDistance) {
        const float x =
            (ringRadius * ringRadius - circleRadius * circleRadius +
             centerDistance * centerDistance) /
            (2.0f * centerDistance);
        return Vec2(x, std::sqrt(std::max(
            0.0f,
            ringRadius * ringRadius - x * x)));
    };
    const Vec2 innerRingCircle = circleIntersectionFromRingCenter(
        ringSafeInnerRadius, exactCircleRadius, 80.0f);
    const Vec2 outerRingCircle = circleIntersectionFromRingCenter(
        ringOuterRadius, exactCircleRadius, 80.0f);
    ExpectTrue("circle/ring intersects inner boundary exactly",
               HasSeedNear(
                   circleRingSeeds,
                   Vec2(500.0f + innerRingCircle.x, innerRingCircle.y)));
    ExpectTrue("circle/ring intersects outer boundary exactly",
               HasSeedNear(
                   circleRingSeeds,
                   Vec2(500.0f + outerRingCircle.x, outerRingCircle.y)));

    SpellData coneBoundarySpell = ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    coneBoundarySpell.spellDelay = 0;
    coneBoundarySpell.range = 200.0f;
    coneBoundarySpell.coneAngleDegrees = 60.0f;
    coneBoundarySpell.coneEdgePadding = 0.0f;
    Threat coneBoundary = ZDEvadeTest::MakeThreat(coneBoundarySpell);
    coneBoundary.startPos = Vec2(0.0f, 0.0f);
    coneBoundary.endPos = Vec2(200.0f, 0.0f);
    coneBoundary.direction = Vec2(1.0f, 0.0f);
    coneBoundary.startTick = 900;
    coneBoundary.endTick = 3000;
    Threat concentricCircle = exactCircle;
    concentricCircle.id += 300;
    concentricCircle.endPos = Vec2(0.0f, 0.0f);
    const std::vector<CandidateSeed> coneSideSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {coneBoundary, concentricCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float coneBoundaryExpansion =
        ExitCenterDistance(0.0f, 0.0f, 0.0f, 0.0f);
    const float coneSideAlong = std::sqrt(
        exactCircleRadius * exactCircleRadius -
        coneBoundaryExpansion * coneBoundaryExpansion);
    const Vec2 upperConeDirection = PolarPoint(1.0f, 30.0f);
    const Vec2 lowerConeDirection = PolarPoint(1.0f, -30.0f);
    const Vec2 expectedUpperConeSide =
        upperConeDirection * coneSideAlong +
        Vec2(-upperConeDirection.y, upperConeDirection.x) *
            coneBoundaryExpansion;
    const Vec2 expectedLowerConeSide =
        lowerConeDirection * coneSideAlong +
        Vec2(lowerConeDirection.y, -lowerConeDirection.x) *
            coneBoundaryExpansion;
    ExpectTrue("cone exact upper side ray intersection",
               HasSeedNear(coneSideSeeds, expectedUpperConeSide));
    ExpectTrue("cone exact lower side ray intersection",
               HasSeedNear(coneSideSeeds, expectedLowerConeSide));

    Threat outerArcCircle = exactCircle;
    outerArcCircle.id += 400;
    outerArcCircle.endPos = Vec2(200.0f, 0.0f);
    const std::vector<CandidateSeed> coneArcSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {coneBoundary, outerArcCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float coneOuterRadius = ExitCenterDistance(
        200.0f, 0.0f, 0.0f, 0.0f);
    const Vec2 upperConeArc = circleIntersectionFromRingCenter(
        coneOuterRadius, exactCircleRadius, 200.0f);
    ExpectTrue("cone exact upper outer-arc intersection",
               HasSeedNear(coneArcSeeds, upperConeArc));
    ExpectTrue("cone exact lower outer-arc intersection",
               HasSeedNear(
                   coneArcSeeds,
                   Vec2(upperConeArc.x, -upperConeArc.y)));

    SpellData expandedConeSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    expandedConeSpell.spellDelay = 0;
    expandedConeSpell.range = 240.0f;
    expandedConeSpell.coneAngleDegrees = 60.0f;
    expandedConeSpell.coneEdgePadding = 3.0f;
    Threat expandedCone = ZDEvadeTest::MakeThreat(expandedConeSpell);
    expandedCone.id = 1801;
    expandedCone.startPos = Vec2(0.0f, 0.0f);
    expandedCone.endPos = Vec2(240.0f, 0.0f);
    expandedCone.direction = Vec2(1.0f, 0.0f);
    expandedCone.positionUncertainty = 5.0f;
    expandedCone.startTick = 900;
    expandedCone.endTick = 3000;

    SpellData coneCrossLineSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    coneCrossLineSpell.spellDelay = 0;
    coneCrossLineSpell.projectileSpeed = 0.0f;
    coneCrossLineSpell.radius = 0.0f;
    Threat coneCrossLine = ZDEvadeTest::MakeThreat(coneCrossLineSpell);
    coneCrossLine.id = 1802;
    coneCrossLine.startPos = Vec2(120.0f, -300.0f);
    coneCrossLine.endPos = Vec2(120.0f, 300.0f);
    coneCrossLine.direction = Vec2(0.0f, 1.0f);
    coneCrossLine.startTick = 900;
    coneCrossLine.endTick = 3000;

    EvadeSettings expandedConeSettings = intersectionSettings;
    expandedConeSettings.endpointBuffer = 7.0f;
    constexpr float expandedHeroRadius = 20.0f;
    const std::vector<CandidateSeed> expandedConeSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {expandedCone, coneCrossLine},
            Vec2(500.0f, 500.0f),
            expandedHeroRadius,
            expandedConeSettings,
            1000);
    ExpectTrue("expanded cone emits exact boundary intersections",
               expandedConeSeeds.size() >= 2);
    for (const CandidateSeed& seed : expandedConeSeeds) {
        ExpectTrue("expanded cone intersection lies outside actual danger",
                   !EvadeGeometry::ContainsAt(
                       expandedCone,
                       seed.position,
                       expandedHeroRadius,
                       expandedConeSettings.endpointBuffer,
                       1000));
        const CandidateEvaluation endpointCheck =
            EvadeGeometry::EvaluateCandidate(
                seed.position,
                PlannerCandidateSource::Intersection,
                -1,
                Vec2(500.0f, 500.0f),
                Vec2(500.0f, 500.0f),
                0.0f,
                1000.0f,
                expandedHeroRadius,
                1000,
                expandedConeSettings,
                {expandedCone});
        ExpectTrue("expanded cone seed evaluates endpoint-safe",
                   endpointCheck.endpointSafe);
    }

    SpellData plannerExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    plannerExplosionSpell.spellDelay = 0;
    plannerExplosionSpell.projectileSpeed = 0.0f;
    plannerExplosionSpell.radius = 10.0f;
    plannerExplosionSpell.hasEndExplosion = true;
    plannerExplosionSpell.secondaryRadius = 100.0f;
    plannerExplosionSpell.endExplosionDelay = 100;
    plannerExplosionSpell.endExplosionDuration = 100;
    Threat plannerExplosion =
        ZDEvadeTest::MakeThreat(plannerExplosionSpell);
    plannerExplosion.id = 1869;
    plannerExplosion.startPos = Vec2(0.0f, 2000.0f);
    plannerExplosion.endPos = Vec2(1000.0f, 2000.0f);
    plannerExplosion.direction = Vec2(1.0f, 0.0f);
    plannerExplosion.startTick = 1000;
    plannerExplosion.endTick = 3000;
    plannerExplosion.collisionExplosionCenter = Vec2(500.0f, 500.0f);
    const std::vector<CandidateSeed> plannerExplosionSeeds =
        EvadePlanner::GenerateCandidateSeeds(
            {plannerExplosion},
            Vec2(500.0f, 500.0f),
            Vec2(800.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const float plannerExplosionExitRadius = ExitCenterDistance(
        plannerExplosion.EndExplosionRadius(), 0.0f, 0.0f, 0.0f);
    bool hasShiftedExplosionExit = false;
    for (const CandidateSeed& seed : plannerExplosionSeeds) {
        if (seed.threatId == plannerExplosion.id &&
            seed.source == PlannerCandidateSource::CircleExit &&
            std::fabs(
                seed.position.Distance(
                    plannerExplosion.EndExplosionCenter()) -
                plannerExplosionExitRadius) < 0.1f) {
            hasShiftedExplosionExit = true;
        }
    }
    ExpectTrue("future collision-shifted explosion provides exact exit seed",
               hasShiftedExplosionExit);
    Threat plannerExplosionCompanion = exactCircle;
    plannerExplosionCompanion.id = 1870;
    plannerExplosionCompanion.endPos =
        plannerExplosion.EndExplosionCenter() + Vec2(150.0f, 0.0f);
    const std::vector<CandidateSeed> plannerExplosionIntersections =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {plannerExplosion, plannerExplosionCompanion},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    ExpectTrue("future end explosion contributes circle intersections",
               !plannerExplosionIntersections.empty());
    for (const CandidateSeed& seed : plannerExplosionIntersections) {
        ExpectNear(
            "explosion intersection lies on end-explosion circle",
            seed.position.Distance(plannerExplosion.EndExplosionCenter()),
            plannerExplosionExitRadius,
            0.1f);
        ExpectNear(
            "explosion intersection lies on companion circle",
            seed.position.Distance(plannerExplosionCompanion.endPos),
            exactCircleRadius,
            0.1f);
    }

    SpellData unsupportedArcSpell = ZDEvadeTest::MakeSpell(ZDSpellType::Arc);
    unsupportedArcSpell.spellDelay = 0;
    unsupportedArcSpell.radius = 100.0f;
    Threat unsupportedArc = ZDEvadeTest::MakeThreat(unsupportedArcSpell);
    unsupportedArc.startPos = Vec2(0.0f, 0.0f);
    unsupportedArc.endPos = Vec2(1000.0f, 0.0f);
    unsupportedArc.startTick = 900;
    unsupportedArc.endTick = 3000;
    const std::vector<CandidateSeed> unsupportedArcSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            {unsupportedArc, exactCircle},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    ExpectTrue("unsupported Arc yields no intersection seeds",
               unsupportedArcSeeds.empty());
    ExpectTrue("unsupported Arc is not feature-gated supported",
               !unsupportedArc.ArcSupported());
    ExpectTrue("unsupported Arc fails geometry closed at a remote point",
               EvadeGeometry::ContainsAt(
                   unsupportedArc,
                   Vec2(5000.0f, 5000.0f),
                   0.0f,
                   0.0f,
                   1000));
    ExpectNear("Arc head does not use straight chord travel",
               unsupportedArc.HeadAtTick(1500).x,
               unsupportedArc.startPos.x);
    ExpectEq("Arc arrival does not use straight chord timing",
             unsupportedArc.ArrivalTick(),
             unsupportedArc.startTick);
    ExpectTrue("unsupported Arc yields no planner candidates",
               EvadePlanner::GenerateCandidateSeeds(
                   {unsupportedArc},
                   Vec2(500.0f, 500.0f),
                   Vec2(800.0f, 500.0f),
                   0.0f,
                   intersectionSettings,
                   1000).empty());

    SpellData explicitlySupportedArc = unsupportedArcSpell;
    explicitlySupportedArc.arcSupported = true;
    explicitlySupportedArc.arcCenterX = 500.0f;
    explicitlySupportedArc.arcCenterY = 100.0f;
    explicitlySupportedArc.arcRadius = 600.0f;
    explicitlySupportedArc.arcStartAngleDegrees = 180.0f;
    explicitlySupportedArc.arcSweepAngleDegrees = 90.0f;
    Threat completeArc = ZDEvadeTest::MakeThreat(explicitlySupportedArc);
    ExpectTrue("complete Arc remains unsupported without exact geometry",
               !completeArc.ArcSupported());
    ExpectTrue("complete Arc remains planner fail-closed",
               EvadePlanner::GenerateCandidateSeeds(
                   {completeArc},
                   Vec2(500.0f, 500.0f),
                   Vec2(800.0f, 500.0f),
                   0.0f,
                   intersectionSettings,
                   1000).empty());
    explicitlySupportedArc.arcSweepAngleDegrees = 0.0f;
    Threat incompleteArc = ZDEvadeTest::MakeThreat(explicitlySupportedArc);
    ExpectTrue("Arc rejects incomplete sweep metadata",
               !incompleteArc.ArcSupported());

    const auto exactSeedsFor = [&](
        const Threat& first,
        const Threat& second) {
        return EvadePlanner::GenerateExactIntersectionSeeds(
            {first, second},
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    };
    Threat staticLineWithUncertainty = exactLine;
    staticLineWithUncertainty.positionUncertainty = 24.0f;
    ExpectTrue("static line boundary ignores stored uncertainty",
               SameSeeds(
                   exactSeedsFor(exactLine, exactCircle),
                   exactSeedsFor(
                       staticLineWithUncertainty,
                       exactCircle)));
    Threat staticCircleWithUncertainty = exactCircle;
    staticCircleWithUncertainty.positionUncertainty = 24.0f;
    ExpectTrue("static circle boundary ignores stored uncertainty",
               SameSeeds(
                   exactSeedsFor(exactCircle, exactLine),
                   exactSeedsFor(
                       staticCircleWithUncertainty,
                       exactLine)));
    SpellData staticRingSpell = ringBoundarySpell;
    staticRingSpell.projectileSpeed = 0.0f;
    Threat staticRing = ZDEvadeTest::MakeThreat(staticRingSpell);
    staticRing.id = ringBoundary.id;
    staticRing.endPos = ringBoundary.endPos;
    staticRing.startTick = ringBoundary.startTick;
    staticRing.endTick = ringBoundary.endTick;
    Threat staticRingWithUncertainty = staticRing;
    staticRingWithUncertainty.positionUncertainty = 24.0f;
    ExpectTrue("static ring boundaries ignore stored uncertainty",
               SameSeeds(
                   exactSeedsFor(staticRing, centerLine),
                   exactSeedsFor(
                       staticRingWithUncertainty,
                       centerLine)));
    SpellData staticConeSpell = coneBoundarySpell;
    staticConeSpell.projectileSpeed = 0.0f;
    Threat staticCone = ZDEvadeTest::MakeThreat(staticConeSpell);
    staticCone.id = coneBoundary.id;
    staticCone.startPos = coneBoundary.startPos;
    staticCone.endPos = coneBoundary.endPos;
    staticCone.direction = coneBoundary.direction;
    staticCone.startTick = coneBoundary.startTick;
    staticCone.endTick = coneBoundary.endTick;
    Threat staticConeWithUncertainty = staticCone;
    staticConeWithUncertainty.positionUncertainty = 24.0f;
    ExpectTrue("static cone boundaries ignore stored uncertainty",
               SameSeeds(
                   exactSeedsFor(staticCone, concentricCircle),
                   exactSeedsFor(
                       staticConeWithUncertainty,
                       concentricCircle)));
    const Vec2 staticConeProbe =
        PolarPoint(100.0f, 30.0f) +
        Vec2(-0.5f, 0.8660254f) * 10.0f;
    ExpectTrue("static cone danger ignores stored uncertainty",
               EvadeGeometry::ContainsAt(
                   staticCone,
                   staticConeProbe,
                   0.0f,
                   0.0f,
                   1000) ==
                   EvadeGeometry::ContainsAt(
                       staticConeWithUncertainty,
                       staticConeProbe,
                       0.0f,
                       0.0f,
                       1000));

    SpellData movingLineSpell = exactLineSpell;
    movingLineSpell.projectileSpeed = 1000.0f;
    Threat movingLine = ZDEvadeTest::MakeThreat(movingLineSpell);
    movingLine.id = 1901;
    movingLine.startPos = Vec2(0.0f, 0.0f);
    movingLine.endPos = Vec2(1000.0f, 0.0f);
    movingLine.direction = Vec2(1.0f, 0.0f);
    movingLine.startTick = 900;
    movingLine.endTick = 3000;
    Threat movingLineWithUncertainty = movingLine;
    movingLineWithUncertainty.positionUncertainty = 24.0f;
    ExpectTrue("moving line boundary includes stored uncertainty",
               !SameSeeds(
                   exactSeedsFor(movingLine, exactCircle),
                   exactSeedsFor(
                       movingLineWithUncertainty,
                       exactCircle)));

    std::vector<Threat> orderingThreats;
    orderingThreats.reserve(17);
    for (int index = 0; index < 17; ++index) {
        SpellData orderingSpell =
            ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
        orderingSpell.spellDelay = (index % 3) * 30;
        orderingSpell.radius = 95.0f;
        orderingSpell.dangerlevel =
            index % 3 == 2 ? 4 : 1;
        Threat orderingThreat = ZDEvadeTest::MakeThreat(orderingSpell);
        const int group = index / 3;
        const int member = index % 3;
        orderingThreat.id =
            2000 + group * 10 + (member == 0 ? 2 : member == 1 ? 1 : 3);
        orderingThreat.endPos = Vec2(
            260.0f + static_cast<float>(index) * 24.0f,
            420.0f + static_cast<float>(index % 2) * 70.0f);
        orderingThreat.startTick = 1000;
        orderingThreat.endTick = 3000;
        orderingThreats.push_back(orderingThreat);
    }
    std::vector<Threat> reversedOrderingThreats = orderingThreats;
    std::reverse(
        reversedOrderingThreats.begin(),
        reversedOrderingThreats.end());
    std::vector<Threat> rotatedOrderingThreats = orderingThreats;
    std::rotate(
        rotatedOrderingThreats.begin(),
        rotatedOrderingThreats.begin() + 7,
        rotatedOrderingThreats.end());
    const std::vector<CandidateSeed> orderedSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            orderingThreats,
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const std::vector<CandidateSeed> reversedOrderedSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            reversedOrderingThreats,
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    const std::vector<CandidateSeed> rotatedOrderedSeeds =
        EvadePlanner::GenerateExactIntersectionSeeds(
            rotatedOrderingThreats,
            Vec2(500.0f, 500.0f),
            0.0f,
            intersectionSettings,
            1000);
    ExpectTrue("threat ordering is deterministic under reversal",
               SameSeeds(orderedSeeds, reversedOrderedSeeds));
    ExpectTrue("threat ordering is deterministic under rotation",
               SameSeeds(orderedSeeds, rotatedOrderedSeeds));

    EvadeSettings dense320Settings = seedSettings;
    dense320Settings.maxCandidates = 320;
    const std::vector<CandidateSeed> dense320Seeds =
        EvadePlanner::GenerateCandidateSeeds(
            denseThreats,
            Vec2(500.0f, 500.0f),
            Vec2(200.0f, 200.0f),
            0.0f,
            dense320Settings,
            1000);
    ExpectTrue("dense 320 fixture reaches exact hard cap",
               dense320Seeds.size() == 320);
    constexpr int dense320Iterations = 40;
    std::size_t dense320SeedCount = 0;
    const auto dense320Start = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < dense320Iterations; ++iteration) {
        dense320SeedCount += EvadePlanner::GenerateCandidateSeeds(
            denseThreats,
            Vec2(500.0f, 500.0f),
            Vec2(200.0f, 200.0f),
            0.0f,
            dense320Settings,
            1000).size();
    }
    const double dense320AverageMicroseconds =
        std::chrono::duration<double, std::micro>(
            std::chrono::steady_clock::now() - dense320Start).count() /
        static_cast<double>(dense320Iterations);
    ExpectTrue("dense 320 benchmark preserves candidate count",
               dense320SeedCount ==
                   static_cast<std::size_t>(dense320Iterations * 320));
    std::printf(
        "INFO: dense 320 generation %.2f us/iteration\n",
        dense320AverageMicroseconds);

    constexpr int performanceIterations = 200;
    std::size_t performanceSeedCount = 0;
    const auto performanceStart = std::chrono::steady_clock::now();
    for (int iteration = 0; iteration < performanceIterations; ++iteration) {
        performanceSeedCount += EvadePlanner::GenerateCandidateSeeds(
            denseThreats,
            Vec2(500.0f, 500.0f),
            Vec2(200.0f, 200.0f),
            0.0f,
            seedSettings,
            1000).size();
    }
    const auto performanceEnd = std::chrono::steady_clock::now();
    const double averageMicroseconds =
        std::chrono::duration<double, std::micro>(
            performanceEnd - performanceStart).count() /
        static_cast<double>(performanceIterations);
    std::printf(
        "INFO: dense candidate generation %.2f us/iteration (%zu seeds)\n",
        averageMicroseconds,
        performanceSeedCount / performanceIterations);

    EvadeSettings hostileSettings;
    hostileSettings.endpointBuffer =
        std::numeric_limits<float>::quiet_NaN();
    hostileSettings.pathBuffer =
        std::numeric_limits<float>::infinity();
    hostileSettings.pathStep =
        std::numeric_limits<float>::quiet_NaN();
    hostileSettings.temporalStepMs =
        std::numeric_limits<float>::infinity();
    hostileSettings.ringStep =
        std::numeric_limits<float>::quiet_NaN();
    hostileSettings.maxSearchRadius =
        std::numeric_limits<float>::infinity();
    hostileSettings.inputDelayMs =
        std::numeric_limits<float>::quiet_NaN();
    hostileSettings.maxThreatHorizonMs =
        std::numeric_limits<float>::infinity();
    const EvadeSettings normalizedHostile =
        NormalizeEvadeSettings(hostileSettings);
    ExpectTrue("settings normalization makes geometry controls finite",
               std::isfinite(normalizedHostile.endpointBuffer) &&
                   std::isfinite(normalizedHostile.pathBuffer) &&
                   std::isfinite(normalizedHostile.pathStep) &&
                   std::isfinite(normalizedHostile.temporalStepMs) &&
                   std::isfinite(normalizedHostile.ringStep) &&
                   std::isfinite(normalizedHostile.maxSearchRadius) &&
                   std::isfinite(normalizedHostile.inputDelayMs) &&
                   std::isfinite(
                       normalizedHostile.maxThreatHorizonMs));
    ExpectTrue("analysis horizon is deterministically capped",
               normalizedHostile.maxThreatHorizonMs <=
                   kMaximumAnalysisHorizonMs);
    ExpectTrue("analysis sample count is deterministically capped",
               AnalysisSampleCount(
                   std::numeric_limits<float>::infinity(),
                   std::numeric_limits<float>::quiet_NaN()) <=
                   kMaximumAnalysisSamples);

    EvadeSettings holdSettings;
    holdSettings.endpointBuffer = 0.0f;
    holdSettings.pathBuffer = 0.0f;
    holdSettings.inputDelayMs = 0.0f;
    holdSettings.minimumTimeMarginMs = 0.0f;
    holdSettings.maxThreatHorizonMs = 1000.0f;

    SpellData occupiedCircleSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    occupiedCircleSpell.spellDelay = 0;
    occupiedCircleSpell.projectileSpeed = 0.0f;
    occupiedCircleSpell.radius = 80.0f;
    occupiedCircleSpell.extraEndTime = 1000;
    Threat occupiedCircle =
        ZDEvadeTest::MakeThreat(occupiedCircleSpell);
    occupiedCircle.id = 2301;
    occupiedCircle.startTick = 900;
    occupiedCircle.endTick = 3000;
    occupiedCircle.endPos = Vec2(500.0f, 500.0f);
    const CandidateEvaluation circleHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            occupiedCircle.endPos,
            occupiedCircle.endPos,
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {occupiedCircle});
    ExpectTrue("UNAV-01 stationary circle has real common-horizon coverage",
               circleHold.valid &&
                   circleHold.collisionCount == 1 &&
                   circleHold.endpointDanger == 3 &&
                   circleHold.pathDanger == 3 &&
                   circleHold.maxDanger == 3 &&
                   circleHold.firstCollisionTimeMs == 0.0f &&
                   circleHold.minimumClearance < 0.0f);
    ExpectNear("UNAV-01 stationary circle exact weighted exposure",
               circleHold.dangerExposureMs,
               2700.0f,
               0.1f);

    SpellData occupiedLineSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    occupiedLineSpell.spellDelay = 0;
    occupiedLineSpell.projectileSpeed = 0.0f;
    occupiedLineSpell.radius = 40.0f;
    occupiedLineSpell.extraEndTime = 1000;
    Threat occupiedLine = ZDEvadeTest::MakeThreat(occupiedLineSpell);
    occupiedLine.id = 2302;
    occupiedLine.startTick = 900;
    occupiedLine.endTick = 3000;
    occupiedLine.startPos = Vec2(300.0f, 500.0f);
    occupiedLine.endPos = Vec2(700.0f, 500.0f);
    occupiedLine.direction = Vec2(1.0f, 0.0f);
    const CandidateEvaluation lineHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {occupiedLine});
    ExpectNear("UNAV-02 stationary line exact weighted exposure",
               lineHold.dangerExposureMs,
               2700.0f,
               0.1f);
    ExpectTrue("UNAV-02 stationary line reports complete danger metrics",
               lineHold.collisionCount == 1 &&
                   lineHold.endpointDanger == 3 &&
                   lineHold.pathDanger == 3 &&
                   lineHold.firstCollisionTimeMs == 0.0f);

    const CandidateEvaluation overlappingHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {occupiedCircle, occupiedLine});
    ExpectTrue("UNAV-10 overlapping hold tracks unique identities",
               overlappingHold.collisionCount == 2 &&
                   overlappingHold.pathDanger == 6 &&
                   overlappingHold.maxDanger == 3 &&
                   overlappingHold.summedExposureDanger == 6);
    ExpectNear("UNAV-10 overlapping hold sums exact weighted exposure",
               overlappingHold.dangerExposureMs,
               5400.0f,
               0.1f);

    Threat duplicateOccupiedCircle = occupiedCircle;
    const CandidateEvaluation duplicateIdentityHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {occupiedCircle, duplicateOccupiedCircle});
    ExpectTrue("stationary duplicate positive identity counts once",
               duplicateIdentityHold.collisionCount == 1 &&
                   duplicateIdentityHold.pathDanger == 3 &&
                   duplicateIdentityHold.summedExposureDanger == 3);
    ExpectNear("stationary duplicate identity exposure counts once",
               duplicateIdentityHold.dangerExposureMs,
               2700.0f,
               0.1f);

    SpellData sequentialExposureSpell = occupiedCircleSpell;
    sequentialExposureSpell.extraEndTime = 0;
    Threat sequentialExposureA =
        ZDEvadeTest::MakeThreat(sequentialExposureSpell);
    sequentialExposureA.id = 2310;
    sequentialExposureA.startTick = 1000;
    sequentialExposureA.endTick = 1400;
    sequentialExposureA.endPos = Vec2(500.0f, 500.0f);
    Threat sequentialExposureB = sequentialExposureA;
    sequentialExposureB.id = 2311;
    sequentialExposureB.startTick = 1600;
    sequentialExposureB.endTick = 2000;
    const CandidateEvaluation sequentialExposureHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {sequentialExposureA, sequentialExposureB});
    ExpectTrue(
        "sequential non-overlapping exposure sums unique identity danger",
        sequentialExposureHold.collisionCount == 2 &&
            sequentialExposureHold.pathDanger == 3 &&
            sequentialExposureHold.summedExposureDanger == 6);

    SpellData overlappingExplosionSpell = occupiedCircleSpell;
    overlappingExplosionSpell.hasEndExplosion = true;
    overlappingExplosionSpell.secondaryRadius = 80.0f;
    overlappingExplosionSpell.endExplosionDelay = 0;
    overlappingExplosionSpell.endExplosionDuration = 1000;
    Threat overlappingExplosion =
        ZDEvadeTest::MakeThreat(overlappingExplosionSpell);
    overlappingExplosion.id = 2304;
    overlappingExplosion.startTick = 900;
    overlappingExplosion.endTick = 3000;
    overlappingExplosion.endPos = Vec2(500.0f, 500.0f);
    overlappingExplosion.collisionExplosionCenter =
        overlappingExplosion.endPos;
    const CandidateEvaluation bodyExplosionHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {overlappingExplosion});
    ExpectTrue("stationary body and explosion share one collision identity",
               bodyExplosionHold.collisionCount == 1 &&
                   bodyExplosionHold.summedExposureDanger ==
                       overlappingExplosion.Danger());
    ExpectNear("stationary body/explosion overlap is exact union exposure",
               bodyExplosionHold.dangerExposureMs,
               2700.0f,
               0.1f);

    SpellData separatedBodyExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    separatedBodyExplosionSpell.spellDelay = 0;
    separatedBodyExplosionSpell.projectileSpeed = 0.0f;
    separatedBodyExplosionSpell.radius = 60.0f;
    separatedBodyExplosionSpell.extraEndTime = 1000;
    separatedBodyExplosionSpell.hasEndExplosion = true;
    separatedBodyExplosionSpell.secondaryRadius = 60.0f;
    separatedBodyExplosionSpell.endExplosionDelay = 0;
    separatedBodyExplosionSpell.endExplosionDuration = 100;
    Threat separatedBodyExplosion =
        ZDEvadeTest::MakeThreat(separatedBodyExplosionSpell);
    separatedBodyExplosion.id = 2305;
    separatedBodyExplosion.startTick = 1000;
    separatedBodyExplosion.endTick = 3000;
    separatedBodyExplosion.endPos = Vec2(500.0f, 500.0f);
    separatedBodyExplosion.collisionExplosionCenter =
        Vec2(600.0f, 500.0f);
    const CandidateEvaluation explosionOnlyHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(620.0f, 500.0f),
            Vec2(620.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {separatedBodyExplosion});
    ExpectTrue("public containment remains explosion inclusive",
               EvadeGeometry::ContainsAt(
                   separatedBodyExplosion,
                   Vec2(620.0f, 500.0f),
                   0.0f,
                   0.0f,
                   1000));
    ExpectTrue("separated explosion-only hold keeps one identity",
               explosionOnlyHold.collisionCount == 1 &&
                   explosionOnlyHold.pathDanger ==
                       separatedBodyExplosion.Danger());
    ExpectNear("explosion-only hold accrues only explosion lifetime",
               explosionOnlyHold.dangerExposureMs,
               100.0f *
                   static_cast<float>(separatedBodyExplosion.Danger()),
               0.1f);
    const CandidateEvaluation bodyOnlyHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(480.0f, 500.0f),
            Vec2(480.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {separatedBodyExplosion});
    ExpectTrue("separated body-only hold keeps one identity",
               bodyOnlyHold.collisionCount == 1 &&
                   bodyOnlyHold.pathDanger ==
                       separatedBodyExplosion.Danger());
    ExpectNear("body-only hold accrues exact body lifetime",
               bodyOnlyHold.dangerExposureMs,
               1000.0f *
                   static_cast<float>(separatedBodyExplosion.Danger()),
               0.1f);
    const CandidateEvaluation separatedOverlapHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(550.0f, 500.0f),
            Vec2(550.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            holdSettings,
            {separatedBodyExplosion});
    ExpectTrue("separated body/explosion overlap keeps one identity",
               separatedOverlapHold.collisionCount == 1 &&
                   separatedOverlapHold.pathDanger ==
                       separatedBodyExplosion.Danger());
    ExpectNear("separated overlap merges lifetimes without double count",
               separatedOverlapHold.dangerExposureMs,
               1000.0f *
                   static_cast<float>(separatedBodyExplosion.Danger()),
               0.1f);

    InstallWalkableTestGrid();
    SpellData endpointHoldSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    endpointHoldSpell.spellDelay = 300;
    endpointHoldSpell.projectileSpeed = 0.0f;
    endpointHoldSpell.radius = 45.0f;
    endpointHoldSpell.extraEndTime = 500;
    Threat endpointHoldThreat =
        ZDEvadeTest::MakeThreat(endpointHoldSpell);
    endpointHoldThreat.id = 2303;
    endpointHoldThreat.startTick = 1000;
    endpointHoldThreat.endTick = 3000;
    endpointHoldThreat.endPos = Vec2(300.0f, 500.0f);
    const CandidateEvaluation shortNativePath =
        EvadeGeometry::EvaluatePathCandidate(
            {Vec2(200.0f, 500.0f), Vec2(300.0f, 500.0f)},
            PlannerCandidateSource::Cursor,
            -1,
            Vec2(300.0f, 500.0f),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            holdSettings,
            {endpointHoldThreat});
    ExpectTrue("short native path includes endpoint hold coverage",
               !shortNativePath.strictSafe &&
                   shortNativePath.endpointDanger == 3 &&
                   shortNativePath.collisionCount == 1 &&
                   shortNativePath.dangerExposureMs > 1000.0f);

    SpellData zeroDurationExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    zeroDurationExplosionSpell.spellDelay = 0;
    zeroDurationExplosionSpell.projectileSpeed = 0.0f;
    zeroDurationExplosionSpell.radius = 5.0f;
    zeroDurationExplosionSpell.extraEndTime = 0;
    zeroDurationExplosionSpell.hasEndExplosion = true;
    zeroDurationExplosionSpell.secondaryRadius = 25.0f;
    zeroDurationExplosionSpell.endExplosionDelay = 0;
    zeroDurationExplosionSpell.endExplosionDuration = 0;
    Threat zeroDurationExplosion =
        ZDEvadeTest::MakeThreat(zeroDurationExplosionSpell);
    zeroDurationExplosion.id = 2401;
    zeroDurationExplosion.startTick = 1000;
    zeroDurationExplosion.endTick = 3000;
    zeroDurationExplosion.endPos = Vec2(800.0f, 800.0f);
    zeroDurationExplosion.collisionExplosionCenter =
        Vec2(500.0f, 500.0f);
    zeroDurationExplosion.projectileTerminated = true;
    zeroDurationExplosion.projectileTerminationTick = 1000;
    ExpectTrue("zero-duration explosion is active through canonical 100ms",
               EvadeGeometry::ContainsAt(
                   zeroDurationExplosion,
                   Vec2(500.0f, 500.0f),
                   0.0f,
                   0.0f,
                   1100));
    ExpectTrue("zero-duration explosion releases after canonical 100ms",
               !EvadeGeometry::ContainsAt(
                   zeroDurationExplosion,
                   Vec2(500.0f, 500.0f),
                   0.0f,
                   0.0f,
                   1101));
    EvadeSettings canonicalExplosionSettings = holdSettings;
    canonicalExplosionSettings.maxThreatHorizonMs = 200.0f;
    const CandidateEvaluation zeroDurationExplosionHold =
        EvadeGeometry::EvaluateStationaryCandidate(
            Vec2(500.0f, 500.0f),
            Vec2(500.0f, 500.0f),
            0.0f,
            0.0f,
            1000,
            canonicalExplosionSettings,
            {zeroDurationExplosion});
    ExpectTrue("stationary zero-duration explosion is reported",
               zeroDurationExplosionHold.collisionCount == 1 &&
                   zeroDurationExplosionHold.pathDanger ==
                       zeroDurationExplosion.Danger());
    ExpectNear("stationary zero-duration explosion has exact 100ms exposure",
               zeroDurationExplosionHold.dangerExposureMs,
               100.0f * static_cast<float>(zeroDurationExplosion.Danger()),
               0.1f);

    EvadeSettings commonHorizonSettings = holdSettings;
    commonHorizonSettings.maxThreatHorizonMs = 150.0f;
    commonHorizonSettings.pathStep = 4.0f;
    commonHorizonSettings.temporalStepMs = 6.0f;
    const std::vector<Vec2> commonHorizonPath = {
        Vec2(100.0f, 500.0f),
        Vec2(200.0f, 500.0f),
        Vec2(400.0f, 500.0f),
        Vec2(500.0f, 500.0f),
    };
    const auto makeTimedPathCircle = [](
        int id,
        const Vec2& center,
        int activationTick) {
        SpellData spell = ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
        spell.spellDelay = 0;
        spell.projectileSpeed = 0.0f;
        spell.radius = 1.0f;
        spell.extraEndTime = 1000;
        Threat threat = ZDEvadeTest::MakeThreat(spell);
        threat.id = id;
        threat.startTick = activationTick;
        threat.endTick = 4000;
        threat.endPos = center;
        return threat;
    };
    const CandidateEvaluation afterHorizonPath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            commonHorizonSettings,
            {makeTimedPathCircle(2402, Vec2(400.0f, 500.0f), 1300)});
    ExpectTrue("polyline ignores hazards wholly after common horizon",
               afterHorizonPath.strictSafe &&
                   afterHorizonPath.collisionCount == 0 &&
                   afterHorizonPath.firstCollisionTimeMs == FLT_MAX);

    const CandidateEvaluation beforeHorizonPath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            commonHorizonSettings,
            {makeTimedPathCircle(2403, Vec2(240.0f, 500.0f), 1140)});
    ExpectTrue("polyline counts hazard before common horizon",
               !beforeHorizonPath.strictSafe &&
                   beforeHorizonPath.collisionCount == 1 &&
                   beforeHorizonPath.firstCollisionTimeMs <= 140.1f);

    const CandidateEvaluation exactHorizonPath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            commonHorizonSettings,
            {makeTimedPathCircle(2404, Vec2(250.0f, 500.0f), 1150)});
    ExpectTrue("polyline counts hazard exactly at common horizon",
               !exactHorizonPath.strictSafe &&
                   exactHorizonPath.collisionCount == 1);
    ExpectNear("exact-horizon collision preserves global timing",
               exactHorizonPath.firstCollisionTimeMs,
               150.0f,
               0.1f);

    SpellData partialExposureSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    partialExposureSpell.spellDelay = 0;
    partialExposureSpell.projectileSpeed = 0.0f;
    partialExposureSpell.radius = 1.0f;
    partialExposureSpell.extraEndTime = 1000;
    partialExposureSpell.hasEndExplosion = true;
    partialExposureSpell.secondaryRadius = 9.0f;
    partialExposureSpell.endExplosionDelay = 0;
    partialExposureSpell.endExplosionDuration = 1000;
    Threat partialExposure =
        ZDEvadeTest::MakeThreat(partialExposureSpell);
    partialExposure.id = 2405;
    partialExposure.startTick = 1000;
    partialExposure.endTick = 4000;
    partialExposure.endPos = Vec2(800.0f, 800.0f);
    partialExposure.collisionExplosionCenter = Vec2(225.0f, 500.0f);
    partialExposure.projectileTerminated = true;
    partialExposure.projectileTerminationTick = 1000;
    const CandidateEvaluation clippedExposurePath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            commonHorizonSettings,
            {partialExposure});
    ExpectNear("partial horizon segment preserves full route arrival",
               clippedExposurePath.arrivalTimeMs,
               400.0f,
               0.1f);
    ExpectNear("partial horizon segment preserves exact first contact",
               clippedExposurePath.firstCollisionTimeMs,
               115.0f,
               0.1f);
    ExpectNear("partial horizon segment preserves exact exposure",
               clippedExposurePath.dangerExposureMs,
               20.0f * static_cast<float>(partialExposure.Danger()),
               0.1f);

    EvadeSettings zeroHorizonPathSettings = commonHorizonSettings;
    zeroHorizonPathSettings.maxThreatHorizonMs = 0.0f;
    const Threat occupiedAtPathStart =
        makeTimedPathCircle(2410, commonHorizonPath.front(), 900);
    const CandidateEvaluation occupiedZeroHorizonPath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            zeroHorizonPathSettings,
            {occupiedAtPathStart});
    ExpectTrue("zero horizon evaluates inclusive occupied start",
               !occupiedZeroHorizonPath.strictSafe &&
                   !occupiedZeroHorizonPath.pathSafe &&
                   occupiedZeroHorizonPath.endpointSafe &&
                   occupiedZeroHorizonPath.endpointDanger == 0 &&
                   occupiedZeroHorizonPath.pathDanger ==
                       occupiedAtPathStart.Danger() &&
                   occupiedZeroHorizonPath.collisionCount == 1);
    ExpectNear("zero horizon occupied start has exact collision time",
               occupiedZeroHorizonPath.firstCollisionTimeMs,
               0.0f,
               0.1f);
    ExpectNear("zero horizon instantaneous occupancy has zero exposure",
               occupiedZeroHorizonPath.dangerExposureMs,
               0.0f,
               0.1f);
    const CandidateEvaluation safeZeroHorizonPath =
        EvadeGeometry::EvaluatePathCandidate(
            commonHorizonPath,
            PlannerCandidateSource::Cursor,
            -1,
            commonHorizonPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            zeroHorizonPathSettings,
            {});
    ExpectTrue("zero horizon preserves genuinely safe start",
               safeZeroHorizonPath.strictSafe &&
                   safeZeroHorizonPath.pathSafe &&
                   safeZeroHorizonPath.endpointSafe &&
                   safeZeroHorizonPath.collisionCount == 0 &&
                   safeZeroHorizonPath.firstCollisionTimeMs == FLT_MAX);

    EvadeSettings inputDelayHorizonSettings = commonHorizonSettings;
    inputDelayHorizonSettings.inputDelayMs = 100.0f;
    inputDelayHorizonSettings.maxThreatHorizonMs = 50.0f;
    inputDelayHorizonSettings.pathBuffer = 0.0f;
    inputDelayHorizonSettings.endpointBuffer = 30.0f;
    const std::vector<Vec2> inputDelayPath = {
        Vec2(100.0f, 500.0f),
        Vec2(200.0f, 500.0f),
    };
    const Threat endpointBufferOnly =
        makeTimedPathCircle(2411, Vec2(132.0f, 500.0f), 900);
    const CandidateEvaluation uncontaminatedInputDelayPath =
        EvadeGeometry::EvaluatePathCandidate(
            inputDelayPath,
            PlannerCandidateSource::Cursor,
            -1,
            inputDelayPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            inputDelayHorizonSettings,
            {endpointBufferOnly});
    ExpectTrue("input-delay path ignores unreached endpoint-only buffer",
               uncontaminatedInputDelayPath.strictSafe &&
                   uncontaminatedInputDelayPath.pathSafe &&
                   uncontaminatedInputDelayPath.endpointSafe &&
                   uncontaminatedInputDelayPath.endpointDanger == 0 &&
                   uncontaminatedInputDelayPath.pathDanger == 0 &&
                   uncontaminatedInputDelayPath.collisionCount == 0 &&
                   uncontaminatedInputDelayPath.dangerExposureMs == 0.0f);

    Threat bodyAtHero =
        makeTimedPathCircle(2412, inputDelayPath.front(), 900);
    SpellData inputDelayExplosionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    inputDelayExplosionSpell.spellDelay = 0;
    inputDelayExplosionSpell.projectileSpeed = 0.0f;
    inputDelayExplosionSpell.radius = 1.0f;
    inputDelayExplosionSpell.extraEndTime = 1000;
    inputDelayExplosionSpell.hasEndExplosion = true;
    inputDelayExplosionSpell.secondaryRadius = 5.0f;
    inputDelayExplosionSpell.endExplosionDelay = 0;
    inputDelayExplosionSpell.endExplosionDuration = 1000;
    Threat explosionAtHero =
        ZDEvadeTest::MakeThreat(inputDelayExplosionSpell);
    explosionAtHero.id = 2413;
    explosionAtHero.startTick = 1000;
    explosionAtHero.endTick = 4000;
    explosionAtHero.endPos = Vec2(800.0f, 800.0f);
    explosionAtHero.collisionExplosionCenter = inputDelayPath.front();
    explosionAtHero.projectileTerminated = true;
    explosionAtHero.projectileTerminationTick = 1000;
    const CandidateEvaluation occupiedInputDelayPath =
        EvadeGeometry::EvaluatePathCandidate(
            inputDelayPath,
            PlannerCandidateSource::Cursor,
            -1,
            inputDelayPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            inputDelayHorizonSettings,
            {endpointBufferOnly, bodyAtHero, explosionAtHero});
    ExpectTrue("input-delay path counts body and explosion at hero only",
               !occupiedInputDelayPath.strictSafe &&
                   !occupiedInputDelayPath.pathSafe &&
                   occupiedInputDelayPath.endpointSafe &&
                   occupiedInputDelayPath.endpointDanger == 0 &&
                   occupiedInputDelayPath.pathDanger ==
                       bodyAtHero.Danger() + explosionAtHero.Danger() &&
                   occupiedInputDelayPath.collisionCount == 2);
    ExpectNear("input-delay stationary path has exact first contact",
               occupiedInputDelayPath.firstCollisionTimeMs,
               0.0f,
               0.1f);
    ExpectNear("input-delay stationary path has exact identity exposure",
               occupiedInputDelayPath.dangerExposureMs,
               50.0f * static_cast<float>(
                   bodyAtHero.Danger() + explosionAtHero.Danger()),
               0.1f);

    const CandidateEvaluation lockedBranchPath =
        EvadeGeometry::EvaluatePathCandidate(
            inputDelayPath,
            PlannerCandidateSource::ConeSide,
            branchCone.id,
            inputDelayPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            inputDelayHorizonSettings,
            {},
            nullptr,
            StabilityBranch::ConeLeft);
    CandidateEvaluation pendingBranchRefresh =
        EvadeGeometry::EvaluateCandidate(
            inputDelayPath.back(),
            PlannerCandidateSource::ConeSide,
            branchCone.id,
            inputDelayPath.front(),
            inputDelayPath.back(),
            0.0f,
            1000.0f,
            1.0f,
            1000,
            inputDelayHorizonSettings,
            {});
    CarryStabilityBranchKey(
        pendingBranchRefresh,
        lockedBranchPath);
    CandidateEvaluation promotedBranch =
        lockedBranchPath;
    promotedBranch.strictSafe = true;
    ExpectTrue(
        "branch key survives lock path and pending reevaluation",
        lockedBranchPath.stabilityBranchKey ==
                StabilityBranch::ConeLeft &&
            pendingBranchRefresh.stabilityBranchKey ==
                StabilityBranch::ConeLeft);
    ExpectTrue(
        "branch key survives fallback to strict promotion",
        ShouldPromoteFallbackEvaluation(
            true,
            promotedBranch.valid,
            promotedBranch.walkable,
            promotedBranch.strictSafe) &&
            promotedBranch.stabilityBranchKey ==
                StabilityBranch::ConeLeft);

    return ZDEvadeTest::Finish("ZDEVADE GEOMETRY RUNTIME");
}
