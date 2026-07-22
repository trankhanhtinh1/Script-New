#include "tests/ZDEvadeTestSupport.h"
#include "plugins/ZDEvade/Database/ThreatDatabase.h"
#include "plugins/ZDEvade/Detection/ThreatDetectionPolicy.h"

#include <algorithm>
#include <array>
#include <climits>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <vector>

using namespace ZDEvade;
using ZDEvadeTest::ExpectEq;
using ZDEvadeTest::ExpectNear;
using ZDEvadeTest::ExpectTrue;

namespace {

struct QueueEvent {
    std::uint64_t id = 0;
    char spellName[64] = {};
    Vec2 start = {};
    Vec2 end = {};
};

struct PendingSpellNameEvent {
    char spellName[64] = {};
    bool spellNameFromSlotFallback = false;
    bool isAutoAttack = false;
    std::uint32_t targetNetworkId = 0;
};

void CopyName(char (&destination)[64], const char* source) {
    const char* value = source ? source : "";
    const std::size_t length =
        std::min(std::strlen(value), sizeof(destination) - 1);
    std::memcpy(destination, value, length);
    destination[length] = '\0';
}

PendingSpellNameEvent MakePendingSpellNameEvent(
        const char* name,
        bool fromSlotFallback,
        bool isAutoAttack = false,
        std::uint32_t targetNetworkId = 0) {
    PendingSpellNameEvent event;
    CopyName(event.spellName, name);
    event.spellNameFromSlotFallback = fromSlotFallback;
    event.isAutoAttack = isAutoAttack;
    event.targetNetworkId = targetNetworkId;
    return event;
}

void MergePendingSpellNameEvent(
        PendingSpellNameEvent& current,
        const PendingSpellNameEvent& incoming) {
    if (ShouldSelectIncomingSpellName(
            current.spellName,
            current.spellNameFromSlotFallback,
            incoming.spellName,
            incoming.spellNameFromSlotFallback)) {
        std::memcpy(
            current.spellName,
            incoming.spellName,
            sizeof(current.spellName));
        current.spellNameFromSlotFallback =
            incoming.spellNameFromSlotFallback;
    }
    current.isAutoAttack =
        current.isAutoAttack || incoming.isAutoAttack;
    if (current.targetNetworkId == 0)
        current.targetNetworkId = incoming.targetNetworkId;
}

void MergeQueueEvent(QueueEvent& current, const QueueEvent& incoming) {
    if (ShouldPreferCastName(current.spellName, incoming.spellName))
        std::memcpy(current.spellName, incoming.spellName, sizeof(current.spellName));
    current.start = PreferredCastPosition(current.start, incoming.start);
    current.end = PreferredCastPosition(current.end, incoming.end);
}

CastEventKey MakeCastKey(std::uint32_t caster,
                         std::uintptr_t identity,
                         int slot,
                         std::int64_t tick,
                         std::initializer_list<const char*> spellNames,
                         const Vec2& start,
                         const Vec2& end,
                         const Vec2& cast = {}) {
    CastEventKey key = {caster, identity, slot, tick};
    for (const char* name : spellNames)
        AddCastSpellName(key.spellNames, name);
    key.startPosition = start;
    key.endPosition = end;
    key.castPosition = cast;
    return key;
}

CastEventKey MakeCastKey(std::uint32_t caster,
                         std::uintptr_t identity,
                         int slot,
                         std::int64_t tick,
                         const char* spell,
                         const Vec2& start,
                         const Vec2& end,
                         const Vec2& cast = {}) {
    return MakeCastKey(
        caster, identity, slot, tick, {spell}, start, end, cast);
}

PendingEventDescriptor MakeDescriptor(
        std::uint64_t eventId,
        PendingPriority priority,
        std::uintptr_t identity = 0,
        int slot = 0,
        std::int64_t tick = 1000,
        const char* spell = "EzrealMysticShot",
        Vec2 start = Vec2(100.0f, 100.0f),
        Vec2 end = Vec2(900.0f, 100.0f)) {
    PendingEventDescriptor descriptor;
    descriptor.key = MakeCastKey(
        static_cast<std::uint32_t>(eventId + 10u), identity, slot, tick,
        spell, start, end);
    descriptor.priority = priority;
    return descriptor;
}

QueueEvent MakeQueueEvent(std::uint64_t id, const char* name = "") {
    QueueEvent event;
    event.id = id;
    CopyName(event.spellName, name);
    return event;
}

} // namespace

int main() {
    ExpectEq("unknown game thread queues raw cast",
             static_cast<int>(DecideImmediateCastDispatch(0u, 41u, false)),
             static_cast<int>(
                 ImmediateCastDispatchDecision::QueueUnknownGameThread));
    ExpectEq("known equal game thread processes raw cast immediately",
             static_cast<int>(DecideImmediateCastDispatch(41u, 41u, false)),
             static_cast<int>(
                 ImmediateCastDispatchDecision::ProcessImmediately));
    ExpectEq("different raw callback thread queues cast",
             static_cast<int>(DecideImmediateCastDispatch(41u, 42u, false)),
             static_cast<int>(
                 ImmediateCastDispatchDecision::QueueWrongThread));
    ExpectEq("reentrant game-thread raw callback queues cast",
             static_cast<int>(DecideImmediateCastDispatch(41u, 41u, true)),
             static_cast<int>(
                 ImmediateCastDispatchDecision::QueueReentrant));
    ExpectTrue("successful immediate cast does not queue fallback",
               !ShouldQueueCastAfterImmediateAttempt(
                   ImmediateCastDispatchDecision::ProcessImmediately,
                   true));
    ExpectTrue("SEH-failed immediate cast queues fallback retry",
               ShouldQueueCastAfterImmediateAttempt(
                   ImmediateCastDispatchDecision::ProcessImmediately,
                   false));
    ExpectTrue("non-immediate dispatch always queues fallback",
               ShouldQueueCastAfterImmediateAttempt(
                   ImmediateCastDispatchDecision::QueueWrongThread,
                   true));
    ExpectTrue("missile drain requires known equal game thread",
               IsKnownGameThread(41u, 41u) &&
                   !IsKnownGameThread(0u, 41u) &&
                   !IsKnownGameThread(41u, 42u));

    ThreatAdmissionCounters arcCounters;
    SpellData lineAdmission = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    ExpectTrue("non-Arc admission remains accepted",
               AdmitThreatData(&lineAdmission, arcCounters));
    ExpectEq("non-Arc admission does not increment Arc drops",
             arcCounters.unsupportedArcDropped, 0);

    SpellData arcAdmission = ZDEvadeTest::MakeSpell(ZDSpellType::Arc);
    ExpectTrue("default Arc admission is rejected",
               !AdmitThreatData(&arcAdmission, arcCounters));
    ExpectEq("default Arc admission increments drop counter",
             arcCounters.unsupportedArcDropped, 1);

    arcAdmission.arcSupported = true;
    ExpectTrue("arcSupported alone remains rejected",
               !AdmitThreatData(&arcAdmission, arcCounters));
    ExpectEq("incomplete explicit Arc increments drop counter",
             arcCounters.unsupportedArcDropped, 2);

    arcAdmission.arcCenterX = 500.0f;
    arcAdmission.arcCenterY = 100.0f;
    arcAdmission.arcRadius = 600.0f;
    arcAdmission.arcStartAngleDegrees = 180.0f;
    arcAdmission.arcSweepAngleDegrees = 90.0f;
    ExpectTrue("complete explicit Arc admission remains rejected",
               !AdmitThreatData(&arcAdmission, arcCounters));
    ExpectEq("complete Arc increments unsupported drop counter",
             arcCounters.unsupportedArcDropped, 3);
    arcAdmission.spellDelay = 250;
    arcAdmission.extraEndTime = 100;
    arcAdmission.projectileSpeed = 2000.0f;
    ExpectEq("Arc lifecycle does not use straight chord travel",
             CalculateThreatEndTick(
                 arcAdmission,
                 Vec2(0.0f, 0.0f),
                 Vec2(1000.0f, 0.0f),
                 1000,
                 0),
             1350);

    SpellData disabledCompleteArc = arcAdmission;
    disabledCompleteArc.arcSupported = false;
    ExpectTrue("complete Arc still requires explicit feature gate",
               !AdmitThreatData(&disabledCompleteArc, arcCounters));

    const float nan = std::numeric_limits<float>::quiet_NaN();
    const auto expectIncompleteArcDrop = [&](
            const char* name,
            float SpellData::* field) {
        SpellData incomplete = arcAdmission;
        incomplete.*field = nan;
        ExpectTrue(name, !AdmitThreatData(&incomplete, arcCounters));
    };
    expectIncompleteArcDrop(
        "Arc admission rejects omitted center x", &SpellData::arcCenterX);
    expectIncompleteArcDrop(
        "Arc admission rejects omitted center y", &SpellData::arcCenterY);
    expectIncompleteArcDrop(
        "Arc admission rejects omitted radius", &SpellData::arcRadius);
    expectIncompleteArcDrop(
        "Arc admission rejects omitted start", &SpellData::arcStartAngleDegrees);
    expectIncompleteArcDrop(
        "Arc admission rejects omitted sweep", &SpellData::arcSweepAngleDegrees);
    SpellData zeroRadiusArc = arcAdmission;
    zeroRadiusArc.arcRadius = 0.0f;
    ExpectTrue("Arc admission rejects non-positive radius",
               !AdmitThreatData(&zeroRadiusArc, arcCounters));
    SpellData zeroSweepArc = arcAdmission;
    zeroSweepArc.arcSweepAngleDegrees = 0.0f;
    ExpectTrue("Arc admission rejects zero sweep",
               !AdmitThreatData(&zeroSweepArc, arcCounters));
    SpellData excessiveSweepArc = arcAdmission;
    excessiveSweepArc.arcSweepAngleDegrees = 360.1f;
    ExpectTrue("Arc admission rejects sweep above one turn",
               !AdmitThreatData(&excessiveSweepArc, arcCounters));
    ExpectEq("all incomplete Arc fixtures increment drop counter",
             arcCounters.unsupportedArcDropped, 12);

    SpellData invalidRadius =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    invalidRadius.radius = nan;
    ExpectTrue("admission rejects NaN radius",
               !IsThreatDataAdmissible(&invalidRadius));
    SpellData invalidInnerRadius =
        ZDEvadeTest::MakeSpell(ZDSpellType::Ring);
    invalidInnerRadius.innerRadius = -1.0f;
    ExpectTrue("admission rejects negative inner radius",
               !IsThreatDataAdmissible(&invalidInnerRadius));
    SpellData invalidRange =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    invalidRange.range = std::numeric_limits<float>::infinity();
    ExpectTrue("admission rejects infinite range",
               !IsThreatDataAdmissible(&invalidRange));
    SpellData invalidCone =
        ZDEvadeTest::MakeSpell(ZDSpellType::Cone);
    invalidCone.coneEdgePadding = nan;
    ExpectTrue("admission rejects NaN cone geometry",
               !IsThreatDataAdmissible(&invalidCone));
    ResetThreatAdmissionCounters(arcCounters);
    ExpectEq("Arc admission counter reset clears runtime state",
             arcCounters.unsupportedArcDropped, 0);

    const CastEventKey processSpell = {42u, 0x1234u, 0, 1000};
    const CastEventKey doCast = {42u, 0x1234u, 0, 1080};
    ExpectTrue("ProcessSpell and DoCast share one logical cast",
               SameLogicalCast(processSpell, doCast));

    const CastEventKey distinctCast = {42u, 0x5678u, 0, 1080};
    ExpectTrue("distinct cast identities remain separate",
               !SameLogicalCast(processSpell, distinctCast));
    const CastEventKey reusedIdentity = {42u, 0x1234u, 0, 1400};
    ExpectTrue("reused cast identity outside tick tolerance stays separate",
               !SameLogicalCast(processSpell, reusedIdentity));

    const CastEventKey fallbackProcessSpell = MakeCastKey(
        42u, 0u, 0, 1000, "EzrealMysticShot",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    CastEventKey fallbackDoCast = MakeCastKey(
        42u, 0u, 0, 1110, "EzrealMysticShot",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    ExpectTrue("identity-free events coalesce within tolerance",
               SameLogicalCast(fallbackProcessSpell, fallbackDoCast));
    fallbackDoCast.tick = 1121;
    ExpectTrue("identity-free events outside tolerance remain separate",
               !SameLogicalCast(fallbackProcessSpell, fallbackDoCast));
    const CastEventKey minimumTick = {42u, 0u, 0, INT_MIN};
    const CastEventKey maximumTick = {42u, 0u, 0, INT_MAX};
    ExpectTrue("extreme tick delta does not overflow or coalesce",
               !SameLogicalCast(minimumTick, maximumTick));
    const CastEventKey minimumWideTick = {
        42u, 0u, 0, std::numeric_limits<std::int64_t>::min()
    };
    const CastEventKey maximumWideTick = {
        42u, 0u, 0, std::numeric_limits<std::int64_t>::max()
    };
    ExpectTrue("int64 tick endpoints compare without overflow",
               !SameLogicalCast(minimumWideTick, maximumWideTick));

    const CastEventKey mixedIdentityProcess = MakeCastKey(
        42u, 0u, 0, 2000, {"Ezreal", "EzrealMysticShot"},
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    const CastEventKey mixedIdentityDoCast = MakeCastKey(
        42u, 0xABC0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(), Vec2(900.0f, 100.0f));
    ExpectTrue("sparse canonical name overlap merges mixed identity",
               SameLogicalCast(mixedIdentityProcess, mixedIdentityDoCast));
    const CastEventKey reorderedNamesLeft = MakeCastKey(
        42u, 0u, 0, 2070, {"Ezreal", "EzrealMysticShot", "EzrealQ"},
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    const CastEventKey reorderedNamesRight = MakeCastKey(
        42u, 0xABC0u, 0, 2070,
        {"ezrealq", "ezrealmysticshot", "ezreal"},
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    ExpectTrue("runtime name ordering does not affect mixed identity merge",
               SameLogicalCast(reorderedNamesLeft, reorderedNamesRight));
    CastEventKey incompatibleFingerprint = mixedIdentityDoCast;
    incompatibleFingerprint.spellNames = {};
    AddCastSpellName(
        incompatibleFingerprint.spellNames, "EzrealEssenceFlux");
    ExpectTrue("mixed identity fallback rejects different spell fingerprints",
               !SameLogicalCast(mixedIdentityProcess, incompatibleFingerprint));
    const CastEventKey endpointReference = MakeCastKey(
        42u, 0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(), Vec2(900.0f, 100.0f));
    const CastEventKey conflictingEndpoint = MakeCastKey(
        42u, 0xABC0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(), Vec2(), Vec2(-900.0f, 100.0f));
    ExpectTrue("missing starts cannot bypass cross-field endpoint conflict",
               !SameLogicalCast(endpointReference, conflictingEndpoint));
    const CastEventKey conflictingStart = MakeCastKey(
        42u, 0xABC0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(900.0f, 900.0f), Vec2(900.0f, 100.0f));
    ExpectTrue("conflicting starts reject mixed identity merge",
               !SameLogicalCast(mixedIdentityProcess, conflictingStart));
    const CastEventKey startOnly = MakeCastKey(
        42u, 0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(100.0f, 100.0f), Vec2());
    const CastEventKey endpointOnly = MakeCastKey(
        42u, 0xABC0u, 0, 2070, {"EzrealMysticShot"},
        Vec2(), Vec2(900.0f, 100.0f));
    ExpectTrue("no comparable geometry rejects mixed identity merge",
               !SameLogicalCast(startOnly, endpointOnly));
    CastEventKey simultaneousDistinct = mixedIdentityDoCast;
    simultaneousDistinct.castIdentity = 0xDEF0u;
    ExpectTrue("simultaneous casts with distinct nonzero identities stay separate",
               !SameLogicalCast(
                   MakeCastKey(42u, 0xABC0u, 0, 2070, "EzrealMysticShot",
                               Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f)),
                   simultaneousDistinct));

    ExpectTrue("delayed launches receive a bind window above 1600ms",
               MissileBindWindowMs(1800, 0) > 1600);
    ExpectTrue("expected launch delay expands missile bind window",
               MissileBindWindowMs(250, 900) > MissileBindWindowMs(250, 0));
    ExpectEq("maximum spell delay saturates at bind cap",
             MissileBindWindowMs(INT_MAX, 0),
             5000);
    ExpectEq("maximum expected launch delay saturates at bind cap",
             MissileBindWindowMs(0, INT_MAX),
             5000);
    ExpectEq("negative delays clamp before tolerance",
             MissileBindWindowMs(INT_MIN, -1),
             250);
    ExpectTrue("missing missile remains bound during first absent frame",
               !ShouldTerminateMissingMissile(
                   true, false, false, 1000, 1000));
    ExpectTrue("missing missile remains bound inside observation grace",
               !ShouldTerminateMissingMissile(
                   true, false, false, 1000, 1249));
    ExpectTrue("missing missile terminates at observation grace boundary",
               ShouldTerminateMissingMissile(
                   true, false, false, 1000, 1250));
    ExpectTrue("valid missile observation prevents jitter termination",
               !ShouldTerminateMissingMissile(
                   true, false, true, 1000, 2000));
    ExpectTrue("unbound or terminated projectiles do not re-terminate",
               !ShouldTerminateMissingMissile(
                   false, false, false, 1000, 2000) &&
               !ShouldTerminateMissingMissile(
                   true, true, false, 1000, 2000));
    ExpectTrue("missile-bound Circular participates in projectile lifecycle",
               UsesMissileLifecycle(ZDSpellType::Circular, true));
    ExpectTrue("missile-bound non-Arc geometries participate in lifecycle",
               UsesMissileLifecycle(ZDSpellType::Line, true) &&
               UsesMissileLifecycle(ZDSpellType::Cone, true) &&
               UsesMissileLifecycle(ZDSpellType::Ring, true));
    ExpectTrue("static cast-only Circular does not follow a missile",
               !UsesMissileLifecycle(ZDSpellType::Circular, false));
    ExpectTrue("unsupported Arc never enters straight missile lifecycle",
               !UsesMissileLifecycle(ZDSpellType::Arc, true));
    ExpectTrue("Circular observation matches its bound missile",
               MatchesMissileLifecycle(
                   ZDSpellType::Circular, true, 77u, 77u));
    ExpectTrue("Circular delete ignores a different missile",
               !MatchesMissileLifecycle(
                   ZDSpellType::Circular, true, 77u, 78u));
    const Vec2 staticCircleCenter(900.0f, 100.0f);
    const Vec2 observedCircleCenter(450.0f, 100.0f);
    ExpectNear("moving Circular terminates at observed projectile x",
               TerminatedProjectileEnd(
                   ZDSpellType::Circular,
                   true,
                   staticCircleCenter,
                   observedCircleCenter).x,
               450.0f);
    ExpectNear("cast-only Circular retains authored impact center",
               TerminatedProjectileEnd(
                   ZDSpellType::Circular,
                   false,
                   staticCircleCenter,
                   observedCircleCenter).x,
               900.0f);
    SpellData missingExplosion =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    missingExplosion.hasEndExplosion = true;
    missingExplosion.secondaryRadius = 280.0f;
    missingExplosion.extraDelay = 1000;
    missingExplosion.extraEndTime = 500;
    missingExplosion.endExplosionRequiresUnitCollision = true;
    Threat missingExplosionThreat =
        ZDEvadeTest::MakeThreat(missingExplosion);
    missingExplosionThreat.projectileTerminated = true;
    missingExplosionThreat.missingMissileTermination = true;
    missingExplosionThreat.projectileTerminationTick = 2000;
    ExpectTrue("missing callback preserves configured end explosion",
               missingExplosionThreat.HasEndExplosionArea());
    ExpectEq("missing callback explosion uses current termination tick",
             missingExplosionThreat.EndExplosionEndTick(),
             3500);
    SpellData missingWithoutExplosion = missingExplosion;
    missingWithoutExplosion.hasEndExplosion = false;
    Threat missingWithoutExplosionThreat =
        ZDEvadeTest::MakeThreat(missingWithoutExplosion);
    missingWithoutExplosionThreat.missingMissileTermination = true;
    ExpectTrue("missing callback does not invent non-explosion area",
               !missingWithoutExplosionThreat.HasEndExplosionArea());
    SpellData movingCircle =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    movingCircle.projectileSpeed = 1300.0f;
    movingCircle.hasEndExplosion = true;
    movingCircle.secondaryRadius = 300.0f;
    movingCircle.endExplosionDelay = 100;
    movingCircle.endExplosionDuration = 500;
    Threat movingCircleThreat = ZDEvadeTest::MakeThreat(movingCircle);
    movingCircleThreat.authoredEndPos = Vec2(1300.0f, 100.0f);
    movingCircleThreat.endPos = observedCircleCenter;
    movingCircleThreat.observedHead = observedCircleCenter;
    movingCircleThreat.observedTick = 1800;
    movingCircleThreat.missileBound = true;
    movingCircleThreat.projectileTerminated = true;
    movingCircleThreat.projectileTerminationTick = 2000;
    ExpectNear("terminated Circular explosion uses last observed center",
               movingCircleThreat.EndExplosionCenter().x,
               450.0f);
    ExpectTrue("terminated Circular preserves configured persistence",
               movingCircleThreat.HasEndExplosionArea() &&
               !movingCircleThreat.IsExpiredAt(2500));
    movingCircleThreat.endTick = 2600;
    ExpectTrue("terminated Circular eventually expires",
               movingCircleThreat.IsExpiredAt(2851));
    Threat saturatedTermination = movingCircleThreat;
    saturatedTermination.endTick = INT_MAX;
    saturatedTermination.projectileTerminationTick = INT_MAX - 1000;
    ExpectTrue("terminated nonpersistent threat cannot remain immortal",
               saturatedTermination.IsExpiredAt(INT_MAX));
    ExpectTrue("Circular terminal persistence survives without explosion",
               RetainProjectileTermination(
                   ZDSpellType::Circular, 400, false));
    ExpectTrue("Line extra time does not invent a terminal area",
               !RetainProjectileTermination(
                   ZDSpellType::Line, 400, false));

    const CastCasterResolutionInput validIdOnlyEnemy = {
        false, 0u, 0u, 42u, true, 42u, 200u, 7u, 100u
    };
    ExpectEq("valid ID-only enemy resolves for detector processing",
             static_cast<int>(DecideCastCasterResolution(validIdOnlyEnemy)),
             static_cast<int>(
                 CastCasterResolutionDecision::UseResolvedNetworkId));
    CastCasterResolutionInput unresolvedIdOnly = validIdOnlyEnemy;
    unresolvedIdOnly.resolvedValid = false;
    ExpectEq("unresolved ID-only cast fails closed",
             static_cast<int>(DecideCastCasterResolution(unresolvedIdOnly)),
             static_cast<int>(CastCasterResolutionDecision::Reject));
    CastCasterResolutionInput wrongTeamIdOnly = validIdOnlyEnemy;
    wrongTeamIdOnly.resolvedTeam = wrongTeamIdOnly.playerTeam;
    ExpectEq("same-team ID-only cast fails closed",
             static_cast<int>(DecideCastCasterResolution(wrongTeamIdOnly)),
             static_cast<int>(CastCasterResolutionDecision::Reject));
    CastCasterResolutionInput unknownTeamIdOnly = validIdOnlyEnemy;
    unknownTeamIdOnly.resolvedTeam = 0;
    ExpectEq("unknown-team ID-only cast fails closed",
             static_cast<int>(DecideCastCasterResolution(unknownTeamIdOnly)),
             static_cast<int>(CastCasterResolutionDecision::Reject));

    SpellData composedLifecycle =
        ZDEvadeTest::MakeSpell(ZDSpellType::Circular);
    composedLifecycle.projectileSpeed = 0.0f;
    composedLifecycle.spellDelay = 0;
    composedLifecycle.extraEndTime = 0;
    composedLifecycle.hasEndExplosion = true;
    composedLifecycle.endExplosionDelay = INT_MAX;
    composedLifecycle.endExplosionDuration = INT_MAX;
    ExpectEq("detector explosion delay and duration compose saturating",
             ThreatLifecycleLinger(
                 composedLifecycle.extraEndTime,
                 composedLifecycle.hasEndExplosion,
                 composedLifecycle.endExplosionDelay,
                 composedLifecycle.endExplosionDuration),
             INT_MAX);
    ExpectEq("detector end tick clamps composed explosion lifetime",
             CalculateThreatEndTick(
                 composedLifecycle,
                 Vec2(),
                 Vec2(),
                 0,
                 0),
             INT_MAX);

    SpellData delayedLifecycle = composedLifecycle;
    delayedLifecycle.hasEndExplosion = false;
    delayedLifecycle.spellDelay = INT_MAX;
    ExpectEq("detector end tick clamps start plus large delay",
             CalculateThreatEndTick(
                 delayedLifecycle,
                 Vec2(),
                 Vec2(),
                 100,
                 0),
             INT_MAX);

    SpellData hweiCastOnly = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    hweiCastOnly.spellName = "HweiR";
    hweiCastOnly.range = 1400.0f;
    hweiCastOnly.projectileSpeed = 1400.0f;
    hweiCastOnly.spellDelay = 250;
    hweiCastOnly.extraEndTime = 0;
    hweiCastOnly.hasEndExplosion = true;
    hweiCastOnly.secondaryRadius = 500.0f;
    hweiCastOnly.extraDelay = 3000;
    Threat hweiThreat = ZDEvadeTest::MakeThreat(hweiCastOnly);
    hweiThreat.endTick = CalculateThreatEndTick(
        hweiCastOnly,
        hweiThreat.startPos,
        hweiThreat.endPos,
        hweiThreat.startTick,
        0);
    ExpectEq("cast-only Hwei fallback delay extends detector end tick",
             hweiThreat.endTick,
             5250);
    ExpectTrue("cast-only Hwei survives through delayed explosion",
               !hweiThreat.IsExpiredAt(5250));

    SpellData sennaCastOnly = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    sennaCastOnly.spellName = "SennaW";
    sennaCastOnly.range = 1200.0f;
    sennaCastOnly.projectileSpeed = 1200.0f;
    sennaCastOnly.spellDelay = 250;
    sennaCastOnly.extraEndTime = 500;
    sennaCastOnly.hasEndExplosion = true;
    sennaCastOnly.secondaryRadius = 280.0f;
    sennaCastOnly.extraDelay = 1000;
    Threat sennaThreat = ZDEvadeTest::MakeThreat(sennaCastOnly);
    sennaThreat.endTick = CalculateThreatEndTick(
        sennaCastOnly,
        sennaThreat.startPos,
        sennaThreat.endPos,
        sennaThreat.startTick,
        0);
    ExpectEq("cast-only Senna fallback delay and duration compose",
             sennaThreat.endTick,
             3750);
    ExpectTrue("cast-only Senna survives through delayed explosion",
               !sennaThreat.IsExpiredAt(3750));

    ThreatDatabase::Initialize();
    const auto matchPendingVeigarName =
        [](const PendingSpellNameEvent& event) {
            ProcessSpellMatchInput input;
            input.authoritativeNames[0] = event.spellName;
            input.spellSlotName = "VeigarBalefulStrike";
            input.spellNameFromSlotFallback =
                event.spellNameFromSlotFallback;
            input.isAutoAttack = event.isAutoAttack;
            input.slot = 0;
            input.targetNetworkId = event.targetNetworkId;
            input.hasUsableCastGeometry = true;
            return MatchProcessSpellDatabaseFirst(
                input,
                [](const char* name) {
                    return ThreatDatabase::FindAny(name, "Veigar");
                });
        };
    const PendingSpellNameEvent veigarFallbackName =
        MakePendingSpellNameEvent(
            "VeigarBalefulStrike", true, true, 1001u);
    const PendingSpellNameEvent veigarAuthoritativeName =
        MakePendingSpellNameEvent("VeigarQ", false);
    const ProcessSpellMatchResult veigarFallbackNameMatch =
        matchPendingVeigarName(veigarFallbackName);
    const ProcessSpellMatchResult veigarAuthoritativeNameMatch =
        matchPendingVeigarName(veigarAuthoritativeName);
    ExpectTrue("Veigar slot-fallback callback is rejected individually",
               veigarFallbackNameMatch.data == nullptr &&
                   veigarFallbackNameMatch.disposition ==
                       ProcessSpellMatchDisposition::RejectAutoAttack);
    ExpectTrue("Veigar authoritative callback is accepted individually",
               veigarAuthoritativeNameMatch.data != nullptr &&
                   veigarAuthoritativeNameMatch.data->spellName ==
                       "VeigarBalefulStrike");

    PendingSpellNameEvent fallbackThenAuthoritative =
        veigarFallbackName;
    MergePendingSpellNameEvent(
        fallbackThenAuthoritative, veigarAuthoritativeName);
    ExpectTrue("authoritative shorter VeigarQ replaces longer fallback",
               std::strcmp(
                   fallbackThenAuthoritative.spellName,
                   "VeigarQ") == 0 &&
                   !fallbackThenAuthoritative
                        .spellNameFromSlotFallback);
    ExpectTrue("fallback flags remain merged into authoritative name",
               fallbackThenAuthoritative.isAutoAttack &&
                   fallbackThenAuthoritative.targetNetworkId == 1001u);
    const ProcessSpellMatchResult fallbackThenAuthoritativeMatch =
        matchPendingVeigarName(fallbackThenAuthoritative);
    ExpectTrue("fallback-first merged Veigar callback is accepted once",
               fallbackThenAuthoritativeMatch.data != nullptr &&
                   fallbackThenAuthoritativeMatch.data->spellName ==
                       "VeigarBalefulStrike");

    PendingSpellNameEvent authoritativeThenFallback =
        veigarAuthoritativeName;
    MergePendingSpellNameEvent(
        authoritativeThenFallback, veigarFallbackName);
    ExpectTrue("longer fallback cannot replace authoritative VeigarQ",
               std::strcmp(
                   authoritativeThenFallback.spellName,
                   "VeigarQ") == 0 &&
                   !authoritativeThenFallback
                        .spellNameFromSlotFallback);
    ExpectTrue("reverse-order fallback flags remain merged",
               authoritativeThenFallback.isAutoAttack &&
                   authoritativeThenFallback.targetNetworkId == 1001u);
    const ProcessSpellMatchResult authoritativeThenFallbackMatch =
        matchPendingVeigarName(authoritativeThenFallback);
    ExpectTrue("authoritative-first merged Veigar callback is accepted once",
               authoritativeThenFallbackMatch.data != nullptr &&
                   authoritativeThenFallbackMatch.data->spellName ==
                       "VeigarBalefulStrike");

    const CastEventKey veigarFallbackKey = MakeCastKey(
        42u, 0xBEEF, 0, 3000, "VeigarBalefulStrike",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    const CastEventKey veigarAuthoritativeKey = MakeCastKey(
        42u, 0xBEEF, 0, 3000, "VeigarQ",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    ExpectTrue("provenance merge preserves identity duplicate coalescing",
               SameLogicalCast(
                   veigarFallbackKey, veigarAuthoritativeKey));

    ExpectTrue("BasicAttack runtime cast names classify as attacks",
               IsExplicitBasicAttackCastName("AhriBasicAttack"));
    ExpectTrue("BasicAttackMissile runtime names classify as attacks",
               IsExplicitBasicAttackCastName(
                   "NautilusBasicAttackMissile"));
    ExpectTrue("CritAttack runtime missile names classify as attacks",
               IsExplicitBasicAttackCastName("EzrealCritAttack"));
    const auto matchEzrealMissile = [](const MissileMatchInput& input) {
        return MatchMissileDatabaseFirst(
            input,
            [](const char* name) {
                return ThreatDatabase::FindMissile(name, "Ezreal");
            });
    };
    MissileMatchInput targetedEzrealQ;
    targetedEzrealQ.names[0] = "EzrealQ";
    targetedEzrealQ.eventTargetNetworkId = 1001u;
    targetedEzrealQ.eventTargetIndex = 1001u;
    targetedEzrealQ.runtimeTargetNetworkId = 1001u;
    const MissileMatchResult targetedEzrealQMatch =
        matchEzrealMissile(targetedEzrealQ);
    ExpectTrue("exact configured missile survives noisy target metadata",
               targetedEzrealQMatch.data != nullptr &&
                   targetedEzrealQMatch.data->spellName == "EzrealQ" &&
                   targetedEzrealQMatch.disposition ==
                       MissileMatchDisposition::Matched);

    MissileMatchInput targetedBasicMissile;
    targetedBasicMissile.names[0] = "EzrealBasicAttackMissile";
    targetedBasicMissile.eventTargetNetworkId = 1001u;
    const MissileMatchResult targetedBasicMissileMatch =
        matchEzrealMissile(targetedBasicMissile);
    ExpectTrue("targeted BasicAttack missile remains rejected",
               targetedBasicMissileMatch.data == nullptr &&
                   targetedBasicMissileMatch.disposition ==
                       MissileMatchDisposition::RejectBasicAttack);

    MissileMatchInput orbwalkerAttackMissile = targetedEzrealQ;
    orbwalkerAttackMissile.hasRuntimeAttackSignature = true;
    const MissileMatchResult orbwalkerAttackMissileMatch =
        matchEzrealMissile(orbwalkerAttackMissile);
    ExpectTrue("SDK orbwalker missile attack evidence rejects before DB match",
               orbwalkerAttackMissileMatch.data == nullptr &&
                   orbwalkerAttackMissileMatch.disposition ==
                       MissileMatchDisposition::RejectBasicAttack);

    MissileMatchInput targetedUnmatchedMissile;
    targetedUnmatchedMissile.names[0] = "EzrealUnknownMissile";
    targetedUnmatchedMissile.eventTargetIndex = 1001u;
    const MissileMatchResult targetedUnmatchedMissileMatch =
        matchEzrealMissile(targetedUnmatchedMissile);
    ExpectTrue("targeted unmatched missile creates no threat",
               targetedUnmatchedMissileMatch.data == nullptr &&
                   targetedUnmatchedMissileMatch.disposition ==
                       MissileMatchDisposition::RejectTargeted);

    struct ChampionProcessFixture {
        const char* champion;
        const char* qName;
        const char* attackName;
        std::size_t attackField;
        bool stalePrimaryQ;
        std::size_t positiveField;
        const char* alternateAuthoritativeName;
        std::size_t alternateAuthoritativeField;
    };
    const std::array<ChampionProcessFixture, 4> championProcessFixtures = {{
        {"Ahri", "AhriQ", "AhriBasicAttack", 1u, true, 1u,
         "AhriQ", 0u},
        {"Nautilus", "NautilusAnchorDrag", "NautilusBasicAttackMissile",
         0u, false, 2u, "NautilusAnchorDrag", 1u},
        {"Ezreal", "EzrealQ", "EzrealCritAttack", 3u, true, 1u,
         "EzrealQ", 2u},
        {"Veigar", "VeigarBalefulStrike", "VeigarBasicAttack",
         2u, true, 2u, "VeigarBalefulStrikeMis", 3u},
    }};
    for (const ChampionProcessFixture& fixture : championProcessFixtures) {
        const auto matchFixture =
            [&](const ProcessSpellMatchInput& input) {
                return MatchProcessSpellDatabaseFirst(
                    input,
                    [&](const char* name) {
                        return ThreatDatabase::FindAny(
                            name, fixture.champion);
                    });
            };

        ProcessSpellMatchInput falseAttack;
        falseAttack.spellSlotName = fixture.qName;
        falseAttack.authoritativeNames[fixture.attackField] =
            fixture.attackName;
        if (fixture.stalePrimaryQ) {
            falseAttack.authoritativeNames[0] = fixture.qName;
            falseAttack.spellNameFromSlotFallback = true;
        }
        falseAttack.isAutoAttack = true;
        falseAttack.slot = 0;
        falseAttack.targetNetworkId = fixture.attackField % 2u == 0u
            ? 1001u
            : 0u;
        falseAttack.targetIndex = fixture.attackField % 2u == 0u
            ? -1
            : 1001;
        falseAttack.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult falseAttackMatch =
            matchFixture(falseAttack);
        ExpectTrue("targeted auto attack with stale Q fallback is rejected",
                   falseAttackMatch.data == nullptr &&
                       falseAttackMatch.disposition ==
                           ProcessSpellMatchDisposition::RejectBasicAttack);
        ExpectTrue("rejected targeted auto attack creates no cast threat",
                   !CreatesCastOriginThreat(falseAttackMatch));

        PendingCastPriorityInput falseAttackPriority;
        falseAttackPriority.noise =
            IsRejectedProcessSpellMatch(falseAttackMatch);
        falseAttackPriority.casterNetworkId = 42u;
        falseAttackPriority.playerNetworkId = 7u;
        falseAttackPriority.databaseMatched =
            falseAttackMatch.data != nullptr;
        ExpectEq("rejected targeted auto attack has no hostile priority",
                 static_cast<int>(
                     ClassifyPendingCastPriority(falseAttackPriority)),
                 static_cast<int>(PendingPriority::Noise));

        ProcessSpellMatchInput slotDerivedAuto;
        slotDerivedAuto.authoritativeNames[0] = fixture.qName;
        slotDerivedAuto.spellSlotName = fixture.qName;
        slotDerivedAuto.spellNameFromSlotFallback = true;
        slotDerivedAuto.isAutoAttack = true;
        slotDerivedAuto.slot = 0;
        slotDerivedAuto.targetNetworkId = 1001u;
        slotDerivedAuto.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult slotDerivedAutoMatch =
            matchFixture(slotDerivedAuto);
        ExpectTrue("slot-derived Q rejects noisy auto without attack alias",
                   slotDerivedAutoMatch.data == nullptr &&
                       slotDerivedAutoMatch.disposition ==
                           ProcessSpellMatchDisposition::RejectAutoAttack);

        ProcessSpellMatchInput realQWithStaleAttack;
        realQWithStaleAttack.authoritativeNames[0] = fixture.qName;
        realQWithStaleAttack.authoritativeNames[
            fixture.attackField == 0u ? 1u : fixture.attackField] =
                fixture.attackName;
        realQWithStaleAttack.spellSlotName = fixture.qName;
        realQWithStaleAttack.isAutoAttack = true;
        realQWithStaleAttack.slot = 0;
        realQWithStaleAttack.targetNetworkId = 1001u;
        realQWithStaleAttack.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult realQWithStaleAttackMatch =
            matchFixture(realQWithStaleAttack);
        ExpectTrue("true canonical Q survives stale explicit attack alias",
                   realQWithStaleAttackMatch.data != nullptr &&
                       realQWithStaleAttackMatch.data->spellName ==
                           fixture.qName &&
                       CreatesCastOriginThreat(
                           realQWithStaleAttackMatch));

        ProcessSpellMatchInput authoritativeQWithStaleAttack;
        authoritativeQWithStaleAttack.authoritativeNames[
            fixture.alternateAuthoritativeField] =
                fixture.alternateAuthoritativeName;
        authoritativeQWithStaleAttack.authoritativeNames[
            fixture.attackField] = fixture.attackName;
        authoritativeQWithStaleAttack.spellSlotName = fixture.qName;
        authoritativeQWithStaleAttack.isAutoAttack = true;
        authoritativeQWithStaleAttack.slot = 0;
        authoritativeQWithStaleAttack.targetNetworkId = 1001u;
        authoritativeQWithStaleAttack.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult authoritativeQWithStaleAttackMatch =
            matchFixture(authoritativeQWithStaleAttack);
        ExpectTrue("authoritative raw field wins over stale attack alias",
                   authoritativeQWithStaleAttackMatch.data != nullptr &&
                       authoritativeQWithStaleAttackMatch.data->spellName ==
                           fixture.qName &&
                       CreatesCastOriginThreat(
                           authoritativeQWithStaleAttackMatch));
        ExpectTrue("coalescing slot-derived then canonical Q keeps authority",
                   ShouldSelectIncomingSpellName(
                       fixture.qName, true, fixture.qName, false));
        ExpectTrue("coalescing canonical then slot-derived Q keeps authority",
                   !ShouldSelectIncomingSpellName(
                       fixture.qName, false, fixture.qName, true));
        ExpectTrue("coalescing two slot-derived Q names preserves fallback",
                   !ShouldSelectIncomingSpellName(
                       fixture.qName, true, fixture.qName, true));

        ProcessSpellMatchInput realQ;
        realQ.authoritativeNames[fixture.positiveField] = fixture.qName;
        realQ.spellSlotName = fixture.qName;
        realQ.isAutoAttack = true;
        realQ.slot = 0;
        realQ.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult realQMatch = matchFixture(realQ);
        ExpectTrue("authoritative real Q survives noisy auto flag",
                   realQMatch.data != nullptr &&
                       realQMatch.data->spellName == fixture.qName &&
                       CreatesCastOriginThreat(realQMatch));

        ProcessSpellMatchInput targetedCanonicalQ;
        targetedCanonicalQ.authoritativeNames[0] = fixture.qName;
        targetedCanonicalQ.spellSlotName = fixture.qName;
        targetedCanonicalQ.isAutoAttack = true;
        targetedCanonicalQ.slot = 0;
        targetedCanonicalQ.targetNetworkId = 1001u;
        targetedCanonicalQ.hasUsableCastGeometry = true;
        const ProcessSpellMatchResult targetedCanonicalQMatch =
            matchFixture(targetedCanonicalQ);
        ExpectTrue("canonical primary survives bare noisy auto and target metadata",
                   targetedCanonicalQMatch.data != nullptr &&
                       targetedCanonicalQMatch.data->spellName ==
                           fixture.qName &&
                       CreatesCastOriginThreat(targetedCanonicalQMatch));
        PendingCastPriorityInput targetedCanonicalPriority;
        targetedCanonicalPriority.noise =
            IsRejectedProcessSpellMatch(targetedCanonicalQMatch);
        targetedCanonicalPriority.casterNetworkId = 42u;
        targetedCanonicalPriority.playerNetworkId = 7u;
        targetedCanonicalPriority.databaseMatched =
            targetedCanonicalQMatch.data != nullptr;
        ExpectEq("pending priority mirrors accepted canonical primary match",
                 static_cast<int>(ClassifyPendingCastPriority(
                     targetedCanonicalPriority)),
                 static_cast<int>(PendingPriority::HostileCast));
    }

    const auto matchVeigarProcess = [](const ProcessSpellMatchInput& input) {
        return MatchProcessSpellDatabaseFirst(
            input,
            [](const char* name) {
                return ThreatDatabase::FindAny(name, "Veigar");
            });
    };
    ProcessSpellMatchInput autoFlaggedVeigar;
    autoFlaggedVeigar.authoritativeNames[0] = "VeigarBalefulStrike";
    autoFlaggedVeigar.isAutoAttack = true;
    const ProcessSpellMatchResult autoFlaggedVeigarMatch =
        matchVeigarProcess(autoFlaggedVeigar);
    ExpectTrue("auto-flagged canonical Veigar Q creates cast threat before missile",
               autoFlaggedVeigarMatch.data != nullptr &&
                   autoFlaggedVeigarMatch.data->spellName ==
                       "VeigarBalefulStrike" &&
                   CreatesCastOriginThreat(autoFlaggedVeigarMatch));

    const CastEventKey autoFlaggedHook = MakeCastKey(
        42u, 0xBEEF, 0, 3000, "VeigarBalefulStrike",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    const CastEventKey normalHook = MakeCastKey(
        42u, 0xBEEF, 0, 3000, "VeigarBalefulStrike",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    ExpectTrue("auto-flagged and normal paired callbacks coalesce",
               SameLogicalCast(autoFlaggedHook, normalHook));
    ExpectTrue("auto flag on one paired callback does not poison canonical cast",
               matchVeigarProcess(autoFlaggedVeigar).data != nullptr);

    ProcessSpellMatchInput slotOnlyVeigar;
    slotOnlyVeigar.spellSlotName = "VeigarBalefulStrike";
    slotOnlyVeigar.slot = 0;
    slotOnlyVeigar.hasUsableCastGeometry = true;
    const ProcessSpellMatchResult slotOnlyVeigarMatch =
        matchVeigarProcess(slotOnlyVeigar);
    ExpectTrue("guarded SpellSlotName-only canonical Veigar Q matches",
               slotOnlyVeigarMatch.data != nullptr &&
                   slotOnlyVeigarMatch.data->spellName ==
                       "VeigarBalefulStrike");
    ProcessSpellMatchInput degenerateSlotOnlyVeigar = slotOnlyVeigar;
    degenerateSlotOnlyVeigar.hasUsableCastGeometry = false;
    ExpectTrue("SpellSlotName fallback rejects degenerate cast geometry",
               matchVeigarProcess(degenerateSlotOnlyVeigar).data == nullptr);
    ProcessSpellMatchInput wrongSlotVeigar = slotOnlyVeigar;
    wrongSlotVeigar.slot = 1;
    ExpectTrue("SpellSlotName fallback requires matching database spellKey",
               matchVeigarProcess(wrongSlotVeigar).data == nullptr);

    ProcessSpellMatchInput basicAttackOnly;
    basicAttackOnly.authoritativeNames[0] = "VeigarBasicAttack";
    basicAttackOnly.isAutoAttack = true;
    const ProcessSpellMatchResult basicAttackMatch =
        matchVeigarProcess(basicAttackOnly);
    ExpectTrue("actual BasicAttack names remain rejected",
               basicAttackMatch.data == nullptr &&
                   basicAttackMatch.disposition ==
                       ProcessSpellMatchDisposition::RejectBasicAttack);

    ProcessSpellMatchInput staleUtilityAndCanonical;
    staleUtilityAndCanonical.authoritativeNames[0] = "Item3153";
    staleUtilityAndCanonical.authoritativeNames[1] =
        "VeigarBalefulStrike";
    staleUtilityAndCanonical.authoritativeNames[2] = "SummonerFlash";
    const ProcessSpellMatchResult staleUtilityMatch =
        matchVeigarProcess(staleUtilityAndCanonical);
    ExpectTrue("utility or item stale field cannot poison valid DB name",
               staleUtilityMatch.data != nullptr &&
                   staleUtilityMatch.data->spellName ==
                       "VeigarBalefulStrike");

    ProcessSpellMatchInput staleBasicAndCanonical;
    staleBasicAndCanonical.authoritativeNames[0] = "VeigarBasicAttack";
    staleBasicAndCanonical.authoritativeNames[1] =
        "VeigarBalefulStrike";
    const ProcessSpellMatchResult staleBasicMatch =
        matchVeigarProcess(staleBasicAndCanonical);
    ExpectTrue("stale basic field cannot poison valid DB name",
               staleBasicMatch.data != nullptr &&
                   staleBasicMatch.data->spellName ==
                       "VeigarBalefulStrike");

    ProcessSpellMatchInput noProcessInput;
    noProcessInput.authoritativeNames[0] = "ViktorEAftershock";
    const ProcessSpellMatchResult noProcessMatch =
        MatchProcessSpellDatabaseFirst(
            noProcessInput,
            [](const char* name) {
                return ThreatDatabase::FindAny(name, "Viktor");
            });
    ExpectTrue("noProcess database entries remain excluded from cast origin",
               noProcessMatch.data != nullptr &&
                   noProcessMatch.data->noProcess &&
                   !CreatesCastOriginThreat(noProcessMatch));

    // Full raw-hook integration requires the live SDK process and cannot link
    // into this standalone executable. This store seam uses the same normalized
    // dedup, missile-bind eligibility, and cast-origin correction helpers as
    // ThreatDetector.
    std::vector<Threat> normalizedStore;
    Threat castOrigin =
        ZDEvadeTest::MakeThreat(*autoFlaggedVeigarMatch.data);
    castOrigin.id = 77;
    castOrigin.startTick = 4000;
    castOrigin.casterNetworkId = 42u;
    castOrigin.castIdentity = 0xBEEF;
    normalizedStore.push_back(castOrigin);

    Threat pairedCallback = castOrigin;
    pairedCallback.id = -1;
    pairedCallback.startTick = 4020;
    Threat* normalizedDuplicate =
        FindNormalizedCastDuplicate(normalizedStore, pairedCallback);
    ExpectTrue("paired callback finds normalized cast store entry",
               normalizedDuplicate != nullptr);
    if (normalizedDuplicate)
        MergeNormalizedCastDuplicate(*normalizedDuplicate, pairedCallback);

    Threat& sameFrameThreat = normalizedStore.front();
    const MissileBindKey sameFrameCastKey = {
        sameFrameThreat.casterNetworkId,
        sameFrameThreat.castIdentity,
        reinterpret_cast<std::uintptr_t>(sameFrameThreat.data),
        sameFrameThreat.direction,
        sameFrameThreat.startTick,
        sameFrameThreat.Delay(),
        sameFrameThreat.Delay()
    };
    const MissileBindObservation sameFrameMissile = {
        42u,
        0xBEEF,
        reinterpret_cast<std::uintptr_t>(sameFrameThreat.data),
        sameFrameThreat.direction,
        4030
    };
    ExpectTrue("same-frame missile binds normalized cast store entry",
               CanBindMissile(sameFrameCastKey, sameFrameMissile));
    CorrectExistingThreatFromMissile(
        sameFrameThreat,
        [](Threat& corrected) {
            corrected.id = 999;
            corrected.startTick = 4025;
            corrected.startPos = Vec2(110.0f, 100.0f);
            corrected.endPos = Vec2(1110.0f, 100.0f);
        });
    ExpectEq("ProcessSpell then same-frame missile keeps one threat",
             static_cast<int>(normalizedStore.size()), 1);
    ExpectEq("same-frame missile preserves cast threat ID",
             sameFrameThreat.id, 77);
    ExpectEq("same-frame missile preserves cast startTick",
             sameFrameThreat.startTick, 4000);
    ExpectTrue("same-frame missile marks existing threat bound",
               sameFrameThreat.missileBound);

    const SpellData* fizzR =
        ThreatDatabase::FindAny("FizzRMissile", "Fizz");
    ExpectTrue("FizzR database entry is a moving Circular missile",
               fizzR != nullptr &&
               fizzR->spellName == "FizzR" &&
               fizzR->spellType == ZDSpellType::Circular &&
               fizzR->projectileSpeed > 1.0f);
    int movingCircularEntries = 0;
    bool allMovingCircularEntriesUseLifecycle = true;
    for (const SpellData& entry : SpellDatabase::Spells) {
        const bool hasMissileName =
            !entry.missileName.empty() ||
            !entry.extraMissileNames.empty();
        if (entry.spellType != ZDSpellType::Circular ||
            !hasMissileName ||
            entry.projectileSpeed <= 1.0f)
            continue;
        ++movingCircularEntries;
        allMovingCircularEntriesUseLifecycle =
            allMovingCircularEntriesUseLifecycle &&
            UsesMissileLifecycle(entry.spellType, true);
    }
    ExpectTrue("database contains multiple moving Circular entries",
               movingCircularEntries > 1);
    ExpectTrue("all moving Circular database entries use missile lifecycle",
               allMovingCircularEntriesUseLifecycle);
    const SpellData* casingMatch =
        ThreatDatabase::FindAny("hweiqq", "hWeI");
    ExpectTrue("database champion and spell matching is case-insensitive",
               casingMatch != nullptr &&
                   casingMatch->spellName == "HweiQQ");
    const SpellData* emptyChampionMatch =
        ThreatDatabase::FindAny("HweiQQ", "");
    ExpectTrue("empty runtime champion permits spell-name lookup",
               emptyChampionMatch != nullptr &&
                   emptyChampionMatch->charName == "Hwei");
    ExpectTrue("known wrong champion still rejects spell-name lookup",
               ThreatDatabase::FindAny("HweiQQ", "Senna") == nullptr);
    ExpectTrue("removed Aatrox self-dash is absent from raw spell database",
               std::none_of(
                   SpellDatabase::Spells.begin(),
                   SpellDatabase::Spells.end(),
                   [](const SpellData& spell) {
                       return spell.spellName == "AatroxE";
                   }));
    ExpectTrue("removed Karma activation is absent from raw spell database",
               std::none_of(
                   SpellDatabase::Spells.begin(),
                   SpellDatabase::Spells.end(),
                   [](const SpellData& spell) {
                       return spell.spellName == "KarmaMantra";
                   }));
    ExpectTrue("ThreatDatabase rejects removed Aatrox self-dash",
               ThreatDatabase::FindAny("AatroxE", "Aatrox") == nullptr &&
               ThreatDatabase::FindCast("AatroxE", "Aatrox") == nullptr);
    ExpectTrue("ThreatDatabase rejects removed Karma activation",
               ThreatDatabase::FindAny("KarmaMantra", "Karma") == nullptr &&
               ThreatDatabase::FindCast("KarmaMantra", "Karma") == nullptr &&
               ThreatDatabase::FindMissile(
                   "KarmaMantra", "Karma") == nullptr);
    ExpectTrue("neighboring Aatrox threat remains available",
               ThreatDatabase::FindAny("AatroxW", "Aatrox") != nullptr);
    ExpectTrue("neighboring Karma threat remains available",
               ThreatDatabase::FindAny("KarmaQHeavy", "Karma") != nullptr);

    const PendingEventDescriptor processPending = {
        processSpell, PendingPriority::HostileCast
    };
    const PendingEventDescriptor doCastPending = {
        doCast, PendingPriority::HostileCast
    };
    const std::array<PendingEventDescriptor, 1> pairedPending = {
        processPending
    };
    const PendingQueueChoice pairedChoice = ChoosePendingQueueAction(
        pairedPending.data(), pairedPending.size(), pairedPending.size(),
        doCastPending);
    ExpectEq("paired ProcessSpell and DoCast coalesce before capacity",
             static_cast<int>(pairedChoice.decision),
             static_cast<int>(PendingQueueDecision::Coalesce));
    ExpectEq("paired event coalesces into existing logical slot",
             static_cast<int>(pairedChoice.index),
             0);
    const PendingEventDescriptor rejectedAttackPending = {
        doCast, PendingPriority::Noise
    };
    const PendingQueueChoice rejectedAttackChoice =
        ChoosePendingQueueAction(
            pairedPending.data(), pairedPending.size(), pairedPending.size(),
            rejectedAttackPending);
    ExpectEq("rejected attack cannot coalesce into protected hostile cast",
             static_cast<int>(rejectedAttackChoice.decision),
             static_cast<int>(PendingQueueDecision::DropLowerPriority));

    ExpectTrue("richer canonical name replaces sparse hook name",
               ShouldPreferCastName("Ezreal", "EzrealMysticShot"));
    ExpectTrue("sparse hook name cannot replace richer canonical name",
               !ShouldPreferCastName("EzrealMysticShot", "Ezreal"));
    const Vec2 preservedStart = PreferredCastPosition(
        Vec2(100.0f, 200.0f), Vec2());
    const Vec2 enrichedEnd = PreferredCastPosition(
        Vec2(), Vec2(900.0f, 200.0f));
    ExpectNear("coalescing preserves existing usable start x",
               preservedStart.x, 100.0f);
    ExpectNear("coalescing preserves richer incoming end x",
               enrichedEnd.x, 900.0f);

    ExpectEq("matched structurally hostile cast receives protection",
             static_cast<int>(ClassifyPendingPriority(false, true, true)),
             static_cast<int>(PendingPriority::HostileCast));
    ExpectEq("unmatched hostile cast is not protected",
             static_cast<int>(ClassifyPendingPriority(false, true, false)),
             static_cast<int>(PendingPriority::StructuralCast));
    ExpectEq("matched ally cast is not protected",
             static_cast<int>(ClassifyPendingPriority(false, false, true)),
             static_cast<int>(PendingPriority::StructuralCast));
    ExpectEq("noise remains noise despite hostile match",
             static_cast<int>(ClassifyPendingPriority(true, true, true)),
             static_cast<int>(PendingPriority::Noise));

    PendingCastPriorityInput matchedIdOnly;
    matchedIdOnly.casterNetworkId = 42u;
    matchedIdOnly.playerNetworkId = 7u;
    matchedIdOnly.playerTeam = 100u;
    matchedIdOnly.databaseMatched = true;
    ExpectEq("matched ID-only cast is protected before resolution",
             static_cast<int>(ClassifyPendingCastPriority(matchedIdOnly)),
             static_cast<int>(PendingPriority::HostileCast));
    PendingCastPriorityInput unmatchedIdOnly = matchedIdOnly;
    unmatchedIdOnly.databaseMatched = false;
    ExpectEq("unmatched ID-only cast remains unprotected",
             static_cast<int>(ClassifyPendingCastPriority(unmatchedIdOnly)),
             static_cast<int>(PendingPriority::StructuralCast));
    PendingCastPriorityInput localIdOnly = matchedIdOnly;
    localIdOnly.casterNetworkId = localIdOnly.playerNetworkId;
    ExpectEq("local ID-only cast remains unprotected",
             static_cast<int>(ClassifyPendingCastPriority(localIdOnly)),
             static_cast<int>(PendingPriority::StructuralCast));
    PendingCastPriorityInput knownAlly = matchedIdOnly;
    knownAlly.senderValid = true;
    knownAlly.senderNetworkId = 50u;
    knownAlly.senderTeam = knownAlly.playerTeam;
    ExpectEq("known ally database match remains unprotected",
             static_cast<int>(ClassifyPendingCastPriority(knownAlly)),
             static_cast<int>(PendingPriority::StructuralCast));
    PendingCastPriorityInput knownEnemy = knownAlly;
    knownEnemy.senderTeam = 200u;
    ExpectEq("known enemy database match remains protected",
             static_cast<int>(ClassifyPendingCastPriority(knownEnemy)),
             static_cast<int>(PendingPriority::HostileCast));

    const std::array<PendingEventDescriptor, 3> pressured = {{
        {{7u, 0x700u, 0, 2000}, PendingPriority::HostileCast},
        {{8u, 0u, 64, 2001}, PendingPriority::Noise},
        {{9u, 0u, 1, 2002}, PendingPriority::StructuralCast},
    }};
    const PendingEventDescriptor incomingNoise = {
        {10u, 0u, 64, 2003}, PendingPriority::Noise
    };
    const PendingQueueChoice noiseChoice = ChoosePendingQueueAction(
        pressured.data(), pressured.size(), pressured.size(), incomingNoise);
    ExpectEq("capacity pressure drops equal-priority incoming noise",
             static_cast<int>(noiseChoice.decision),
             static_cast<int>(PendingQueueDecision::DropCapacityAllProtected));

    const std::array<PendingEventDescriptor, 3> protectedHostiles = {{
        {{7u, 0x700u, 0, 2000}, PendingPriority::HostileCast},
        {{8u, 0x800u, 1, 2001}, PendingPriority::HostileCast},
        {{9u, 0x900u, 2, 2002}, PendingPriority::HostileCast},
    }};
    const PendingQueueChoice droppedNoise = ChoosePendingQueueAction(
        protectedHostiles.data(), protectedHostiles.size(),
        protectedHostiles.size(), incomingNoise);
    ExpectEq("lower-priority noise is dropped when all entries are protected",
             static_cast<int>(droppedNoise.decision),
             static_cast<int>(PendingQueueDecision::DropLowerPriority));

    const std::array<PendingEventDescriptor, 3> replaceable = {{
        {{7u, 0u, 0, 2000}, PendingPriority::Noise},
        {{8u, 0u, 1, 2001}, PendingPriority::Noise},
        {{9u, 0u, 2, 2002}, PendingPriority::StructuralCast},
    }};
    const PendingEventDescriptor incomingHostile = {
        {10u, 0xA00u, 3, 2003}, PendingPriority::HostileCast
    };
    const PendingQueueChoice replacement = ChoosePendingQueueAction(
        replaceable.data(), replaceable.size(), replaceable.size(),
        incomingHostile);
    ExpectEq("higher-priority cast records lower-priority replacement reason",
             static_cast<int>(replacement.decision),
             static_cast<int>(PendingQueueDecision::ReplaceLowerPriority));
    ExpectEq("replacement deterministically chooses oldest lowest priority",
             static_cast<int>(replacement.index),
             0);

    using Queue = PendingEventQueue<QueueEvent, 3>;
    Queue mergeQueue;
    QueueEvent sparse = MakeQueueEvent(1u, "Ezreal");
    sparse.start = Vec2(100.0f, 100.0f);
    QueueEvent rich = MakeQueueEvent(2u, "EzrealMysticShot");
    rich.end = Vec2(900.0f, 100.0f);
    PendingEventDescriptor sparseDescriptor;
    sparseDescriptor.key = mixedIdentityProcess;
    sparseDescriptor.priority = PendingPriority::HostileCast;
    PendingEventDescriptor richDescriptor;
    richDescriptor.key = mixedIdentityDoCast;
    richDescriptor.priority = PendingPriority::HostileCast;
    ExpectEq("real queue appends first paired hook",
             static_cast<int>(mergeQueue.Push(
                 sparse, sparseDescriptor, MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Append));
    ExpectEq("real queue coalesces mixed-identity paired hook",
             static_cast<int>(mergeQueue.Push(
                 rich, richDescriptor, MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Coalesce));
    ExpectEq("real queue count remains one after merge",
             static_cast<int>(mergeQueue.Size()), 1);
    ExpectEq("real queue records coalesce counter",
             mergeQueue.Counters().coalesced, 1);
    QueueEvent merged;
    ExpectTrue("real queue pops merged logical event", mergeQueue.Pop(merged));
    ExpectTrue("real merge preserves richer name",
               std::strcmp(merged.spellName, "EzrealMysticShot") == 0);
    ExpectNear("real merge preserves sparse start", merged.start.x, 100.0f);
    ExpectNear("real merge enriches missing end", merged.end.x, 900.0f);

    Queue protectedQueue;
    protectedQueue.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::HostileCast),
        MergeQueueEvent);
    protectedQueue.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::HostileCast),
        MergeQueueEvent);
    protectedQueue.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::HostileCast),
        MergeQueueEvent);
    const PendingQueueDecision protectedDecision = protectedQueue.Push(
        MakeQueueEvent(4u), MakeDescriptor(4u, PendingPriority::HostileCast),
        MergeQueueEvent);
    ExpectEq("all-hostile pressure protects incoming hostile in overflow",
             static_cast<int>(protectedDecision),
             static_cast<int>(PendingQueueDecision::AppendProtectedOverflow));
    ExpectEq("all-hostile pressure increments protected-overflow counter",
             protectedQueue.Counters().protectedOverflow, 1);
    ExpectEq("protected overflow contributes to queue size",
             static_cast<int>(protectedQueue.Size()), 4);
    QueueEvent ordered;
    protectedQueue.Pop(ordered);
    ExpectTrue("protected overflow preserves first FIFO entry", ordered.id == 1u);
    protectedQueue.Pop(ordered);
    ExpectTrue("protected overflow preserves second FIFO entry", ordered.id == 2u);
    protectedQueue.Pop(ordered);
    ExpectTrue("protected overflow preserves third FIFO entry", ordered.id == 3u);
    protectedQueue.Pop(ordered);
    ExpectTrue("protected overflow preserves fourth FIFO entry", ordered.id == 4u);

    Queue overflowCoalesce;
    overflowCoalesce.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowCoalesce.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowCoalesce.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowCoalesce.Push(
        MakeQueueEvent(4u, "Ezreal"),
        MakeDescriptor(4u, PendingPriority::HostileCast),
        MergeQueueEvent);
    ExpectEq("matching event coalesces against protected overflow",
             static_cast<int>(overflowCoalesce.Push(
                 MakeQueueEvent(40u, "EzrealMysticShot"),
                 MakeDescriptor(4u, PendingPriority::HostileCast),
                 MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Coalesce));
    ExpectEq("overflow coalesce does not grow queue",
             static_cast<int>(overflowCoalesce.Size()), 4);
    ExpectEq("overflow coalesce increments coalesced counter",
             overflowCoalesce.Counters().coalesced, 1);
    for (std::uint64_t expected = 1; expected <= 4; ++expected) {
        ExpectTrue("overflow coalesce queue remains poppable",
                   overflowCoalesce.Pop(ordered));
        ExpectTrue("overflow coalesce preserves global FIFO",
                   ordered.id == expected);
    }
    ExpectTrue("overflow coalesce enriches protected event",
               std::strcmp(ordered.spellName, "EzrealMysticShot") == 0);

    Queue headReplacement;
    headReplacement.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::Noise),
        MergeQueueEvent);
    headReplacement.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::StructuralCast),
        MergeQueueEvent);
    headReplacement.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::HostileCast),
        MergeQueueEvent);
    ExpectEq("ring head replacement selects strictly lower priority",
             static_cast<int>(headReplacement.Push(
                 MakeQueueEvent(4u),
                 MakeDescriptor(4u, PendingPriority::HostileCast),
                 MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::ReplaceLowerPriority));
    headReplacement.Pop(ordered);
    ExpectTrue("head replacement keeps retained FIFO first", ordered.id == 2u);
    headReplacement.Pop(ordered);
    ExpectTrue("head replacement keeps retained FIFO second", ordered.id == 3u);
    headReplacement.Pop(ordered);
    ExpectTrue("head replacement appends incoming at FIFO tail", ordered.id == 4u);

    Queue middleReplacement;
    middleReplacement.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::Noise),
        MergeQueueEvent);
    middleReplacement.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::StructuralCast),
        MergeQueueEvent);
    middleReplacement.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::Noise),
        MergeQueueEvent);
    middleReplacement.Pop(ordered);
    middleReplacement.Push(
        MakeQueueEvent(4u), MakeDescriptor(4u, PendingPriority::HostileCast),
        MergeQueueEvent);
    middleReplacement.Push(
        MakeQueueEvent(5u), MakeDescriptor(5u, PendingPriority::StructuralCast),
        MergeQueueEvent);
    middleReplacement.Pop(ordered);
    ExpectTrue("middle replacement keeps oldest retained entry", ordered.id == 2u);
    middleReplacement.Pop(ordered);
    ExpectTrue("middle replacement keeps wrapped retained entry", ordered.id == 4u);
    middleReplacement.Pop(ordered);
    ExpectTrue("middle replacement appends replacement at tail", ordered.id == 5u);
    ExpectEq("wrapped remove records replacement counter",
             middleReplacement.Counters().replacedLowerPriority, 1);

    Queue overflowWrap;
    overflowWrap.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowWrap.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowWrap.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowWrap.Push(
        MakeQueueEvent(4u), MakeDescriptor(4u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowWrap.Push(
        MakeQueueEvent(5u), MakeDescriptor(5u, PendingPriority::HostileCast),
        MergeQueueEvent);
    overflowWrap.Pop(ordered);
    ExpectTrue("overflow refill pops oldest fixed event", ordered.id == 1u);
    overflowWrap.Push(
        MakeQueueEvent(6u), MakeDescriptor(6u, PendingPriority::HostileCast),
        MergeQueueEvent);
    for (std::uint64_t expected = 2; expected <= 6; ++expected) {
        ExpectTrue("wrapped overflow queue remains poppable",
                   overflowWrap.Pop(ordered));
        ExpectTrue("wrapped overflow refill preserves global FIFO",
                   ordered.id == expected);
    }
    ExpectEq("wrapped overflow decisions are counted",
             overflowWrap.Counters().protectedOverflow, 3);

    Queue clearQueue;
    clearQueue.Push(
        MakeQueueEvent(1u), MakeDescriptor(1u, PendingPriority::HostileCast),
        MergeQueueEvent);
    clearQueue.Push(
        MakeQueueEvent(2u), MakeDescriptor(2u, PendingPriority::HostileCast),
        MergeQueueEvent);
    clearQueue.Push(
        MakeQueueEvent(3u), MakeDescriptor(3u, PendingPriority::HostileCast),
        MergeQueueEvent);
    clearQueue.Push(
        MakeQueueEvent(4u), MakeDescriptor(4u, PendingPriority::HostileCast),
        MergeQueueEvent);
    clearQueue.Push(
        MakeQueueEvent(9u), MakeDescriptor(9u, PendingPriority::Noise),
        MergeQueueEvent);
    ExpectEq("pre-clear protected overflow counter is populated",
             clearQueue.Counters().protectedOverflow, 1);
    ExpectEq("pre-clear lower-priority drop counter is populated",
             clearQueue.Counters().droppedLowerPriority, 1);
    clearQueue.Clear();
    ExpectEq("clear removes fixed and protected overflow entries",
             static_cast<int>(clearQueue.Size()), 0);
    ExpectEq("clear resets protected overflow counter",
             clearQueue.Counters().protectedOverflow, 0);
    ExpectEq("clear resets lower-priority drop counter",
             clearQueue.Counters().droppedLowerPriority, 0);
    ExpectEq("clear resets protected-overflow-limit drop counter",
             clearQueue.Counters().droppedProtectedOverflowLimit, 0);
    ExpectTrue("clear leaves queue empty", !clearQueue.Pop(ordered));
    ExpectEq("queue appends normally after clear",
             static_cast<int>(clearQueue.Push(
                 MakeQueueEvent(10u),
                 MakeDescriptor(10u, PendingPriority::HostileCast),
                 MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Append));

    using StressQueue = PendingEventQueue<QueueEvent, 256>;
    ExpectEq("runtime queue overflow budget is four fixed capacities",
             static_cast<int>(StressQueue::ProtectedOverflowCapacity()),
             1024);
    StressQueue stressQueue;
    constexpr std::uint64_t kStressCount = 1024;
    for (std::uint64_t id = 1; id <= kStressCount; ++id) {
        const PendingQueueDecision decision = stressQueue.Push(
            MakeQueueEvent(id),
            MakeDescriptor(id, PendingPriority::HostileCast),
            MergeQueueEvent);
        const PendingQueueDecision expected = id <= 256
            ? PendingQueueDecision::Append
            : PendingQueueDecision::AppendProtectedOverflow;
        ExpectEq("stress hostile enqueue uses fixed then protected overflow",
                 static_cast<int>(decision),
                 static_cast<int>(expected));
    }
    ExpectEq("stress queue size includes protected overflow",
             static_cast<int>(stressQueue.Size()),
             static_cast<int>(kStressCount));
    ExpectEq("stress queue counts every protected overflow decision",
             stressQueue.Counters().protectedOverflow,
             static_cast<int>(kStressCount - 256));
    for (std::uint64_t expected = 1; expected <= kStressCount; ++expected) {
        ExpectTrue("stress hostile queue remains poppable",
                   stressQueue.Pop(ordered));
        ExpectTrue("stress hostile queue preserves exact FIFO",
                   ordered.id == expected);
    }
    ExpectTrue("stress hostile queue drains completely",
               !stressQueue.Pop(ordered));
    ExpectEq("in-budget stress has no overflow-limit drops",
             stressQueue.Counters().droppedProtectedOverflowLimit, 0);

    StressQueue capacityBoundQueue;
    constexpr std::uint64_t kPhysicalBudget =
        256 + StressQueue::ProtectedOverflowCapacity();
    for (std::uint64_t id = 1; id <= kPhysicalBudget; ++id) {
        const PendingQueueDecision decision = capacityBoundQueue.Push(
            MakeQueueEvent(id),
            MakeDescriptor(id, PendingPriority::HostileCast),
            MergeQueueEvent);
        const PendingQueueDecision expected = id <= 256
            ? PendingQueueDecision::Append
            : PendingQueueDecision::AppendProtectedOverflow;
        ExpectEq("physical-budget hostile event is retained",
                 static_cast<int>(decision),
                 static_cast<int>(expected));
    }
    ExpectEq("queue reaches exact fixed plus overflow budget",
             static_cast<int>(capacityBoundQueue.Size()),
             static_cast<int>(kPhysicalBudget));
    ExpectEq("overflow budget counts protected decisions",
             capacityBoundQueue.Counters().protectedOverflow,
             static_cast<int>(StressQueue::ProtectedOverflowCapacity()));
    ExpectEq("coalescing still scans full bounded overflow",
             static_cast<int>(capacityBoundQueue.Push(
                 MakeQueueEvent(9999u, "EzrealMysticShot"),
                 MakeDescriptor(
                     kPhysicalBudget,
                     PendingPriority::HostileCast),
                 MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Coalesce));
    ExpectEq("distinct hostile beyond physical budget is dropped explicitly",
             static_cast<int>(capacityBoundQueue.Push(
                 MakeQueueEvent(kPhysicalBudget + 1u),
                 MakeDescriptor(
                     kPhysicalBudget + 1u,
                     PendingPriority::HostileCast),
                 MergeQueueEvent)),
             static_cast<int>(
                 PendingQueueDecision::DropProtectedOverflowLimit));
    ExpectEq("overflow-limit drop leaves bounded size unchanged",
             static_cast<int>(capacityBoundQueue.Size()),
             static_cast<int>(kPhysicalBudget));
    ExpectEq("overflow-limit drop counter increments",
             capacityBoundQueue.Counters().droppedProtectedOverflowLimit, 1);
    for (std::uint64_t expected = 1;
         expected <= kPhysicalBudget;
         ++expected) {
        ExpectTrue("capacity-bound hostile queue remains poppable",
                   capacityBoundQueue.Pop(ordered));
        ExpectTrue("capacity-bound hostile queue preserves exact FIFO",
                   ordered.id == expected);
    }
    ExpectTrue("full-overflow coalesce enriches final FIFO event",
               std::strcmp(ordered.spellName, "EzrealMysticShot") == 0);
    capacityBoundQueue.Clear();
    ExpectEq("clear resets populated overflow-limit drop counter",
             capacityBoundQueue.Counters().droppedProtectedOverflowLimit, 0);

    Queue wrapQueue;
    wrapQueue.Push(
        MakeQueueEvent(std::numeric_limits<std::uint64_t>::max()),
        MakeDescriptor(20u, PendingPriority::StructuralCast),
        MergeQueueEvent);
    wrapQueue.Push(
        MakeQueueEvent(0u),
        MakeDescriptor(21u, PendingPriority::StructuralCast),
        MergeQueueEvent);
    wrapQueue.Pop(ordered);
    ExpectTrue("UINT64_MAX event token remains FIFO first",
               ordered.id == std::numeric_limits<std::uint64_t>::max());
    wrapQueue.Pop(ordered);
    ExpectTrue("post-wrap zero token remains FIFO second", ordered.id == 0u);
    ExpectEq("UINT64_MAX logical ring index is overflow-safe",
             static_cast<int>(LogicalRingIndex(
                 std::numeric_limits<std::uint64_t>::max(), 1u, 3u)),
             1);

    const MissileBindKey delayedCast = {
        42u, 0x1234u, 77u, Vec2(1.0f, 0.0f), 1000, 2000, 2000
    };
    const MissileBindObservation delayedMissile = {
        42u, 0x1234u, 77u, Vec2(1.0f, 0.0f), 3100
    };
    ExpectTrue("matching delayed missile binds beyond legacy 1600ms",
               CanBindMissile(delayedCast, delayedMissile));
    MissileBindObservation wrongIdentity = delayedMissile;
    wrongIdentity.castIdentity = 0x5678u;
    ExpectTrue("wrong available cast identity does not bind",
               !CanBindMissile(delayedCast, wrongIdentity));
    MissileBindObservation wrongDirection = delayedMissile;
    wrongDirection.direction = Vec2(-1.0f, 0.0f);
    ExpectTrue("wrong missile direction does not bind",
               !CanBindMissile(delayedCast, wrongDirection));
    MissileBindObservation wrongSpell = delayedMissile;
    wrongSpell.spellIdentity = 88u;
    ExpectTrue("wrong missile spell identity does not bind",
               !CanBindMissile(delayedCast, wrongSpell));

    const MissileBindKey spreadLeft = {
        42u, 0x1234u, 77u, Vec2(1.0f, -0.25f), 1000, 2000, 2000
    };
    const MissileBindKey spreadRight = {
        42u, 0x1234u, 77u, Vec2(1.0f, 0.25f), 1000, 2000, 2000
    };
    MissileBindObservation leftMissile = delayedMissile;
    leftMissile.direction = spreadLeft.direction;
    MissileBindObservation rightMissile = delayedMissile;
    rightMissile.direction = spreadRight.direction;
    ExpectTrue("multi-projectile cast binds left missile to left lane",
               CanBindMissile(spreadLeft, leftMissile, 0.999f) &&
               !CanBindMissile(spreadRight, leftMissile, 0.999f));
    ExpectTrue("multi-projectile cast binds right missile to right lane",
               CanBindMissile(spreadRight, rightMissile, 0.999f) &&
               !CanBindMissile(spreadLeft, rightMissile, 0.999f));

    const Vec2 velocity = ProjectedVelocity(
        Vec2(300.0f, 40.0f), Vec2(1.0f, 0.0f), 150.0f);
    ExpectNear("projected velocity keeps route-axis speed", velocity.x, 2000.0f);
    ExpectNear("projected velocity discards lateral drift", velocity.y, 0.0f);
    ExpectNear("projected speed ignores lateral displacement",
               FilteredProjectedSpeed(
                   1000.0f,
                   Vec2(300.0f, 400.0f),
                   Vec2(1.0f, 0.0f),
                   150.0f),
               1250.0f);
    ExpectNear("forward speed outlier preserves prior estimate",
               FilteredProjectedSpeed(
                   1450.0f,
                   Vec2(30000.0f, 0.0f),
                   Vec2(1.0f, 0.0f),
                   100.0f),
               1450.0f);
    ExpectNear("backward observation preserves prior estimate",
               FilteredProjectedSpeed(
                   1450.0f,
                   Vec2(-50.0f, 0.0f),
                   Vec2(1.0f, 0.0f),
                   50.0f),
               1450.0f);

    const Vec2 curvedRoute = ObservationRouteDirection(
        Vec2(1.0f, 0.0f),
        Vec2(),
        Vec2(100.0f, 100.0f),
        Vec2(300.0f, 300.0f),
        Vec2(100.0f, 100.0f),
        true,
        false);
    ExpectNear("curved endpoint updates route direction x",
               curvedRoute.x,
               0.7071068f);
    ExpectNear("curved endpoint updates route direction y",
               curvedRoute.y,
               0.7071068f);
    ExpectNear("curved speed uses new diagonal route frame",
               FilteredProjectedSpeed(
                   1000.0f,
                   Vec2(100.0f, 100.0f),
                   curvedRoute,
                   100.0f),
               1103.5533f,
               0.01f);

    const Vec2 lateralTurnRoute = ObservationRouteDirection(
        Vec2(1.0f, 0.0f),
        Vec2(),
        Vec2(0.0f, 200.0f),
        Vec2(0.0f, 1000.0f),
        Vec2(0.0f, 200.0f),
        true,
        false);
    ExpectNear("lateral endpoint turn updates route x",
               lateralTurnRoute.x,
               0.0f);
    ExpectNear("lateral endpoint turn updates route y",
               lateralTurnRoute.y,
               1.0f);
    ExpectNear("lateral turn speed uses new route frame",
               FilteredProjectedSpeed(
                   1000.0f,
                   Vec2(0.0f, 200.0f),
                   lateralTurnRoute,
                   100.0f),
               1250.0f);

    const Vec2 movementFallbackRoute = ObservationRouteDirection(
        Vec2(),
        Vec2(),
        Vec2(500.0f, 500.0f),
        Vec2(1000.0f, 500.0f),
        Vec2(0.0f, 100.0f),
        true,
        false);
    ExpectNear("zero steering route uses observed movement x",
               movementFallbackRoute.x,
               0.0f);
    ExpectNear("zero steering route uses observed movement y",
               movementFallbackRoute.y,
               1.0f);
    const Vec2 heldZeroRoute = ObservationRouteDirection(
        Vec2(),
        Vec2(),
        Vec2(500.0f, 500.0f),
        Vec2(1000.0f, 500.0f),
        Vec2(),
        true,
        false);
    ExpectTrue("zero steering route without movement remains zero",
               heldZeroRoute.IsZero());

    const Vec2 facingSample(3.0f, 4.0f);
    const Vec2 fallbackSample(-1.0f, 0.0f);
    const Vec2 chargeDirection =
        SionChargeDirection(facingSample, fallbackSample);
    ExpectNear("Sion charge follows facing x", chargeDirection.x, 0.6f);
    ExpectNear("Sion charge follows facing y", chargeDirection.y, 0.8f);
    ExpectNear("Sion charge direction stays normalized",
               chargeDirection.x * chargeDirection.x +
                   chargeDirection.y * chargeDirection.y,
               1.0f);
    const Vec2 fallbackDirection =
        SionChargeDirection(Vec2(), Vec2(-2.0f, 0.0f));
    ExpectNear("invalid Sion facing uses normalized fallback",
               fallbackDirection.x,
               -1.0f);
    ExpectNear("invalid Sion facing uses fallback y", fallbackDirection.y, 0.0f);
    ExpectNear("invalid Sion fallback direction stays normalized",
               fallbackDirection.x * fallbackDirection.x +
                   fallbackDirection.y * fallbackDirection.y,
               1.0f);
    const Vec2 sionOrigin(100.0f, 200.0f);
    const Vec2 sionCorridorEnd = SionChargeCorridorEnd(
        sionOrigin, Vec2(1.0f, 0.0f), 7500.0f);
    ExpectNear("Sion corridor uses authored database range",
               sionCorridorEnd.x,
               7600.0f);
    ExpectTrue("Sion corridor covers target 1200 ahead",
               SionChargeCorridorCoversPoint(
                   sionOrigin,
                   Vec2(1.0f, 0.0f),
                   7500.0f,
                   120.0f,
                   Vec2(1300.0f, 200.0f)));

    const Vec2 wiredFacing = facingSample.Normalized();
    const Vec2 wiredDirection(-wiredFacing.y, wiredFacing.x);
    ExpectTrue("legacy Sion perpendicular wiring diverges from charge facing",
               std::fabs(wiredDirection.x - chargeDirection.x) > 0.01f ||
                   std::fabs(wiredDirection.y - chargeDirection.y) > 0.01f);
    ExpectNear("legacy Sion perpendicular wiring is 90deg from facing",
               wiredDirection.x,
               -0.8f);
    ExpectNear("legacy Sion perpendicular wiring y", wiredDirection.y, 0.6f);

    return ZDEvadeTest::Finish("ZDEVADE DETECTOR POLICY");
}
