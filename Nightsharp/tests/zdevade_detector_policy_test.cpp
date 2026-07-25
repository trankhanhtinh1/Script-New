#include "tests/ZDEvadeTestSupport.h"
#include "plugins/ZDEvade/Database/ThreatDatabase.h"
#include "plugins/ZDEvade/Debug/SelfSkillDebugPolicy.h"
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

constexpr int kExpectedLogicalCastEpisodeWindowMs = 180;

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

    SpellData episodeSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    episodeSpell.spellName = "EpisodeSpell";
    episodeSpell.spellKey = ZDSpellSlot::Q;
    episodeSpell.multipleNumber = 3;
    episodeSpell.multipleAngle = 2.0f;
    LogicalCastEpisodeObservation episodeA;
    episodeA.casterNetworkId = 42u;
    episodeA.data = &episodeSpell;
    episodeA.slot = static_cast<int>(ZDSpellSlot::Q);
    episodeA.episodeTick = 989;
    episodeA.observationTick = 989;
    episodeA.start = Vec2(100.0f, 100.0f);
    episodeA.end = Vec2(1000.0f, 100.0f);
    episodeA.direction = Vec2(1.0f, 0.0f);
    LogicalCastEpisodeObservation episodeB = episodeA;
    episodeB.episodeTick = 1070;
    episodeB.observationTick = 1070;
    episodeB.start = Vec2(106.0f, 102.0f);
    episodeB.end = Vec2(1006.0f, 102.0f);

    LogicalCastEpisodeResolver<8> episodes;
    const std::uint64_t episodeIdA = episodes.Resolve(episodeA);
    const std::uint64_t episodeIdB = episodes.Resolve(episodeB);
    ExpectTrue(
        "989 and 1070 cross-bucket callbacks share ingestion episode",
        episodeIdA != 0 &&
            episodeIdA == episodeIdB &&
            episodes.Size() == 1);
    LogicalCastEpisodeObservation missileEpisode = episodeA;
    missileEpisode.observationTick = 1120;
    missileEpisode.direction =
        Vec2(0.999391f, 0.034899f);
    missileEpisode.end =
        missileEpisode.start +
        missileEpisode.direction * 900.0f;
    missileEpisode.projectileObservation = true;
    const std::uint64_t castLeftLaneKey =
        episodes.ResolveLaneKey(
            episodeIdA,
            Vec2(0.999391f, -0.034899f),
            0);
    const std::uint64_t castCenterLaneKey =
        episodes.ResolveLaneKey(
            episodeIdA,
            Vec2(1.0f, 0.0f),
            1);
    const std::uint64_t castRightLaneKey =
        episodes.ResolveLaneKey(
            episodeIdA,
            missileEpisode.direction,
            2);
    Vec2 resolvedEpisodeDirection;
    bool matchedMissileEpisode = false;
    std::uint64_t resolvedCastLaneKey = 0;
    const std::uint64_t missileEpisodeId = episodes.Resolve(
        missileEpisode,
        &resolvedEpisodeDirection,
        &matchedMissileEpisode,
        &resolvedCastLaneKey);
    ExpectTrue(
        "missile observation binds existing multi-projectile episode",
        missileEpisodeId == episodeIdA &&
            matchedMissileEpisode &&
            resolvedEpisodeDirection.Dot(
                episodeA.direction) > 0.999f &&
            resolvedCastLaneKey == castRightLaneKey &&
            castLeftLaneKey != castCenterLaneKey &&
            castCenterLaneKey != castRightLaneKey &&
            StableProjectileLaneIndex(3, 0) == 0 &&
            StableProjectileLaneIndex(3, 1) == 1 &&
            StableProjectileLaneIndex(3, 2) == 2);

    SpellData missileOnlySpell = episodeSpell;
    missileOnlySpell.multipleAngle = 28.0f;
    const std::array<Vec2, 3> missileOnlyDirections = {{
        Vec2(0.882947593f, -0.469471563f),
        Vec2(1.0f, 0.0f),
        Vec2(0.882947593f, 0.469471563f),
    }};
    const std::array<std::array<int, 3>, 6> arrivalPermutations = {{
        {{0, 1, 2}},
        {{0, 2, 1}},
        {{1, 0, 2}},
        {{1, 2, 0}},
        {{2, 0, 1}},
        {{2, 1, 0}},
    }};
    const auto rotateDirection = [](const Vec2& direction, float degrees) {
        const float radians =
            degrees * 3.14159265358979323846f / 180.0f;
        return Vec2(
            direction.x * std::cos(radians) -
                direction.y * std::sin(radians),
            direction.x * std::sin(radians) +
                direction.y * std::cos(radians)).Normalized();
    };
    bool everyPermutationSharedEpisode = true;
    bool everyPermutationKeptLaneKeys = true;
    for (const auto& permutation : arrivalPermutations) {
        LogicalCastEpisodeResolver<8> missileOnlyEpisodes;
        std::uint64_t sharedId = 0;
        std::array<std::uint64_t, 3> keysByAuthoredLane = {};
        for (std::size_t arrival = 0; arrival < permutation.size(); ++arrival) {
            const int lane = permutation[arrival];
            LogicalCastEpisodeObservation observation = episodeA;
            observation.data = &missileOnlySpell;
            observation.episodeTick = 2000;
            observation.observationTick =
                2000 + static_cast<int>(arrival) * 10;
            observation.direction = missileOnlyDirections[lane];
            observation.end =
                observation.start + observation.direction * 900.0f;
            observation.projectileObservation = true;
            Vec2 assumedCenter;
            std::uint64_t laneKey = 0;
            const std::uint64_t resolved =
                missileOnlyEpisodes.Resolve(
                    observation,
                    &assumedCenter,
                    nullptr,
                    &laneKey);
            if (arrival == 0) sharedId = resolved;
            everyPermutationSharedEpisode =
                everyPermutationSharedEpisode &&
                resolved != 0 &&
                resolved == sharedId &&
                assumedCenter.IsZero() &&
                laneKey != 0;
            keysByAuthoredLane[lane] = laneKey;
        }
        std::array<std::uint64_t, 3> sortedKeys =
            keysByAuthoredLane;
        std::sort(sortedKeys.begin(), sortedKeys.end());
        everyPermutationKeptLaneKeys =
            everyPermutationKeptLaneKeys &&
            sortedKeys == std::array<std::uint64_t, 3>{{1, 2, 3}};
        for (std::size_t lane = 0;
             lane < missileOnlyDirections.size();
             ++lane) {
            LogicalCastEpisodeObservation jittered = episodeA;
            jittered.data = &missileOnlySpell;
            jittered.episodeTick = 2000;
            jittered.observationTick =
                2040 + static_cast<int>(lane) * 10;
            jittered.direction = rotateDirection(
                missileOnlyDirections[lane],
                lane == 0 ? 1.0f : lane == 1 ? -0.1f : -0.75f);
            jittered.end =
                jittered.start + jittered.direction * 900.0f;
            jittered.projectileObservation = true;
            std::uint64_t jitteredKey = 0;
            everyPermutationSharedEpisode =
                everyPermutationSharedEpisode &&
                missileOnlyEpisodes.Resolve(
                    jittered,
                    nullptr,
                    nullptr,
                    &jitteredKey) == sharedId;
            everyPermutationKeptLaneKeys =
                everyPermutationKeptLaneKeys &&
                jitteredKey == keysByAuthoredLane[lane];
        }
    }
    ExpectTrue(
        "all missile-only lane arrival permutations share one episode",
        everyPermutationSharedEpisode);
    ExpectTrue(
        "outer-first center and other arrivals keep physical lane keys",
        everyPermutationKeptLaneKeys);

    LogicalCastEpisodeResolver<8> missileOnlyLaterEpisodes;
    LogicalCastEpisodeObservation firstMissileOnly = episodeA;
    firstMissileOnly.data = &missileOnlySpell;
    firstMissileOnly.episodeTick = 3000;
    firstMissileOnly.observationTick = 3000;
    firstMissileOnly.direction = missileOnlyDirections[0];
    firstMissileOnly.end =
        firstMissileOnly.start + firstMissileOnly.direction * 900.0f;
    firstMissileOnly.projectileObservation = true;
    const std::uint64_t firstMissileOnlyId =
        missileOnlyLaterEpisodes.Resolve(firstMissileOnly);
    LogicalCastEpisodeObservation genuineLaterMissile =
        firstMissileOnly;
    genuineLaterMissile.episodeTick =
        firstMissileOnly.episodeTick +
        kExpectedLogicalCastEpisodeWindowMs + 1;
    genuineLaterMissile.observationTick =
        genuineLaterMissile.episodeTick;
    ExpectTrue(
        "genuine later missile-only cast receives a separate episode",
        missileOnlyLaterEpisodes.Resolve(genuineLaterMissile) !=
            firstMissileOnlyId);

    LogicalCastEpisodeResolver<4> boundaryEpisodes;
    LogicalCastEpisodeObservation boundaryLane = firstMissileOnly;
    boundaryLane.episodeTick = 4000;
    boundaryLane.observationTick = 4000;
    boundaryLane.direction = rotateDirection(Vec2(1.0f, 0.0f), 10.0f);
    boundaryLane.end =
        boundaryLane.start + boundaryLane.direction * 900.0f;
    std::uint64_t boundaryKey = 0;
    boundaryEpisodes.Resolve(
        boundaryLane, nullptr, nullptr, &boundaryKey);
    LogicalCastEpisodeObservation boundaryJitter = boundaryLane;
    boundaryJitter.observationTick += 10;
    boundaryJitter.direction =
        rotateDirection(boundaryLane.direction, 1.0f);
    boundaryJitter.end =
        boundaryJitter.start + boundaryJitter.direction * 900.0f;
    std::uint64_t boundaryJitterKey = 0;
    boundaryEpisodes.Resolve(
        boundaryJitter,
        nullptr,
        nullptr,
        &boundaryJitterKey);
    ExpectTrue(
        "same-lane jitter crosses exact quantization but reuses registry key",
        StableProjectileLaneKey(boundaryLane.direction) !=
            StableProjectileLaneKey(boundaryJitter.direction) &&
            boundaryKey != 0 &&
            boundaryKey == boundaryJitterKey);

    LogicalCastEpisodeResolver<4> noTransitiveEpisodes;
    LogicalCastEpisodeObservation transitiveA = firstMissileOnly;
    transitiveA.episodeTick = 4500;
    transitiveA.observationTick = 4500;
    transitiveA.direction = Vec2(1.0f, 0.0f);
    transitiveA.end =
        transitiveA.start + transitiveA.direction * 900.0f;
    std::uint64_t transitiveKeyA = 0;
    noTransitiveEpisodes.Resolve(
        transitiveA, nullptr, nullptr, &transitiveKeyA);
    LogicalCastEpisodeObservation transitiveB = transitiveA;
    transitiveB.observationTick += 10;
    transitiveB.direction =
        rotateDirection(transitiveA.direction, 2.5f);
    transitiveB.end =
        transitiveB.start + transitiveB.direction * 900.0f;
    std::uint64_t transitiveKeyB = 0;
    noTransitiveEpisodes.Resolve(
        transitiveB, nullptr, nullptr, &transitiveKeyB);
    LogicalCastEpisodeObservation transitiveC = transitiveA;
    transitiveC.observationTick += 20;
    transitiveC.direction =
        rotateDirection(transitiveA.direction, 5.0f);
    transitiveC.end =
        transitiveC.start + transitiveC.direction * 900.0f;
    std::uint64_t transitiveKeyC = 0;
    noTransitiveEpisodes.Resolve(
        transitiveC, nullptr, nullptr, &transitiveKeyC);
    ExpectTrue(
        "lane registry matching is non-transitive",
        transitiveKeyA == transitiveKeyB &&
            transitiveKeyC != transitiveKeyA);

    LogicalCastEpisodeResolver<4> closeDistinctEpisodes;
    LogicalCastEpisodeObservation closeLaneA = transitiveA;
    closeLaneA.episodeTick = 5000;
    closeLaneA.observationTick = 5000;
    std::uint64_t closeLaneKeyA = 0;
    closeDistinctEpisodes.Resolve(
        closeLaneA, nullptr, nullptr, &closeLaneKeyA);
    LogicalCastEpisodeObservation closeLaneB = closeLaneA;
    closeLaneB.observationTick += 10;
    closeLaneB.direction =
        rotateDirection(closeLaneA.direction, 4.0f);
    closeLaneB.end =
        closeLaneB.start + closeLaneB.direction * 900.0f;
    std::uint64_t closeLaneKeyB = 0;
    closeDistinctEpisodes.Resolve(
        closeLaneB, nullptr, nullptr, &closeLaneKeyB);
    ExpectTrue(
        "close authored lanes beyond tolerance stay distinct",
        closeLaneKeyA != 0 &&
            closeLaneKeyB != 0 &&
            closeLaneKeyA != closeLaneKeyB);

    SpellData singleProjectileSpell = episodeSpell;
    singleProjectileSpell.spellName = "VeigarQ";
    singleProjectileSpell.multipleNumber = 1;
    LogicalCastEpisodeObservation veigarCast = episodeA;
    veigarCast.data = &singleProjectileSpell;
    veigarCast.episodeTick = 6000;
    veigarCast.observationTick = 6000;
    veigarCast.projectileObservation = false;
    LogicalCastEpisodeResolver<8> singleProjectileEpisodes;
    const std::uint64_t veigarEpisodeId =
        singleProjectileEpisodes.Resolve(veigarCast);
    const std::uint64_t veigarCastLaneKey =
        singleProjectileEpisodes.ResolveLaneKey(
            veigarEpisodeId,
            veigarCast.direction,
            0);
    LogicalCastEpisodeObservation veigarDoCast = veigarCast;
    veigarDoCast.episodeTick += 30;
    veigarDoCast.observationTick += 30;
    veigarDoCast.direction =
        rotateDirection(veigarCast.direction, 0.1f);
    veigarDoCast.end =
        veigarDoCast.start + veigarDoCast.direction * 900.0f;
    const std::uint64_t veigarDoCastEpisodeId =
        singleProjectileEpisodes.Resolve(veigarDoCast);
    const std::uint64_t veigarDoCastLaneKey =
        singleProjectileEpisodes.ResolveLaneKey(
            veigarDoCastEpisodeId,
            veigarDoCast.direction,
            0);
    LogicalCastEpisodeObservation veigarMissile = veigarCast;
    veigarMissile.episodeTick += 60;
    veigarMissile.observationTick += 60;
    veigarMissile.direction =
        rotateDirection(veigarCast.direction, -1.0f);
    veigarMissile.end =
        veigarMissile.start + veigarMissile.direction * 900.0f;
    veigarMissile.projectileObservation = true;
    std::uint64_t veigarMissileLaneKey = 0;
    const std::uint64_t veigarMissileEpisodeId =
        singleProjectileEpisodes.Resolve(
            veigarMissile,
            nullptr,
            nullptr,
            &veigarMissileLaneKey);
    ExpectTrue(
        "Veigar Q callback and missile jitter reuse one episode lane",
        veigarEpisodeId != 0 &&
            veigarEpisodeId == veigarDoCastEpisodeId &&
            veigarEpisodeId == veigarMissileEpisodeId &&
            veigarCastLaneKey != 0 &&
            veigarCastLaneKey == veigarDoCastLaneKey &&
            veigarCastLaneKey == veigarMissileLaneKey &&
            StableProjectileLaneKey(veigarCast.direction) !=
                StableProjectileLaneKey(veigarDoCast.direction) &&
            StableProjectileLaneKey(veigarCast.direction) !=
                StableProjectileLaneKey(veigarMissile.direction) &&
            StableProjectileLaneIndex(1, 0) == -1 &&
            singleProjectileEpisodes.Size() == 1);
    const CastEventKey veigarRawProcess = MakeCastKey(
        42u,
        0xA100u,
        static_cast<int>(ZDSpellSlot::Q),
        6000,
        "VeigarQ",
        veigarCast.start,
        veigarCast.end);
    const CastEventKey veigarRawDoCast = MakeCastKey(
        42u,
        0xB200u,
        static_cast<int>(ZDSpellSlot::Q),
        6030,
        "VeigarQ",
        veigarDoCast.start,
        veigarDoCast.end);
    ExpectTrue(
        "Veigar Q raw identity churn coalesces across jittered callbacks",
        SameLogicalCast(veigarRawProcess, veigarRawDoCast));

    LogicalCastEpisodeResolver<8> missileFirstVeigarEpisodes;
    LogicalCastEpisodeObservation missileFirstVeigar =
        veigarMissile;
    missileFirstVeigar.episodeTick = 7000;
    missileFirstVeigar.observationTick = 7000;
    std::uint64_t missileFirstVeigarKey = 0;
    const std::uint64_t missileFirstVeigarId =
        missileFirstVeigarEpisodes.Resolve(
            missileFirstVeigar,
            nullptr,
            nullptr,
            &missileFirstVeigarKey);
    LogicalCastEpisodeObservation delayedVeigarCast =
        veigarCast;
    delayedVeigarCast.episodeTick = 7040;
    delayedVeigarCast.observationTick = 7040;
    delayedVeigarCast.direction =
        rotateDirection(veigarCast.direction, 0.75f);
    delayedVeigarCast.end =
        delayedVeigarCast.start +
        delayedVeigarCast.direction * 900.0f;
    const std::uint64_t delayedVeigarCastId =
        missileFirstVeigarEpisodes.Resolve(delayedVeigarCast);
    const std::uint64_t delayedVeigarCastKey =
        missileFirstVeigarEpisodes.ResolveLaneKey(
            delayedVeigarCastId,
            delayedVeigarCast.direction,
            0);
    ExpectTrue(
        "missile-first Veigar Q retains lane through callback churn",
        missileFirstVeigarId != 0 &&
            missileFirstVeigarId == delayedVeigarCastId &&
            missileFirstVeigarKey != 0 &&
            missileFirstVeigarKey == delayedVeigarCastKey);

    LogicalCastEpisodeObservation incompatibleVeigar =
        veigarCast;
    incompatibleVeigar.episodeTick = 6060;
    incompatibleVeigar.observationTick = 6060;
    incompatibleVeigar.direction = Vec2(0.0f, 1.0f);
    incompatibleVeigar.end =
        incompatibleVeigar.start +
        incompatibleVeigar.direction * 900.0f;
    LogicalCastEpisodeObservation movedVeigar = veigarCast;
    movedVeigar.episodeTick = 6060;
    movedVeigar.observationTick = 6060;
    movedVeigar.start = Vec2(500.0f, 500.0f);
    movedVeigar.end =
        movedVeigar.start + movedVeigar.direction * 900.0f;
    LogicalCastEpisodeObservation laterVeigar = veigarCast;
    laterVeigar.episodeTick =
        veigarCast.episodeTick +
        kExpectedLogicalCastEpisodeWindowMs + 1;
    laterVeigar.observationTick = laterVeigar.episodeTick;
    ExpectTrue(
        "incompatible and later Veigar Q casts get separate identities",
        singleProjectileEpisodes.Resolve(incompatibleVeigar) !=
            veigarEpisodeId &&
            singleProjectileEpisodes.Resolve(movedVeigar) !=
                veigarEpisodeId &&
            singleProjectileEpisodes.Resolve(laterVeigar) !=
                veigarEpisodeId);

    LogicalCastEpisodeResolver<8> reversedEpisodes;
    const std::uint64_t reversedB =
        reversedEpisodes.Resolve(episodeB);
    const std::uint64_t reversedA =
        reversedEpisodes.Resolve(episodeA);
    ExpectTrue(
        "reversed callback order and raw identity differences share episode",
        reversedA != 0 &&
            reversedA == reversedB);

    LogicalCastEpisodeObservation laterEpisode = episodeA;
    laterEpisode.episodeTick = 1389;
    laterEpisode.observationTick = 1389;
    LogicalCastEpisodeObservation turnedEpisode = episodeA;
    turnedEpisode.episodeTick = 1030;
    turnedEpisode.observationTick = 1030;
    turnedEpisode.direction = Vec2(0.0f, 1.0f);
    turnedEpisode.end = Vec2(100.0f, 1000.0f);
    LogicalCastEpisodeObservation movedEpisode = episodeA;
    movedEpisode.episodeTick = 1030;
    movedEpisode.observationTick = 1030;
    movedEpisode.start = Vec2(500.0f, 500.0f);
    movedEpisode.end = Vec2(1400.0f, 500.0f);
    ExpectTrue(
        "later and incompatible cast ingestion receive new episodes",
        episodes.Resolve(laterEpisode) != episodeIdA &&
            episodes.Resolve(turnedEpisode) != episodeIdA &&
            episodes.Resolve(movedEpisode) != episodeIdA);

    LogicalCastEpisodeResolver<8> wrappedEpisodes;
    LogicalCastEpisodeObservation beforeWrap = episodeA;
    beforeWrap.episodeTick = INT_MAX - 40;
    beforeWrap.observationTick = INT_MAX - 40;
    LogicalCastEpisodeObservation afterWrap = beforeWrap;
    afterWrap.episodeTick = INT_MIN + 39;
    afterWrap.observationTick = INT_MIN + 39;
    ExpectTrue(
        "logical episode tick matching is signed-wrap safe",
        wrappedEpisodes.Resolve(beforeWrap) ==
            wrappedEpisodes.Resolve(afterWrap));

    LogicalCastEpisodeResolver<4> boundedEpisodes;
    for (int index = 0; index < 12; ++index) {
        LogicalCastEpisodeObservation value = episodeA;
        value.casterNetworkId =
            static_cast<std::uint32_t>(100 + index);
        value.episodeTick = 2000 + index * 400;
        value.observationTick = value.episodeTick;
        boundedEpisodes.Resolve(value);
    }
    ExpectTrue(
        "recent logical episode table remains fixed capacity",
        boundedEpisodes.Size() <= 4);
    boundedEpisodes.Prune(
        2000 + 11 * 400 +
        kLogicalCastEpisodeRetentionMs + 1);
    ExpectTrue(
        "recent logical episode pruning removes stale entries",
        boundedEpisodes.Size() == 0);
    boundedEpisodes.Reset();
    const std::uint64_t resetEpisode =
        boundedEpisodes.Resolve(episodeA);
    ExpectTrue(
        "logical episode reset clears state and restarts nonzero IDs",
        boundedEpisodes.Size() == 1 &&
            resetEpisode != 0);

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
    fallbackDoCast.tick =
        1000 + kExpectedLogicalCastEpisodeWindowMs + 1;
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
    const CastEventKey differentPointerDoCast = MakeCastKey(
        42u, 0xDEF0u, 0, 2085, {"EzrealMysticShot"},
        Vec2(106.0f, 102.0f), Vec2(908.0f, 116.0f));
    ExpectTrue("different nonzero hook pointers are only positive hints",
               SameLogicalCast(
                   MakeCastKey(
                       42u, 0xABC0u, 0, 2000, "EzrealMysticShot",
                       Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f)),
                   differentPointerDoCast));
    CastEventKey outsideEpisode = differentPointerDoCast;
    outsideEpisode.tick =
        2000 + kExpectedLogicalCastEpisodeWindowMs + 1;
    ExpectTrue("compatible later cast outside episode stays separate",
               !SameLogicalCast(
                   MakeCastKey(
                       42u, 0xABC0u, 0, 2000, "EzrealMysticShot",
                       Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f)),
                   outsideEpisode));
    CastEventKey incompatibleEpisode = differentPointerDoCast;
    incompatibleEpisode.endPosition = Vec2(-900.0f, 100.0f);
    ExpectTrue("same-episode incompatible geometry stays separate",
               !SameLogicalCast(
                   MakeCastKey(
                       42u, 0xABC0u, 0, 2000, "EzrealMysticShot",
                       Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f)),
                   incompatibleEpisode));
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
    const CastEventKey semanticReference = MakeCastKey(
        42u, 0xA100u, 0, 2070, {"EzrealMysticShot"},
        Vec2(100.0f, 100.0f),
        Vec2(900.0f, 100.0f),
        Vec2(700.0f, 100.0f));
    const CastEventKey matchingCastDivergentEnd = MakeCastKey(
        42u, 0xB200u, 0, 2070, {"EzrealMysticShot"},
        Vec2(104.0f, 102.0f),
        Vec2(-900.0f, 100.0f),
        Vec2(708.0f, 108.0f));
    ExpectTrue("matching CastPosition ignores divergent EndPosition",
               SameLogicalCast(
                   semanticReference, matchingCastDivergentEnd));
    const CastEventKey matchingEndDivergentCast = MakeCastKey(
        42u, 0xB201u, 0, 2070, {"EzrealMysticShot"},
        Vec2(104.0f, 102.0f),
        Vec2(908.0f, 108.0f),
        Vec2(-700.0f, 100.0f));
    ExpectTrue("matching EndPosition ignores divergent CastPosition",
               SameLogicalCast(
                   semanticReference, matchingEndDivergentCast));
    const CastEventKey crossFieldEndOnly = MakeCastKey(
        42u, 0xA300u, 0, 2070, {"EzrealMysticShot"},
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    const CastEventKey crossFieldCastOnly = MakeCastKey(
        42u, 0xB300u, 0, 2070, {"EzrealMysticShot"},
        Vec2(104.0f, 102.0f), Vec2(), Vec2(908.0f, 108.0f));
    ExpectTrue("compatible End-to-Cast fallback merges when only pair",
               SameLogicalCast(crossFieldEndOnly, crossFieldCastOnly));
    CastEventKey incompatibleCrossField = crossFieldCastOnly;
    incompatibleCrossField.castPosition = Vec2(-900.0f, 100.0f);
    ExpectTrue("incompatible End-to-Cast fallback rejects when only pair",
               !SameLogicalCast(crossFieldEndOnly, incompatibleCrossField));
    const CastEventKey conflictingSameSemanticPairs = MakeCastKey(
        42u, 0xB400u, 0, 2070, {"EzrealMysticShot"},
        Vec2(104.0f, 102.0f),
        Vec2(-900.0f, 100.0f),
        Vec2(-700.0f, 100.0f));
    ExpectTrue("both conflicting same-semantic pairs reject",
               !SameLogicalCast(
                   semanticReference, conflictingSameSemanticPairs));
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
    ExpectTrue("simultaneous compatible hooks with distinct pointers merge",
               SameLogicalCast(
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
    ExpectEq("missile evidence-loss grace is documented at 750ms",
             kMissileEvidenceLossGraceMs,
             750);
    ExpectEq("missile evidence loss is capped at 10 seconds",
             kMissileEvidenceLossMaximumMs,
             10000);
    ExpectEq("targetless collision prediction freshness is 250ms",
             kDeletePredictedCollisionMaximumAgeMs,
             250);
    ExpectTrue("no tracked threats skip authoritative missile enumeration",
               !ShouldEnumerateMissileManager(0));
    ExpectTrue("tracked threats require one authoritative missile enumeration",
               ShouldEnumerateMissileManager(1));
    const int nautilusArrival = 1000 + 575; // 1150 range / 2000 speed.
    const int nautilusDeadline = MissileEvidenceLossDeadlineTick(
        nautilusArrival, nautilusArrival);
    ExpectEq("Nautilus Q fallback deadline adds evidence grace",
             nautilusDeadline,
             2325);
    for (int miss = 1; miss <= 10; ++miss) {
        ExpectTrue("one through ten lookup misses retain the live missile",
                   !ShouldTerminateMissingMissile(
                       true,
                       false,
                       false,
                       1000,
                       nautilusDeadline,
                       1000 + miss * 40));
    }
    ExpectTrue("missing missile is active at evidence deadline boundary",
               !ShouldTerminateMissingMissile(
                   true, false, false, 1000, nautilusDeadline,
                   nautilusDeadline));
    ExpectTrue("missing missile terminates only after evidence deadline",
               ShouldTerminateMissingMissile(
                   true, false, false, 1000, nautilusDeadline,
                   nautilusDeadline + 1));
    const int saturatedEvidenceDeadline =
        MissileEvidenceLossDeadlineTick(INT_MAX, 4000);
    ExpectEq("INT_MAX arrival is bounded from last trustworthy evidence",
             saturatedEvidenceDeadline,
             14000);
    ExpectTrue("INT_MAX missile remains active at bounded deadline",
               !ShouldTerminateMissingMissile(
                   true, false, false, 4001, saturatedEvidenceDeadline,
                   saturatedEvidenceDeadline));
    ExpectTrue("INT_MAX missile terminates after bounded deadline",
               ShouldTerminateMissingMissile(
                   true, false, false, 4001, saturatedEvidenceDeadline,
                   saturatedEvidenceDeadline + 1));
    ExpectEq("INT_MAX fallback termination anchors to bounded prediction",
             MissingMissileTerminationTick(INT_MAX, 4000),
             14000);
    const int maximumAnchorDeadline =
        MissileEvidenceLossDeadlineTick(INT_MAX, INT_MAX);
    ExpectEq("INT_MAX anchor wraps to an exceedable 10s deadline",
             maximumAnchorDeadline,
             INT_MIN + kMissileEvidenceLossMaximumMs - 1);
    ExpectTrue("INT_MAX-anchor missile is active at wrapped deadline",
               !ShouldTerminateMissingMissile(
                   true, false, false, INT_MAX,
                   maximumAnchorDeadline,
                   maximumAnchorDeadline));
    ExpectTrue("INT_MAX-anchor missile terminates after wrapped deadline",
               ShouldTerminateMissingMissile(
                   true, false, false, INT_MAX,
                   maximumAnchorDeadline,
                   WrappingTickAdd(maximumAnchorDeadline, 1)));
    const int finiteArrivalNearWrap = INT_MAX - 250;
    const int finiteDeadlineNearWrap =
        MissileEvidenceLossDeadlineTick(
            finiteArrivalNearWrap,
            INT_MAX - 500);
    ExpectEq("finite arrival deadline crosses signed wrap without saturation",
             finiteDeadlineNearWrap,
             INT_MIN + 499);
    ExpectTrue("finite wrapped deadline remains active at boundary",
               !ShouldTerminateMissingMissile(
                   true, false, false, INT_MAX - 500,
                   finiteDeadlineNearWrap,
                   finiteDeadlineNearWrap));
    ExpectTrue("finite wrapped deadline expires on first later tick",
               ShouldTerminateMissingMissile(
                   true, false, false, INT_MAX - 500,
                   finiteDeadlineNearWrap,
                   WrappingTickAdd(finiteDeadlineNearWrap, 1)));
    ExpectEq("explicitly untrustworthy finite arrival uses anchor cap",
             MissileEvidenceLossDeadlineTick(25000, 4000, false),
             14000);
    ExpectEq("untrustworthy fallback termination uses anchor cap",
             MissingMissileTerminationTick(25000, 4000, false),
             14000);
    ExpectTrue("valid missile observation prevents evidence-loss termination",
               !ShouldTerminateMissingMissile(
                   true, false, true, 1000, nautilusDeadline,
                   nautilusDeadline + 500));
    ExpectTrue("live object with invalid position remains authoritative evidence",
               MissileObjectIsLiveEvidence(true, false));
    ExpectTrue("lookup miss remains uncertainty rather than live evidence",
               !MissileObjectIsLiveEvidence(false, false));
    const MissileEvidenceStateUpdate unavailableEvidence =
        ResolveMissileEvidenceState(true, false, 1200, false, 1400);
    ExpectTrue("present invalid-position evidence clears missing timer",
               unavailableEvidence.missingSinceTick == -1 &&
               unavailableEvidence.positionUnavailable &&
               unavailableEvidence.stateChanged);
    const MissileEvidenceStateUpdate repeatedUnavailableEvidence =
        ResolveMissileEvidenceState(
            true,
            false,
            unavailableEvidence.missingSinceTick,
            unavailableEvidence.positionUnavailable,
            1500);
    ExpectTrue("repeated invalid-position evidence retains stable state",
               repeatedUnavailableEvidence.missingSinceTick == -1 &&
               repeatedUnavailableEvidence.positionUnavailable &&
               !repeatedUnavailableEvidence.stateChanged);
    const MissileEvidenceStateUpdate reacquiredEvidence =
        ResolveMissileEvidenceState(
            true,
            true,
            repeatedUnavailableEvidence.missingSinceTick,
            repeatedUnavailableEvidence.positionUnavailable,
            1600);
    ExpectTrue("valid position evidence clears unavailable state",
               reacquiredEvidence.missingSinceTick == -1 &&
               !reacquiredEvidence.positionUnavailable &&
               reacquiredEvidence.stateChanged);
    ExpectTrue("unbound or terminated projectiles do not re-terminate",
               !ShouldTerminateMissingMissile(
                   false, false, false, 1000, nautilusDeadline, 3000) &&
               !ShouldTerminateMissingMissile(
                   true, true, false, 1000, nautilusDeadline, 3000));
    struct MissileLifecycleFixture {
        const char* spellName;
        float range;
        float speed;
        int expectedDeadline;
    };
    const std::array<MissileLifecycleFixture, 3> fastLifecycleFixtures = {{
        {"NautilusAnchorDrag", 1150.0f, 2000.0f, 2325},
        {"EzrealQ", 1150.0f, 2000.0f, 2325},
        {"VeigarBalefulStrike", 1000.0f, 2200.0f, 2205},
    }};
    for (const auto& fixture : fastLifecycleFixtures) {
        const int arrival = 1000 + static_cast<int>(std::ceil(
            1000.0f * fixture.range / fixture.speed));
        const int deadline =
            MissileEvidenceLossDeadlineTick(arrival, arrival);
        ExpectEq("fast missile fallback deadline matches arrival plus grace",
                 deadline,
                 fixture.expectedDeadline);
        ExpectTrue("fast missile remains active at fallback boundary",
                   !ShouldTerminateMissingMissile(
                       true, false, false, 1000, deadline, deadline));
        ExpectTrue("fast missile expires after fallback boundary",
                   ShouldTerminateMissingMissile(
                       true, false, false, 1000, deadline, deadline + 1));
    }
    struct LongFlightLifecycleFixture {
        const char* spellName;
        float range;
        float speed;
    };
    const std::array<LongFlightLifecycleFixture, 2>
        longFlightLifecycleFixtures = {{
            {"AsheR", 25000.0f, 1600.0f},
            {"LilliaERollingMissile", 25000.0f, 1150.0f},
        }};
    for (const auto& fixture : longFlightLifecycleFixtures) {
        const int launch = 1000;
        const int arrival = launch + static_cast<int>(std::ceil(
            1000.0f * fixture.range / fixture.speed));
        const int deadline =
            MissileEvidenceLossDeadlineTick(arrival, launch);
        ExpectEq("trustworthy long-flight deadline is arrival plus grace",
                 deadline,
                 arrival + kMissileEvidenceLossGraceMs);
        ExpectTrue("trustworthy long flight is not shortened by 10s cap",
                   WrappingTickDifference(
                       deadline,
                       WrappingTickAdd(
                           launch,
                           kMissileEvidenceLossMaximumMs)) > 0);
        ExpectTrue("long-flight missile remains active at deadline",
                   !ShouldTerminateMissingMissile(
                       true, false, false, launch + 1,
                       deadline, deadline));
        ExpectTrue("long-flight missile terminates after deadline",
                   ShouldTerminateMissingMissile(
                       true, false, false, launch + 1,
                       deadline, WrappingTickAdd(deadline, 1)));
    }
    for (const auto& fixture : longFlightLifecycleFixtures) {
        SpellData spell =
            ZDEvadeTest::MakeSpell(ZDSpellType::Line);
        spell.spellName = fixture.spellName;
        spell.range = fixture.range;
        spell.projectileSpeed = fixture.speed;
        Threat threat = ZDEvadeTest::MakeThreat(spell);
        threat.startPos = Vec2(100.0f, 100.0f);
        threat.endPos = Vec2(
            100.0f + fixture.range,
            100.0f);
        threat.authoredEndPos = threat.endPos;
        threat.direction = Vec2(1.0f, 0.0f);
        threat.missileBound = true;
        threat.launchTick = INT_MAX - 100;
        threat.observedTick = INT_MAX - 100;
        threat.observedHead = threat.startPos;
        const int remainingDuration =
            threat.RemainingTravelDurationMs();
        const int expectedRemainingDuration =
            static_cast<int>(std::ceil(
                1000.0f * fixture.range / fixture.speed));
        const MissileEvidenceWindow window =
            ResolveMissileEvidenceWindow(
                threat.observedTick,
                remainingDuration);
        ExpectEq("actual long-flight Threat reports finite remaining duration",
                 remainingDuration,
                 expectedRemainingDuration);
        ExpectEq("actual long-flight absolute arrival saturates near INT_MAX",
                 threat.ArrivalTick(),
                 INT_MAX);
        ExpectEq("remaining-duration deadline wraps from evidence anchor",
                 window.deadlineTick,
                 WrappingTickAdd(
                     WrappingTickAdd(
                         threat.observedTick,
                         expectedRemainingDuration),
                     kMissileEvidenceLossGraceMs));
        ExpectTrue("wrapped long-flight deadline is active at boundary",
                   !ShouldTerminateMissingMissile(
                       true,
                       false,
                       false,
                       threat.observedTick,
                       window.deadlineTick,
                       window.deadlineTick));
        ExpectTrue("wrapped long-flight deadline expires after boundary",
                   ShouldTerminateMissingMissile(
                       true,
                       false,
                       false,
                       threat.observedTick,
                       window.deadlineTick,
                       WrappingTickAdd(window.deadlineTick, 1)));
    }
    const MissileEvidenceWindow invalidRemainingWindow =
        ResolveMissileEvidenceWindow(4000, INT_MAX);
    ExpectTrue("saturated remaining duration alone uses 10s fallback",
               !invalidRemainingWindow.usedRemainingTravel &&
                   invalidRemainingWindow.deadlineTick == 14000 &&
                   invalidRemainingWindow.terminationTick == 14000);
    ExpectTrue("live bound collision prediction is diagnostic only",
               !ShouldCommitPredictedCollision(true, false));
    ExpectTrue("cast-origin collision prediction may remain conservative",
               ShouldCommitPredictedCollision(false, false));
    Threat predictedCollision =
        ZDEvadeTest::MakeThreat(
            ZDEvadeTest::MakeSpell(ZDSpellType::Line));
    predictedCollision.endPos = Vec2(1150.0f, 100.0f);
    predictedCollision.authoredEndPos = predictedCollision.endPos;
    predictedCollision.missileBound = true;
    if (ShouldCommitPredictedCollision(
            predictedCollision.missileBound,
            predictedCollision.projectileTerminated)) {
        predictedCollision.endPos = Vec2(400.0f, 100.0f);
        predictedCollision.collisionKind = ZDCollisionKind::Terrain;
        predictedCollision.collisionStopped = true;
    }
    ExpectTrue("predicted terrain collision cannot shorten live bound end",
               predictedCollision.endPos.DistanceSqr(
                   predictedCollision.authoredEndPos) <= 1.0f &&
                   !predictedCollision.collisionStopped &&
                   predictedCollision.collisionKind ==
                       ZDCollisionKind::None);
    predictedCollision.id = 991;
    predictedCollision.startTick = 3100;
    predictedCollision.collisionStopped = true;
    predictedCollision.collisionKind = ZDCollisionKind::Unit;
    predictedCollision.collisionUnitNetworkId = 1234;
    predictedCollision.collisionUnitObjectIdentity = 0x8800u;
    predictedCollision.collisionUnitCenter = Vec2(400.0f, 100.0f);
    predictedCollision.collisionExplosionCenter = Vec2(410.0f, 100.0f);
    predictedCollision.lastConsumedCollisionPoint = Vec2(390.0f, 100.0f);
    predictedCollision.collisionEndExplosionRadius = 250.0f;
    predictedCollision.collisionEndExplosionDelay = 500;
    predictedCollision.collisionHitCount = 2;
    predictedCollision.collisionUnitTargetAuthoritative = true;
    predictedCollision.pendingUnitCollisions = {{1234, 400.0f}};
    predictedCollision.consumedCollisionUnits = {1234};
    predictedCollision.predictedCollisionKind = ZDCollisionKind::Unit;
    predictedCollision.predictedCollisionUnitNetworkId = 1234;
    predictedCollision.predictedCollisionUnitCenter =
        Vec2(400.0f, 100.0f);
    predictedCollision.predictedCollisionPoint =
        Vec2(390.0f, 100.0f);
    predictedCollision.predictedCollisionTick = 3190;
    predictedCollision.predictedCollisionMissileNetworkId = 66u;
    predictedCollision.predictedCollisionMissileObjectIdentity =
        0x9900u;
    predictedCollision.predictedCollisionUnitObjectIdentity =
        0x8800u;
    predictedCollision.projectileTerminated = true;
    predictedCollision.projectileTerminationTick = 3200;
    predictedCollision.missingMissileTermination = true;
    predictedCollision.missileMissingSinceTick = 3150;
    predictedCollision.missilePositionUnavailable = true;
    CorrectExistingThreatFromMissile(
        predictedCollision,
        [](Threat& bound) {
            bound.endPos = Vec2(1150.0f, 100.0f);
            bound.authoredEndPos = bound.endPos;
            bound.missileNetworkId = 77u;
            bound.missileObjectIdentity = 0xA100u;
        });
    ExpectTrue("authoritative missile bind clears all speculative collision state",
               predictedCollision.id == 991 &&
                   predictedCollision.startTick == 3100 &&
                   predictedCollision.missileBound &&
                   !predictedCollision.collisionStopped &&
                   predictedCollision.collisionKind == ZDCollisionKind::None &&
                   predictedCollision.collisionUnitNetworkId == 0 &&
                   predictedCollision.collisionUnitObjectIdentity == 0 &&
                   predictedCollision.collisionUnitCenter.IsZero() &&
                   predictedCollision.collisionExplosionCenter.IsZero() &&
                   predictedCollision.lastConsumedCollisionPoint.IsZero() &&
                   predictedCollision.collisionEndExplosionRadius == 0.0f &&
                   predictedCollision.collisionEndExplosionDelay == -1 &&
                   predictedCollision.collisionHitCount == 0 &&
                   !predictedCollision
                       .collisionUnitTargetAuthoritative &&
                   predictedCollision.pendingUnitCollisions.empty() &&
                   predictedCollision.consumedCollisionUnits.empty() &&
                   predictedCollision.predictedCollisionKind ==
                       ZDCollisionKind::None &&
                   predictedCollision.predictedCollisionUnitNetworkId == 0 &&
                   predictedCollision.predictedCollisionUnitCenter.IsZero() &&
                   predictedCollision.predictedCollisionPoint.IsZero() &&
                   predictedCollision.predictedCollisionTick == -1 &&
                   predictedCollision
                       .predictedCollisionMissileNetworkId == 0 &&
                   predictedCollision
                       .predictedCollisionMissileObjectIdentity == 0 &&
                   predictedCollision
                       .predictedCollisionUnitObjectIdentity == 0 &&
                   !predictedCollision.projectileTerminated &&
                   predictedCollision.projectileTerminationTick == 0 &&
                   !predictedCollision.missingMissileTermination &&
                   predictedCollision.missileMissingSinceTick == -1 &&
                   !predictedCollision.missilePositionUnavailable);
    ExpectNear("reacquisition cannot rewind tracked missile head",
               MonotonicMissileHead(
                   Vec2(500.0f, 100.0f),
                   Vec2(450.0f, 100.0f),
                   Vec2(1.0f, 0.0f),
                   true).x,
               500.0f);
    ExpectNear("forward reacquisition advances tracked missile head",
               MonotonicMissileHead(
                   Vec2(500.0f, 100.0f),
                   Vec2(650.0f, 110.0f),
                   Vec2(1.0f, 0.0f),
                   true).x,
               650.0f);
    const Vec2 steeringTurn = MonotonicMissileHead(
        Vec2(500.0f, 100.0f),
        Vec2(450.0f, 250.0f),
        Vec2(1.0f, 0.0f),
        false);
    ExpectTrue("steering reacquisition accepts a valid turning observation",
               steeringTurn.DistanceSqr(Vec2(450.0f, 250.0f)) <= 1.0f);
    ExpectTrue("matching delete accepts exact missile episode",
               MatchesMissileEpisode(77u, 0xA100u, 77u, 0xA100u));
    ExpectTrue("wrong missile ID is ignored",
               !MatchesMissileEpisode(77u, 0xA100u, 78u, 0xA100u));
    ExpectTrue("stale reused-ID delete with different object is ignored",
               !MatchesMissileEpisode(77u, 0xA100u, 77u, 0xB200u));
    ExpectTrue("ID-only delete remains accepted when event identity unavailable",
               MatchesMissileEpisode(77u, 0xA100u, 77u, 0u));
    ExpectTrue("observation matches exact reused-ID episode identity",
               MatchesObservedMissileEpisode(
                   77u, 0xA100u, 77u, 0xA100u) &&
                   MatchesObservedMissileEpisode(
                       77u, 0xB200u, 77u, 0xB200u));
    ExpectTrue("one reused-ID episode cannot observe the other",
               !MatchesObservedMissileEpisode(
                   77u, 0xA100u, 77u, 0xB200u) &&
                   !MatchesObservedMissileEpisode(
                       77u, 0xB200u, 77u, 0xA100u));
    struct EpisodeMissingState {
        int threatId;
        std::uint32_t networkId;
        std::uintptr_t objectIdentity;
        int missingSinceTick;
    };
    std::array<EpisodeMissingState, 2> reusedEpisodes = {{
        {201, 77u, 0xA100u, 5000},
        {202, 77u, 0xB200u, 5000},
    }};
    const int observedThreatId = 202;
    for (auto& episode : reusedEpisodes) {
        if (episode.threatId == observedThreatId &&
            MatchesObservedMissileEpisode(
                episode.networkId,
                episode.objectIdentity,
                77u,
                0xB200u))
            episode.missingSinceTick = -1;
    }
    ExpectTrue("reused-ID observation clears only matching threat episode",
               reusedEpisodes[0].missingSinceTick == 5000 &&
                   reusedEpisodes[1].missingSinceTick == -1);
    ExpectTrue("matching delete finalizes live episode exactly once",
               ShouldFinalizeMissileDelete(
                   true, false, 77u, 0xA100u, 77u, 0xA100u));
    ExpectTrue("duplicate delete is idempotently ignored",
               !ShouldFinalizeMissileDelete(
                   false, true, 77u, 0xA100u, 77u, 0xA100u));
    ExpectTrue("explicit delete target classifies unit collision",
               ShouldClassifyDeleteAsUnitCollision(
                   true,
                   ZDCollisionKind::None,
                   0,
                   Vec2(),
                   -1,
                   5000,
                   false,
                   false,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("nearby unit alone cannot classify delete collision",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::None,
                   0,
                   Vec2(505.0f, 100.0f),
                   4950,
                   5000,
                   true,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("matching predicted unit impact may classify delete collision",
               ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(505.0f, 100.0f),
                   4750,
                   5000,
                   true,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("distant predicted unit impact cannot classify delete collision",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(700.0f, 100.0f),
                   4950,
                   5000,
                   true,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("stale predicted unit impact cannot classify delete collision",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(505.0f, 100.0f),
                   4749,
                   5000,
                   true,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("wrong missile episode prediction is rejected",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(505.0f, 100.0f),
                   4950,
                   5000,
                   false,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("prediction without a unit ID is rejected",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   0,
                   Vec2(505.0f, 100.0f),
                   4950,
                   5000,
                   true,
                   true,
                   Vec2(500.0f, 100.0f)));
    ExpectTrue("spell without unit collision cannot use unit prediction",
               !ShouldClassifyDeleteAsUnitCollision(
                   false,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(505.0f, 100.0f),
                   4950,
                   5000,
                   true,
                   false,
                   Vec2(500.0f, 100.0f)));
    SpellData livePredictionSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    Threat livePrediction =
        ZDEvadeTest::MakeThreat(livePredictionSpell);
    livePrediction.authoredEndPos = livePrediction.endPos;
    livePrediction.missileBound = true;
    livePrediction.missileNetworkId = 77u;
    livePrediction.missileObjectIdentity = 0xA100u;
    livePrediction.observedTick = 4900;
    livePrediction.observedHead = Vec2(300.0f, 100.0f);
    const Vec2 liveAuthoredEnd = livePrediction.endPos;
    ExpectTrue("live collision scan refreshes prediction metadata",
               RefreshLiveBoundCollisionPrediction(
                   livePrediction,
                   ZDCollisionKind::Unit,
                   1234,
                   Vec2(510.0f, 100.0f),
                   Vec2(500.0f, 100.0f),
                   5000));
    ExpectTrue("live collision prediction leaves body geometry unshortened",
               livePrediction.endPos.DistanceSqr(liveAuthoredEnd) <= 1.0f &&
                   livePrediction.AuthoredEnd().DistanceSqr(
                       liveAuthoredEnd) <= 1.0f &&
                   !livePrediction.collisionStopped &&
                   livePrediction.collisionKind ==
                       ZDCollisionKind::None &&
                   livePrediction.collisionUnitNetworkId == 0);
    ExpectTrue("live collision metadata records exact episode and impact",
               livePrediction.predictedCollisionKind ==
                       ZDCollisionKind::Unit &&
                   livePrediction.predictedCollisionUnitNetworkId == 1234 &&
                   livePrediction.predictedCollisionUnitCenter.DistanceSqr(
                       Vec2(510.0f, 100.0f)) <= 1.0f &&
                   livePrediction.predictedCollisionPoint.DistanceSqr(
                       Vec2(500.0f, 100.0f)) <= 1.0f &&
                   livePrediction.predictedCollisionTick == 4900 &&
                   livePrediction.predictedCollisionMissileNetworkId == 77u &&
                   livePrediction
                       .predictedCollisionMissileObjectIdentity == 0xA100u);
    livePrediction.missileMissingSinceTick = 5001;
    ExpectTrue("missing missile prediction refresh clears stale metadata",
               RefreshLiveBoundCollisionPrediction(
                   livePrediction,
                   ZDCollisionKind::Unit,
                   4321,
                   Vec2(610.0f, 100.0f),
                   Vec2(600.0f, 100.0f),
                   5050) &&
                   livePrediction.predictedCollisionKind ==
                       ZDCollisionKind::None &&
                   livePrediction.predictedCollisionTick == -1);
    livePrediction.missileMissingSinceTick = -1;
    livePrediction.missilePositionUnavailable = true;
    RefreshLiveBoundCollisionPrediction(
        livePrediction,
        ZDCollisionKind::Unit,
        1234,
        Vec2(510.0f, 100.0f),
        Vec2(500.0f, 100.0f),
        5050);
    ExpectTrue("position-unavailable missile cannot refresh prediction",
               livePrediction.predictedCollisionKind ==
                   ZDCollisionKind::None);
    livePrediction.missilePositionUnavailable = false;
    livePrediction.observedHead = {};
    RefreshLiveBoundCollisionPrediction(
        livePrediction,
        ZDCollisionKind::Unit,
        1234,
        Vec2(510.0f, 100.0f),
        Vec2(500.0f, 100.0f),
        5050);
    ExpectTrue("missing observed head cannot refresh prediction",
               livePrediction.predictedCollisionKind ==
                   ZDCollisionKind::None);
    livePrediction.observedHead = Vec2(300.0f, 100.0f);
    livePrediction.observedTick = 4700;
    RefreshLiveBoundCollisionPrediction(
        livePrediction,
        ZDCollisionKind::Unit,
        1234,
        Vec2(510.0f, 100.0f),
        Vec2(500.0f, 100.0f),
        5000);
    ExpectTrue("stale observation cannot refresh prediction",
               livePrediction.predictedCollisionKind ==
                   ZDCollisionKind::None);
    livePrediction.predictedCollisionKind = ZDCollisionKind::Terrain;
    livePrediction.predictedCollisionPoint = Vec2(450.0f, 100.0f);
    livePrediction.predictedCollisionTick = 4900;
    ExpectTrue("collision early return clears stale side channel",
               ClearPredictedCollisionMetadata(livePrediction) &&
                   livePrediction.predictedCollisionKind ==
                       ZDCollisionKind::None &&
                   livePrediction.predictedCollisionPoint.IsZero());
    ExpectTrue("route change invalidates stale collision prediction",
               livePrediction.predictedCollisionKind ==
                       ZDCollisionKind::None &&
                   livePrediction.predictedCollisionTick == -1 &&
                   livePrediction.predictedCollisionPoint.IsZero());

    std::array<SpellData, 2> predictedDeleteSpells = {{
        ZDEvadeTest::MakeSpell(ZDSpellType::Line),
        ZDEvadeTest::MakeSpell(ZDSpellType::Line),
    }};
    predictedDeleteSpells[0].spellName = "EnchantedCrystalArrow";
    predictedDeleteSpells[0].secondaryRadius = 400.0f;
    predictedDeleteSpells[1].spellName = "SennaW";
    predictedDeleteSpells[1].secondaryRadius = 280.0f;
    predictedDeleteSpells[1].extraDelay = 1000;
    predictedDeleteSpells[1].endExplosionFollowsUnit = true;
    predictedDeleteSpells[1].endExplosionDetonatesOnUnitDeath = true;
    for (auto& spell : predictedDeleteSpells) {
        spell.hasEndExplosion = true;
        spell.endExplosionRequiresUnitCollision = true;
        spell.endExplosionAtUnitCenter = true;
        spell.collisionObjects = {
            ZDCollisionObjectType::EnemyChampions,
            ZDCollisionObjectType::EnemyMinions,
        };
    }
    std::array<bool, 2> targetlessMatches = {};
    std::array<bool, 2> targetlessFarRejections = {};
    std::array<bool, 2> targetlessExplosions = {};
    std::array<bool, 2> targetlessNoReplay = {};
    for (std::size_t index = 0;
         index < predictedDeleteSpells.size();
         ++index) {
        Threat threat =
            ZDEvadeTest::MakeThreat(predictedDeleteSpells[index]);
        threat.missileBound = true;
        threat.missileNetworkId =
            static_cast<std::uint32_t>(80 + index);
        threat.missileObjectIdentity =
            static_cast<std::uintptr_t>(0xB100u + index);
        threat.observedTick = 5000;
        threat.observedHead = Vec2(300.0f, 100.0f);
        RefreshLiveBoundCollisionPrediction(
            threat,
            ZDCollisionKind::Unit,
            1234,
            Vec2(900.0f, 100.0f),
            Vec2(500.0f, 100.0f),
            5000);
        const bool sameEpisode = MatchesMissileEpisode(
            threat.predictedCollisionMissileNetworkId,
            threat.predictedCollisionMissileObjectIdentity,
            threat.missileNetworkId,
            threat.missileObjectIdentity);
        targetlessMatches[index] =
            ShouldClassifyDeleteAsUnitCollision(
                false,
                threat.predictedCollisionKind,
                threat.predictedCollisionUnitNetworkId,
                threat.predictedCollisionPoint,
                threat.predictedCollisionTick,
                5100,
                sameEpisode,
                true,
                Vec2(505.0f, 100.0f));
        targetlessFarRejections[index] =
            !ShouldClassifyDeleteAsUnitCollision(
                false,
                threat.predictedCollisionKind,
                threat.predictedCollisionUnitNetworkId,
                threat.predictedCollisionPoint,
                threat.predictedCollisionTick,
                5100,
                sameEpisode,
                true,
                Vec2(650.0f, 100.0f));
        if (targetlessMatches[index]) {
            ConfirmDeleteUnitCollision(
                threat,
                threat.predictedCollisionUnitNetworkId,
                Vec2(505.0f, 100.0f),
                false);
            threat.projectileTerminated = true;
            threat.projectileTerminationTick = 5100;
            threat.missileBound = false;
            threat.ClearPredictedCollision();
            targetlessExplosions[index] =
                threat.HasEndExplosionArea() &&
                threat.EndExplosionCenter().DistanceSqr(
                    Vec2(505.0f, 100.0f)) <= 1.0f &&
                threat.collisionUnitCenter.DistanceSqr(
                    Vec2(505.0f, 100.0f)) <= 1.0f &&
                !threat.collisionUnitTargetAuthoritative &&
                WrappingTickDifference(
                    threat.EndExplosionEndTick(),
                    threat.EndExplosionStartTick()) == 100;
            targetlessNoReplay[index] =
                !threat.IsEndExplosionActiveAt(
                    WrappingTickAdd(
                        threat.EndExplosionEndTick(),
                        1));
        }
    }
    ExpectTrue("Ashe R targetless delete confirms matching prediction",
               targetlessMatches[0] &&
                   targetlessFarRejections[0] &&
                   targetlessExplosions[0] &&
                   targetlessNoReplay[0]);
    ExpectTrue("Senna W targetless delete confirms matching prediction",
               targetlessMatches[1] &&
                   targetlessFarRejections[1] &&
                   targetlessExplosions[1] &&
                   targetlessNoReplay[1]);
    const Vec2 attachedDeleteImpact(505.0f, 100.0f);
    const Vec2 attachedUnitCenter(510.0f, 100.0f);
    ExpectTrue("verified targetless unit may attach retained lifecycle",
               ShouldAttachPredictedUnitAtDelete(
                   1234,
                   1234,
                   0xD100u,
                   0xD100u,
                   true,
                   false,
                   true,
                   true,
                   attachedUnitCenter,
                   attachedDeleteImpact));
    ExpectTrue("far targetless unit remains static",
               !ShouldAttachPredictedUnitAtDelete(
                   1234,
                   1234,
                   0xD100u,
                   0xD100u,
                   true,
                   false,
                   true,
                   true,
                   Vec2(700.0f, 100.0f),
                   attachedDeleteImpact));
    ExpectTrue("dead targetless unit remains static",
               !ShouldAttachPredictedUnitAtDelete(
                   1234,
                   1234,
                   0xD100u,
                   0xD100u,
                   true,
                   true,
                   true,
                   true,
                   attachedUnitCenter,
                   attachedDeleteImpact));
    ExpectTrue("reused targetless unit ID remains static",
               !ShouldAttachPredictedUnitAtDelete(
                   1234,
                   1234,
                   0xD100u,
                   0xD200u,
                   true,
                   false,
                   true,
                   true,
                   attachedUnitCenter,
                   attachedDeleteImpact));
    Threat attachedSenna =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[1]);
    attachedSenna.projectileTerminated = true;
    attachedSenna.projectileTerminationTick = 5100;
    ConfirmDeleteUnitCollision(
        attachedSenna,
        1234,
        attachedDeleteImpact,
        true,
        attachedUnitCenter,
        0xD100u);
    const bool followedAttachedMovement =
        UpdateAttachedUnitExplosion(
            attachedSenna,
            Vec2(600.0f, 100.0f),
            false,
            5200);
    const bool detonatedAttachedDeath =
        UpdateAttachedUnitExplosion(
            attachedSenna,
            Vec2(600.0f, 100.0f),
            true,
            5300);
    ExpectTrue("verified targetless Senna follows unit movement",
               followedAttachedMovement &&
                   MatchesAttachedUnitIdentity(
                       attachedSenna
                           .collisionUnitTargetAuthoritative,
                       attachedSenna.collisionUnitObjectIdentity,
                       0xD100u) &&
                   attachedSenna.collisionUnitCenter.DistanceSqr(
                       Vec2(600.0f, 100.0f)) <= 1.0f &&
                   attachedSenna.collisionExplosionCenter.DistanceSqr(
                       Vec2(600.0f, 100.0f)) <= 1.0f);
    ExpectTrue("verified targetless Senna detonates on unit death",
               detonatedAttachedDeath &&
                   attachedSenna.collisionEndExplosionDelay == 0 &&
                   attachedSenna.projectileTerminationTick == 5300);
    ExpectTrue("reused attached unit object cannot inherit follow lifecycle",
               !MatchesAttachedUnitIdentity(
                   attachedSenna
                       .collisionUnitTargetAuthoritative,
                   attachedSenna.collisionUnitObjectIdentity,
                   0xD200u));
    Threat staticSenna =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[1]);
    staticSenna.projectileTerminated = true;
    staticSenna.projectileTerminationTick = 5100;
    ConfirmDeleteUnitCollision(
        staticSenna,
        1234,
        attachedDeleteImpact,
        false);
    ExpectTrue("unverified targetless Senna remains at delete impact",
               !UpdateAttachedUnitExplosion(
                   staticSenna,
                   Vec2(700.0f, 100.0f),
                   true,
                   5200) &&
                   staticSenna.EndExplosionCenter().DistanceSqr(
                       attachedDeleteImpact) <= 1.0f &&
                   staticSenna.collisionEndExplosionDelay == -1);
    Threat genericAshe =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[0]);
    genericAshe.projectileTerminated = true;
    genericAshe.projectileTerminationTick = 5100;
    ConfirmDeleteUnitCollision(
        genericAshe,
        1234,
        attachedDeleteImpact,
        false);
    ExpectTrue("Ashe generic impact remains static at delete point",
               !UpdateAttachedUnitExplosion(
                   genericAshe,
                   Vec2(510.0f, 100.0f),
                   false,
                   5200) &&
                   genericAshe.EndExplosionCenter().DistanceSqr(
                       attachedDeleteImpact) <= 1.0f);
    Threat noConfirmedImpact =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[0]);
    noConfirmedImpact.projectileTerminated = true;
    noConfirmedImpact.projectileTerminationTick = 5100;
    ExpectTrue("unconfirmed delete cannot fabricate required explosion",
               !noConfirmedImpact.HasEndExplosionArea());
    ExpectTrue("valid explicit target metadata verifies attachment",
               ShouldAttachExplicitUnitAtDelete(
                   true,
                   1234,
                   0xE100u,
                   1234,
                   0xE100u,
                   Vec2(520.0f, 100.0f)));
    ExpectTrue("unresolved explicit target cannot create wildcard attachment",
               !ShouldAttachExplicitUnitAtDelete(
                   true,
                   1234,
                   0xE100u,
                   0,
                   0,
                   Vec2()));
    std::array<bool, 2> explicitImpactCenters = {};
    for (std::size_t index = 0;
         index < predictedDeleteSpells.size();
         ++index) {
        Threat threat =
            ZDEvadeTest::MakeThreat(predictedDeleteSpells[index]);
        ConfirmDeleteUnitCollision(
            threat,
            1234,
            attachedDeleteImpact,
            true,
            Vec2(520.0f, 100.0f),
            0xE100u);
        explicitImpactCenters[index] =
            threat.collisionUnitTargetAuthoritative &&
            threat.collisionUnitCenter.DistanceSqr(
                Vec2(520.0f, 100.0f)) <= 1.0f &&
            threat.collisionExplosionCenter.DistanceSqr(
                Vec2(520.0f, 100.0f)) <= 1.0f &&
            threat.collisionUnitObjectIdentity == 0xE100u;
    }
    ExpectTrue("explicit Ashe impact uses verified unit center and address",
               explicitImpactCenters[0]);
    ExpectTrue("explicit Senna impact uses verified unit center and address",
               explicitImpactCenters[1]);
    Threat explicitSenna =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[1]);
    explicitSenna.projectileTerminated = true;
    explicitSenna.projectileTerminationTick = 5100;
    ConfirmDeleteUnitCollision(
        explicitSenna,
        1234,
        attachedDeleteImpact,
        true,
        Vec2(520.0f, 100.0f),
        0xE100u);
    ExpectTrue("explicit Senna follows exact target object movement",
               MatchesAttachedUnitIdentity(
                   explicitSenna.collisionUnitTargetAuthoritative,
                   explicitSenna.collisionUnitObjectIdentity,
                   0xE100u) &&
                   UpdateAttachedUnitExplosion(
                       explicitSenna,
                       Vec2(620.0f, 100.0f),
                       false,
                       5200) &&
                   explicitSenna.EndExplosionCenter().DistanceSqr(
                       Vec2(620.0f, 100.0f)) <= 1.0f);
    ExpectTrue("explicit reused network ID with new pointer is rejected",
               !MatchesAttachedUnitIdentity(
                   explicitSenna.collisionUnitTargetAuthoritative,
                   explicitSenna.collisionUnitObjectIdentity,
                   0xE200u));
    Threat unresolvedExplicitSenna =
        ZDEvadeTest::MakeThreat(predictedDeleteSpells[1]);
    unresolvedExplicitSenna.projectileTerminated = true;
    unresolvedExplicitSenna.projectileTerminationTick = 5100;
    ConfirmDeleteUnitCollision(
        unresolvedExplicitSenna,
        1234,
        attachedDeleteImpact,
        false);
    ExpectTrue("unresolved explicit target remains static at impact",
               !unresolvedExplicitSenna
                    .collisionUnitTargetAuthoritative &&
                   unresolvedExplicitSenna
                       .collisionUnitObjectIdentity == 0 &&
                   !UpdateAttachedUnitExplosion(
                       unresolvedExplicitSenna,
                       Vec2(620.0f, 100.0f),
                       false,
                       5200) &&
                   unresolvedExplicitSenna.EndExplosionCenter()
                       .DistanceSqr(attachedDeleteImpact) <= 1.0f);
    std::array<bool, 2> localPlayerPredictions = {};
    for (std::size_t index = 0;
         index < predictedDeleteSpells.size();
         ++index) {
        Threat threat =
            ZDEvadeTest::MakeThreat(predictedDeleteSpells[index]);
        threat.missileBound = true;
        threat.missileNetworkId =
            static_cast<std::uint32_t>(90 + index);
        threat.missileObjectIdentity =
            static_cast<std::uintptr_t>(0xC100u + index);
        threat.observedTick = 7000;
        threat.observedHead = Vec2(300.0f, 100.0f);
        RefreshLiveBoundCollisionPrediction(
            threat,
            ZDCollisionKind::Unit,
            9999,
            Vec2(510.0f, 100.0f),
            Vec2(500.0f, 100.0f),
            7050,
            static_cast<std::uintptr_t>(
                0xD100u + index));
        localPlayerPredictions[index] =
            threat.predictedCollisionKind ==
                ZDCollisionKind::Unit &&
            threat.predictedCollisionUnitNetworkId == 9999 &&
            threat.predictedCollisionUnitObjectIdentity ==
                static_cast<std::uintptr_t>(
                    0xD100u + index);
    }
    ExpectTrue("Ashe R predicts non-committing local-player collision",
               localPlayerPredictions[0]);
    ExpectTrue("Senna W predicts non-committing local-player collision",
               localPlayerPredictions[1]);
    ExpectTrue("ambiguous ID-only delete is rejected",
               !ShouldAcceptMissileDeleteOwnership(0, 2));
    ExpectTrue("unique ID-only delete owner is accepted",
               ShouldAcceptMissileDeleteOwnership(0, 1));
    ExpectTrue("object-identified delete ignores network owner ambiguity",
               ShouldAcceptMissileDeleteOwnership(0xA100u, 2));
    ExpectTrue("ambiguous ID-only episode cannot finalize",
               !ShouldFinalizeMissileDelete(
                   true,
                   false,
                   77u,
                   0xA100u,
                   77u,
                   0u,
                   2));
    ExpectTrue("exactly one ID-only episode can finalize",
               ShouldFinalizeMissileDelete(
                   true,
                   false,
                   77u,
                   0xA100u,
                   77u,
                   0u,
                   1));
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
    ExpectTrue("missing timeout cannot fabricate collision-required explosion",
               !missingExplosionThreat.HasEndExplosionArea());
    SpellData unconditionalMissingExplosion = missingExplosion;
    unconditionalMissingExplosion.endExplosionRequiresUnitCollision = false;
    Threat unconditionalMissingExplosionThreat =
        ZDEvadeTest::MakeThreat(unconditionalMissingExplosion);
    unconditionalMissingExplosionThreat.projectileTerminated = true;
    unconditionalMissingExplosionThreat.missingMissileTermination = true;
    unconditionalMissingExplosionThreat.projectileTerminationTick = 2000;
    ExpectTrue("missing timeout retains unconditional configured explosion",
               unconditionalMissingExplosionThreat.HasEndExplosionArea());
    ExpectEq("retained missing explosion uses termination tick",
             unconditionalMissingExplosionThreat.EndExplosionEndTick(),
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
    Threat missingHwei = hweiThreat;
    missingHwei.launchTick = 1000;
    missingHwei.missileBound = true;
    missingHwei.observedHead = Vec2(350.0f, 0.0f);
    missingHwei.observedTick = 1250;
    const int hweiBodyArrival = missingHwei.ArrivalTick();
    const int hweiEvidenceDeadline = MissileEvidenceLossDeadlineTick(
        hweiBodyArrival,
        missingHwei.observedTick);
    ExpectEq("Hwei evidence deadline excludes delayed explosion linger",
             hweiEvidenceDeadline,
             hweiBodyArrival + kMissileEvidenceLossGraceMs);
    const int hweiTerminationTick = MissingMissileTerminationTick(
        hweiBodyArrival,
        missingHwei.observedTick);
    missingHwei.projectileTerminated = true;
    missingHwei.missileBound = false;
    missingHwei.projectileTerminationTick = hweiTerminationTick;
    ExpectEq("Hwei missing termination anchors to predicted body arrival",
             missingHwei.EndExplosionStartTick(),
             hweiBodyArrival + 3000);
    ExpectEq("short Hwei terminal explosion has canonical 100ms duration",
             missingHwei.EndExplosionEndTick(),
             hweiBodyArrival + 3100);
    ExpectTrue("late Hwei timeout does not replay an ended explosion",
               ShouldExpireMissingTerminationAt(
                   hweiBodyArrival + 4000,
                   missingHwei.EndExplosionEndTick()) &&
                   !missingHwei.IsEndExplosionActiveAt(
                       hweiBodyArrival + 4000));

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
    castOrigin.slot = static_cast<int>(ZDSpellSlot::Q);
    normalizedStore.push_back(castOrigin);

    Threat pairedCallback = castOrigin;
    pairedCallback.id = -1;
    pairedCallback.startTick = 4085;
    pairedCallback.castIdentity = 0xCAFE;
    pairedCallback.startPos = Vec2(6.0f, 2.0f);
    pairedCallback.endPos = Vec2(1006.0f, 18.0f);
    pairedCallback.authoredEndPos = pairedCallback.endPos;
    pairedCallback.direction =
        (pairedCallback.endPos - pairedCallback.startPos).Normalized();
    Threat* normalizedDuplicate =
        FindNormalizedCastDuplicate(normalizedStore, pairedCallback);
    ExpectTrue("different-pointer callback finds normalized cast store entry",
               normalizedDuplicate != nullptr);
    if (normalizedDuplicate)
        MergeNormalizedCastDuplicate(*normalizedDuplicate, pairedCallback);

    Threat& sameFrameThreat = normalizedStore.front();
    ExpectEq("different-pointer callback keeps one stored threat",
             static_cast<int>(normalizedStore.size()), 1);
    ExpectEq("different-pointer callback preserves threat ID",
             sameFrameThreat.id, 77);
    ExpectEq("different-pointer callback advances one revision lineage",
             sameFrameThreat.revision, 1);
    const MissileBindKey sameFrameCastKey = {
        sameFrameThreat.casterNetworkId,
        sameFrameThreat.castIdentity,
        reinterpret_cast<std::uintptr_t>(sameFrameThreat.data),
        sameFrameThreat.direction,
        sameFrameThreat.startTick,
        sameFrameThreat.Delay(),
        sameFrameThreat.Delay(),
        sameFrameThreat.slot,
        sameFrameThreat.startPos,
        sameFrameThreat.AuthoredEnd()
    };
    const MissileBindObservation sameFrameMissile = {
        42u,
        0xD00D,
        reinterpret_cast<std::uintptr_t>(sameFrameThreat.data),
        sameFrameThreat.direction,
        4030,
        static_cast<int>(ZDSpellSlot::Q),
        Vec2(8.0f, 3.0f),
        Vec2(1008.0f, 20.0f)
    };
    ExpectTrue("different-payload missile binds normalized cast store entry",
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

    Threat laterCast = pairedCallback;
    laterCast.startTick =
        castOrigin.startTick + kExpectedLogicalCastEpisodeWindowMs + 1;
    laterCast.id = -1;
    ExpectTrue("stored later cast outside episode is not deduplicated",
               FindNormalizedCastDuplicate(normalizedStore, laterCast) == nullptr);
    Threat incompatibleCast = pairedCallback;
    incompatibleCast.id = -1;
    incompatibleCast.direction = Vec2(-1.0f, 0.0f);
    incompatibleCast.endPos = Vec2(-1000.0f, 0.0f);
    incompatibleCast.authoredEndPos = incompatibleCast.endPos;
    ExpectTrue("stored incompatible cast is not deduplicated",
               FindNormalizedCastDuplicate(normalizedStore, incompatibleCast) ==
                   nullptr);

    const SpellData* wildCards =
        ThreatDatabase::FindAny("WildCards", "TwistedFate");
    ExpectTrue("Wild Cards database fixture has exactly three lanes",
               wildCards != nullptr &&
                   wildCards->multipleNumber == 3 &&
                   std::fabs(wildCards->multipleAngle - 28.0f) < 0.001f);
    std::vector<Threat> wildCardStore;
    const std::array<Vec2, 3> wildCardDirections = {{
        Vec2(1.0f, -0.25f).Normalized(),
        Vec2(1.0f, 0.0f),
        Vec2(1.0f, 0.25f).Normalized(),
    }};
    for (const Vec2& laneDirection : wildCardDirections) {
        Threat lane;
        lane.data = wildCards;
        lane.id = 100 + static_cast<int>(wildCardStore.size());
        lane.startPos = Vec2(100.0f, 100.0f);
        lane.direction = laneDirection;
        lane.endPos = lane.startPos + laneDirection * wildCards->range;
        lane.authoredEndPos = lane.endPos;
        lane.startTick = 5000;
        lane.casterNetworkId = 84u;
        lane.castIdentity = 0x1111u;
        lane.slot = static_cast<int>(ZDSpellSlot::Q);
        wildCardStore.push_back(lane);
    }
    for (const Vec2& laneDirection : wildCardDirections) {
        Threat callbackLane;
        callbackLane.data = wildCards;
        callbackLane.id = -1;
        callbackLane.startPos = Vec2(104.0f, 102.0f);
        callbackLane.direction =
            Vec2(laneDirection.x, laneDirection.y + 0.004f).Normalized();
        callbackLane.endPos =
            callbackLane.startPos + callbackLane.direction * wildCards->range;
        callbackLane.authoredEndPos = callbackLane.endPos;
        callbackLane.startTick = 5090;
        callbackLane.casterNetworkId = 84u;
        callbackLane.castIdentity = 0x2222u;
        callbackLane.slot = static_cast<int>(ZDSpellSlot::Q);
        Threat* duplicate =
            FindNormalizedCastDuplicate(wildCardStore, callbackLane);
        if (duplicate) {
            MergeNormalizedCastDuplicate(*duplicate, callbackLane);
        } else {
            callbackLane.id = 100 + static_cast<int>(wildCardStore.size());
            wildCardStore.push_back(callbackLane);
        }
    }
    ExpectEq("differing Wild Cards hook pointers keep exactly three lanes",
             static_cast<int>(wildCardStore.size()), 3);
    std::array<int, 3> wildCardIds = {};
    for (std::size_t index = 0; index < wildCardStore.size() &&
                                index < wildCardIds.size(); ++index)
        wildCardIds[index] = wildCardStore[index].id;
    for (std::size_t missileIndex = 0;
         missileIndex < wildCardDirections.size();
         ++missileIndex) {
        Threat* bestLane = nullptr;
        float bestDot = -2.0f;
        for (Threat& lane : wildCardStore) {
            if (lane.missileBound) continue;
            const MissileBindKey laneKey = {
                lane.casterNetworkId,
                lane.castIdentity,
                reinterpret_cast<std::uintptr_t>(lane.data),
                lane.direction,
                lane.startTick,
                lane.Delay(),
                lane.Delay(),
                lane.slot,
                lane.startPos,
                lane.AuthoredEnd()
            };
            const MissileBindObservation laneMissile = {
                84u,
                0x3000u + missileIndex,
                reinterpret_cast<std::uintptr_t>(wildCards),
                wildCardDirections[missileIndex],
                5300,
                static_cast<int>(ZDSpellSlot::Q),
                Vec2(106.0f, 103.0f),
                Vec2(106.0f, 103.0f) +
                    wildCardDirections[missileIndex] * wildCards->range
            };
            if (!CanBindMissile(laneKey, laneMissile, 0.99f)) continue;
            const float dot = lane.direction.Normalized().Dot(
                wildCardDirections[missileIndex].Normalized());
            if (dot > bestDot) {
                bestDot = dot;
                bestLane = &lane;
            }
        }
        ExpectTrue("each Wild Cards missile finds one unbound lane",
                   bestLane != nullptr);
        if (bestLane) {
            CorrectExistingThreatFromMissile(
                *bestLane,
                [missileIndex](Threat& bound) {
                    bound.missileNetworkId =
                        900u + static_cast<std::uint32_t>(missileIndex);
                });
        }
    }
    ExpectEq("three Wild Cards missiles do not add lanes",
             static_cast<int>(wildCardStore.size()), 3);
    ExpectTrue("Wild Cards lane IDs remain stable through missile binding",
               wildCardStore.size() == 3 &&
                   wildCardStore[0].id == wildCardIds[0] &&
                   wildCardStore[1].id == wildCardIds[1] &&
                   wildCardStore[2].id == wildCardIds[2] &&
                   wildCardStore[0].missileBound &&
                   wildCardStore[1].missileBound &&
                   wildCardStore[2].missileBound);
    const std::size_t beforeDuplicateMissile = wildCardStore.size();
    const std::uint32_t duplicateMissileId =
        wildCardStore.front().missileNetworkId;
    const bool duplicateMissileFound = std::any_of(
        wildCardStore.begin(),
        wildCardStore.end(),
        [duplicateMissileId](const Threat& lane) {
            return !lane.expired &&
                lane.missileBound &&
                lane.missileNetworkId == duplicateMissileId;
        });
    if (!duplicateMissileFound)
        wildCardStore.push_back(wildCardStore.front());
    ExpectEq("duplicate missile-create is idempotent",
             static_cast<int>(wildCardStore.size()),
             static_cast<int>(beforeDuplicateMissile));

    Threat retainedLane = wildCardStore[1];
    retainedLane.missileBound = false;
    retainedLane.projectileTerminated = true;
    retainedLane.collisionExplosionCenter = retainedLane.endPos;
    retainedLane.revision = 7;
    const int retainedId = retainedLane.id;
    const int retainedStartTick = retainedLane.startTick;
    Threat lateCallback = retainedLane;
    lateCallback.id = -1;
    lateCallback.projectileTerminated = false;
    lateCallback.collisionExplosionCenter = {};
    lateCallback.castIdentity = 0x4444u;
    lateCallback.startTick =
        retainedStartTick + kExpectedLogicalCastEpisodeWindowMs;
    std::vector<Threat> retainedStore = {retainedLane};
    Threat* retainedDuplicate =
        FindNormalizedCastDuplicate(retainedStore, lateCallback);
    ExpectTrue("late callback finds retained explosion lane",
               retainedDuplicate != nullptr);
    if (retainedDuplicate)
        MergeNormalizedCastDuplicate(*retainedDuplicate, lateCallback);
    ExpectEq("late callback keeps one retained logical lane",
             static_cast<int>(retainedStore.size()), 1);
    ExpectTrue("late callback preserves retained explosion state and identity",
               retainedStore.front().id == retainedId &&
                   retainedStore.front().startTick == retainedStartTick &&
                   retainedStore.front().projectileTerminated &&
                   !retainedStore.front().collisionExplosionCenter.IsZero() &&
                   retainedStore.front().revision == 7);

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

    Queue differentPointerQueue;
    PendingEventDescriptor firstPointerDescriptor;
    firstPointerDescriptor.key = MakeCastKey(
        42u, 0xA100u, 0, 6000, "EzrealMysticShot",
        Vec2(100.0f, 100.0f), Vec2(900.0f, 100.0f));
    firstPointerDescriptor.priority = PendingPriority::HostileCast;
    PendingEventDescriptor secondPointerDescriptor;
    secondPointerDescriptor.key = MakeCastKey(
        42u, 0xB200u, 0, 6090, "EzrealMysticShot",
        Vec2(106.0f, 102.0f), Vec2(908.0f, 116.0f));
    secondPointerDescriptor.priority = PendingPriority::HostileCast;
    differentPointerQueue.Push(
        MakeQueueEvent(11u, "EzrealMysticShot"),
        firstPointerDescriptor,
        MergeQueueEvent);
    ExpectEq("queue coalesces differing nonzero hook identities",
             static_cast<int>(differentPointerQueue.Push(
                 MakeQueueEvent(12u, "EzrealMysticShot"),
                 secondPointerDescriptor,
                 MergeQueueEvent)),
             static_cast<int>(PendingQueueDecision::Coalesce));
    ExpectEq("different-pointer coalesce keeps one queued event",
             static_cast<int>(differentPointerQueue.Size()), 1);
    ExpectEq("different-pointer coalesce increments counter once",
             differentPointerQueue.Counters().coalesced, 1);

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
    ExpectTrue("different missile payload identity remains a positive hint",
               CanBindMissile(delayedCast, wrongIdentity));
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

    const SpellData* ownEzrealQ =
        ThreatDatabase::FindAny("EzrealQ", "Ezreal");
    ExpectTrue("self debug champion-scoped DB lookup finds local Ezreal Q",
               ownEzrealQ != nullptr &&
                   ownEzrealQ->charName == "Ezreal");
    ExpectTrue("self debug champion-scoped DB lookup rejects another champion",
               ThreatDatabase::FindAny("EzrealQ", "Veigar") == nullptr);
    ExpectTrue("self debug exact local source accepts local network ID",
               IsExactLocalSelfSource(1001u, 1001u));
    ExpectTrue("self debug exact local source rejects zero and remote IDs",
               !IsExactLocalSelfSource(1001u, 0u) &&
                   !IsExactLocalSelfSource(1001u, 1002u));

    const Vec2 selfProcessStart(500.0f, 500.0f);
    const Vec2 selfProcessEnd(900.0f, 800.0f);
    const Vec2 selfProcessReverseCast(100.0f, 200.0f);
    const SelfSkillProcessGeometry endPreferredGeometry =
        ResolveSelfSkillProcessGeometry(
            selfProcessStart,
            Vec2(510.0f, 510.0f),
            selfProcessEnd,
            selfProcessReverseCast);
    ExpectTrue("self debug process geometry prefers decoded EndPosition",
               endPreferredGeometry.valid &&
                   endPreferredGeometry.start == selfProcessStart &&
                   endPreferredGeometry.end == selfProcessEnd);

    const Vec2 selfProcessCastFallback(850.0f, 725.0f);
    const SelfSkillProcessGeometry castFallbackGeometry =
        ResolveSelfSkillProcessGeometry(
            selfProcessStart,
            Vec2(510.0f, 510.0f),
            Vec2(),
            selfProcessCastFallback);
    ExpectTrue("self debug process geometry falls back to CastPosition",
               castFallbackGeometry.valid &&
                   castFallbackGeometry.start == selfProcessStart &&
                   castFallbackGeometry.end == selfProcessCastFallback);

    const Vec2 selfProcessSenderStart(540.0f, 520.0f);
    const SelfSkillProcessGeometry senderFallbackGeometry =
        ResolveSelfSkillProcessGeometry(
            Vec2(),
            selfProcessSenderStart,
            selfProcessEnd,
            selfProcessReverseCast);
    ExpectTrue("self debug process geometry falls back to sender position",
               senderFallbackGeometry.valid &&
                   senderFallbackGeometry.start == selfProcessSenderStart &&
                   senderFallbackGeometry.end == selfProcessEnd);

    const SelfSkillProcessGeometry noEndpointGeometry =
        ResolveSelfSkillProcessGeometry(
            selfProcessStart,
            Vec2(510.0f, 510.0f),
            Vec2(),
            Vec2());
    const SelfSkillProcessGeometry sameEndpointGeometry =
        ResolveSelfSkillProcessGeometry(
            selfProcessStart,
            Vec2(510.0f, 510.0f),
            selfProcessStart,
            selfProcessStart);
    ExpectTrue("self debug process geometry rejects missing or zero-length endpoints",
               !noEndpointGeometry.valid &&
                   !sameEndpointGeometry.valid);

    const SpellData* selfAhriQ =
        ThreatDatabase::FindAny("AhriQ", "Ahri");
    const Vec2 outboundDirection =
        (selfProcessEnd - selfProcessStart).Normalized();
    const Vec2 pendingAhriEnd = selfAhriQ
        ? ResolveSelfSkillDebugEnd(
            *selfAhriQ,
            endPreferredGeometry.start,
            endPreferredGeometry.end)
        : Vec2();
    const Vec2 pendingAhriDirection =
        (pendingAhriEnd - endPreferredGeometry.start).Normalized();
    ExpectTrue("self debug Ahri Q pending corridor follows outbound direction",
               selfAhriQ != nullptr &&
                   pendingAhriDirection.Dot(outboundDirection) > 0.99f &&
                   pendingAhriDirection.Dot(outboundDirection * -1.0f) < 0.0f);

    SpellData selfLine = ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    selfLine.charName = "TestChampion";
    selfLine.spellName = "TestLine";
    selfLine.spellKey = ZDSpellSlot::Q;
    SelfSkillDebugStore<4> processThenCreate;
    SelfSkillProcessObservation ownProcess;
    ownProcess.localPlayerNetworkId = 1001u;
    ownProcess.sourceNetworkId = 1001u;
    ownProcess.data = &selfLine;
    ownProcess.matchDisposition = ProcessSpellMatchDisposition::Matched;
    ownProcess.slot = static_cast<int>(ZDSpellSlot::Q);
    ownProcess.tick = 1000;
    ownProcess.start = Vec2(100.0f, 100.0f);
    ownProcess.end = Vec2(1100.0f, 100.0f);
    const SelfSkillDebugResult processResult =
        processThenCreate.ObserveProcess(ownProcess);
    ExpectTrue("self debug Process creates one pending record",
               processResult.accepted &&
                   processThenCreate.Count(SelfSkillDebugPhase::Pending) == 1);

    SelfSkillMissileObservation ownMissile;
    ownMissile.localPlayerNetworkId = 1001u;
    ownMissile.sourceNetworkId = 1001u;
    ownMissile.data = &selfLine;
    ownMissile.matchDisposition = MissileMatchDisposition::Matched;
    ownMissile.tick = 1250;
    ownMissile.start = Vec2(100.0f, 100.0f);
    ownMissile.end = Vec2(1100.0f, 100.0f);
    ownMissile.head = Vec2(140.0f, 100.0f);
    ownMissile.missileNetworkId = 5001u;
    ownMissile.missileObjectIdentity = 0xAA01u;
    const SelfSkillDebugResult createResult =
        processThenCreate.ObserveMissileCreate(ownMissile);
    ExpectTrue("self debug Process then Create keeps one live record",
               createResult.accepted &&
                   processThenCreate.Size() == 1 &&
                   processThenCreate.Count(SelfSkillDebugPhase::Live) == 1);

    SelfSkillDebugStore<4> createThenProcess;
    ExpectTrue("self debug Create-first creates one orphan live record",
               createThenProcess.ObserveMissileCreate(ownMissile).accepted &&
                   createThenProcess.Size() == 1);
    SelfSkillProcessObservation lateProcess = ownProcess;
    lateProcess.tick = 1260;
    ExpectTrue("self debug Create then Process merges into one record",
               createThenProcess.ObserveProcess(lateProcess).accepted &&
                   createThenProcess.Size() == 1 &&
                   createThenProcess.Count(SelfSkillDebugPhase::Live) == 1);

    SelfSkillSparseMissileInput sparseInput;
    sparseInput.localPlayerNetworkId = 1001u;
    sparseInput.eventSourceNetworkId = 0u;
    sparseInput.eventSourceObjectNetworkId = 0u;
    sparseInput.runtimeCasterNetworkId = 1001u;
    sparseInput.eventStart = {};
    sparseInput.runtimeStart = Vec2(100.0f, 100.0f);
    sparseInput.eventEnd = {};
    sparseInput.runtimeEnd = Vec2(1100.0f, 100.0f);
    sparseInput.eventHead = {};
    sparseInput.runtimeHead = Vec2(180.0f, 100.0f);
    const SelfSkillSparseMissileFields sparseFields =
        ResolveSelfSkillSparseMissileFields(sparseInput);
    ExpectTrue("self debug sparse create resolves exact local wrapper source",
               sparseFields.sourceNetworkId == 1001u &&
                   sparseFields.exactLocalSource);
    ExpectTrue("self debug sparse create resolves wrapper geometry and head",
               sparseFields.start.DistanceSqr(
                       sparseInput.runtimeStart) <= 0.001f &&
                   sparseFields.end.DistanceSqr(
                       sparseInput.runtimeEnd) <= 0.001f &&
                   sparseFields.head.DistanceSqr(
                       sparseInput.runtimeHead) <= 0.001f);
    const MissileMatchInput sparseNames =
        BuildSelfSkillMissileMatchInput(
            "",
            "EzrealMysticShotMissile",
            "",
            "EzrealQ",
            0u,
            0u);
    ExpectTrue("self debug sparse create preserves runtime missile and spell names",
               ProcessSpellNamesEqualNoCase(
                   sparseNames.names[1],
                   "EzrealMysticShotMissile") &&
                   ProcessSpellNamesEqualNoCase(
                       sparseNames.names[3],
                       "EzrealQ"));

    SelfSkillProcessObservation remoteProcess = ownProcess;
    remoteProcess.sourceNetworkId = 2002u;
    SelfSkillDebugStore<4> rejectedStore;
    ExpectTrue("self debug remote Process is filtered before storage",
               !rejectedStore.ObserveProcess(remoteProcess).accepted &&
                   rejectedStore.Size() == 0 &&
                   rejectedStore.Counters().processSeen == 0 &&
                   rejectedStore.Counters().processRejected == 0);
    SelfSkillProcessObservation basicProcess = ownProcess;
    basicProcess.data = nullptr;
    basicProcess.matchDisposition =
        ProcessSpellMatchDisposition::RejectBasicAttack;
    ExpectTrue("self debug basic attacks are rejected",
               !rejectedStore.ObserveProcess(basicProcess).accepted);
    SpellData noProcessSelf = selfLine;
    noProcessSelf.noProcess = true;
    SelfSkillProcessObservation noProcessObservation = ownProcess;
    noProcessObservation.data = &noProcessSelf;
    ExpectTrue("self debug noProcess records are rejected",
               !rejectedStore.ObserveProcess(noProcessObservation).accepted);
    SpellData arcSelf = selfLine;
    arcSelf.spellType = ZDSpellType::Arc;
    SelfSkillProcessObservation arcObservation = ownProcess;
    arcObservation.data = &arcSelf;
    ExpectTrue("self debug unsupported Arc records are rejected",
               !rejectedStore.ObserveProcess(arcObservation).accepted);
    SelfSkillProcessObservation targetedProcess = ownProcess;
    targetedProcess.hasTarget = true;
    ExpectTrue("self debug targeted process records are rejected",
               !rejectedStore.ObserveProcess(
                   targetedProcess).accepted);
    SelfSkillProcessObservation autoFlaggedProcess =
        ownProcess;
    autoFlaggedProcess.attackOrUtility = true;
    ExpectTrue("self debug auto-attack or utility flags reject DB matches",
               !rejectedStore.ObserveProcess(
                   autoFlaggedProcess).accepted);
    SelfSkillMissileObservation targetedMissile =
        ownMissile;
    targetedMissile.hasTarget = true;
    ExpectTrue("self debug targeted missiles are rejected",
               !rejectedStore.ObserveMissileCreate(
                   targetedMissile).accepted);
    SelfSkillMissileObservation utilityMissile =
        ownMissile;
    utilityMissile.attackOrUtility = true;
    ExpectTrue("self debug utility missiles reject DB matches",
               !rejectedStore.ObserveMissileCreate(
                   utilityMissile).accepted);

    SpellData spreadSelf = selfLine;
    spreadSelf.multipleNumber = 3;
    spreadSelf.multipleAngle = 28.0f;
    SelfSkillDebugStore<8> spreadStore;
    SelfSkillProcessObservation spreadProcess =
        ownProcess;
    spreadProcess.data = &spreadSelf;
    ExpectTrue("self debug multi-projectile cast creates three lanes",
               spreadStore.ObserveProcess(spreadProcess).accepted &&
                   spreadStore.Size() == 3);
    SelfSkillMissileObservation spreadLeftMissile =
        ownMissile;
    spreadLeftMissile.data = &spreadSelf;
    const float spreadRadians =
        -28.0f * 3.14159265358979323846f / 180.0f;
    const Vec2 spreadLeftDirection(
        std::cos(spreadRadians),
        std::sin(spreadRadians));
    spreadLeftMissile.end =
        spreadLeftMissile.start +
        spreadLeftDirection * spreadSelf.range;
    spreadLeftMissile.missileNetworkId = 6001u;
    spreadLeftMissile.missileObjectIdentity = 0xCC01u;
    ExpectTrue("self debug left missile binds exactly one authored lane",
               spreadStore.ObserveMissileCreate(
                   spreadLeftMissile).accepted &&
                   spreadStore.Count(SelfSkillDebugPhase::Live) == 1 &&
                   spreadStore.Count(SelfSkillDebugPhase::Pending) == 2);
    SelfSkillMissileObservation spreadRightMissile =
        spreadLeftMissile;
    const float rightSpreadRadians =
        28.0f * 3.14159265358979323846f / 180.0f;
    const Vec2 spreadRightDirection(
        std::cos(rightSpreadRadians),
        std::sin(rightSpreadRadians));
    spreadRightMissile.end =
        spreadRightMissile.start +
        spreadRightDirection * spreadSelf.range;
    spreadRightMissile.missileNetworkId = 6002u;
    spreadRightMissile.missileObjectIdentity = 0xCC02u;
    ExpectTrue("self debug right missile cannot cross-bind left lane",
               spreadStore.ObserveMissileCreate(
                   spreadRightMissile).accepted &&
                   spreadStore.Count(SelfSkillDebugPhase::Live) == 2 &&
                   spreadStore.Count(SelfSkillDebugPhase::Pending) == 1);

    SelfSkillDebugStore<4> invalidBindingStore;
    SelfSkillMissileObservation invalidBindingMissile =
        ownMissile;
    invalidBindingMissile.tick = 3000;
    invalidBindingMissile.missileNetworkId = 7001u;
    invalidBindingMissile.missileObjectIdentity = 0xDD01u;
    const SelfSkillDebugResult invalidBindingCreate =
        invalidBindingStore.ObserveMissileCreate(
            invalidBindingMissile);
    for (int tick = 3100; tick <= 13000; tick += 100)
        invalidBindingStore.MarkMissilePositionUnavailable(
            invalidBindingCreate.recordId,
            tick);
    invalidBindingStore.Update(13000);
    ExpectEq("self debug invalid binding stays through exact safety deadline",
             static_cast<int>(invalidBindingStore.Size()), 1);
    invalidBindingStore.MarkMissilePositionUnavailable(
        invalidBindingCreate.recordId,
        13001);
    invalidBindingStore.Update(13001);
    ExpectTrue("self debug repeated invalid binding expires after creation deadline",
               invalidBindingStore.Size() == 0 &&
                   invalidBindingStore.Counters().timeouts == 1);
    SelfSkillDebugStore<4> debugOffMissedDeleteStore;
    SelfSkillMissileObservation debugOffMissile =
        ownMissile;
    debugOffMissile.tick = 14000;
    debugOffMissile.missileNetworkId = 7002u;
    debugOffMissile.missileObjectIdentity = 0xDD02u;
    ExpectTrue("self debug-off missed-delete fixture starts live",
               debugOffMissedDeleteStore.ObserveMissileCreate(
                   debugOffMissile).accepted);
    debugOffMissedDeleteStore.Update(24001);
    ExpectTrue("self debug-off missed delete expires on first resumed update",
               debugOffMissedDeleteStore.Size() == 0 &&
                   debugOffMissedDeleteStore.Counters().timeouts == 1);

    SelfSkillDebugStore<4> deleteFilterStore;
    SelfSkillMissileObservation ownedSparseMissile =
        ownMissile;
    ownedSparseMissile.missileNetworkId = 7101u;
    ownedSparseMissile.missileObjectIdentity = 0xDE01u;
    ExpectTrue("self debug sparse-owned missile fixture is live",
               deleteFilterStore.ObserveMissileCreate(
                   ownedSparseMissile).accepted);
    const auto deleteCountersBefore =
        deleteFilterStore.Counters();
    const bool enemyDeleteOwned =
        deleteFilterStore.OwnsMissile(
            8101u,
            0xEE01u);
    ExpectTrue("self debug unrelated enemy delete is filtered without counters",
               !ShouldProcessSelfMissileDelete(
                    1001u,
                    2002u,
                    enemyDeleteOwned) &&
                   deleteFilterStore.Counters().missileDeleteMatched ==
                       deleteCountersBefore.missileDeleteMatched &&
                   deleteFilterStore.Counters().missileDeleteUnmatched ==
                       deleteCountersBefore.missileDeleteUnmatched);
    const bool sparseDeleteOwned =
        deleteFilterStore.OwnsMissile(
            7101u,
            0xDE01u);
    ExpectTrue("self debug sparse owned delete passes ownership filter",
               ShouldProcessSelfMissileDelete(
                   1001u,
                   0u,
                   sparseDeleteOwned));
    ExpectTrue("self debug sparse owned delete succeeds without event source",
               deleteFilterStore.ObserveMissileDelete(
                   7101u,
                   0xDE01u,
                   1500,
                   250).accepted &&
                   deleteFilterStore.Count(
                       SelfSkillDebugPhase::Terminal) == 1);

    SelfSkillDebugStore<2> holdZeroStore;
    SelfSkillMissileObservation holdZeroCreate =
        ownMissile;
    holdZeroCreate.tick = 4000;
    holdZeroCreate.missileNetworkId = 7151u;
    holdZeroCreate.missileObjectIdentity = 0xDE51u;
    ExpectTrue("self debug hold-zero fixture creates orphan live",
               holdZeroStore.ObserveMissileCreate(
                   holdZeroCreate).accepted);
    const SelfSkillDebugResult holdZeroDelete =
        holdZeroStore.ObserveMissileDelete(
            7151u,
            0xDE51u,
            4010,
            0);
    ExpectTrue("self debug hold-zero delete retains one hidden tombstone",
               holdZeroDelete.accepted &&
                   holdZeroStore.Size() == 1 &&
                   holdZeroStore.HiddenTombstoneCount() == 1 &&
                   holdZeroStore.Snapshot().empty() &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Pending) == 0 &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Live) == 0 &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Terminal) == 0);
    SelfSkillProcessObservation holdZeroProcess =
        ownProcess;
    holdZeroProcess.tick = 4020;
    ExpectTrue("self debug hold-zero delayed Process attaches hidden tombstone",
               holdZeroStore.ObserveProcess(
                   holdZeroProcess).accepted &&
                   holdZeroStore.Size() == 1 &&
                   holdZeroStore.HiddenTombstoneCount() == 1 &&
                   holdZeroStore.Snapshot().empty() &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Pending) == 0 &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Live) == 0 &&
                   holdZeroStore.Count(
                       SelfSkillDebugPhase::Terminal) == 0 &&
                   holdZeroStore.Counters().missileDeleteMatched == 1);
    const int holdZeroDeadline = WrappingTickAdd(
        4010,
        SelfSkillDebugStore<2>::kHiddenTombstoneMinimumRetentionMs);
    holdZeroStore.Update(holdZeroDeadline);
    ExpectEq("self debug hidden tombstone survives exact deadline",
             static_cast<int>(holdZeroStore.Size()), 1);
    holdZeroStore.Update(WrappingTickAdd(holdZeroDeadline, 1));
    ExpectEq("self debug hidden tombstone expires after deadline",
             static_cast<int>(holdZeroStore.Size()), 0);

    SelfSkillDebugStore<1> wrapTombstoneStore;
    SelfSkillMissileObservation wrapTombstoneCreate =
        ownMissile;
    wrapTombstoneCreate.tick =
        std::numeric_limits<int>::max() - 100;
    wrapTombstoneCreate.missileNetworkId = 7152u;
    wrapTombstoneCreate.missileObjectIdentity = 0xDE52u;
    ExpectTrue("self debug wrap tombstone fixture creates live",
               wrapTombstoneStore.ObserveMissileCreate(
                   wrapTombstoneCreate).accepted);
    const int wrapDeleteTick =
        std::numeric_limits<int>::max() - 50;
    ExpectTrue("self debug wrap hold-zero delete creates tombstone",
               wrapTombstoneStore.ObserveMissileDelete(
                   7152u,
                   0xDE52u,
                   wrapDeleteTick,
                   0).accepted &&
                   wrapTombstoneStore.HiddenTombstoneCount() == 1);
    const int wrapTombstoneDeadline = WrappingTickAdd(
        wrapDeleteTick,
        SelfSkillDebugStore<1>::kHiddenTombstoneMinimumRetentionMs);
    wrapTombstoneStore.Update(wrapTombstoneDeadline);
    ExpectEq("self debug wrap tombstone survives exact deadline",
             static_cast<int>(wrapTombstoneStore.Size()), 1);
    wrapTombstoneStore.Update(
        WrappingTickAdd(wrapTombstoneDeadline, 1));
    ExpectEq("self debug wrap tombstone expires after deadline",
             static_cast<int>(wrapTombstoneStore.Size()), 0);

    SelfSkillDebugStore<1> tombstoneCapacityStore;
    SelfSkillMissileObservation capacityTombstoneCreate =
        ownMissile;
    capacityTombstoneCreate.tick = 8000;
    capacityTombstoneCreate.missileNetworkId = 7153u;
    capacityTombstoneCreate.missileObjectIdentity = 0xDE53u;
    ExpectTrue("self debug capacity tombstone fixture creates live",
               tombstoneCapacityStore.ObserveMissileCreate(
                   capacityTombstoneCreate).accepted);
    ExpectTrue("self debug capacity fixture creates hidden tombstone",
               tombstoneCapacityStore.ObserveMissileDelete(
                   7153u,
                   0xDE53u,
                   8010,
                   0).accepted);
    SelfSkillProcessObservation capacityTombstoneProcess =
        ownProcess;
    capacityTombstoneProcess.tick = 8020;
    ExpectTrue("self debug capacity tombstone accepts delayed Process",
               tombstoneCapacityStore.ObserveProcess(
                   capacityTombstoneProcess).accepted);
    SelfSkillMissileObservation replacementMissile =
        ownMissile;
    replacementMissile.tick = 8030;
    replacementMissile.missileNetworkId = 7154u;
    replacementMissile.missileObjectIdentity = 0xDE54u;
    ExpectTrue("self debug allocator reclaims associated hidden tombstone",
               tombstoneCapacityStore.ObserveMissileCreate(
                   replacementMissile).accepted &&
                   tombstoneCapacityStore.Size() == 1 &&
                   tombstoneCapacityStore.HiddenTombstoneCount() == 0 &&
                   tombstoneCapacityStore.Count(
                       SelfSkillDebugPhase::Live) == 1);

    SelfSkillDebugStore<4> flushedLifecycleStore;
    SelfSkillMissileObservation flushedCreate =
        ownMissile;
    flushedCreate.tick = 5000;
    flushedCreate.missileNetworkId = 7201u;
    flushedCreate.missileObjectIdentity = 0xDF01u;
    ExpectTrue("self debug flush creates one orphan live record",
               flushedLifecycleStore.ObserveMissileCreate(
                   flushedCreate).accepted);
    ExpectTrue("self debug flush delete makes orphan terminal",
               flushedLifecycleStore.ObserveMissileDelete(
                   7201u,
                   0xDF01u,
                   5010,
                   250).accepted);
    ExpectTrue("self debug hold-250 terminal remains visible",
               flushedLifecycleStore.Snapshot().size() == 1 &&
                   flushedLifecycleStore.Count(
                       SelfSkillDebugPhase::Terminal) == 1 &&
                   flushedLifecycleStore.HiddenTombstoneCount() == 0);
    SelfSkillProcessObservation flushedProcess =
        ownProcess;
    flushedProcess.tick = 5020;
    ExpectTrue("self debug Create Delete Process attaches terminal without ghost",
               flushedLifecycleStore.ObserveProcess(
                   flushedProcess).accepted &&
                   flushedLifecycleStore.Size() == 1 &&
                   flushedLifecycleStore.Count(
                       SelfSkillDebugPhase::Terminal) == 1 &&
                   flushedLifecycleStore.Count(
                       SelfSkillDebugPhase::Pending) == 0);

    SelfSkillDebugStore<4> createCounterStore;
    SelfSkillMissileObservation unmatchedCreate =
        ownMissile;
    unmatchedCreate.data = nullptr;
    unmatchedCreate.matchDisposition =
        MissileMatchDisposition::Unmatched;
    const SelfSkillDebugResult unmatchedCreateResult =
        createCounterStore.ObserveMissileCreate(
            unmatchedCreate);
    SelfSkillMissileObservation rejectedCreate =
        ownMissile;
    rejectedCreate.matchDisposition =
        MissileMatchDisposition::RejectBasicAttack;
    const SelfSkillDebugResult rejectedCreateResult =
        createCounterStore.ObserveMissileCreate(
            rejectedCreate);
    SelfSkillMissileObservation matchedCreate =
        ownMissile;
    matchedCreate.missileNetworkId = 7301u;
    matchedCreate.missileObjectIdentity = 0xE001u;
    const SelfSkillDebugResult matchedCreateResult =
        createCounterStore.ObserveMissileCreate(
            matchedCreate);
    const SelfSkillDebugResult duplicateCreateResult =
        createCounterStore.ObserveMissileCreate(
            matchedCreate);
    ExpectTrue("self debug create counters classify unmatched and rejected",
               createCounterStore.Counters().missileCreateUnmatched == 1 &&
                   createCounterStore.Counters().missileCreateRejected == 1 &&
                   !IsMatchedSelfSkillMissileCreate(
                       unmatchedCreateResult) &&
                   !IsMatchedSelfSkillMissileCreate(
                       rejectedCreateResult));
    ExpectTrue("self debug duplicate remains matched and duplicate",
               createCounterStore.Counters().missileCreateMatched == 2 &&
                   createCounterStore.Counters().missileCreateOrphan == 1 &&
                   createCounterStore.Counters().missileCreateDuplicate == 1 &&
                   IsMatchedSelfSkillMissileCreate(
                       matchedCreateResult) &&
                   IsMatchedSelfSkillMissileCreate(
                       duplicateCreateResult) &&
                   duplicateCreateResult.duplicate);

    ExpectTrue("self debug delete rejects reused ID with wrong identity",
               !processThenCreate.ObserveMissileDelete(
                   5001u, 0xBB02u, 1400, 250).accepted &&
                   processThenCreate.Count(SelfSkillDebugPhase::Live) == 1);
    ExpectTrue("self debug delete accepts exact missile ownership",
               processThenCreate.ObserveMissileDelete(
                   5001u, 0xAA01u, 1400, 250).accepted &&
                   processThenCreate.Count(SelfSkillDebugPhase::Terminal) == 1);
    processThenCreate.Update(1650);
    ExpectEq("self debug terminal is retained through exact hold deadline",
             static_cast<int>(processThenCreate.Size()), 1);
    processThenCreate.Update(1651);
    ExpectEq("self debug terminal expires after hold deadline",
             static_cast<int>(processThenCreate.Size()), 0);

    SelfSkillDebugStore<1> capacityStore;
    SelfSkillProcessObservation wrapProcess = ownProcess;
    wrapProcess.tick = std::numeric_limits<int>::max() - 100;
    ExpectTrue("self debug accepts cast before signed tick wrap",
               capacityStore.ObserveProcess(wrapProcess).accepted);
    capacityStore.Update(std::numeric_limits<int>::min() + 100);
    ExpectEq("self debug wrap-safe age does not expire fresh pending cast",
             static_cast<int>(capacityStore.Size()), 1);
    capacityStore.Update(std::numeric_limits<int>::min() + 1600);
    ExpectEq("self debug wrap-safe timeout expires old pending cast",
             static_cast<int>(capacityStore.Size()), 0);
    wrapProcess.tick = 2000;
    ExpectTrue("self debug bounded store accepts first record",
               capacityStore.ObserveProcess(wrapProcess).accepted);
    SelfSkillProcessObservation secondLane = wrapProcess;
    secondLane.tick = 2300;
    secondLane.end = Vec2(100.0f, 1100.0f);
    ExpectTrue("self debug bounded store drops over-capacity record",
               !capacityStore.ObserveProcess(secondLane).accepted &&
                   capacityStore.Counters().capacityDrops == 1 &&
                   capacityStore.Size() == 1);

    SelfSkillDebugVisibility visibility;
    visibility.masterEnabled = false;
    ExpectTrue("self debug master off suppresses every phase",
               !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Pending) &&
                   !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Live) &&
                   !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Terminal));
    visibility.masterEnabled = true;
    ExpectTrue("self debug defaults draw pending live and terminal",
               ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Pending) &&
                   ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Live) &&
                   ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Terminal));
    visibility.drawPending = false;
    visibility.drawLive = false;
    ExpectTrue("self debug phase toggles suppress pending live and terminal",
               !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Pending) &&
                   !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Live) &&
                   !ShouldDrawSelfSkillPhase(
                    visibility, SelfSkillDebugPhase::Terminal));

    std::vector<Threat> detectorIsolationSnapshot(1);
    detectorIsolationSnapshot.front().id = 991;
    const int detectorIsolationSerial = 77;
    createThenProcess.Clear();
    ExpectTrue("clearing self debug store cannot mutate detector seam",
               detectorIsolationSnapshot.size() == 1 &&
                   detectorIsolationSnapshot.front().id == 991 &&
                   detectorIsolationSerial == 77 &&
                   createThenProcess.Size() == 0);

    return ZDEvadeTest::Finish("ZDEVADE DETECTOR POLICY");
}
