#include "tests/ZDEvadeTestSupport.h"
#include "core/CoreEvadeState.h"
#include "plugins/ZDEvade/ZDEvadeActivationPolicy.h"
#include "plugins/ZDEvade/Detection/ThreatDetectionPolicy.h"
#include "plugins/ZDEvade/Evade/EvadeMoveResultAdapter.h"
#include "plugins/ZDEvade/Evade/EvadeRoutingPolicy.h"
#include "plugins/ZDEvade/EvadeSpells/EvadeSpellDatabase.h"

#include <array>
#include <climits>
#include <cmath>
#include <limits>
#include <string_view>
#include <vector>

using namespace ZDEvade;
using ZDEvadeTest::ExpectTrue;

namespace {

bool BlocksSlotOne(int slot) {
    return slot == 1;
}

bool BlocksSlotTwo(int slot) {
    return slot == 2;
}

void TestCoreEvadeOwnerAggregation() {
    using namespace CoreEvadeState;

    ClearAll();
    OwnerToken ownerA = AcquireOwner();
    OwnerToken ownerB = AcquireOwner();
    ExpectTrue("two explicit evade owners acquire fixed-capacity slots",
               ownerA && ownerB);
    ExpectTrue("owner A state is accepted",
               SetOwnerState(ownerA, true, false, 1200));
    ExpectTrue("owner B state is accepted",
               SetOwnerState(ownerB, true, true, 1800));
    ExpectTrue("owner A and B aggregate active state",
               StrictEvadeActive);
    ExpectTrue("attack blocking aggregates with OR semantics",
               StrictAttackBlockActive);
    ExpectTrue("combo deadline aggregates with max semantics",
               ComboBlockUntilTick == 1800);
    ExpectTrue("releasing owner A succeeds",
               ReleaseOwner(ownerA));
    ExpectTrue("releasing A leaves owner B aggregate active",
               StrictEvadeActive &&
               StrictAttackBlockActive &&
               ComboBlockUntilTick == 1800);
    ExpectTrue("releasing owner B succeeds independently",
               ReleaseOwner(ownerB));
    ExpectTrue("releasing both owners clears their aggregate",
               !StrictEvadeActive &&
               !StrictAttackBlockActive &&
               ComboBlockUntilTick == 0);

    ClearAll();
    ownerA = AcquireOwner();
    ownerB = AcquireOwner();
    SetOwnerState(ownerA, true, false, 0);
    SetOwnerState(ownerB, true, false, 0);
    ExpectTrue("null owner predicate preserves block-all semantics",
               AreSpellCastsBlocked(100, 3));
    ExpectTrue("owner A spell predicate is accepted",
               SetOwnerSpellBlockPredicate(ownerA, BlocksSlotOne));
    ExpectTrue("owner B spell predicate is accepted",
               SetOwnerSpellBlockPredicate(ownerB, BlocksSlotTwo));
    ExpectTrue("owner A independently blocks its spell slot",
               AreSpellCastsBlocked(100, 1));
    ExpectTrue("owner B independently blocks its spell slot",
               AreSpellCastsBlocked(100, 2));
    ExpectTrue("unmatched slot is permitted across owner predicates",
               !AreSpellCastsBlocked(100, 3));
    SetOwnerState(ownerA, false, false, 0);
    ExpectTrue("inactive owner predicate no longer contributes",
               !AreSpellCastsBlocked(100, 1) &&
               AreSpellCastsBlocked(100, 2));
    SetOwnerState(ownerA, false, false, 250);
    ExpectTrue("deadline-only owner predicate contributes before expiry",
               AreSpellCastsBlocked(249, 1));
    ExpectTrue("deadline-only owner predicate expires at its boundary",
               !AreSpellCastsBlocked(250, 1) &&
               AreSpellCastsBlocked(250, 2));

    OwnerToken inactiveOwner = AcquireOwner();
    ExpectTrue("inactive owner release succeeds",
               inactiveOwner && ReleaseOwner(inactiveOwner));
    ExpectTrue("repeated owner release is idempotent",
               !ReleaseOwner(inactiveOwner));
    ExpectTrue("released token cannot mutate aggregate state",
               !SetOwnerState(inactiveOwner, true, true, 9000));

    ClearAll();
    const OwnerToken staleOwner = AcquireOwner();
    const std::size_t reusedSlot = staleOwner.slot;
    const std::uint32_t staleGeneration = staleOwner.generation;
    ExpectTrue("owner acquired for generation-reuse test",
               staleOwner && ReleaseOwner(staleOwner));
    const OwnerToken replacementOwner = AcquireOwner();
    ExpectTrue("released fixed slot is reused with a new generation",
               replacementOwner &&
               replacementOwner.slot == reusedSlot &&
               replacementOwner.generation != staleGeneration);
    ExpectTrue("replacement owner state is accepted",
               SetOwnerState(replacementOwner, true, true, 4400));
    ExpectTrue("stale generation cannot mutate reused slot",
               !SetOwnerState(staleOwner, false, false, 0) &&
               !SetOwnerSpellBlockPredicate(
                   staleOwner, BlocksSlotOne) &&
               !ReleaseOwner(staleOwner));
    ExpectTrue("stale generation leaves replacement aggregate intact",
               StrictEvadeActive &&
               StrictAttackBlockActive &&
               ComboBlockUntilTick == 4400);
    ReleaseOwner(replacementOwner);

    ClearAll();
    SetEvadeInterventionState(true, false);
    BlockComboUntil(7000);
    std::array<OwnerToken, OwnerCapacity> owners = {};
    bool acquiredAllExplicitSlots = true;
    for (std::size_t index = 0;
         index + 1 < OwnerCapacity;
         ++index) {
        owners[index] = AcquireOwner();
        acquiredAllExplicitSlots =
            acquiredAllExplicitSlots && static_cast<bool>(owners[index]);
    }
    const OwnerToken exhausted = AcquireOwner();
    ExpectTrue("all explicit owner slots can be acquired",
               acquiredAllExplicitSlots);
    ExpectTrue("owner capacity exhaustion returns an invalid token",
               !exhausted);
    ExpectTrue("invalid exhausted token fails without aggregate mutation",
               !SetOwnerState(exhausted, false, true, 1) &&
               StrictEvadeActive &&
               !StrictAttackBlockActive &&
               ComboBlockUntilTick == 7000);

    ClearAll();
    const OwnerToken zdevadeOwner = AcquireOwner();
    SetOwnerState(zdevadeOwner, true, true, 5000);
    SetOwnerSpellBlockPredicate(zdevadeOwner, BlocksSlotTwo);
    // Mirrors KuroEvade's legacy load/active/unload calls without changing
    // KuroEvade itself.
    SetSpellBlockPredicate(BlocksSlotOne);
    SetEvadeInterventionState(true, false);
    BlockComboUntil(9000);
    ExpectTrue("Kuro-style legacy state aggregates beside ZDEvade owner",
               StrictEvadeActive &&
               StrictAttackBlockActive &&
               ComboBlockUntilTick == 9000 &&
               AreSpellCastsBlocked(100, 1) &&
               AreSpellCastsBlocked(100, 2));
    SetEvadeInterventionState(false, false);
    SetStrictEvadeActive(false);
    ClearComboBlock(0);
    SetSpellBlockPredicate(nullptr);
    ExpectTrue("Kuro-style legacy false cannot clear ZDEvade activity",
               StrictEvadeActive && StrictAttackBlockActive);
    ExpectTrue("Kuro-style legacy combo clear leaves ZDEvade deadline",
               ComboBlockUntilTick == 5000);
    ExpectTrue("Kuro-style predicate clear leaves ZDEvade predicate",
               !AreSpellCastsBlocked(100, 1) &&
               AreSpellCastsBlocked(100, 2));

    ClearAll();
    ExpectTrue("ClearAll clears every explicit and legacy owner",
               !StrictEvadeActive &&
               !StrictAttackBlockActive &&
               ComboBlockUntilTick == 0 &&
               !AreSpellCastsBlocked(100, 2));
    ExpectTrue("ClearAll invalidates pre-shutdown owner tokens",
               !SetOwnerState(zdevadeOwner, true, true, 9000) &&
               !ReleaseOwner(zdevadeOwner));
}

void TestOtherEvadePolicy() {
    const OtherEvadeState neither = {};
    const OtherEvadeState kuroOnly = {true, false};
    const OtherEvadeState ezOnly = {false, true};
    const OtherEvadeState both = {true, true};

    ExpectTrue("neither other evade permits initial load",
               CanActivateZDEvade(neither));
    ExpectTrue("kuro evade refuses initial load",
               !CanActivateZDEvade(kuroOnly));
    ExpectTrue("ez evade refuses initial load",
               !CanActivateZDEvade(ezOnly));
    ExpectTrue("both other evades refuse initial load",
               !CanActivateZDEvade(both));

    const OtherEvadeDecision stayActive =
        DecideOtherEvadeState(neither, false, true);
    ExpectTrue("neither other evade stays active without cleanup",
               !stayActive.suspended &&
               !stayActive.suspendNow &&
               !stayActive.releaseNow);

    const OtherEvadeDecision suspendForKuro =
        DecideOtherEvadeState(kuroOnly, false, false);
    ExpectTrue("kuro loading suspends and cleans up once",
               suspendForKuro.suspended &&
               suspendForKuro.suspendNow &&
               !suspendForKuro.releaseNow &&
               suspendForKuro.reason == OtherEvadeReason::KuroEvade);

    const OtherEvadeDecision remainSuspendedForKuro =
        DecideOtherEvadeState(kuroOnly, true, true);
    ExpectTrue("unchanged kuro suspension does not repeat cleanup",
               remainSuspendedForKuro.suspended &&
               !remainSuspendedForKuro.suspendNow &&
               !remainSuspendedForKuro.releaseNow);

    const OtherEvadeDecision suspendForEz =
        DecideOtherEvadeState(ezOnly, false, false);
    ExpectTrue("ez loading suspends and identifies ez",
               suspendForEz.suspended &&
               suspendForEz.suspendNow &&
               suspendForEz.reason == OtherEvadeReason::EzEvade);

    const OtherEvadeDecision suspendForBoth =
        DecideOtherEvadeState(both, false, false);
    ExpectTrue("both loading suspend with combined reason",
               suspendForBoth.suspended &&
               suspendForBoth.suspendNow &&
               suspendForBoth.reason == OtherEvadeReason::Multiple);

    const OtherEvadeDecision switchWhileSuspended =
        DecideOtherEvadeState(ezOnly, true, true);
    ExpectTrue("other evade changes do not repeat suspension cleanup",
               switchWhileSuspended.suspended &&
               !switchWhileSuspended.suspendNow &&
               !switchWhileSuspended.releaseNow);

    const OtherEvadeDecision holdUntilTick =
        DecideOtherEvadeState(neither, true, false);
    ExpectTrue("input callback holds suspension until tick releases it",
               holdUntilTick.suspended &&
               !holdUntilTick.suspendNow &&
               !holdUntilTick.releaseNow);

    const OtherEvadeDecision release =
        DecideOtherEvadeState(neither, true, true);
    ExpectTrue("last other evade unloading releases suspension",
               !release.suspended &&
               !release.suspendNow &&
               release.releaseNow &&
               release.reason == OtherEvadeReason::None);
}

void TestLegacyControlRestore() {
    CoreEvadeState::ClearAll();
    const CoreEvadeState::OwnerToken validOwner =
        CoreEvadeState::AcquireOwner();
    CoreEvadeState::SetOwnerState(validOwner, true, true, 1200);
    const bool validOwnerReleased =
        CoreEvadeState::SetOwnerState(validOwner, false, false, 0);
    const LegacyControlRestoreInput ownedState = {
        true, true, false, false, true, true, true, true,
        validOwnerReleased, CoreEvadeState::StrictEvadeActive};
    const LegacyControlRestoreDecision externalHandoff =
        DecideLegacyControlRestore(
            ownedState,
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("handoff without active external owner restores matching flags",
               externalHandoff.restoreMoveEnabled &&
               externalHandoff.restoreAttackEnabled);
    ExpectTrue("handoff without active external owner permits core release",
               externalHandoff.restoreInterventionState &&
               externalHandoff.restoreComboBlock);

    const CoreEvadeState::OwnerToken handoffOwner =
        CoreEvadeState::AcquireOwner();
    CoreEvadeState::SetOwnerState(handoffOwner, true, true, 1800);
    CoreEvadeState::SetEvadeInterventionState(true, false);
    const bool handoffOwnerReleased =
        CoreEvadeState::SetOwnerState(handoffOwner, false, false, 0);
    const LegacyControlRestoreDecision activeExternalHandoff =
        DecideLegacyControlRestore(
            {
                true, true, false, false, false, false, true, true,
                handoffOwnerReleased, CoreEvadeState::StrictEvadeActive,
            },
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("handoff with active other owner preserves its orb flags",
               !activeExternalHandoff.restoreMoveEnabled &&
               !activeExternalHandoff.restoreAttackEnabled);
    ExpectTrue("handoff with active other owner preserves its core state",
               !activeExternalHandoff.restoreInterventionState &&
               !activeExternalHandoff.restoreComboBlock);
    CoreEvadeState::SetEvadeInterventionState(false, false);
    CoreEvadeState::ReleaseOwner(handoffOwner);

    const LegacyControlRestoreDecision normalRelease =
        DecideLegacyControlRestore(
            ownedState,
            LegacyControlExitMode::NormalRestore);
    ExpectTrue("normal owned release still restores orbwalker flags",
               normalRelease.restoreMoveEnabled &&
               normalRelease.restoreAttackEnabled);
    ExpectTrue("normal owned release still restores legacy global state",
               normalRelease.restoreInterventionState &&
               normalRelease.restoreComboBlock);

    const CoreEvadeState::OwnerToken staleOwner =
        CoreEvadeState::AcquireOwner();
    CoreEvadeState::SetOwnerState(staleOwner, true, true, 1400);
    CoreEvadeState::ReleaseOwner(staleOwner);
    const bool staleOwnerReleased =
        CoreEvadeState::SetOwnerState(staleOwner, false, false, 0);
    const LegacyControlRestoreDecision staleRelease =
        DecideLegacyControlRestore(
            {
                true, true, false, false, true, true, true, true,
                staleOwnerReleased, CoreEvadeState::StrictEvadeActive,
            },
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("stale handoff token cannot restore flags",
               !staleRelease.restoreMoveEnabled &&
               !staleRelease.restoreAttackEnabled);
    ExpectTrue("stale handoff token cannot mutate core state",
               !staleRelease.restoreInterventionState &&
               !staleRelease.restoreComboBlock);
    ExpectTrue("destructor-style owner release succeeds after control exit",
               CoreEvadeState::ReleaseOwner(validOwner));
    ExpectTrue("destructor-style repeated owner release is idempotent",
               !CoreEvadeState::ReleaseOwner(validOwner));

    const LegacyControlRestoreDecision inactive =
        DecideLegacyControlRestore({
            false, true, false, false, false, false, true, true, true});
    ExpectTrue("inactive release is a complete no-op",
               !inactive.restoreMoveEnabled &&
               !inactive.restoreAttackEnabled &&
               !inactive.restoreInterventionState &&
               !inactive.restoreComboBlock);

    const LegacyControlRestoreDecision sameImplementation =
        DecideLegacyControlRestore({
            true, true, false, false, true, true, true, true, true});
    ExpectTrue("same implementation restores unchanged imposed state",
               sameImplementation.restoreMoveEnabled &&
               sameImplementation.restoreAttackEnabled &&
               sameImplementation.restoreInterventionState &&
               sameImplementation.restoreComboBlock);

    const LegacyControlRestoreDecision changedImplementation =
        DecideLegacyControlRestore({
            true, false, false, false, true, true, true, true, true});
    ExpectTrue("implementation change prevents orbwalker restoration",
               !changedImplementation.restoreMoveEnabled &&
               !changedImplementation.restoreAttackEnabled);
    const LegacyControlRestoreDecision changedImplementationHandoff =
        DecideLegacyControlRestore(
            {
                true, false, false, false, true, true, true, true,
                true, false,
            },
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("handoff implementation change prevents orb restoration",
               !changedImplementationHandoff.restoreMoveEnabled &&
               !changedImplementationHandoff.restoreAttackEnabled);

    const LegacyControlRestoreDecision externalMoveMutation =
        DecideLegacyControlRestore({
            true, true, true, false, true, true, true, true, true});
    ExpectTrue("external move mutation is never overwritten",
               !externalMoveMutation.restoreMoveEnabled &&
               externalMoveMutation.restoreAttackEnabled);

    const LegacyControlRestoreDecision externalAttackMutation =
        DecideLegacyControlRestore({
            true, true, false, false, false, true, true, true, true});
    ExpectTrue("external attack mutation is never overwritten",
               externalAttackMutation.restoreMoveEnabled &&
               !externalAttackMutation.restoreAttackEnabled);

    const LegacyControlRestoreDecision externalMutationHandoff =
        DecideLegacyControlRestore(
            {
                true, true, true, false, false, true, true, true,
                true, false,
            },
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("handoff external flag mutations are never overwritten",
               !externalMutationHandoff.restoreMoveEnabled &&
               !externalMutationHandoff.restoreAttackEnabled);

    const LegacyControlRestoreDecision inactiveHandoff =
        DecideLegacyControlRestore(
            {
                false, true, false, false, false, false, true, true,
                true, false,
            },
            LegacyControlExitMode::ExternalOwnerHandoff);
    ExpectTrue("inactive external handoff is a complete no-op",
               !inactiveHandoff.restoreMoveEnabled &&
               !inactiveHandoff.restoreAttackEnabled &&
               !inactiveHandoff.restoreInterventionState &&
               !inactiveHandoff.restoreComboBlock);

}

void TestAllowAttacksPolicy() {
    const AttackControlDecision disabledAllow =
        DecideAttackControl(false, false, false, true);
    ExpectTrue("allow attacks preserves a disabled baseline",
               !disabledAllow.baselineAttackEnabled &&
               !disabledAllow.imposedAttackEnabled);

    const AttackControlDecision enabledAllow =
        DecideAttackControl(false, false, true, true);
    ExpectTrue("allow attacks preserves an enabled baseline",
               enabledAllow.baselineAttackEnabled &&
               enabledAllow.imposedAttackEnabled);

    const AttackControlDecision disabledBlock =
        DecideAttackControl(false, false, false, false);
    const AttackControlDecision enabledBlock =
        DecideAttackControl(false, false, true, false);
    ExpectTrue("blocking attacks disables either prior state",
               !disabledBlock.imposedAttackEnabled &&
               !enabledBlock.imposedAttackEnabled);

    const AttackControlDecision repeatedDisabled =
        DecideAttackControl(true, false, true, true);
    const AttackControlDecision repeatedEnabled =
        DecideAttackControl(true, true, false, true);
    ExpectTrue("repeated control retains original disabled baseline",
               !repeatedDisabled.baselineAttackEnabled &&
               !repeatedDisabled.imposedAttackEnabled);
    ExpectTrue("repeated control retains original enabled baseline",
               repeatedEnabled.baselineAttackEnabled &&
               repeatedEnabled.imposedAttackEnabled);

    const LegacyControlRestoreDecision unchangedDisabled =
        DecideLegacyControlRestore({
            true, true, false, false, false, false, true, true, true});
    ExpectTrue("disabled imposed attack state remains restore-guarded",
               unchangedDisabled.restoreAttackEnabled);
    const LegacyControlRestoreDecision externallyEnabled =
        DecideLegacyControlRestore({
            true, true, false, false, true, false, true, true, true});
    ExpectTrue("external attack enable prevents stale restore",
               !externallyEnabled.restoreAttackEnabled);
}

void TestActiveEvadeSpellDatabase() {
    EvadeSpellDatabase::Initialize();
    constexpr std::array<std::string_view, 4> unsafeNames = {
        "AmbessaE", "BelvethE", "SeraphineE", "TaliyahW"
    };
    constexpr std::array<std::string_view, 6> validNeighbors = {
        "AmbessaR", "BriarR", "SejuaniQ",
        "SettR", "TahmKenchR", "UdyrE"
    };
    std::array<int, unsafeNames.size()> unsafeCounts = {};
    std::array<int, validNeighbors.size()> neighborCounts = {};
    const EvadeSpellData* galioE = nullptr;
    for (const EvadeSpellData& spell : EvadeSpellDatabase::Spells) {
        if (spell.charName == "Galio" && spell.spellKey == EvadeSpellSlot::E)
            galioE = &spell;
        for (std::size_t index = 0; index < unsafeNames.size(); ++index) {
            if (spell.spellName == unsafeNames[index])
                ++unsafeCounts[index];
        }
        for (std::size_t index = 0; index < validNeighbors.size(); ++index) {
            if (spell.spellName == validNeighbors[index])
                ++neighborCounts[index];
        }
    }
    for (std::size_t index = 0; index < unsafeNames.size(); ++index) {
        ExpectTrue(
            "unsafe evade record cannot enter active candidates",
            unsafeCounts[index] == 0);
    }
    for (std::size_t index = 0; index < validNeighbors.size(); ++index) {
        ExpectTrue(
            "valid neighboring evade record remains active",
            neighborCounts[index] > 0);
    }
    ExpectTrue(
        "Galio E uses current Justice Punch runtime metadata",
        galioE != nullptr &&
        galioE->name == "Justice Punch" &&
        galioE->spellName == "GalioE" &&
        galioE->spellKey == EvadeSpellSlot::E &&
        std::fabs(galioE->range - 650.0f) < 0.001f &&
        std::fabs(galioE->speed - 2300.0f) < 0.001f &&
        !galioE->fixedRange &&
        galioE->evadeType == EvadeType::Dash &&
        galioE->castType == EvadeCastType::Position);
    ExpectTrue(
        "strict non-item validation accepts current Galio E runtime name",
        galioE != nullptr &&
        StrictEvadeSpellNameMatches(
            *galioE, "GalioE", "", ""));
    ExpectTrue(
        "strict non-item validation rejects obsolete Galio runtime name",
        galioE != nullptr &&
        !StrictEvadeSpellNameMatches(
            *galioE, "GalioRighteousGust", "", ""));
    std::printf(
        "TRACE active evade records: total=%zu unsafe-matches=%d\n",
        EvadeSpellDatabase::Spells.size(),
        unsafeCounts[0] + unsafeCounts[1] +
            unsafeCounts[2] + unsafeCounts[3]);
}

void TestObservedRoutePolicy() {
    ObservedRouteEvaluation invalid;
    invalid.evaluated = true;
    ExpectTrue("NTM-01 empty threat context ignores invalid native route",
               !IsObservedRouteUnsafe(2, invalid, false));

    ObservedRouteEvaluation nonWalkable;
    nonWalkable.evaluated = true;
    nonWalkable.valid = true;
    ExpectTrue("NTM-02 expired threat context ignores terrain-only route",
               !IsObservedRouteUnsafe(2, nonWalkable, false));
    ExpectTrue("NTM-03 irrelevant threat context does not arm navigation",
               !IsNavigationInterventionArmed(
                   false, false, false, false));
    ExpectTrue("NTM-04 wall proximity alone does not arm navigation",
               !IsObservedRouteUnsafe(2, nonWalkable, false));
    const bool activeStrictContext = IsNavigationInterventionArmed(
        true, false, false, true);
    ExpectTrue("NTM-05 actionable danger fails closed on invalid route",
               IsObservedRouteUnsafe(
                   2, invalid, activeStrictContext));
    ExpectTrue("NTM-06 actionable danger fails closed on non-walkable route",
               IsObservedRouteUnsafe(
                   2, nonWalkable, activeStrictContext));
    ExpectTrue("NTM-07 fewer than two points never forms an unsafe route",
               !IsObservedRouteUnsafe(1, invalid, true));
    ObservedRouteEvaluation notEvaluated;
    ExpectTrue("NTM-08 unevaluated route is never unsafe",
               !IsObservedRouteUnsafe(2, notEvaluated, true));

    ObservedRouteEvaluation safe;
    safe.evaluated = true;
    safe.valid = true;
    safe.walkable = true;
    safe.pathSafe = true;
    safe.endpointSafe = true;
    ExpectTrue("evaluated safe observed route remains safe",
               !IsObservedRouteUnsafe(2, safe, true));

    ObservedRouteEvaluation crossing = safe;
    crossing.pathSafe = false;
    ExpectTrue("NTM-09 threat crossing arms route while hero is initially safe",
               IsObservedRouteUnsafe(2, crossing, false));
    ObservedRouteEvaluation unsafeEndpoint = safe;
    unsafeEndpoint.endpointSafe = false;
    ExpectTrue("NTM-10 unsafe endpoint arms route without point danger",
               IsObservedRouteUnsafe(2, unsafeEndpoint, false));

    const ThreatFreeActionDecision updateWithoutThreat =
        DecideThreatFreeAction(
            false,
            ThreatFreeDecisionSite::Update);
    ExpectTrue("NTM-11 threat-free update clears every stale intent",
               updateWithoutThreat.applies &&
                   updateWithoutThreat.releaseControl &&
                   updateWithoutThreat.clearIntents);
    ExpectTrue("NTM-12 threat-free update never stops or replans",
               !updateWithoutThreat.stopMovement &&
                   !updateWithoutThreat.replan &&
                   !updateWithoutThreat.deferInput);

    const bool terrainOnlyUnsafe =
        IsObservedRouteUnsafe(2, nonWalkable, false);
    ReleaseHysteresisInput terrainOnlyRelease;
    terrainOnlyRelease.controlActive = true;
    terrainOnlyRelease.currentPathUnsafe = terrainOnlyUnsafe;
    ReleaseDecisionInput terrainOnlyStop;
    terrainOnlyStop.currentPathUnsafe = terrainOnlyUnsafe;
    ExpectTrue("NTM-13 active evade releases without terrain-only stop",
               DecideReleaseHysteresis(terrainOnlyRelease) ==
                       ReleaseHysteresisAction::Release &&
                   !MustStopBeforeRelease(terrainOnlyStop));

    ExpectTrue("idle path-acquisition danger arms navigation",
               IsNavigationInterventionArmed(
                   false, false, true, false));
    ExpectTrue("idle release-margin-only danger does not arm navigation",
               !IsNavigationInterventionArmed(
                   false, false, false, true));
    ExpectTrue("active release-margin danger arms navigation",
               IsNavigationInterventionArmed(
                   true, false, false, true));
    ExpectTrue("exact danger always arms navigation",
               IsNavigationInterventionArmed(
                   false, true, false, false));

    const Vec2 start(100.0f, 100.0f);
    std::vector<Vec2> cachedWaypoints;
    for (int index = 1; index <= 30; ++index) {
        cachedWaypoints.emplace_back(
            100.0f + static_cast<float>(index) * 10.0f,
            100.0f);
    }
    const Vec2 lateBend(380.0f, 350.0f);
    cachedWaypoints[27] = lateBend;
    cachedWaypoints.insert(
        cachedWaypoints.begin() + 28,
        lateBend);
    const std::vector<Vec2> observed = NormalizeObservedWaypoints(
        start,
        cachedWaypoints,
        cachedWaypoints.back());
    ExpectTrue("observed route retains more than 24 waypoints",
               observed.size() == 31);
    ExpectTrue("observed route retains a valid late bend",
               observed[28].Distance(lateBend) < 0.001f);
    ExpectTrue("observed route deduplicates consecutive points",
               observed[29].Distance(lateBend) > 1.0f);
    ExpectTrue("existing fallback endpoint is not appended twice",
               observed.back().Distance(cachedWaypoints.back()) < 0.001f);
}

void TestThreatFreeActionPolicy() {
    MoveRouteEvaluation unsafeManualRoute;
    unsafeManualRoute.evaluated = true;
    unsafeManualRoute.valid = true;
    unsafeManualRoute.walkable = true;
    MoveRouteEvaluation invalidManualRoute;
    invalidManualRoute.evaluated = true;

    MoveIntentState staleManualState;
    ExpectTrue("stale unsafe manual fixture is deferred",
               staleManualState.RecordManual(
                   Vec2(600.0f, 100.0f),
                   1000,
                   1,
                   unsafeManualRoute,
                   true,
                   50) == MoveIntentRecordResult::Deferred);
    const ThreatFreeActionDecision emptyUpdate =
        DecideThreatFreeAction(
            false,
            ThreatFreeDecisionSite::Update);
    if (emptyUpdate.clearIntents) staleManualState.Clear();
    ExpectTrue("empty-threat update removes stale manual deferred state",
               emptyUpdate.applies &&
                   emptyUpdate.releaseControl &&
                   !staleManualState.HasManual() &&
                   !staleManualState.HasDeferred() &&
                   !staleManualState.HasGoal());
    ExpectTrue("empty-threat update performs no terrain intervention",
               !emptyUpdate.stopMovement &&
                   !emptyUpdate.replan &&
                   !emptyUpdate.deferInput);

    MoveIntentState automatedState;
    automatedState.Record(
        Vec2(625.0f, 100.0f),
        MoveIntentSource::Orbwalker,
        1050,
        1,
        true);
    ExpectTrue("orbwalker fixture starts deferred",
               automatedState.HasDeferred());
    if (emptyUpdate.clearIntents) automatedState.Clear();
    automatedState.Record(
        Vec2(630.0f, 100.0f),
        MoveIntentSource::ObservedPath,
        1060,
        1,
        true);
    ExpectTrue("observed-path fixture starts deferred",
               automatedState.HasDeferred());
    if (emptyUpdate.clearIntents) automatedState.Clear();
    ExpectTrue("empty-threat action clears automated deferred intents",
               !automatedState.HasDeferred() &&
                   !automatedState.HasGoal());

    MoveRouteEvaluation safeManualRoute = unsafeManualRoute;
    safeManualRoute.strictSafe = true;
    MoveIntentState adoptionState;
    ExpectTrue("safe-manual fixture starts adoption ownership",
               adoptionState.RecordManual(
                   Vec2(640.0f, 100.0f),
                   1070,
                   2,
                   safeManualRoute,
                   true,
                   50) == MoveIntentRecordResult::SafeManual &&
                   adoptionState.HasSafeManualAdoption());
    if (emptyUpdate.clearIntents) adoptionState.Clear();
    ExpectTrue("empty-threat action clears safe-manual adoption",
               !adoptionState.HasSafeManualAdoption() &&
                   !adoptionState.HasManual() &&
                   !adoptionState.HasGoal());

    MoveIntentState expiredBeforeUpdateState;
    expiredBeforeUpdateState.RecordManual(
        Vec2(650.0f, 100.0f),
        1100,
        2,
        unsafeManualRoute,
        true,
        50);
    const ThreatFreeActionDecision orbAfterExpiry =
        DecideThreatFreeAction(
            false,
            ThreatFreeDecisionSite::MoveRequest);
    if (orbAfterExpiry.clearIntents) expiredBeforeUpdateState.Clear();
    ExpectTrue("orb request after expiry clears stale ownership immediately",
               orbAfterExpiry.applies &&
                   orbAfterExpiry.releaseControl &&
                   orbAfterExpiry.clearIntents &&
                   !expiredBeforeUpdateState.HasManual());
    ExpectTrue("orb request after expiry proceeds natively without defer",
               orbAfterExpiry.allowNativeInput &&
                   !orbAfterExpiry.stopMovement &&
                   !orbAfterExpiry.replan &&
                   !orbAfterExpiry.deferInput);

    ObservedRouteEvaluation wallInvalidRequest;
    wallInvalidRequest.evaluated = true;
    const bool wallInvalidActionable =
        IsNavigationInterventionArmed(
            true, false, false, false) ||
        IsObservedThreatRouteUnsafe(2, wallInvalidRequest);
    const ThreatFreeActionDecision manualWallBeforeUpdate =
        DecideThreatFreeAction(
            wallInvalidActionable,
            ThreatFreeDecisionSite::MoveRequest);
    ExpectTrue("wall-invalid manual request ignores stale active state",
               manualWallBeforeUpdate.applies &&
                   manualWallBeforeUpdate.releaseControl &&
                   manualWallBeforeUpdate.clearIntents &&
                   manualWallBeforeUpdate.allowNativeInput);
    ExpectTrue("wall-invalid threat-free manual request is never blocked",
               !manualWallBeforeUpdate.stopMovement &&
                   !manualWallBeforeUpdate.replan &&
                   !manualWallBeforeUpdate.deferInput);

    const bool activeThreatActionable =
        IsNavigationInterventionArmed(
            true, false, false, true) ||
        IsObservedThreatRouteUnsafe(2, wallInvalidRequest);
    const ThreatFreeActionDecision activeThreatRequest =
        DecideThreatFreeAction(
            activeThreatActionable,
            ThreatFreeDecisionSite::MoveRequest);
    ExpectTrue("active threat bypasses threat-free input fast path",
               !activeThreatRequest.applies &&
                   !activeThreatRequest.releaseControl &&
                   !activeThreatRequest.clearIntents &&
                   !activeThreatRequest.allowNativeInput);
    ExpectTrue("active threat still blocks nav-invalid manual input",
               DecideManualRouteAction(true, invalidManualRoute) ==
                   ManualRouteAction::PreserveAndBlock);
}

void TestExternalMoveDecisionPolicy() {
    ExternalMoveRouteEvaluation safeRoute;
    safeRoute.valid = true;
    safeRoute.walkable = true;
    safeRoute.pathSafe = true;
    safeRoute.endpointSafe = true;
    safeRoute.strictSafe = true;

    const auto decide = [&](MoveIntentSource source,
                            bool controllerOwnsMovement,
                            bool actionableThreatContext,
                            const ExternalMoveRouteEvaluation& route) {
        return DecideExternalMove({
            source,
            controllerOwnsMovement,
            actionableThreatContext,
            route,
        });
    };

    ExternalMoveRouteEvaluation insideDestination = safeRoute;
    insideDestination.endpointSafe = false;
    insideDestination.strictSafe = false;
    const ExternalMoveDecision inside = decide(
        MoveIntentSource::Manual, false, true, insideDestination);
    ExpectTrue("active destination inside threat is consumed and discarded",
               !inside.allowNative &&
                   inside.consume &&
                   !inside.adoptGoal &&
                   inside.discardBlockedIntent);

    ExternalMoveRouteEvaluation crossesThenExits = safeRoute;
    crossesThenExits.pathSafe = false;
    crossesThenExits.strictSafe = false;
    const ExternalMoveDecision crossing = decide(
        MoveIntentSource::Manual, false, true, crossesThenExits);
    ExpectTrue("active route crossing then exiting is consumed and discarded",
               !crossing.allowNative &&
                   crossing.consume &&
                   !crossing.adoptGoal &&
                   crossing.discardBlockedIntent);

    const ExternalMoveDecision safeOutward = decide(
        MoveIntentSource::Manual, false, true, safeRoute);
    ExpectTrue("safe strict outward manual route proceeds natively",
               safeOutward.allowNative &&
                   !safeOutward.consume &&
                   safeOutward.adoptGoal &&
                   !safeOutward.discardBlockedIntent);

    ExternalMoveRouteEvaluation bestEffortExit = safeRoute;
    bestEffortExit.pathSafe = false;
    bestEffortExit.strictSafe = false;
    bestEffortExit.startsInThreat = true;
    bestEffortExit.coverageNoWorseThanHold = true;
    bestEffortExit.makesExitProgress = true;
    const ExternalMoveDecision manualBestEffort = decide(
        MoveIntentSource::Manual, true, true, bestEffortExit);
    const ExternalMoveDecision orbBestEffort = decide(
        MoveIntentSource::Orbwalker, true, true, bestEffortExit);
    ExpectTrue("best-effort manual exit is adopted and allowed natively",
               manualBestEffort.allowNative &&
                   !manualBestEffort.consume &&
                   manualBestEffort.adoptGoal &&
                   !manualBestEffort.discardBlockedIntent);
    ExpectTrue("best-effort controlled orb exit is adopted but consumed",
               !orbBestEffort.allowNative &&
                   orbBestEffort.consume &&
                   orbBestEffort.adoptGoal &&
                   !orbBestEffort.discardBlockedIntent);

    ExternalMoveRouteEvaluation newThreatExit = bestEffortExit;
    newThreatExit.enteredNewThreat = true;
    ExternalMoveRouteEvaluation worseExit = bestEffortExit;
    worseExit.coverageNoWorseThanHold = false;
    ExternalMoveRouteEvaluation noProgressExit = bestEffortExit;
    noProgressExit.makesExitProgress = false;
    const ExternalMoveDecision transfer = decide(
        MoveIntentSource::Manual, true, true, newThreatExit);
    const ExternalMoveDecision worse = decide(
        MoveIntentSource::Manual, true, true, worseExit);
    const ExternalMoveDecision noProgress = decide(
        MoveIntentSource::Manual, true, true, noProgressExit);
    ExpectTrue("A-to-B transfer is blocked and discarded",
               transfer.consume &&
                   transfer.discardBlockedIntent &&
                   !transfer.adoptGoal);
    ExpectTrue("worse exposure and no-progress exits are blocked",
               worse.consume &&
                   worse.discardBlockedIntent &&
                   noProgress.consume &&
                   noProgress.discardBlockedIntent);

    ExternalMoveRouteEvaluation tangentContact = safeRoute;
    tangentContact.pathSafe = false;
    tangentContact.strictSafe = false;
    const ExternalMoveDecision tangent = decide(
        MoveIntentSource::Manual, false, true, tangentContact);
    const ExternalMoveDecision epsilonOutside = decide(
        MoveIntentSource::Manual, false, true, safeRoute);
    ExpectTrue("tangent contact blocks while epsilon-clear route proceeds",
               tangent.consume &&
                   tangent.discardBlockedIntent &&
                   epsilonOutside.allowNative &&
                   epsilonOutside.adoptGoal);

    ExternalMoveRouteEvaluation geometricReentry = safeRoute;
    geometricReentry.reenteredDanger = true;
    const ExternalMoveDecision reentry = decide(
        MoveIntentSource::Manual, false, true, geometricReentry);
    ExpectTrue("geometric re-entry blocks despite time-resolved safety",
               reentry.consume &&
                   reentry.discardBlockedIntent &&
                   !reentry.allowNative &&
                   !reentry.adoptGoal);

    ExternalMoveRouteEvaluation invalidRoute;
    ExternalMoveRouteEvaluation nonWalkable = safeRoute;
    nonWalkable.walkable = false;
    ExternalMoveRouteEvaluation nonStrict = safeRoute;
    nonStrict.strictSafe = false;
    const ExternalMoveDecision invalid = decide(
        MoveIntentSource::ObservedPath, true, true, invalidRoute);
    const ExternalMoveDecision wall = decide(
        MoveIntentSource::Orbwalker, false, true, nonWalkable);
    const ExternalMoveDecision fallback = decide(
        MoveIntentSource::Manual, true, true, nonStrict);
    ExpectTrue("invalid non-walkable and non-strict active routes fail closed",
               invalid.consume && invalid.discardBlockedIntent &&
                   wall.consume && wall.discardBlockedIntent &&
                   fallback.consume && fallback.discardBlockedIntent);

    const ExternalMoveDecision expired = decide(
        MoveIntentSource::Manual, true, false, geometricReentry);
    ExpectTrue("expired threat context permits native movement without adoption",
               expired.allowNative &&
                   !expired.consume &&
                   !expired.adoptGoal &&
                   !expired.discardBlockedIntent);

    const ExternalMoveDecision controlledManual = decide(
        MoveIntentSource::Manual, true, true, safeRoute);
    const ExternalMoveDecision controlledOrb = decide(
        MoveIntentSource::Orbwalker, true, true, safeRoute);
    ExpectTrue("controlled safe manual keeps native adoption behavior",
               controlledManual.allowNative &&
                   !controlledManual.consume &&
                   controlledManual.adoptGoal);
    ExpectTrue("controlled safe orb updates intent but cannot issue natively",
               !controlledOrb.allowNative &&
                   controlledOrb.consume &&
                   controlledOrb.adoptGoal &&
                   !controlledOrb.discardBlockedIntent);

    const ExternalMoveDecision outsideOrb = decide(
        MoveIntentSource::Orbwalker, false, true, safeRoute);
    ExpectTrue("outside control safe orb proceeds natively and is adopted",
               outsideOrb.allowNative &&
                   !outsideOrb.consume &&
                   outsideOrb.adoptGoal);

    MoveRouteEvaluation unsafeManualRoute;
    unsafeManualRoute.evaluated = true;
    unsafeManualRoute.valid = true;
    unsafeManualRoute.walkable = true;
    MoveIntentState repeatedBlockedState;
    repeatedBlockedState.RecordManual(
        Vec2(600.0f, 100.0f),
        1000,
        1,
        unsafeManualRoute,
        true,
        50);
    for (int click = 0; click < 2; ++click) {
        const ExternalMoveDecision blocked = decide(
            click == 0
                ? MoveIntentSource::Manual
                : MoveIntentSource::Orbwalker,
            true,
            true,
            crossesThenExits);
        if (blocked.discardBlockedIntent)
            repeatedBlockedState.Clear();
        ExpectTrue("repeated blocked input leaves no replayable destination",
                   blocked.consume &&
                       !repeatedBlockedState.HasGoal() &&
                       !repeatedBlockedState.HasDeferred() &&
                       !repeatedBlockedState.HasManual());
    }
    ExpectTrue("active context cannot resume discarded deferred destination",
               DecideDeferredRoute(
                   repeatedBlockedState.HasDeferred(),
                   false) == DeferredRouteAction::None);

    const ExternalMoveDecision threatOrderA = decide(
        MoveIntentSource::ObservedPath, true, true, crossesThenExits);
    const ExternalMoveDecision threatOrderB = decide(
        MoveIntentSource::ObservedPath, true, true, crossesThenExits);
    ExpectTrue("multiple-threat aggregate action is order independent",
               threatOrderA.allowNative == threatOrderB.allowNative &&
                   threatOrderA.consume == threatOrderB.consume &&
                   threatOrderA.adoptGoal == threatOrderB.adoptGoal &&
                   threatOrderA.discardBlockedIntent ==
                       threatOrderB.discardBlockedIntent);
}

void TestStableRouteStrictPriority() {
    StableRouteMetrics fallbackCurrent;
    fallbackCurrent.strictSafe = false;
    StableRouteMetrics strictProposed;
    strictProposed.strictSafe = true;
    strictProposed.coverage.endpointDanger = 3;
    strictProposed.coverage.collisionCount = 2;
    ExpectTrue(
        "strict proposed route replaces safer-coverage fallback current",
        !KeepStableRoute(
            fallbackCurrent,
            strictProposed,
            1.0f,
            true));

    StableRouteMetrics strictCurrent = strictProposed;
    StableRouteMetrics fallbackProposed = fallbackCurrent;
    ExpectTrue(
        "strict current route beats better-coverage fallback proposed",
        KeepStableRoute(
            strictCurrent,
            fallbackProposed,
            1.0f,
            false));

    StableRouteMetrics sameStrictCurrent;
    sameStrictCurrent.strictSafe = true;
    StableRouteMetrics sameStrictProposed = sameStrictCurrent;
    sameStrictProposed.coverage.endpointDanger = 1;
    ExpectTrue(
        "coverage keeps current within equal strictness",
        KeepStableRoute(
            sameStrictCurrent,
            sameStrictProposed,
            1.0f,
            false));
    ExpectTrue(
        "strict commitment ignores coverage score improvement",
        KeepStableRoute(
            sameStrictProposed,
            sameStrictCurrent,
            1.0f,
            false));
    sameStrictCurrent.minimumClearance = 1.0f;
    sameStrictProposed.minimumClearance = 500.0f;
    sameStrictProposed.cursorDistance = 0.0f;
    sameStrictCurrent.cursorDistance = 5000.0f;
    ExpectTrue(
        "strict commitment ignores material clearance and cursor score",
        KeepStableRoute(
            sameStrictCurrent,
            sameStrictProposed,
            1.0f,
            false));
}

void TestRouteCommitmentFrames() {
    ThreatCoverage baseline;
    baseline.collisionCount = 1;
    baseline.endpointDanger = 1;
    baseline.pathDanger = 1;
    baseline.maxDanger = 1;
    baseline.dangerExposureMs = 200.0f;
    baseline.firstCollisionTimeMs = 124.0f;

    LockedRouteValidationInput strictLeft;
    strictLeft.hasLock = true;
    strictLeft.evaluationValid = true;
    strictLeft.walkable = true;
    strictLeft.strictSafe = true;
    strictLeft.startsInThreat = true;
    strictLeft.exitedStartEnvelope = true;
    strictLeft.coverage = baseline;
    strictLeft.baselineCoverage = baseline;
    const LockedRouteValidation strictFrame =
        ClassifyLockedRoute(strictLeft);
    ExpectTrue("strict L begins hard-valid and strict-safe",
               strictFrame.hardValid &&
                   strictFrame.safety == LockedRouteSafety::StrictSafe);

    LockedRouteValidationInput fallbackLeft = strictLeft;
    fallbackLeft.strictSafe = false;
    fallbackLeft.coverage.dangerExposureMs += 20.0f;
    fallbackLeft.coverage.firstCollisionTimeMs -= 20.0f;
    const LockedRouteValidation degradedFrame =
        ClassifyLockedRoute(fallbackLeft);
    ExpectTrue("strict L transiently demotes in place to fallback L",
               degradedFrame.hardValid &&
                   degradedFrame.safety ==
                       LockedRouteSafety::FallbackNoWorse);
    LockedRouteValidationInput longitudinalCommitted =
        fallbackLeft;
    longitudinalCommitted.exitedStartEnvelope = false;
    const LockedRouteValidation invalidLongitudinal =
        ClassifyLockedRoute(longitudinalCommitted);
    ExpectTrue(
        "committed longitudinal fallback without true exit invalidates",
        !invalidLongitudinal.hardValid &&
            invalidLongitudinal.safety ==
                LockedRouteSafety::Unsafe);
    UnavoidableDecisionInput oppositeLateral;
    oppositeLateral.holdCoverage = baseline;
    oppositeLateral.candidateCoverage = baseline;
    oppositeLateral.candidateAvailable = true;
    oppositeLateral.candidateValid = true;
    oppositeLateral.candidateWalkable = true;
    oppositeLateral.candidateMakesProgress = true;
    oppositeLateral.candidateStartsInThreat = true;
    oppositeLateral.candidateExitedStartEnvelope = true;
    oppositeLateral.fallbackLockActive = true;
    oppositeLateral.lockCoverage = baseline;
    oppositeLateral.lockValid = invalidLongitudinal.hardValid;
    oppositeLateral.lockWalkable = true;
    ExpectTrue(
        "invalid no-exit lock permits opposite lateral route",
        DecideUnavoidableAction(oppositeLateral).action ==
                UnavoidableAction::MoveFallback &&
            !DecideUnavoidableAction(oppositeLateral)
                 .retainLockedFallback);
    oppositeLateral.candidateAvailable = false;
    ExpectTrue(
        "invalid no-exit lock without route reaches hold and spell policy",
        DecideUnavoidableAction(oppositeLateral).action ==
                UnavoidableAction::Hold &&
            DecideReachedCommitmentEscalation(
                false,
                true,
                true) ==
                ReachedCommitmentEscalation::TryEvadeSpell);
    UnavoidableDecisionInput delayedTransfer =
        oppositeLateral;
    delayedTransfer.fallbackLockActive = false;
    delayedTransfer.candidateAvailable = true;
    delayedTransfer.candidateEnteredNewThreat = true;
    ExpectTrue(
        "equal-bucket A-to-delayed-B transfer is never progress",
        DecideUnavoidableAction(delayedTransfer).action ==
            UnavoidableAction::Hold);
    delayedTransfer.candidateEnteredNewThreat = false;
    delayedTransfer.candidateReenteredDanger = true;
    ExpectTrue(
        "equal-bucket re-entry is never admitted as fallback progress",
        DecideUnavoidableAction(delayedTransfer).action ==
            UnavoidableAction::Hold);
    delayedTransfer.candidateReenteredDanger = false;
    ExpectTrue(
        "clean exit from current threat remains legitimate progress",
        DecideUnavoidableAction(delayedTransfer).action ==
            UnavoidableAction::MoveFallback);
    ExpectTrue("degradation window uses bounded target lock",
               DegradationCommitWindowMs(20) == 90 &&
                   DegradationCommitWindowMs(120) == 120 &&
                   DegradationCommitWindowMs(400) == 160);

    StableRouteMetrics fallbackMetrics;
    fallbackMetrics.coverage = fallbackLeft.coverage;
    StableRouteMetrics strictEquivalent = fallbackMetrics;
    strictEquivalent.strictSafe = true;
    strictEquivalent.coverage.dangerExposureMs -= 10.0f;
    strictEquivalent.coverage.firstCollisionTimeMs += 10.0f;
    strictEquivalent.minimumClearance += 12.0f;
    strictEquivalent.travelDistance -= 12.0f;
    ExpectTrue("strict-equivalent R replaces fallback L during window",
               !KeepStableRoute(
                   fallbackMetrics,
                   strictEquivalent,
                   -1.0f,
                   false,
                   true));

    ExpectTrue("same fallback L promotes back to strict L in place",
               ShouldPromoteFallbackEvaluation(true, true, true, true));

    StableRouteMetrics materialCoverage = strictEquivalent;
    materialCoverage.coverage.collisionCount = 0;
    ExpectTrue("material discrete coverage improvement switches immediately",
               !KeepStableRoute(
                   fallbackMetrics,
                   materialCoverage,
                   -1.0f,
                   true,
                   true));
    StableRouteMetrics materialExposure = fallbackMetrics;
    materialExposure.coverage.dangerExposureMs -= 26.0f;
    ExpectTrue("material temporal coverage improvement switches immediately",
               !KeepStableRoute(
                   fallbackMetrics,
                   materialExposure,
                   -1.0f,
                   true,
                   true));

    LockedRouteValidationInput blockedLeft = fallbackLeft;
    blockedLeft.walkable = false;
    ExpectTrue("blocked L is immediately hard-invalid",
               !ClassifyLockedRoute(blockedLeft).hardValid);
    LockedRouteValidationInput reachedLeft = fallbackLeft;
    reachedLeft.reached = true;
    const LockedRouteValidation reachedValidation =
        ClassifyLockedRoute(reachedLeft);
    ExpectTrue("reached L remains hard-valid for extension policy",
               reachedValidation.hardValid &&
                   reachedValidation.reached &&
                   reachedValidation.safety ==
                       LockedRouteSafety::FallbackNoWorse);
    const float endpointTolerance =
        EndpointReachTolerance(kDefaultEndpointMargin);
    const float reachedDistances[] = {
        0.0f,
        0.5f,
        0.999f,
        1.0f,
        endpointTolerance,
    };
    for (const float distance : reachedDistances) {
        const LockedRouteValidation boundary =
            ClassifyLockedEndpointBoundary({
                baseline,
                true,
                true,
                distance,
                endpointTolerance,
                25.0f,
            });
        ExpectTrue(
            "runtime endpoint boundary is hard-valid reached fallback",
            boundary.hardValid &&
                boundary.reached &&
                boundary.safety ==
                    LockedRouteSafety::FallbackNoWorse);
    }
    const LockedRouteValidation outsideBoundary =
        ClassifyLockedEndpointBoundary({
            baseline,
            true,
            true,
            endpointTolerance + 0.001f,
            endpointTolerance,
            25.0f,
        });
    const LockedRouteValidation wallInvalidBoundary =
        ClassifyLockedEndpointBoundary({
            baseline,
            true,
            false,
            0.0f,
            endpointTolerance,
            25.0f,
        });
    ExpectTrue(
        "endpoint shortcut is exact and requires stored walkability",
        !outsideBoundary.hardValid &&
            !outsideBoundary.reached &&
            !wallInvalidBoundary.hardValid &&
            !wallInvalidBoundary.reached);
    ExpectTrue(
        "reached endpoint releases only after danger clears",
        ShouldExecuteReleaseOrDeferredResume(false, false) &&
            !ShouldExecuteReleaseOrDeferredResume(
                false,
                true));
    LockedRouteValidationInput failedLeft = fallbackLeft;
    failedLeft.hardMoveFailure = true;
    ExpectTrue("hard move failure immediately invalidates L",
               !ClassifyLockedRoute(failedLeft).hardValid);

    UnavoidableDecisionInput manualSwitch;
    manualSwitch.holdCoverage = baseline;
    manualSwitch.candidateCoverage = baseline;
    manualSwitch.candidateAvailable = true;
    manualSwitch.candidateValid = true;
    manualSwitch.candidateWalkable = true;
    manualSwitch.candidateMakesProgress = true;
    manualSwitch.fallbackLockActive = true;
    manualSwitch.lockCoverage = fallbackLeft.coverage;
    manualSwitch.lockValid = true;
    manualSwitch.lockWalkable = true;
    manualSwitch.currentManualEpoch = 2;
    manualSwitch.lockManualEpoch = 1;
    ExpectTrue("manual epoch switches immediately away from old L",
               !DecideUnavoidableAction(manualSwitch)
                    .retainLockedFallback);
    UnavoidableDecisionInput longitudinalNoExit;
    longitudinalNoExit.holdCoverage = baseline;
    longitudinalNoExit.candidateCoverage = baseline;
    longitudinalNoExit.candidateAvailable = true;
    longitudinalNoExit.candidateValid = true;
    longitudinalNoExit.candidateWalkable = true;
    longitudinalNoExit.candidateMakesProgress = true;
    longitudinalNoExit.candidateStartsInThreat = true;
    longitudinalNoExit.candidateExitedStartEnvelope = false;
    ExpectTrue(
        "longitudinal travel inside starting line cannot become progress",
        DecideUnavoidableAction(longitudinalNoExit).action ==
            UnavoidableAction::Hold);

    StableRouteMetrics fallbackExitCurrent;
    fallbackExitCurrent.coverage = baseline;
    fallbackExitCurrent.exitedStartEnvelope = true;
    fallbackExitCurrent.exitDistance = 120.0f;
    fallbackExitCurrent.travelDistance = 180.0f;
    StableRouteMetrics fallbackExitShorter = fallbackExitCurrent;
    fallbackExitShorter.exitDistance = 70.0f;
    fallbackExitShorter.travelDistance = 105.0f;
    ExpectTrue(
        "fallback lock may shorten materially on the same physical side",
        ShouldReplaceCommittedFallback(
            fallbackExitCurrent,
            fallbackExitShorter,
            true));
    ExpectTrue(
        "fallback lock cannot shorten by flipping physical side",
        !ShouldReplaceCommittedFallback(
            fallbackExitCurrent,
            fallbackExitShorter,
            false));
    StableRouteMetrics fallbackNoExit = fallbackExitShorter;
    fallbackNoExit.exitedStartEnvelope = false;
    fallbackNoExit.exitDistance =
        std::numeric_limits<float>::infinity();
    ExpectTrue(
        "fallback no-exit target cannot replace true exit",
        !ShouldReplaceCommittedFallback(
            fallbackExitCurrent,
            fallbackNoExit,
            true));
    bool tenNoisyReplansStable = true;
    for (int frame = 0; frame < 10; ++frame) {
        StableRouteMetrics noisyOpposite = fallbackExitShorter;
        noisyOpposite.exitDistance +=
            static_cast<float>((frame % 3) - 1) * 4.0f;
        noisyOpposite.travelDistance +=
            static_cast<float>((frame % 2) ? 6 : -6);
        tenNoisyReplansStable =
            tenNoisyReplansStable &&
            !ShouldReplaceCommittedFallback(
                fallbackExitCurrent,
                noisyOpposite,
                false);
    }
    ExpectTrue(
        "ten noisy opposite-side fallback replans retain exact lock",
        tenNoisyReplansStable);

    StableRouteMetrics noisyFallback = fallbackMetrics;
    noisyFallback.coverage.dangerExposureMs += 24.0f;
    noisyFallback.coverage.firstCollisionTimeMs -= 24.0f;
    noisyFallback.timeMarginMs -= 24.0f;
    noisyFallback.minimumClearance -= 15.0f;
    noisyFallback.travelDistance += 15.0f;
    noisyFallback.cursorDistance = 5000.0f;
    ExpectTrue("fallback metric and cursor noise retain L under timer",
               KeepStableRoute(
                   fallbackMetrics,
                   noisyFallback,
                   -1.0f,
                   true,
                   false));
    StableRouteMetrics cursorMovedCurrent = fallbackMetrics;
    cursorMovedCurrent.cursorDistance = 5000.0f;
    StableRouteMetrics cursorMovedProposed = fallbackMetrics;
    ExpectTrue("cursor movement alone cannot switch fallback under timer",
               KeepStableRoute(
                   cursorMovedCurrent,
                   cursorMovedProposed,
                   1.0f,
                   true,
                   false));
    ThreatCoverage dangerOne;
    dangerOne.collisionCount = 1;
    dangerOne.pathDanger = 1;
    dangerOne.maxDanger = 1;
    ThreatCoverage dangerOneBucket = dangerOne;
    dangerOneBucket.dangerExposureMs = 24.0f;
    ThreatCoverage dangerOneBeyond = dangerOne;
    dangerOneBeyond.dangerExposureMs = 25.0f;
    ExpectTrue("danger-1 exposure bucket is 25 danger-ms",
               DangerExposureBucketSize(
                   dangerOne,
                   25.0f) == 25.0f &&
               EquivalentThreatCoverageAtResolution(
                   dangerOne,
                   dangerOneBucket,
                   25.0f) &&
                   !ThreatCoverageNoWorseAtResolution(
                       dangerOneBeyond,
                       dangerOne,
                       25.0f));
    ThreatCoverage dangerFive = dangerOne;
    dangerFive.pathDanger = 5;
    dangerFive.maxDanger = 5;
    ThreatCoverage dangerFiveBuckets = dangerFive;
    dangerFiveBuckets.dangerExposureMs = 124.0f;
    ThreatCoverage dangerFiveBeyond = dangerFive;
    dangerFiveBeyond.dangerExposureMs = 125.0f;
    ExpectTrue("danger-5 exposure bucket is 125 danger-ms",
               DangerExposureBucketSize(
                   dangerFive,
                   25.0f) == 125.0f &&
               EquivalentThreatCoverageAtResolution(
                   dangerFive,
                   dangerFiveBuckets,
                   25.0f) &&
                   !ThreatCoverageNoWorseAtResolution(
                       dangerFiveBeyond,
                       dangerFive,
                       25.0f));
    ThreatCoverage twoDangerFive = dangerFive;
    twoDangerFive.collisionCount = 2;
    twoDangerFive.pathDanger = 10;
    ThreatCoverage twoDangerFiveWithin = twoDangerFive;
    twoDangerFiveWithin.dangerExposureMs = 249.0f;
    ThreatCoverage twoDangerFiveCross = twoDangerFive;
    twoDangerFiveCross.dangerExposureMs = 250.0f;
    ExpectTrue("two danger-5 overlaps use 250 danger-ms buckets",
               DangerExposureBucketSize(
                   twoDangerFive,
                   25.0f) == 250.0f &&
                   EquivalentThreatCoverageAtResolution(
                       twoDangerFive,
                       twoDangerFiveWithin,
                       25.0f) &&
                   !ThreatCoverageNoWorseAtResolution(
                       twoDangerFiveCross,
                       twoDangerFive,
                       25.0f));

    const float transitiveExposure[] = {0.0f, 24.0f, 48.0f};
    for (int left = 0; left < 3; ++left) {
        for (int middle = 0; middle < 3; ++middle) {
            for (int right = 0; right < 3; ++right) {
                ThreatCoverage a = dangerOne;
                ThreatCoverage b = dangerOne;
                ThreatCoverage c = dangerOne;
                a.dangerExposureMs = transitiveExposure[left];
                b.dangerExposureMs = transitiveExposure[middle];
                c.dangerExposureMs = transitiveExposure[right];
                const bool ab = EquivalentThreatCoverageAtResolution(
                    a, b, 25.0f);
                const bool bc = EquivalentThreatCoverageAtResolution(
                    b, c, 25.0f);
                const bool ac = EquivalentThreatCoverageAtResolution(
                    a, c, 25.0f);
                ExpectTrue(
                    "quantized exposure equivalence is transitive",
                    !ab || !bc || ac);
            }
        }
    }
    ThreatCoverage exposureZero = dangerOne;
    ThreatCoverage exposureTwentyFour = dangerOne;
    exposureTwentyFour.dangerExposureMs = 24.0f;
    ThreatCoverage exposureFortyEight = dangerOne;
    exposureFortyEight.dangerExposureMs = 48.0f;
    ExpectTrue("0 and 24 share deterministic half-open bucket",
               EquivalentThreatCoverageAtResolution(
                   exposureZero,
                   exposureTwentyFour,
                   25.0f));
    ExpectTrue("24 and 48 cross deterministic bucket boundary",
               !EquivalentThreatCoverageAtResolution(
                   exposureTwentyFour,
                   exposureFortyEight,
                   25.0f));
    ExpectTrue("continuous retention metrics share bucket semantics",
               TemporalMetricBucketId(
                   0.0f,
                   25.0f,
                   false) ==
                       TemporalMetricBucketId(
                           24.0f,
                           25.0f,
                           false) &&
                   TemporalMetricBucketId(
                       24.0f,
                       25.0f,
                       false) !=
                       TemporalMetricBucketId(
                           48.0f,
                           25.0f,
                           false));
    StableRouteMetrics metricBucketCurrent = fallbackMetrics;
    metricBucketCurrent.minimumClearance = 0.0f;
    StableRouteMetrics metricBucketWithin = metricBucketCurrent;
    metricBucketWithin.minimumClearance = 19.0f;
    StableRouteMetrics metricBucketCross = metricBucketCurrent;
    metricBucketCross.minimumClearance = 20.0f;
    ExpectTrue("within-bucket clearance noise retains current route",
               KeepStableRoute(
                   metricBucketCurrent,
                   metricBucketWithin,
                   1.0f,
                   false,
                   false));
    ExpectTrue("cross-bucket clearance gain is material",
               !KeepStableRoute(
                   metricBucketCurrent,
                   metricBucketCross,
                   1.0f,
                   false,
                   false));

    const std::uint64_t maximumBucket =
        std::numeric_limits<std::uint64_t>::max();
    ExpectTrue("exposure nonfinite values normalize deterministically",
               DangerExposureBucketId(
                   std::numeric_limits<float>::quiet_NaN(),
                   dangerOne,
                   25.0f) == maximumBucket &&
                   DangerExposureBucketId(
                       std::numeric_limits<float>::infinity(),
                       dangerOne,
                       25.0f) == maximumBucket &&
                   DangerExposureBucketId(
                       -std::numeric_limits<float>::infinity(),
                       dangerOne,
                       25.0f) == maximumBucket);
    ExpectTrue("contact nonfinite values normalize by safety direction",
               TemporalMetricBucketId(
                   std::numeric_limits<float>::quiet_NaN(),
                   25.0f,
                   false) == 0 &&
                   TemporalMetricBucketId(
                       std::numeric_limits<float>::infinity(),
                       25.0f,
                       false) == 0 &&
                   TemporalMetricBucketId(
                       -std::numeric_limits<float>::infinity(),
                       25.0f,
                       false) == 0);

    ThreatCoverage discreteWorse = dangerFive;
    ++discreteWorse.collisionCount;
    ExpectTrue("discrete coverage remains exact despite exposure deadband",
               !ThreatCoverageNoWorseAtResolution(
                   discreteWorse,
                   dangerFive,
                   25.0f));

    ExpectTrue("twelve units short remains commanded and revalidated",
               !IsMoveTargetReached(
                   12.0f,
                   EndpointReachTolerance(kDefaultEndpointMargin),
                   false) &&
                   !IsRouteTargetReached(
                       12.0f,
                       kDefaultEndpointMargin,
                       false) &&
                   KeepStableRoute(
                       fallbackMetrics,
                       noisyFallback,
                       -1.0f,
                       true,
                       false));
    LockedRouteValidationInput twelveShort = fallbackLeft;
    twelveShort.reached = IsRouteTargetReached(
        12.0f,
        kDefaultEndpointMargin,
        false);
    ExpectTrue("twelve-short lock remains hard-valid for revalidation",
               ClassifyLockedRoute(twelveShort).hardValid);
    ExpectTrue("only exact-safe endpoint reach bypasses retention",
               IsRouteTargetReached(
                   EndpointReachTolerance(kDefaultEndpointMargin),
                   kDefaultEndpointMargin,
                   false));
    ExpectTrue("exact danger inside tolerance retains endpoint command",
               !IsRouteTargetReached(
                   1.0f,
                   kDefaultEndpointMargin,
                   true) &&
                   !IsMoveTargetReached(
                       1.0f,
                       EndpointReachTolerance(kDefaultEndpointMargin),
                       true));

    ExpectTrue("target-lock expiry allows genuine strict improvement",
               !KeepStableRoute(
                   fallbackMetrics,
                   strictEquivalent,
                   -1.0f,
                   false,
                   false));

    LockedRouteValidationInput secondThreat = fallbackLeft;
    ++secondThreat.coverage.collisionCount;
    secondThreat.coverage.dangerExposureMs = 1.0f;
    ExpectTrue("new threat cannot retain discretely worse L",
               ClassifyLockedRoute(secondThreat).safety ==
                   LockedRouteSafety::Unsafe);
    LockedRouteValidationInput safeSecondThreat = fallbackLeft;
    ++safeSecondThreat.coverage.collisionCount;
    ++safeSecondThreat.baselineCoverage.collisionCount;
    ExpectTrue("new second threat retains L while it remains no-worse",
               ClassifyLockedRoute(safeSecondThreat).safety ==
                   LockedRouteSafety::FallbackNoWorse);

    StrictCommitmentInput deferredResume;
    deferredResume.committedState = true;
    deferredResume.route = {
        true, true, true, true, true, false,
    };
    deferredResume.deferredResumeReady = true;
    ExpectTrue("safe deferred resume wins over target commitment",
               !ShouldRetainCommittedStrictTarget(deferredResume));
    ExpectTrue("release-margin danger cannot block strict deferred resume",
               ShouldExecuteReleaseOrDeferredResume(true, true));
    ExpectTrue("ordinary release-margin danger remains protected",
               !ShouldExecuteReleaseOrDeferredResume(false, true));

    const Vec2 challengerOrigin(0.0f, 100.0f);
    const std::uint64_t oneThreatFingerprint =
        StableThreatSetFingerprint({17});
    const std::uint64_t twoThreatFingerprint =
        StableThreatSetFingerprint({17, 23});
    ExpectTrue("threat fingerprint is order independent and set based",
               twoThreatFingerprint ==
                       StableThreatSetFingerprint({23, 17}) &&
                   twoThreatFingerprint ==
                       StableThreatSetFingerprint({23, 17, 17}));
    ExpectTrue("adding or removing a threat changes fingerprint",
               oneThreatFingerprint != twoThreatFingerprint &&
                   oneThreatFingerprint !=
                       StableThreatSetFingerprint({}));
    ExpectTrue("uninitialized threat IDs are stable across order and value",
               StableThreatSetFingerprint({-1, 17, -1}) ==
                       StableThreatSetFingerprint({17, -9, -4}) &&
                   StableThreatSetFingerprint({-1, 17}) ==
                       StableThreatSetFingerprint({-99, 17}) &&
                   StableThreatSetFingerprint({-1, 17}) !=
                       StableThreatSetFingerprint({-1, 17, -1}));

    ContinuousChallengerState challenger;
    const Vec2 challengerRight(200.0f, 100.0f);
    ContinuousChallengerDecision boundaryWin =
        AdvanceContinuousChallenger(
            challenger,
            challengerOrigin,
            challengerRight,
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1000,
            1200.0f,
            true);
    ContinuousChallengerDecision boundaryLoss =
        AdvanceContinuousChallenger(
            boundaryWin.state,
            challengerOrigin,
            challengerRight,
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1045,
            1200.0f,
            false);
    ContinuousChallengerDecision boundaryWinAgain =
        AdvanceContinuousChallenger(
            boundaryLoss.state,
            challengerOrigin,
            challengerRight,
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1090,
            1200.0f,
            true);
    ExpectTrue("alternating 24.9 and 25.0 boundary cannot switch",
               !boundaryWin.switchReady &&
                   !boundaryLoss.switchReady &&
                   !boundaryWinAgain.switchReady &&
                   boundaryWinAgain.state.consecutiveWins == 1);

    const ContinuousChallengerDecision repeatedWin =
        AdvanceContinuousChallenger(
            boundaryWinAgain.state,
            challengerOrigin,
            Vec2(209.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1135,
            1200.0f,
            true);
    ExpectTrue("same nearby challenger wins twice then switches once",
               repeatedWin.switchReady &&
                   repeatedWin.state.consecutiveWins == 2);

    const ContinuousChallengerDecision driftingFirst =
        AdvanceContinuousChallenger(
            {},
            challengerOrigin,
            Vec2(200.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1400,
            1200.0f,
            true);
    const ContinuousChallengerDecision driftingSecond =
        AdvanceContinuousChallenger(
            driftingFirst.state,
            challengerOrigin,
            Vec2(260.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1445,
            1200.0f,
            true);
    ExpectTrue("same-side high-speed 60-unit drift accumulates two wins",
               !driftingFirst.switchReady &&
                   driftingSecond.switchReady &&
                   driftingSecond.state.consecutiveWins == 2 &&
                   driftingSecond.state.direction.x > 0.99f &&
                   driftingSecond.state.stabilityBranchKey ==
                       StabilityBranch::ConeLeft);

    ContinuousChallengerState narrowConeAlternating;
    bool narrowConeReady = false;
    for (int frame = 0; frame < 6; ++frame) {
        const bool left = (frame % 2) == 0;
        const ContinuousChallengerDecision decision =
            AdvanceContinuousChallenger(
                narrowConeAlternating,
                challengerOrigin,
                left
                    ? Vec2(200.0f, 135.0f)
                    : Vec2(200.0f, 65.0f),
                4,
                17,
                left
                    ? StabilityBranch::ConeLeft
                    : StabilityBranch::ConeRight,
                4,
                oneThreatFingerprint,
                1500 + frame * 45,
                1200.0f,
                true);
        narrowConeAlternating = decision.state;
        narrowConeReady =
            narrowConeReady || decision.switchReady;
    }
    ExpectTrue(
        "narrow cone branches reset despite close direction and distance",
        !narrowConeReady &&
            narrowConeAlternating.consecutiveWins == 1);

    ContinuousChallengerState closeCircleAlternating;
    bool closeCircleReady = false;
    for (int frame = 0; frame < 6; ++frame) {
        const bool counterClockwise = (frame % 2) == 0;
        const ContinuousChallengerDecision decision =
            AdvanceContinuousChallenger(
                closeCircleAlternating,
                challengerOrigin,
                counterClockwise
                    ? Vec2(200.0f, 125.0f)
                    : Vec2(200.0f, 75.0f),
                3,
                17,
                counterClockwise
                    ? StabilityBranch::CircleCounterClockwise
                    : StabilityBranch::CircleClockwise,
                4,
                oneThreatFingerprint,
                1800 + frame * 45,
                1200.0f,
                true);
        closeCircleAlternating = decision.state;
        closeCircleReady =
            closeCircleReady || decision.switchReady;
    }
    ExpectTrue(
        "close circle tangent branches reset challenger wins",
        !closeCircleReady &&
            closeCircleAlternating.consecutiveWins == 1);

    ContinuousChallengerState alternating;
    bool alternatingReady = false;
    for (int frame = 0; frame < 6; ++frame) {
        const Vec2 side = (frame % 2) == 0
            ? Vec2(200.0f, 100.0f)
            : Vec2(-200.0f, 100.0f);
        const ContinuousChallengerDecision decision =
            AdvanceContinuousChallenger(
                alternating,
                challengerOrigin,
                side,
                2,
                17,
                StabilityBranch::ConeLeft,
                4,
                oneThreatFingerprint,
                1200 + frame * 45,
                1200.0f,
                true);
        alternating = decision.state;
        alternatingReady = alternatingReady ||
            decision.switchReady;
    }
    ExpectTrue("alternating L and R never accumulate challenger wins",
               !alternatingReady &&
                   alternating.consecutiveWins == 1);

    const ContinuousChallengerDecision sameIdSerialFirst =
        AdvanceContinuousChallenger(
            {},
            challengerOrigin,
            challengerRight,
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1600,
            1200.0f,
            true);
    const ContinuousChallengerDecision sameIdSerialSecond =
        AdvanceContinuousChallenger(
            sameIdSerialFirst.state,
            challengerOrigin,
            Vec2(245.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1645,
            1200.0f,
            true);
    ExpectTrue("same threat IDs keep serial-noise challenger hysteresis",
               !sameIdSerialFirst.switchReady &&
                   sameIdSerialSecond.switchReady &&
                   sameIdSerialSecond.state.consecutiveWins == 2 &&
                   RequiresContinuousSwitchHysteresis(
                       true,
                       false,
                       false,
                       false,
                       false));

    const ContinuousChallengerDecision addedThreatReset =
        AdvanceContinuousChallenger(
            sameIdSerialFirst.state,
            challengerOrigin,
            Vec2(245.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            twoThreatFingerprint,
            1645,
            1200.0f,
            true);
    ExpectTrue("new second threat resets stale challenger wins",
               !addedThreatReset.switchReady &&
                   addedThreatReset.state.consecutiveWins == 1 &&
                   addedThreatReset.state.threatSetFingerprint ==
                       twoThreatFingerprint);
    ExpectTrue("new threat-set improvement switches immediately",
               !RequiresContinuousSwitchHysteresis(
                   true,
                   false,
                   false,
                   false,
                   true));

    const ContinuousChallengerDecision removedThreatReset =
        AdvanceContinuousChallenger(
            addedThreatReset.state,
            challengerOrigin,
            Vec2(275.0f, 100.0f),
            2,
            17,
            StabilityBranch::ConeLeft,
            4,
            oneThreatFingerprint,
            1690,
            1200.0f,
            true);
    ExpectTrue("removed threat resets stale challenger wins",
               !removedThreatReset.switchReady &&
                   removedThreatReset.state.consecutiveWins == 1 &&
                   removedThreatReset.state.threatSetFingerprint ==
                       oneThreatFingerprint);

    ExpectTrue("continuous-only admissible change requires hysteresis",
               RequiresContinuousSwitchHysteresis(
                   true,
                   false,
                   false,
                   false));
    ExpectTrue("new-threat discrete improvement switches immediately",
               !RequiresContinuousSwitchHysteresis(
                   true,
                   false,
                   true,
                   false));
    ExpectTrue("strict, hard-invalid, and manual changes switch immediately",
               !RequiresContinuousSwitchHysteresis(
                   true,
                   true,
                   false,
                   false) &&
                   !RequiresContinuousSwitchHysteresis(
                       false,
                       false,
                       false,
                       false) &&
                   !RequiresContinuousSwitchHysteresis(
                       true,
                       false,
                       false,
                       true));
}

void TestSimpleCommittedRoutePolicyFrames() {
    struct Harness {
        CommittedRouteIdentity commitment;
        Vec2 committedTarget = {};
        int switches = 0;
        int commits = 0;
        int lastCommittedBranch = StabilityBranch::Unknown;

        CommittedRouteDecision Step(
            std::uint64_t fingerprint,
            std::uint64_t manualEpoch,
            bool currentHardValid,
            bool currentNoWorse,
            bool reached,
            int sourceThreatId,
            int branchKey,
            const Vec2& direction,
            const Vec2& target,
            MoveIssueResult moveResult,
            bool threatSetEmpty = false,
            bool candidateSyntheticExtension = false,
            bool reachedExtensionEvaluated = false,
            bool candidateStartsInThreat = false,
            bool candidateExitedStartEnvelope = false) {
            CommittedRoutePolicyInput input;
            input.commitment = commitment;
            input.threatSetFingerprint = fingerprint;
            input.manualEpoch = manualEpoch;
            input.threatSetEmpty = threatSetEmpty;
            input.currentHardValid = currentHardValid;
            input.currentNoWorse = currentNoWorse;
            input.currentReached = reached;
            input.candidateAvailable =
                target.IsValid() &&
                !target.IsZero();
            input.candidateStartsInThreat =
                candidateStartsInThreat;
            input.candidateExitedStartEnvelope =
                candidateExitedStartEnvelope;
            input.candidateSourceThreatId = sourceThreatId;
            input.candidateStabilityBranchKey = branchKey;
            input.candidateDirection = direction;
            input.candidateSyntheticExtension =
                candidateSyntheticExtension;
            input.reachedExtensionEvaluated =
                reachedExtensionEvaluated;
            const CommittedRouteDecision decision =
                DecideCommittedRoute(input);
            commitment = decision.commitment;
            if (decision.action ==
                CommittedRouteAction::ReleaseReachedBranch) {
                commitment = {};
            }
            if (decision.action ==
                    CommittedRouteAction::ProposeSameBranch ||
                decision.action ==
                    CommittedRouteAction::ProposeSameDirectionExtension ||
                decision.action ==
                    CommittedRouteAction::ProposeBranchSwitch) {
                const TargetCommitDecision targetCommit =
                    DecideTargetCommit(moveResult, currentHardValid);
                if (targetCommit.commitProposed) {
                    commitment = CommitProposedRoute(
                        decision,
                        input);
                    committedTarget = target;
                    ++commits;
                    if (lastCommittedBranch !=
                            StabilityBranch::Unknown &&
                        lastCommittedBranch !=
                            commitment.stabilityBranchKey) {
                        ++switches;
                    }
                    lastCommittedBranch =
                        commitment.stabilityBranchKey;
                }
            }
            return decision;
        }
    };

    const std::uint64_t oneThreat =
        StableThreatSetFingerprint({701});
    const std::uint64_t twoThreats =
        StableThreatSetFingerprint({701, 702});
    const int left = StabilityBranch::LineAnalyticalLeft;
    const int right = StabilityBranch::LineAnalyticalRight;
    const Vec2 leftDirection(0.0f, 1.0f);
    const Vec2 rightDirection(0.0f, -1.0f);
    const float endpointTolerance =
        EndpointReachTolerance(kDefaultEndpointMargin);
    Harness noExitInitial;
    const CommittedRouteDecision rejectedInitialNoExit =
        noExitInitial.Step(
            oneThreat,
            1,
            false,
            false,
            false,
            701,
            StabilityBranch::LineEndCap,
            Vec2(1.0f, 0.0f),
            Vec2(500.0f, 100.0f),
            MoveIssueResult::Issued,
            false,
            false,
            false,
            true,
            false);
    ExpectTrue(
        "initial longitudinal no-exit candidate cannot be committed",
        rejectedInitialNoExit.action ==
                CommittedRouteAction::KeepCommitted &&
            !noExitInitial.commitment.active &&
            noExitInitial.commits == 0);

    Harness noExitSameBranch;
    noExitSameBranch.Step(
        oneThreat,
        2,
        false,
        false,
        false,
        701,
        left,
        leftDirection,
        Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued,
        false,
        false,
        false,
        true,
        true);
    const Vec2 acceptedTrueExitTarget =
        noExitSameBranch.committedTarget;
    const CommittedRouteDecision rejectedSameBranchNoExit =
        noExitSameBranch.Step(
            oneThreat,
            2,
            false,
            false,
            false,
            701,
            left,
            leftDirection,
            Vec2(100.0f, 420.0f),
            MoveIssueResult::Issued,
            false,
            false,
            false,
            true,
            false);
    ExpectTrue(
        "same-branch no-exit fallback is unavailable",
        rejectedSameBranchNoExit.action ==
                CommittedRouteAction::KeepCommitted &&
            noExitSameBranch.commits == 1 &&
            noExitSameBranch.committedTarget.Distance(
                acceptedTrueExitTarget) < 0.001f);

    Harness staleNoExitBranch;
    staleNoExitBranch.commitment = {
        oneThreat,
        701,
        StabilityBranch::LineEndCap,
        Vec2(1.0f, 0.0f),
        2,
        true,
    };
    staleNoExitBranch.lastCommittedBranch =
        StabilityBranch::LineEndCap;
    const CommittedRouteDecision escapedStaleNoExit =
        staleNoExitBranch.Step(
            oneThreat,
            2,
            false,
            false,
            false,
            701,
            left,
            leftDirection,
            Vec2(100.0f, 220.0f),
            MoveIssueResult::Issued,
            false,
            false,
            false,
            true,
            true);
    ExpectTrue(
        "bad committed line branch switches once to analytical true exit",
        escapedStaleNoExit.action ==
                CommittedRouteAction::ProposeBranchSwitch &&
            staleNoExitBranch.commitment.stabilityBranchKey == left &&
            staleNoExitBranch.switches == 1);

    const float endpointDistances[] = {
        0.0f,
        0.5f,
        0.999f,
        1.0f,
        endpointTolerance,
    };
    ThreatCoverage reachedBaseline;
    Harness endpointNoFlip;
    endpointNoFlip.Step(
        oneThreat, 2, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const Vec2 endpointTarget =
        endpointNoFlip.committedTarget;
    bool allBoundariesKeepCommitment = true;
    for (const float distance : endpointDistances) {
        const LockedRouteValidation boundary =
            ClassifyLockedEndpointBoundary({
                reachedBaseline,
                true,
                true,
                distance,
                endpointTolerance,
                25.0f,
            });
        const CommittedRouteDecision decision =
            endpointNoFlip.Step(
                oneThreat,
                2,
                boundary.hardValid,
                boundary.safety !=
                    LockedRouteSafety::Unsafe,
                boundary.reached,
                701,
                right,
                rightDirection,
                Vec2(100.0f, -260.0f),
                MoveIssueResult::Issued);
        allBoundariesKeepCommitment =
            allBoundariesKeepCommitment &&
            decision.action ==
                CommittedRouteAction::KeepCommitted &&
            endpointNoFlip.committedTarget.Distance(
                endpointTarget) < 0.001f;
    }
    ExpectTrue(
        "all reached boundaries defer planner branch flip to extension",
        allBoundariesKeepCommitment &&
            endpointNoFlip.commits == 1 &&
            endpointNoFlip.switches == 0 &&
            endpointNoFlip.commitment.stabilityBranchKey ==
                left);

    SpellData semanticSpell =
        ZDEvadeTest::MakeSpell(ZDSpellType::Line);
    semanticSpell.spellName = "VeigarQ";
    semanticSpell.spellKey = ZDSpellSlot::Q;
    Threat semanticA = ZDEvadeTest::MakeThreat(semanticSpell);
    semanticA.id = 701;
    semanticA.logicalCastEpisodeId = 5001;
    semanticA.casterNetworkId = 99;
    semanticA.slot = static_cast<int>(ZDSpellSlot::Q);
    semanticA.startTick = 989;
    semanticA.startPos = Vec2(100.0f, 100.0f);
    semanticA.endPos = Vec2(1100.0f, 100.0f);
    semanticA.direction = Vec2(1.0f, 0.0f);
    semanticA.projectileLaneKey = 1;
    Threat semanticB = semanticA;
    semanticB.id = 702;
    semanticB.logicalCastEpisodeId =
        semanticA.logicalCastEpisodeId;
    semanticB.startTick = 1070;
    semanticB.startPos = Vec2(108.0f, 101.0f);
    semanticB.direction = Vec2(0.999848f, 0.017452f);
    semanticB.endPos =
        semanticB.startPos + semanticB.direction * 1000.0f;
    semanticB.missileBound = true;
    Threat semanticC = semanticA;
    semanticC.id = 706;
    semanticC.startTick = 1050;
    semanticC.direction = Vec2(0.999998f, -0.001745f);
    semanticC.endPos =
        semanticC.startPos + semanticC.direction * 1000.0f;
    const std::uint64_t semanticAOnly =
        StableSemanticThreatSetFingerprint({semanticA});
    const std::uint64_t semanticDuplicates =
        StableSemanticThreatSetFingerprint({semanticA, semanticB});
    ExpectTrue(
        "Veigar Q A A+B B transitions keep one semantic fingerprint",
        semanticAOnly == semanticDuplicates &&
            semanticDuplicates ==
                StableSemanticThreatSetFingerprint(
                    {semanticB, semanticA}) &&
            semanticDuplicates ==
                StableSemanticThreatSetFingerprint(
                    {semanticC, semanticB}) &&
            semanticDuplicates ==
                StableSemanticThreatSetFingerprint({semanticB}) &&
            semanticDuplicates ==
                StableSemanticThreatSetFingerprint({semanticC}) &&
            SemanticThreatGroupCount({semanticA, semanticB}) == 1);

    Threat laterCast = semanticA;
    laterCast.id = 703;
    laterCast.logicalCastEpisodeId = 5002;
    laterCast.startTick = semanticA.startTick + 400;
    Threat incompatibleGeometry = semanticA;
    incompatibleGeometry.id = 704;
    incompatibleGeometry.logicalCastEpisodeId = 5003;
    incompatibleGeometry.direction = Vec2(0.0f, 1.0f);
    incompatibleGeometry.endPos = Vec2(100.0f, 1100.0f);
    ExpectTrue(
        "true second cast and incompatible geometry change fingerprint",
        StableSemanticThreatSetFingerprint(
            {semanticA, laterCast}) != semanticAOnly &&
            StableSemanticThreatSetFingerprint(
                {laterCast}) != semanticAOnly &&
            StableSemanticThreatSetFingerprint(
                {semanticA, incompatibleGeometry}) != semanticAOnly &&
            StableSemanticThreatSetFingerprint(
                {incompatibleGeometry}) != semanticAOnly);

    SpellData wildSpell = semanticSpell;
    wildSpell.spellName = "WildCards";
    Threat wildCenter = ZDEvadeTest::MakeThreat(wildSpell);
    wildCenter.id = 710;
    wildCenter.logicalCastEpisodeId = 6001;
    wildCenter.projectileIndex = 1;
    wildCenter.casterNetworkId = semanticA.casterNetworkId;
    wildCenter.slot = semanticA.slot;
    wildCenter.startTick = semanticA.startTick;
    wildCenter.startPos = semanticA.startPos;
    wildCenter.endPos = semanticA.endPos;
    wildCenter.direction = semanticA.direction;
    wildCenter.projectileLaneKey =
        StableProjectileLaneKey(wildCenter.direction);
    Threat wildLeft = wildCenter;
    wildLeft.id = 711;
    wildLeft.projectileIndex = 0;
    wildLeft.direction = Vec2(0.882948f, 0.469472f);
    wildLeft.projectileLaneKey =
        StableProjectileLaneKey(wildLeft.direction);
    wildLeft.endPos =
        wildLeft.startPos + wildLeft.direction * 1000.0f;
    Threat wildRight = wildCenter;
    wildRight.id = 712;
    wildRight.projectileIndex = 2;
    wildRight.direction = Vec2(0.882948f, -0.469472f);
    wildRight.projectileLaneKey =
        StableProjectileLaneKey(wildRight.direction);
    wildRight.endPos =
        wildRight.startPos + wildRight.direction * 1000.0f;
    const std::uint64_t wildLanes =
        StableSemanticThreatSetFingerprint(
            {wildLeft, wildCenter, wildRight});
    ExpectTrue(
        "Wild Cards lanes remain distinct and order independent",
        SemanticThreatGroupCount(
            {wildLeft, wildCenter, wildRight}) == 3 &&
            wildLanes == StableSemanticThreatSetFingerprint(
                {wildRight, wildLeft, wildCenter}));
    Threat renumberedWildLeft = wildLeft;
    renumberedWildLeft.projectileIndex = 2;
    ExpectTrue(
        "logical fingerprint uses stable lane key not mutable lane index",
        StableSemanticThreatSetFingerprint({wildLeft}) ==
            StableSemanticThreatSetFingerprint(
                {renumberedWildLeft}));
    Threat missileOnlyA = wildLeft;
    missileOnlyA.logicalCastEpisodeId = 6100;
    missileOnlyA.projectileIndex = -1;
    missileOnlyA.projectileLaneKey = 1;
    Threat jitteredMissileOnlyA = missileOnlyA;
    jitteredMissileOnlyA.direction =
        Vec2(0.874620f, 0.484810f);
    jitteredMissileOnlyA.endPos =
        jitteredMissileOnlyA.startPos +
        jitteredMissileOnlyA.direction * 1000.0f;
    Threat missileOnlyB = wildRight;
    missileOnlyB.logicalCastEpisodeId =
        missileOnlyA.logicalCastEpisodeId;
    missileOnlyB.projectileIndex = -1;
    missileOnlyB.projectileLaneKey = 2;
    Threat jitteredMissileOnlyB = missileOnlyB;
    jitteredMissileOnlyB.direction =
        Vec2(0.891007f, -0.453990f);
    jitteredMissileOnlyB.endPos =
        jitteredMissileOnlyB.startPos +
        jitteredMissileOnlyB.direction * 1000.0f;
    ExpectTrue(
        "A A+B B missile-only fingerprints keep stable lane identity",
        StableSemanticThreatSetFingerprint({missileOnlyA}) ==
            StableSemanticThreatSetFingerprint(
                {jitteredMissileOnlyA}) &&
            StableSemanticThreatSetFingerprint(
                {missileOnlyA, missileOnlyB}) ==
            StableSemanticThreatSetFingerprint(
                {jitteredMissileOnlyB, jitteredMissileOnlyA}) &&
            StableSemanticThreatSetFingerprint({missileOnlyB}) ==
            StableSemanticThreatSetFingerprint(
                {jitteredMissileOnlyB}));

    Threat closeLaneA = wildCenter;
    closeLaneA.id = 720;
    closeLaneA.projectileIndex = 0;
    closeLaneA.direction = Vec2(1.0f, 0.0f);
    closeLaneA.projectileLaneKey =
        StableProjectileLaneKey(closeLaneA.direction);
    closeLaneA.endPos =
        closeLaneA.startPos + closeLaneA.direction * 1000.0f;
    Threat closeLaneB = closeLaneA;
    closeLaneB.id = 721;
    closeLaneB.projectileIndex = 1;
    closeLaneB.direction = Vec2(0.999391f, 0.034899f);
    closeLaneB.projectileLaneKey =
        StableProjectileLaneKey(closeLaneB.direction);
    closeLaneB.endPos =
        closeLaneB.startPos + closeLaneB.direction * 1000.0f;
    ExpectTrue(
        "close multi-projectile lanes stay distinct by lane key",
        SameSemanticThreatEpisode(closeLaneA, closeLaneB) == false &&
            SemanticThreatGroupCount({closeLaneA, closeLaneB}) == 2);
    std::vector<Threat> indexedStore = {closeLaneA};
    ExpectTrue(
        "detector dedup never merges different known lane keys",
        FindNormalizedCastDuplicate(indexedStore, closeLaneB) ==
            nullptr);
    Threat unknownLaneCorrection = closeLaneA;
    unknownLaneCorrection.projectileIndex = -1;
    unknownLaneCorrection.projectileLaneKey = 0;
    const int indexedThreatId = closeLaneA.id;
    const std::uint64_t indexedLaneKey =
        closeLaneA.projectileLaneKey;
    MergeNormalizedCastDuplicate(
        indexedStore.front(),
        unknownLaneCorrection);
    CorrectExistingThreatFromMissile(
        indexedStore.front(),
        [](Threat& bound) {
            bound.missileBound = true;
            ++bound.revision;
        });
    ExpectTrue(
        "dedup binding and missile correction preserve lane identity",
        StableProjectileLaneIndex(3, 0) == 0 &&
            StableProjectileLaneIndex(1, 0) == -1 &&
            indexedStore.front().id == indexedThreatId &&
            indexedStore.front().logicalCastEpisodeId ==
                closeLaneA.logicalCastEpisodeId &&
            indexedStore.front().projectileLaneKey ==
                indexedLaneKey &&
            indexedStore.front().projectileIndex == 0 &&
            indexedStore.front().missileBound);
    Threat retainedEpisode = indexedStore.front();
    retainedEpisode.projectileTerminated = true;
    retainedEpisode.missileBound = false;
    retainedEpisode.missingMissileTermination = true;
    std::vector<Threat> lifecycleSurvivors = {
        retainedEpisode,
        semanticA,
    };
    lifecycleSurvivors[1].expired = true;
    lifecycleSurvivors.erase(
        std::remove_if(
            lifecycleSurvivors.begin(),
            lifecycleSurvivors.end(),
            [](const Threat& threat) {
                return threat.expired;
            }),
        lifecycleSurvivors.end());
    ExpectTrue(
        "retained missing and delete lifecycle preserves logical lane",
        lifecycleSurvivors.size() == 1 &&
            lifecycleSurvivors.front().logicalCastEpisodeId ==
                closeLaneA.logicalCastEpisodeId &&
            lifecycleSurvivors.front().projectileLaneKey ==
                indexedLaneKey &&
            lifecycleSurvivors.front().projectileIndex == 0);

    Threat chainA = semanticA;
    chainA.id = 730;
    chainA.logicalCastEpisodeId = 0;
    chainA.projectileIndex = -1;
    chainA.direction = Vec2(1.0f, 0.0f);
    chainA.endPos = chainA.startPos + chainA.direction * 1000.0f;
    Threat chainB = chainA;
    chainB.id = 731;
    chainB.direction = Vec2(0.996195f, 0.087156f);
    chainB.endPos = chainB.startPos + chainB.direction * 1000.0f;
    Threat chainC = chainA;
    chainC.id = 732;
    chainC.direction = Vec2(0.984808f, 0.173648f);
    chainC.endPos = chainC.startPos + chainC.direction * 1000.0f;
    ExpectTrue(
        "unknown-lane transitive direction chain cannot collapse",
        SameSemanticThreatEpisode(chainA, chainB) &&
            SameSemanticThreatEpisode(chainB, chainC) &&
            !SameSemanticThreatEpisode(chainA, chainC) &&
            SemanticThreatGroupCount({chainA, chainB, chainC}) == 2 &&
            StableSemanticThreatSetFingerprint(
                {chainA, chainB, chainC}) ==
                StableSemanticThreatSetFingerprint(
                    {chainC, chainA, chainB}));

    Harness noisySingleThreat;
    noisySingleThreat.Step(
        oneThreat, 4, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 240.0f),
        MoveIssueResult::Issued);
    for (int frame = 0; frame < 10; ++frame) {
        noisySingleThreat.Step(
            oneThreat,
            4,
            true,
            (frame % 3) != 0,
            false,
            701,
            right,
            rightDirection,
            Vec2(
                100.0f + static_cast<float>(frame * 11),
                -220.0f - static_cast<float>(frame * 7)),
            MoveIssueResult::Issued);
    }
    ExpectTrue(
        "one Veigar Q keeps exactly one branch across ten noisy replans",
        noisySingleThreat.commitment.active &&
            noisySingleThreat.commitment.stabilityBranchKey == left &&
            noisySingleThreat.commits == 1 &&
            noisySingleThreat.switches == 0);

    Harness driftingTarget;
    driftingTarget.Step(
        oneThreat, 8, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const Vec2 firstCommittedTarget = driftingTarget.committedTarget;
    bool keptEveryProposal = true;
    for (int frame = 0; frame < 10; ++frame) {
        const float drift = 0.02f +
            static_cast<float>(frame) * 0.012f;
        const CommittedRouteDecision decision =
            driftingTarget.Step(
                oneThreat,
                8,
                true,
                true,
                false,
                701,
                left,
                Vec2(drift, 1.0f - drift * 0.25f),
                Vec2(
                    118.0f + static_cast<float>(frame * 13),
                    310.0f + static_cast<float>(frame * 17)),
                MoveIssueResult::Issued);
        keptEveryProposal =
            keptEveryProposal &&
            decision.action ==
                CommittedRouteAction::KeepCommitted &&
            driftingTarget.committedTarget.Distance(
                firstCommittedTarget) < 0.001f;
    }
    ExpectTrue(
        "ten same-branch moving proposals retain exact locked target",
        keptEveryProposal &&
            driftingTarget.commits == 1 &&
            driftingTarget.commitment.stabilityBranchKey == left &&
            driftingTarget.switches == 0);

    Harness fallbackFlicker;
    fallbackFlicker.Step(
        oneThreat, 81, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 225.0f),
        MoveIssueResult::Issued);
    const Vec2 flickerTarget = fallbackFlicker.committedTarget;
    bool flickerStayedExact = true;
    for (int frame = 0; frame < 10; ++frame) {
        const CommittedRouteDecision decision =
            fallbackFlicker.Step(
                oneThreat,
                81,
                true,
                true,
                false,
                701,
                left,
                Vec2(
                    frame % 2 == 0 ? 0.03f : -0.03f,
                    1.0f),
                Vec2(
                    90.0f + static_cast<float>(frame * 4),
                    260.0f + static_cast<float>(frame * 9)),
                MoveIssueResult::Issued);
        flickerStayedExact =
            flickerStayedExact &&
            decision.action ==
                CommittedRouteAction::KeepCommitted &&
            fallbackFlicker.committedTarget.Distance(
                flickerTarget) < 0.001f;
    }
    ExpectTrue(
        "strict fallback flicker does not change exact target",
        flickerStayedExact &&
            fallbackFlicker.commits == 1);

    Harness unsafeSameBranch;
    unsafeSameBranch.Step(
        oneThreat, 82, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 230.0f),
        MoveIssueResult::Issued);
    const CommittedRouteDecision unsafeReplacement =
        unsafeSameBranch.Step(
            oneThreat, 82, true, false, false,
            701, left, Vec2(0.08f, 0.996f),
            Vec2(118.0f, 330.0f),
            MoveIssueResult::Issued);
    ExpectTrue(
        "unsafe current route allows valid same-branch replacement",
        unsafeReplacement.action ==
                CommittedRouteAction::ProposeSameBranch &&
            unsafeSameBranch.commits == 2 &&
            unsafeSameBranch.switches == 0 &&
            unsafeSameBranch.committedTarget.Distance(
                Vec2(118.0f, 330.0f)) < 0.001f);

    Harness addedSafeThreat;
    addedSafeThreat.Step(
        oneThreat, 3, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    addedSafeThreat.Step(
        twoThreats, 3, true, true, false,
        702, right, rightDirection, Vec2(100.0f, -260.0f),
        MoveIssueResult::Issued);
    ExpectTrue(
        "safe second threat adopts fingerprint without switching",
        addedSafeThreat.commitment.threatSetFingerprint == twoThreats &&
            addedSafeThreat.commitment.stabilityBranchKey == left &&
            addedSafeThreat.switches == 0);

    Harness addedBlockingThreat;
    addedBlockingThreat.Step(
        oneThreat, 5, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    addedBlockingThreat.Step(
        twoThreats, 5, true, false, false,
        702, right, rightDirection, Vec2(100.0f, -260.0f),
        MoveIssueResult::Issued);
    for (int frame = 0; frame < 10; ++frame) {
        addedBlockingThreat.Step(
            twoThreats, 5, true, (frame % 2) == 0, false,
            701, left, leftDirection,
            Vec2(120.0f + frame, 250.0f + frame),
            MoveIssueResult::Issued);
    }
    ExpectTrue(
        "blocking second threat permits one switch then locks two-ID set",
        addedBlockingThreat.commitment.threatSetFingerprint == twoThreats &&
            addedBlockingThreat.commitment.stabilityBranchKey == right &&
            addedBlockingThreat.switches == 1);

    addedBlockingThreat.Step(
        oneThreat, 5, true, true, false,
        701, left, leftDirection, Vec2(140.0f, 280.0f),
        MoveIssueResult::Issued);
    ExpectTrue(
        "threat removal keeps valid committed direction",
        addedBlockingThreat.commitment.threatSetFingerprint == oneThreat &&
            addedBlockingThreat.commitment.stabilityBranchKey == right &&
            addedBlockingThreat.switches == 1);

    Harness allowedInvalidations;
    allowedInvalidations.Step(
        oneThreat, 9, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    allowedInvalidations.Step(
        oneThreat, 9, false, false, false,
        701, right, rightDirection, Vec2(100.0f, -220.0f),
        MoveIssueResult::Issued);
    allowedInvalidations.Step(
        oneThreat, 9, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 260.0f),
        MoveIssueResult::Issued);
    allowedInvalidations.Step(
        oneThreat, 10, true, true, false,
        701, right, rightDirection, Vec2(100.0f, -280.0f),
        MoveIssueResult::Issued);
    ExpectTrue(
        "wall hard failure and manual epoch each allow a switch",
        allowedInvalidations.switches == 3 &&
            allowedInvalidations.commitment.manualEpoch == 10 &&
            allowedInvalidations.commitment.stabilityBranchKey == right);

    Harness reachedInDanger;
    reachedInDanger.Step(
        oneThreat, 11, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const Vec2 reachedDirectionBefore =
        reachedInDanger.commitment.normalizedDirection;
    const CommittedRouteDecision reachedExtension =
        reachedInDanger.Step(
        oneThreat, 11, true, true, true,
        701, left, Vec2(0.04f, 0.999f), Vec2(112.0f, 390.0f),
        MoveIssueResult::Issued, false, true);
    ExpectTrue(
        "reached exact-safe target extends stored direction only",
        reachedExtension.action ==
                CommittedRouteAction::ProposeSameDirectionExtension &&
            reachedInDanger.commits == 2 &&
            reachedInDanger.switches == 0 &&
            reachedInDanger.commitment.stabilityBranchKey == left &&
            reachedInDanger.commitment.normalizedDirection.Distance(
                reachedDirectionBefore) < 0.001f &&
            reachedInDanger.committedTarget.Distance(
                Vec2(112.0f, 390.0f)) < 0.001f);

    Harness syntheticReached;
    syntheticReached.Step(
        semanticAOnly, 12, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const Vec2 syntheticHero(100.0f, 218.0f);
    const Vec2 syntheticTarget =
        BuildCommittedRouteExtensionTarget(
            syntheticHero,
            syntheticReached.commitment,
            65.0f,
            kDefaultEndpointMargin,
            35.0f,
            760.0f);
    const CommittedRouteDecision syntheticDecision =
        syntheticReached.Step(
            semanticAOnly,
            12,
            true,
            true,
            true,
            701,
            left,
            syntheticTarget - syntheticHero,
            syntheticTarget,
            MoveIssueResult::Issued,
            false,
            true);
    ExpectTrue(
        "missing generated branch uses accepted same-direction extension",
        syntheticDecision.action ==
                CommittedRouteAction::ProposeSameDirectionExtension &&
            syntheticReached.commits == 2 &&
            syntheticReached.switches == 0 &&
            syntheticReached.committedTarget.Distance(
                syntheticTarget) < 0.001f);

    Harness noExitSyntheticReached;
    noExitSyntheticReached.Step(
        semanticAOnly, 121, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const CommittedRouteDecision rejectedNoExitExtension =
        noExitSyntheticReached.Step(
            semanticAOnly,
            121,
            true,
            true,
            true,
            701,
            left,
            syntheticTarget - syntheticHero,
            syntheticTarget,
            MoveIssueResult::Issued,
            false,
            true,
            true,
            true,
            false);
    ExpectTrue(
        "reached longitudinal no-exit extension releases commitment",
        rejectedNoExitExtension.action ==
                CommittedRouteAction::ReleaseReachedBranch &&
            rejectedNoExitExtension.continuePlannerPipeline &&
            !noExitSyntheticReached.commitment.active &&
            noExitSyntheticReached.commits == 1);

    Harness wallBlockedExtension;
    wallBlockedExtension.Step(
        semanticAOnly, 13, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    const CommittedRouteDecision releasedReached =
        wallBlockedExtension.Step(
            semanticAOnly,
            13,
            true,
            true,
            true,
            701,
            left,
            leftDirection,
            {},
            MoveIssueResult::Issued,
            false,
            false,
            true);
    wallBlockedExtension.Step(
        semanticAOnly,
        13,
        true,
        true,
        false,
        701,
        right,
        rightDirection,
        Vec2(100.0f, -280.0f),
        MoveIssueResult::Issued);
    for (int frame = 0; frame < 10; ++frame) {
        wallBlockedExtension.Step(
            semanticAOnly,
            13,
            true,
            true,
            false,
            701,
            left,
            leftDirection,
            Vec2(100.0f, 280.0f),
            MoveIssueResult::Issued);
    }
    ExpectTrue(
        "wall-invalid extension releases branch and switches exactly once",
        releasedReached.action ==
                CommittedRouteAction::ReleaseReachedBranch &&
            releasedReached.continuePlannerPipeline &&
            wallBlockedExtension.commits == 2 &&
            wallBlockedExtension.switches == 1 &&
            wallBlockedExtension.commitment.stabilityBranchKey ==
                right);

    const ReachedCommitmentEscalation spellEscalation =
        DecideReachedCommitmentEscalation(false, true, true);
    const ReachedCommitmentEscalation noPlanEscalation =
        DecideReachedCommitmentEscalation(false, true, false);
    ExpectTrue(
        "no alternate reaches spell then normal no-plan safety",
        spellEscalation ==
                ReachedCommitmentEscalation::TryEvadeSpell &&
            noPlanEscalation ==
                ReachedCommitmentEscalation::NoPlanSafety &&
            DecideNoPlanHoldAction(
                true,
                StopIssueResult::Issued) ==
                NoPlanHoldAction::Hold);

    Harness duplicateChurn;
    duplicateChurn.Step(
        semanticAOnly, 14, false, false, false,
        701, left, leftDirection, Vec2(100.0f, 220.0f),
        MoveIssueResult::Issued);
    for (int frame = 0; frame < 10; ++frame) {
        const std::uint64_t duplicateFrame =
            (frame % 3) == 1
            ? StableSemanticThreatSetFingerprint(
                {semanticA, semanticB})
            : StableSemanticThreatSetFingerprint({semanticA});
        duplicateChurn.Step(
            duplicateFrame, 14, true, true, false,
            701, right, rightDirection, Vec2(100.0f, -240.0f),
            MoveIssueResult::Issued);
    }
    duplicateChurn.Step(
        StableSemanticThreatSetFingerprint(
            {semanticA, incompatibleGeometry}),
        14,
        true,
        false,
        false,
        704,
        right,
        rightDirection,
        Vec2(100.0f, -280.0f),
        MoveIssueResult::Issued);
    ExpectTrue(
        "duplicate churn locks one branch and real blocker switches once",
        duplicateChurn.switches == 1 &&
            duplicateChurn.commits == 2 &&
            duplicateChurn.commitment.stabilityBranchKey == right);

    reachedInDanger.Step(
        StableThreatSetFingerprint({}),
        11,
        true,
        true,
        false,
        -1,
        StabilityBranch::Unknown,
        {},
        {},
        MoveIssueResult::AlreadyFollowing,
        true);
    ExpectTrue(
        "empty threat set clears committed route identity",
        !reachedInDanger.commitment.active);
}

void TestNoPlanStopActions() {
    const NoPlanHoldAction failedFrame =
        DecideNoPlanHoldAction(true, StopIssueResult::Failed);
    const NoPlanHoldAction throttledFrame =
        DecideNoPlanHoldAction(true, StopIssueResult::Throttled);
    const NoPlanHoldAction issuedFrame =
        DecideNoPlanHoldAction(true, StopIssueResult::Issued);
    const NoPlanHoldAction settledFrame =
        DecideNoPlanHoldAction(false, StopIssueResult::Failed);

    ExpectTrue("failed stop retains control without latching hold",
               failedFrame == NoPlanHoldAction::RetryStop);
    ExpectTrue("throttled stop retains control for later retry",
               throttledFrame == NoPlanHoldAction::RetryStop);
    ExpectTrue("issued stop enters no-plan hold",
               issuedFrame == NoPlanHoldAction::Hold);
    ExpectTrue("already stopped player remains in no-plan hold",
               settledFrame == NoPlanHoldAction::Hold);
    ExpectTrue("failed-throttled-issued sequence never oscillates to release",
               failedFrame != NoPlanHoldAction::Hold &&
                   throttledFrame != NoPlanHoldAction::Hold &&
                   issuedFrame == NoPlanHoldAction::Hold &&
                   settledFrame == NoPlanHoldAction::Hold);
}

void TestFirstActionableAcquisitionPolicy() {
    ActionableAcquisitionInput firstInput;
    firstInput.controllerOwnsMovement = false;
    firstInput.exactDanger = true;
    firstInput.pathAcquisitionDanger = true;
    firstInput.directDanger = true;
    firstInput.unsafeThreatPath = true;
    firstInput.verifiedSuppressingHold = false;
    firstInput.threatSerial = 41;
    firstInput.handledAcquisitionSerial = 40;
    const FirstActionableAcquisitionPolicy first =
        DecideFirstActionableAcquisition(firstInput);
    const AcquisitionActionSequence firstActions =
        DecideAcquisitionActionSequence(first, true);
    constexpr bool nearWallWouldHold = true;
    constexpr int windupRemainingMs = 300;
    constexpr int updateTick = 1020;
    constexpr int recentMoveAttemptTick = 1000;
    const bool comfortGatePassed =
        !nearWallWouldHold || first.skipComfortHold;
    const bool windupGatePassed =
        windupRemainingMs <= 0 ||
        first.skipWindupPreservation;
    const MoveCadenceAction firstMoveCadence =
        DecideMoveCadence(
            false,
            false,
            true,
            updateTick,
            recentMoveAttemptTick,
            recentMoveAttemptTick,
            first.moveMinimumIntervalMs,
            260);
    bool everyActionabilitySourceAcquires = true;
    for (int source = 0; source < 4; ++source) {
        ActionableAcquisitionInput sourceInput = firstInput;
        sourceInput.exactDanger = source == 0;
        sourceInput.pathAcquisitionDanger = source == 1;
        sourceInput.directDanger = source == 2;
        sourceInput.unsafeThreatPath = source == 3;
        everyActionabilitySourceAcquires =
            everyActionabilitySourceAcquires &&
            DecideFirstActionableAcquisition(sourceInput)
                .firstActionableAcquisition;
    }

    ExpectTrue(
        "ACQ-01 actionable fresh serial forces plan then move in one update",
        first.firstActionableAcquisition &&
            everyActionabilitySourceAcquires &&
            first.forceReplan &&
            first.skipComfortHold &&
            first.skipWindupPreservation &&
            first.moveMinimumIntervalMs == 20 &&
            comfortGatePassed &&
            windupGatePassed &&
            firstMoveCadence == MoveCadenceAction::Issue &&
            firstActions.count == 2 &&
            firstActions.actions[0] == AcquisitionFrameAction::Plan &&
            firstActions.actions[1] == AcquisitionFrameAction::Move);
    ExpectTrue(
        "ACQ-02 first acquisition records only after frame processing",
        first.recordSerialAfterProcessing);

    ActionableAcquisitionInput futureInput = firstInput;
    futureInput.exactDanger = false;
    futureInput.pathAcquisitionDanger = false;
    futureInput.directDanger = false;
    futureInput.unsafeThreatPath = false;
    futureInput.threatSerial = 42;
    const FirstActionableAcquisitionPolicy future =
        DecideFirstActionableAcquisition(futureInput);
    ExpectTrue(
        "ACQ-03 revision-only future threat preserves comfort waits",
        !future.firstActionableAcquisition &&
            !future.forceReplan &&
            !future.skipComfortHold &&
            !future.skipWindupPreservation &&
            !future.recordSerialAfterProcessing);

    ActionableAcquisitionInput protectedInput = firstInput;
    protectedInput.verifiedSuppressingHold = true;
    protectedInput.threatSerial = 43;
    const FirstActionableAcquisitionPolicy protectedDecision =
        DecideFirstActionableAcquisition(protectedInput);
    const AcquisitionActionSequence protectedActions =
        DecideAcquisitionActionSequence(protectedDecision, true);
    ExpectTrue(
        "ACQ-04 verified invulnerability suppresses first plan and move",
        protectedDecision.firstActionableAcquisition &&
            protectedDecision.suppressForVerifiedHold &&
            !protectedDecision.forceReplan &&
            protectedActions.count == 1 &&
            protectedActions.actions[0] ==
                AcquisitionFrameAction::Suppress);

    const AcquisitionActionSequence noPlanActions =
        DecideAcquisitionActionSequence(first, false);
    ExpectTrue(
        "ACQ-05 first acquisition no-plan stops and retries next update",
        first.stopImmediatelyWithoutPlan &&
            first.noPlanRetryDelayMs == 0 &&
            noPlanActions.count == 2 &&
            noPlanActions.actions[0] == AcquisitionFrameAction::Plan &&
            noPlanActions.actions[1] == AcquisitionFrameAction::Stop);

    ActionableAcquisitionInput sameSerialInput = firstInput;
    sameSerialInput.handledAcquisitionSerial =
        sameSerialInput.threatSerial;
    const FirstActionableAcquisitionPolicy sameSerial =
        DecideFirstActionableAcquisition(sameSerialInput);
    ExpectTrue(
        "ACQ-06 same serial later may use normal comfort policies",
        !sameSerial.firstActionableAcquisition &&
            !sameSerial.skipComfortHold &&
            !sameSerial.skipWindupPreservation);

    ActionableAcquisitionInput newSerialInput = sameSerialInput;
    ++newSerialInput.threatSerial;
    const FirstActionableAcquisitionPolicy newSerial =
        DecideFirstActionableAcquisition(newSerialInput);
    ExpectTrue(
        "ACQ-07 a new actionable serial creates a new first acquisition",
        newSerial.firstActionableAcquisition &&
            newSerial.forceReplan);

    const MoveFailureClassification coreThrottle =
        AdaptCoreMoveIssueResult(
            CoreControl::OrderIssueResult::Throttled,
            0);
    const TargetCommitDecision throttleCommit =
        DecideTargetCommit(coreThrottle.result, false);
    ExpectTrue(
        "ACQ-08 Core throttle keeps target and retries at emergency cadence",
        coreThrottle.result == MoveIssueResult::Throttled &&
            coreThrottle.consecutiveFailures == 0 &&
            throttleCommit.retryProposed &&
            !throttleCommit.commitProposed &&
            DecideMoveCadence(
                false,
                false,
                false,
                1020,
                0,
                1000,
                first.moveMinimumIntervalMs,
                260) == MoveCadenceAction::Issue);

    const MoveIssueResult retryResults[] = {
        MoveIssueResult::Throttled,
        MoveIssueResult::RetryableFailure,
    };
    bool twoFrameRetryPersistence = true;
    for (const MoveIssueResult retryResult : retryResults) {
        bool pendingProposal = true;
        const TargetCommitDecision firstFrame =
            DecideTargetCommit(retryResult, true);
        if (ShouldClearPendingTargetForExactCommitment(
                true,
                firstFrame.retryProposed)) {
            pendingProposal = false;
        }
        const TargetCommitDecision secondFrame =
            pendingProposal
            ? DecideTargetCommit(
                MoveIssueResult::Issued,
                true)
            : TargetCommitDecision{};
        twoFrameRetryPersistence =
            twoFrameRetryPersistence &&
            firstFrame.retryProposed &&
            !firstFrame.commitProposed &&
            pendingProposal &&
            secondFrame.commitProposed;
    }
    ExpectTrue(
        "two-frame fallback shortening retries then commits on acceptance",
        twoFrameRetryPersistence);
    ExpectTrue(
        "exact retention clears non-retry pending proposals only",
        ShouldClearPendingTargetForExactCommitment(
            true,
            false) &&
            !ShouldClearPendingTargetForExactCommitment(
                true,
                true));
    LockedRouteValidationInput unsafeRefreshInput;
    unsafeRefreshInput.hasLock = true;
    unsafeRefreshInput.evaluationValid = true;
    unsafeRefreshInput.walkable = true;
    unsafeRefreshInput.startsInThreat = true;
    unsafeRefreshInput.exitedStartEnvelope = false;
    const LockedRouteValidation unsafeRefresh =
        ClassifyLockedRoute(unsafeRefreshInput);
    ExpectTrue(
        "throttled pending proposal clears when refresh becomes unsafe",
        !ShouldRetainRefreshedPendingTarget(
            true,
            true,
            true,
            unsafeRefresh.safety));
    LockedRouteValidationInput usableRefreshInput =
        unsafeRefreshInput;
    usableRefreshInput.exitedStartEnvelope = true;
    const LockedRouteValidation usableRefresh =
        ClassifyLockedRoute(usableRefreshInput);
    ExpectTrue(
        "throttled pending proposal persists while refresh stays usable",
        ShouldRetainRefreshedPendingTarget(
            true,
            true,
            true,
            usableRefresh.safety));
}

void TestNoPlanRetryPolicy() {
    const int sixHundredMsRetry = NoPlanRetryDelayMs(600.0f, 25.0f);
    const int thirtyMsRetry = NoPlanRetryDelayMs(30.0f, 25.0f);
    const float largestNonSentinelFinite = std::nextafter(
        std::numeric_limits<float>::max(),
        0.0f);
    std::printf(
        "TRACE no-plan retry: collision=600ms margin=25ms delay=%dms\n",
        sixHundredMsRetry);
    std::printf(
        "TRACE no-plan retry: collision=30ms margin=25ms delay=%dms\n",
        thirtyMsRetry);

    ExpectTrue("600ms collision uses bounded 120ms retry",
               sixHundredMsRetry == 120);
    ExpectTrue("30ms imminent collision retries next frame",
               thirtyMsRetry == 0);
    ExpectTrue("finite retry occurs strictly before collision margin",
               NoPlanRetryDelayMs(100.0f, 25.0f) < 75);
    ExpectTrue("finite non-imminent retry remains at least 20ms",
               NoPlanRetryDelayMs(100.0f, 25.0f) >= 20);
    ExpectTrue("largest non-sentinel finite collision clamps before cast",
               NoPlanRetryDelayMs(
                   largestNonSentinelFinite,
                   25.0f) == 120);
    ExpectTrue("INT_MAX-adjacent collision clamps before cast",
               NoPlanRetryDelayMs(
                   static_cast<float>(INT_MAX),
                   25.0f) == 120);
    ExpectTrue("NaN collision retries next frame",
               NoPlanRetryDelayMs(
                   std::numeric_limits<float>::quiet_NaN(),
                   25.0f) == 0);
    ExpectTrue("positive infinity collision retries next frame",
               NoPlanRetryDelayMs(
                   std::numeric_limits<float>::infinity(),
                   25.0f) == 0);
    ExpectTrue("negative infinity collision retries next frame",
               NoPlanRetryDelayMs(
                   -std::numeric_limits<float>::infinity(),
                   25.0f) == 0);
    ExpectTrue("negative collision retries next frame",
               NoPlanRetryDelayMs(-1.0f, 25.0f) == 0);
    ExpectTrue("zero collision retries next frame",
               NoPlanRetryDelayMs(0.0f, 25.0f) == 0);
    ExpectTrue("no predicted collision uses modest retry",
               NoPlanRetryDelayMs(FLT_MAX, 25.0f) == 120);

    constexpr int planTick = 1000;
    constexpr int threatSerial = 17;
    constexpr std::uint64_t manualGeneration = 4;
    const NoPlanRetrySchedule zeroCollision = ScheduleNoPlanRetry(
        planTick, 0.0f, 25.0f, threatSerial, manualGeneration);
    const NoPlanRetrySchedule thirtyCollision = ScheduleNoPlanRetry(
        planTick, 30.0f, 25.0f, threatSerial, manualGeneration);
    const NoPlanRetrySchedule sixHundredCollision = ScheduleNoPlanRetry(
        planTick, 600.0f, 25.0f, threatSerial, manualGeneration);
    const NoPlanRetrySchedule noCollision = ScheduleNoPlanRetry(
        planTick, FLT_MAX, 25.0f, threatSerial, manualGeneration);

    ExpectTrue("zero collision retries on the next controller frame",
               ShouldReplanRoute(
                   false, planTick, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   zeroCollision));
    ExpectTrue("30ms collision retries on the next controller frame",
               ShouldReplanRoute(
                   false, planTick, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   thirtyCollision));
    ExpectTrue("invalid lock does not bypass 600ms no-plan cadence",
               !ShouldReplanRoute(
                   false, planTick + 119, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   sixHundredCollision));
    ExpectTrue("600ms no-plan retry fires at bounded deadline",
               ShouldReplanRoute(
                   false, planTick + 120, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   sixHundredCollision));
    ExpectTrue("FLT_MAX no-plan hold does not plan every frame",
               !ShouldReplanRoute(
                   false, planTick + 1, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   noCollision));
    ExpectTrue("FLT_MAX no-plan retry remains bounded",
               ShouldReplanRoute(
                   false, planTick + 120, planTick, 70,
                   threatSerial, threatSerial, manualGeneration,
                   noCollision));
    ExpectTrue("new threat serial breaks no-plan cadence immediately",
               ShouldReplanRoute(
                   false, planTick + 1, planTick, 70,
                   threatSerial + 1, threatSerial, manualGeneration,
                   sixHundredCollision));
    ExpectTrue("manual generation breaks no-plan cadence immediately",
               ShouldReplanRoute(
                   false, planTick + 1, planTick, 70,
                   threatSerial, threatSerial, manualGeneration + 1,
                   sixHundredCollision));

    RequestGenerationState generations;
    generations.moveRequestGeneration = 40;
    generations.manualRequestGeneration = manualGeneration;
    for (int frame = 0; frame < 32; ++frame) {
        generations = AdvanceRequestGenerations(
            generations,
            MoveIntentSource::Orbwalker);
        ExpectTrue("routine orb request does not break no-plan cadence",
                   !ShouldReplanRoute(
                       false, planTick + 1 + frame, planTick, 70,
                       threatSerial, threatSerial,
                       generations.manualRequestGeneration,
                       sixHundredCollision));
    }
    ExpectTrue("routine orb requests still advance generic ownership",
               generations.moveRequestGeneration == 72 &&
                   generations.manualRequestGeneration ==
                       manualGeneration);

    const RequestGenerationState observedGeneration =
        AdvanceRequestGenerations(
            generations,
            MoveIntentSource::ObservedPath);
    const RequestGenerationState controllerGeneration =
        AdvanceRequestGenerations(
            observedGeneration,
            MoveIntentSource::Controller);
    ExpectTrue("observed and controller requests leave generations unchanged",
               controllerGeneration.moveRequestGeneration ==
                       generations.moveRequestGeneration &&
                   controllerGeneration.manualRequestGeneration ==
                       generations.manualRequestGeneration);

    generations = AdvanceRequestGenerations(
        generations,
        MoveIntentSource::Manual);
    ExpectTrue("unsafe WndProc manual request breaks cadence immediately",
               ShouldReplanRoute(
                   false, planTick + 1, planTick, 70,
                   threatSerial, threatSerial,
                   generations.manualRequestGeneration,
                   sixHundredCollision));

    RequestGenerationState wrappingGeneration;
    wrappingGeneration.moveRequestGeneration =
        std::numeric_limits<std::uint64_t>::max();
    wrappingGeneration.manualRequestGeneration =
        std::numeric_limits<std::uint64_t>::max();
    const NoPlanRetrySchedule wrappingSchedule = ScheduleNoPlanRetry(
        planTick,
        600.0f,
        25.0f,
        threatSerial,
        wrappingGeneration.manualRequestGeneration);
    wrappingGeneration = AdvanceRequestGenerations(
        wrappingGeneration,
        MoveIntentSource::Orbwalker);
    ExpectTrue("generic generation wrap does not break no-plan cadence",
               wrappingGeneration.moveRequestGeneration == 0 &&
                   wrappingGeneration.manualRequestGeneration ==
                       std::numeric_limits<std::uint64_t>::max() &&
                   !ShouldReplanRoute(
                       false, planTick + 1, planTick, 70,
                       threatSerial, threatSerial,
                       wrappingGeneration.manualRequestGeneration,
                       wrappingSchedule));
    wrappingGeneration = AdvanceRequestGenerations(
        wrappingGeneration,
        MoveIntentSource::Manual);
    ExpectTrue("manual generation wrap safely breaks no-plan cadence",
               wrappingGeneration.manualRequestGeneration == 0 &&
                   ShouldReplanRoute(
                       false, planTick + 1, planTick, 70,
                       threatSerial, threatSerial,
                       wrappingGeneration.manualRequestGeneration,
                       wrappingSchedule));
}

void TestStopThrottleReleasePolicy() {
    ExpectTrue("immediate release preserves last stop timestamp",
               StopThrottleTickAfterReset(
                   StopThrottleResetMode::ImmediateRelease,
                   1050,
                   1000,
                   180) == 1000);
    ExpectTrue("full reset retains stop timestamp during safe grace",
               StopThrottleTickAfterReset(
                   StopThrottleResetMode::FullReset,
                   1179,
                   1000,
                   180) == 1000);
    ExpectTrue("full reset may clear stop timestamp after safe grace",
               StopThrottleTickAfterReset(
                   StopThrottleResetMode::FullReset,
                   1180,
                   1000,
                   180) == 0);
}

void TestMoveRefreshCadencePolicy() {
    ExpectTrue("matching progressing path follows before refresh expiry",
               DecideMoveCadence(
                   true, false, false, 1259, 1000, 1000, 75, 260) ==
                   MoveCadenceAction::AlreadyFollowing);
    ExpectTrue("matching progressing path refreshes at exact boundary",
               DecideMoveCadence(
                   true, false, false, 1260, 1000, 1000, 75, 260) ==
                   MoveCadenceAction::Issue);
    ExpectTrue("zero refresh immediately issues matching path",
               DecideMoveCadence(
                   true, false, false, 1100, 1000, 0, 75, 0) ==
                   MoveCadenceAction::Issue);
    ExpectTrue("refresh below minimum defers to minimum interval",
               DecideMoveCadence(
                   true, false, false, 1030, 1000, 1000, 75, 20) ==
                   MoveCadenceAction::Throttled);
    ExpectTrue("target change bypasses matching-path refresh",
               DecideMoveCadence(
                   true, false, true, 1100, 1050, 1000, 75, 260) ==
                   MoveCadenceAction::Issue);
    ExpectTrue("stuck path bypasses already-following refresh",
               DecideMoveCadence(
                   true, true, false, 1100, 1000, 1000, 75, 260) ==
                   MoveCadenceAction::Issue);
    ExpectTrue("minimum interval precedes stuck-path reissue",
               DecideMoveCadence(
                   true, true, false, 1050, 1000, 1020, 75, 260) ==
                   MoveCadenceAction::Throttled);
    ExpectTrue("refresh expiry still honors minimum move interval",
               DecideMoveCadence(
                   true, false, false, 1260, 1000, 1240, 75, 260) ==
                   MoveCadenceAction::Throttled);
    ExpectTrue("unissued matching path receives initial command",
               DecideMoveCadence(
                   true, false, false, 1100, 0, 0, 75, 260) ==
                   MoveCadenceAction::Issue);
    Vec2 committedTarget(100.0f, 100.0f);
    const Vec2 proposedTarget(200.0f, 100.0f);
    const MoveCadenceAction stalePathThrottle = DecideMoveCadence(
        true, false, true, 1050, 1000, 1020, 75, 260);
    ExpectTrue("stale PathEnd plus throttled change keeps command L",
               stalePathThrottle == MoveCadenceAction::Throttled &&
                   committedTarget.Distance(
                       Vec2(100.0f, 100.0f)) < 0.001f);
    const MoveCadenceAction allowedChange = DecideMoveCadence(
        true, false, true, 1100, 1000, 1020, 75, 260);
    ExpectTrue("allowed issue attempt still awaits command acceptance",
               allowedChange == MoveCadenceAction::Issue &&
                   committedTarget.Distance(
                       Vec2(100.0f, 100.0f)) < 0.001f);

    const TargetCommitDecision throttledProposal =
        DecideTargetCommit(MoveIssueResult::Throttled, true);
    if (throttledProposal.commitProposed)
        committedTarget = proposedTarget;
    ExpectTrue("L command then throttled R proposal keeps committed L",
               throttledProposal.retainCommitted &&
                   throttledProposal.retryProposed &&
                   !throttledProposal.commitProposed &&
                   committedTarget.Distance(
                       Vec2(100.0f, 100.0f)) < 0.001f);
    const TargetCommitDecision retryableProposal =
        DecideTargetCommit(MoveIssueResult::RetryableFailure, true);
    ExpectTrue("retryable R proposal also keeps L without direction loss",
               retryableProposal.retainCommitted &&
                   retryableProposal.retryProposed &&
                   !retryableProposal.commitProposed);
    const TargetCommitDecision retryableWithoutOld =
        DecideTargetCommit(MoveIssueResult::RetryableFailure, false);
    ExpectTrue("retry remains pending but cannot retain invalid old route",
               !retryableWithoutOld.retainCommitted &&
                   retryableWithoutOld.retryProposed &&
                   !retryableWithoutOld.commitProposed);
    const TargetCommitDecision acceptedProposal =
        DecideTargetCommit(MoveIssueResult::Issued, true);
    if (acceptedProposal.commitProposed)
        committedTarget = proposedTarget;
    ExpectTrue("next accepted R proposal commits exactly once",
               acceptedProposal.commitProposed &&
                   !acceptedProposal.retryProposed &&
                   committedTarget.Distance(proposedTarget) < 0.001f);
    const TargetCommitDecision alreadyFollowingProposal =
        DecideTargetCommit(MoveIssueResult::AlreadyFollowing, true);
    ExpectTrue("already-following R also accepts target commitment",
               alreadyFollowingProposal.commitProposed &&
                   !alreadyFollowingProposal.retryProposed);
    const TargetCommitDecision hardFailureWithoutOld =
        DecideTargetCommit(MoveIssueResult::HardFailure, false);
    ExpectTrue("hard failure may clear when no valid old route remains",
               !hardFailureWithoutOld.retainCommitted &&
                   !hardFailureWithoutOld.retryProposed &&
                   !hardFailureWithoutOld.commitProposed);
}

void TestUnavoidableActionMatrix() {
    const auto coverage = [](
        int collisions,
        int endpointDanger,
        int pathDanger,
        int maxDanger,
        float exposure,
        float firstCollision) {
        return ThreatCoverage{
            collisions,
            endpointDanger,
            pathDanger,
            maxDanger,
            exposure,
            firstCollision,
        };
    };
    const auto decide = [](
        const ThreatCoverage& hold,
        const ThreatCoverage& native,
        bool nativeAvailable,
        const ThreatCoverage& candidate,
        bool candidateAvailable,
        bool candidateWalkable,
        bool progress) {
        UnavoidableDecisionInput input;
        input.holdCoverage = hold;
        input.nativeCoverage = native;
        input.nativeAvailable = nativeAvailable;
        input.candidateCoverage = candidate;
        input.candidateAvailable = candidateAvailable;
        input.candidateValid = candidateAvailable;
        input.candidateWalkable = candidateWalkable;
        input.candidateMakesProgress = progress;
        return DecideUnavoidableAction(input);
    };

    const ThreatCoverage occupied =
        coverage(1, 2, 2, 2, 900.0f, 0.0f);
    const ThreatCoverage shorter =
        coverage(1, 2, 2, 2, 420.0f, 0.0f);
    const ThreatCoverage equal = occupied;
    const ThreatCoverage worseEndpoint =
        coverage(1, 3, 1, 3, 100.0f, 50.0f);
    const ThreatCoverage saferNative =
        coverage(1, 1, 1, 1, 300.0f, 0.0f);

    ExpectTrue("UNAV-01 occupied circle moves equal-coverage exit progress",
               decide(
                   occupied, {}, false, equal, true, true, true).action ==
                   UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-02 occupied line moves equal-coverage exit progress",
               decide(
                   occupied, {}, false, equal, true, true, true).action ==
                   UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-03 impact before exit still moves on shorter exposure",
               decide(
                   occupied, {}, false, shorter, true, true, true).action ==
                   UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-04 all equal without progress holds",
               decide(
                   occupied, {}, false, equal, true, true, false).action ==
                   UnavoidableAction::Hold);
    ExpectTrue("UNAV-05 shorter exposure selects walking fallback",
               decide(
                   occupied, {}, false, shorter, true, true, false).action ==
                   UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-06 safer native route remains native",
               decide(
                   occupied,
                   saferNative,
                   true,
                   {},
                   false,
                   false,
                   false).action == UnavoidableAction::KeepNative);
    ExpectTrue("UNAV-07 exact native tie remains native",
               decide(
                   occupied,
                   occupied,
                   true,
                   {},
                   false,
                   false,
                   false).action == UnavoidableAction::KeepNative);
    ExpectTrue("UNAV-08 planner improvement beats native reference",
               decide(
                   occupied,
                   occupied,
                   true,
                   shorter,
                   true,
                   true,
                   false).action == UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-09 no walkable planner keeps native",
               decide(
                   occupied,
                   saferNative,
                   true,
                   shorter,
                   true,
                   false,
                   true).action == UnavoidableAction::KeepNative);

    const ThreatCoverage overlapHold =
        coverage(2, 4, 4, 3, 1200.0f, 0.0f);
    const ThreatCoverage overlapExit =
        coverage(1, 2, 2, 2, 600.0f, 0.0f);
    ExpectTrue("UNAV-10 overlapping danger chooses lower coverage action",
               decide(
                   overlapHold,
                   {},
                   false,
                   overlapExit,
                   true,
                   true,
                   false).action == UnavoidableAction::MoveFallback);
    ExpectTrue("UNAV-11 earlier-field worsening is prohibited",
               decide(
                   occupied,
                   occupied,
                   true,
                   worseEndpoint,
                   true,
                   true,
                   true).action == UnavoidableAction::KeepNative);

    UnavoidableDecisionInput stableLock;
    stableLock.holdCoverage = occupied;
    stableLock.candidateCoverage = equal;
    stableLock.candidateAvailable = true;
    stableLock.candidateValid = true;
    stableLock.candidateWalkable = true;
    stableLock.candidateMakesProgress = true;
    stableLock.fallbackLockActive = true;
    stableLock.lockCoverage = shorter;
    stableLock.lockValid = true;
    stableLock.lockWalkable = true;
    stableLock.currentManualEpoch = 12;
    stableLock.lockManualEpoch = 12;
    const UnavoidableDecision stableDecision =
        DecideUnavoidableAction(stableLock);
    ExpectTrue("UNAV-12 equal fallback keeps stable walking action",
               stableDecision.action == UnavoidableAction::MoveFallback &&
                   stableDecision.retainLockedFallback);

    UnavoidableDecisionInput manualEpoch = stableLock;
    manualEpoch.currentManualEpoch = 13;
    manualEpoch.candidateCoverage = shorter;
    const UnavoidableDecision epochDecision =
        DecideUnavoidableAction(manualEpoch);
    ExpectTrue("UNAV-13 manual epoch invalidates old fallback lock",
               epochDecision.action == UnavoidableAction::MoveFallback &&
                   !epochDecision.retainLockedFallback);

    UnavoidableDecisionInput strictThroughFinalGate = stableLock;
    strictThroughFinalGate.nativeAvailable = true;
    strictThroughFinalGate.nativeCoverage = shorter;
    strictThroughFinalGate.candidateStrictSafe = true;
    strictThroughFinalGate.candidateCoverage = worseEndpoint;
    const UnavoidableDecision strictFinalDecision =
        DecideUnavoidableAction(strictThroughFinalGate);
    ExpectTrue(
        "UNAV-14 strict candidate passes final gate over fallback and native",
        strictFinalDecision.action == UnavoidableAction::MoveFallback &&
            !strictFinalDecision.retainLockedFallback);
}

} // namespace

int main() {
    TestCoreEvadeOwnerAggregation();
    TestOtherEvadePolicy();
    TestLegacyControlRestore();
    TestAllowAttacksPolicy();
    TestActiveEvadeSpellDatabase();
    TestObservedRoutePolicy();
    TestThreatFreeActionPolicy();
    TestExternalMoveDecisionPolicy();
    TestStableRouteStrictPriority();
    TestRouteCommitmentFrames();
    TestSimpleCommittedRoutePolicyFrames();
    TestNoPlanStopActions();
    TestFirstActionableAcquisitionPolicy();
    TestNoPlanRetryPolicy();
    TestStopThrottleReleasePolicy();
    TestMoveRefreshCadencePolicy();
    TestUnavoidableActionMatrix();

    ReleaseDecisionInput unsafeWithoutPlan;
    unsafeWithoutPlan.hasUsablePlan = false;
    unsafeWithoutPlan.currentPathUnsafe = true;
    unsafeWithoutPlan.currentThreatSerial = 17;
    unsafeWithoutPlan.releaseThreatSerial = 17;
    unsafeWithoutPlan.currentMoveRequestGeneration = 4;
    unsafeWithoutPlan.releaseMoveRequestGeneration = 4;
    ExpectTrue("unsafe path without a usable plan stops before release",
               MustStopBeforeRelease(unsafeWithoutPlan));
    ExpectTrue("unsafe path without a usable plan preserves deferred intent",
               MustPreserveDeferredDestination(unsafeWithoutPlan));

    ReleaseDecisionInput sameThreatSet;
    sameThreatSet.hasUsablePlan = false;
    sameThreatSet.currentPathUnsafe = true;
    sameThreatSet.currentThreatSerial = 17;
    sameThreatSet.releaseThreatSerial = 17;
    sameThreatSet.currentMoveRequestGeneration = 4;
    sameThreatSet.releaseMoveRequestGeneration = 4;
    ExpectTrue("same threat serial and move generation keep release cooldown",
               !MustBreakReleaseCooldown(sameThreatSet));

    ReleaseDecisionInput newThreatSerial = sameThreatSet;
    newThreatSerial.currentThreatSerial = 18;
    ExpectTrue("new threat serial breaks release cooldown",
               MustBreakReleaseCooldown(newThreatSerial));

    ReleaseDecisionInput moveRequest = sameThreatSet;
    moveRequest.currentMoveRequestGeneration = 5;
    ExpectTrue("any move-request generation breaks release cooldown",
               MustBreakReleaseCooldown(moveRequest));

    ExpectTrue("already stopped movement permits release",
               CanReleaseAfterStop(false, StopIssueResult::Failed));
    ExpectTrue("issued stop permits release while movement is still observed",
               CanReleaseAfterStop(true, StopIssueResult::Issued));
    ExpectTrue("throttled stop keeps control while movement continues",
               !CanReleaseAfterStop(true, StopIssueResult::Throttled));
    ExpectTrue("failed stop keeps control while movement continues",
               !CanReleaseAfterStop(true, StopIssueResult::Failed));

    ExpectTrue("idle acquisition uses path buffer",
               ControlThreatBuffer(false, 8.0f, 48.0f) == 8.0f);
    ExpectTrue("active control uses release margin",
               ControlThreatBuffer(true, 8.0f, 48.0f) == 48.0f);

    ReleaseHysteresisInput strictEndpoint;
    strictEndpoint.controlActive = true;
    strictEndpoint.strictEndpointReached = true;
    strictEndpoint.releaseMarginDanger = true;
    ExpectTrue("strict endpoint holds inside release margin",
               DecideReleaseHysteresis(strictEndpoint) ==
                   ReleaseHysteresisAction::HoldAtStrictEndpoint);

    ReleaseHysteresisInput lowerReleaseMargin = strictEndpoint;
    lowerReleaseMargin.releaseMarginDanger = false;
    ExpectTrue("lower release margin deterministically releases",
               DecideReleaseHysteresis(lowerReleaseMargin) ==
                   ReleaseHysteresisAction::Release);

    ReleaseHysteresisInput higherReleaseMargin = strictEndpoint;
    ExpectTrue("higher release margin deterministically holds",
               DecideReleaseHysteresis(higherReleaseMargin) ==
                   ReleaseHysteresisAction::HoldAtStrictEndpoint);

    ReleaseHysteresisInput movingMissileAtEndpoint = strictEndpoint;
    movingMissileAtEndpoint.exactDanger = true;
    ExpectTrue("moving missile danger replans from held endpoint",
               DecideReleaseHysteresis(movingMissileAtEndpoint) ==
                   ReleaseHysteresisAction::Plan);

    ReleaseHysteresisInput unsafePathAtEndpoint = strictEndpoint;
    unsafePathAtEndpoint.currentPathUnsafe = true;
    ExpectTrue("unsafe path replans from held endpoint",
               DecideReleaseHysteresis(unsafePathAtEndpoint) ==
                   ReleaseHysteresisAction::Plan);

    ReleaseHysteresisInput idleOutsidePathBuffer;
    idleOutsidePathBuffer.releaseMarginDanger = true;
    ExpectTrue("idle ignores release margin for acquisition",
               DecideReleaseHysteresis(idleOutsidePathBuffer) ==
                   ReleaseHysteresisAction::Release);
    idleOutsidePathBuffer.pathAcquisitionDanger = true;
    ExpectTrue("idle acquires with path buffer",
               DecideReleaseHysteresis(idleOutsidePathBuffer) ==
                   ReleaseHysteresisAction::Plan);

    ExpectTrue("none hold replans while active",
               !HoldMaySuppressPlanning(HoldProtectionKind::None, true));
    ExpectTrue("speed buff hold replans while active",
               !HoldMaySuppressPlanning(HoldProtectionKind::SpeedBuff, true));
    ExpectTrue("shield hold replans while active",
               !HoldMaySuppressPlanning(HoldProtectionKind::Shield, true));
    ExpectTrue("displacement hold replans while active",
               !HoldMaySuppressPlanning(HoldProtectionKind::Displacement, true));
    ExpectTrue("invulnerable hold may suppress planning while active",
               HoldMaySuppressPlanning(HoldProtectionKind::Invulnerable, true));
    ExpectTrue("untargetable hold may suppress planning while active",
               HoldMaySuppressPlanning(HoldProtectionKind::Untargetable, true));
    ExpectTrue("expired invulnerable hold replans",
               !HoldMaySuppressPlanning(HoldProtectionKind::Invulnerable, false));
    ExpectTrue("expired untargetable hold replans",
               !HoldMaySuppressPlanning(HoldProtectionKind::Untargetable, false));

    ExpectTrue("observed invulnerability verifies active invulnerable hold",
               VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Invulnerable, true, true, false));
    ExpectTrue("estimated invulnerable hold without flag replans",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Invulnerable, true, false, false));
    ExpectTrue("observed untargetability verifies active untargetable hold",
               VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Untargetable, true, false, true));
    ExpectTrue("observed invulnerability also verifies untargetable hold",
               VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Untargetable, true, true, false));
    ExpectTrue("estimated untargetable hold without flags replans",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Untargetable, true, false, false));
    ExpectTrue("ended invulnerability flag stops suppression before estimate",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Invulnerable, true, false, false));
    ExpectTrue("ended untargetable flag stops suppression before estimate",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Untargetable, true, false, false));
    ExpectTrue("expired observed invulnerability cannot suppress",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Invulnerable, false, true, false));
    ExpectTrue("shield cannot suppress with both observed flags",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Shield, true, true, true));
    ExpectTrue("displacement cannot suppress with both observed flags",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::Displacement, true, true, true));
    ExpectTrue("speed buff cannot suppress with both observed flags",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::SpeedBuff, true, true, true));
    ExpectTrue("none cannot suppress with both observed flags",
               !VerifiedHoldMaySuppressPlanning(
                   HoldProtectionKind::None, true, true, true));

    ExpectTrue("expired estimated hold is cleared",
               ShouldClearEstimatedHold(
                   HoldProtectionKind::Displacement,
                   false,
                   true,
                   false,
                   false));
    ExpectTrue("pre-activation suppressing hold keeps its estimate",
               !ShouldClearEstimatedHold(
                   HoldProtectionKind::Invulnerable,
                   true,
                   false,
                   false,
                   false));
    ExpectTrue("post-activation unobserved invulnerability clears early",
               ShouldClearEstimatedHold(
                   HoldProtectionKind::Invulnerable,
                   true,
                   true,
                   false,
                   false));
    ExpectTrue("post-activation observed invulnerability remains",
               !ShouldClearEstimatedHold(
                   HoldProtectionKind::Invulnerable,
                   true,
                   true,
                   true,
                   false));
    ExpectTrue("post-activation unobserved untargetability clears early",
               ShouldClearEstimatedHold(
                   HoldProtectionKind::Untargetable,
                   true,
                   true,
                   false,
                   false));
    ExpectTrue("invulnerability observation retains untargetable hold",
               !ShouldClearEstimatedHold(
                   HoldProtectionKind::Untargetable,
                   true,
                   true,
                   true,
                   false));
    ExpectTrue("non-suppressing active estimate remains bookkeeping only",
               !ShouldClearEstimatedHold(
                   HoldProtectionKind::Shield,
                   true,
                   true,
                   false,
                   false));

    EvadeSpellData protectionData;
    protectionData.evadeType = EvadeType::Invulnerability;
    protectionData.untargetable = true;
    ExpectTrue("untargetable takes precedence over evade type",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Untargetable);
    protectionData.untargetable = false;
    ExpectTrue("invulnerability maps to invulnerable hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Invulnerable);
    protectionData.evadeType = EvadeType::Dash;
    ExpectTrue("dash maps to displacement hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Displacement);
    protectionData.evadeType = EvadeType::Blink;
    ExpectTrue("blink maps to displacement hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Displacement);
    protectionData.evadeType = EvadeType::Shield;
    ExpectTrue("shield maps to shield hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Shield);
    protectionData.evadeType = EvadeType::SpellShield;
    ExpectTrue("spell shield maps to shield hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Shield);
    protectionData.evadeType = EvadeType::WindWall;
    ExpectTrue("wind wall maps to shield hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::Shield);
    protectionData.evadeType = EvadeType::MovementSpeedBuff;
    ExpectTrue("movement speed maps to speed hold",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::SpeedBuff);
    protectionData.evadeType = static_cast<EvadeType>(999);
    ExpectTrue("unknown evade type maps to no hold protection",
               HoldProtectionFor(protectionData) ==
                   HoldProtectionKind::None);

    ExpectTrue("issued move preserves strict lock",
               !MoveResultInvalidatesLock(MoveIssueResult::Issued));
    ExpectTrue("already-following move preserves strict lock",
               !MoveResultInvalidatesLock(MoveIssueResult::AlreadyFollowing));
    ExpectTrue("throttled move preserves strict lock",
               !MoveResultInvalidatesLock(MoveIssueResult::Throttled));
    ExpectTrue("retryable move failure preserves strict lock",
               !MoveResultInvalidatesLock(
                   MoveIssueResult::RetryableFailure));
    ExpectTrue("hard move failure invalidates strict lock",
               MoveResultInvalidatesLock(MoveIssueResult::HardFailure));

    ExpectTrue("first real move failure remains retryable",
               ClassifyMoveFailure(0).result ==
                   MoveIssueResult::RetryableFailure);
    ExpectTrue("first real move failure starts streak",
               ClassifyMoveFailure(0).consecutiveFailures == 1);
    ExpectTrue("second real move failure remains retryable",
               ClassifyMoveFailure(1).result ==
                   MoveIssueResult::RetryableFailure);
    ExpectTrue("second real move failure advances streak",
               ClassifyMoveFailure(1).consecutiveFailures == 2);
    ExpectTrue("third consecutive real move failure hard-fails",
               ClassifyMoveFailure(2).result ==
                   MoveIssueResult::HardFailure);
    ExpectTrue("third real move failure records threshold",
               ClassifyMoveFailure(2).consecutiveFailures == 3);
    ExpectTrue("issued move resets real-failure streak",
               NextMoveFailureStreak(
                   MoveIssueResult::Issued, 2) == 0);
    ExpectTrue("already-following move resets real-failure streak",
               NextMoveFailureStreak(
                   MoveIssueResult::AlreadyFollowing, 2) == 0);
    ExpectTrue("throttling does not count as a real failure",
               NextMoveFailureStreak(
                   MoveIssueResult::Throttled, 2) == 2);

    const MoveFailureClassification issuedCoreResult =
        AdaptCoreMoveIssueResult(
            CoreControl::OrderIssueResult::Issued, 2);
    ExpectTrue("core issued maps to issued",
               issuedCoreResult.result == MoveIssueResult::Issued);
    ExpectTrue("core issued resets failure streak",
               issuedCoreResult.consecutiveFailures == 0);

    const MoveFailureClassification throttledCoreResult =
        AdaptCoreMoveIssueResult(
            CoreControl::OrderIssueResult::Throttled, 2);
    ExpectTrue("core throttled maps to throttled",
               throttledCoreResult.result ==
                   MoveIssueResult::Throttled);
    ExpectTrue("core throttled preserves failure streak",
               throttledCoreResult.consecutiveFailures == 2);

    const MoveFailureClassification blockedCoreResult =
        AdaptCoreMoveIssueResult(
            CoreControl::OrderIssueResult::Blocked, 0);
    ExpectTrue("core blocked maps to retryable failure",
               blockedCoreResult.result ==
                   MoveIssueResult::RetryableFailure);
    ExpectTrue("core blocked advances failure streak",
               blockedCoreResult.consecutiveFailures == 1);
    ExpectTrue("third core blocked result hard-fails",
               AdaptCoreMoveIssueResult(
                   CoreControl::OrderIssueResult::Blocked, 2).result ==
                   MoveIssueResult::HardFailure);

    const MoveFailureClassification failedCoreResult =
        AdaptCoreMoveIssueResult(
            CoreControl::OrderIssueResult::Failed, 0);
    ExpectTrue("core native failure maps to retryable failure",
               failedCoreResult.result ==
                   MoveIssueResult::RetryableFailure);
    ExpectTrue("core native failure advances failure streak",
               failedCoreResult.consecutiveFailures == 1);
    ExpectTrue("third core native failure hard-fails",
               AdaptCoreMoveIssueResult(
                   CoreControl::OrderIssueResult::Failed, 2).result ==
                   MoveIssueResult::HardFailure);

    ExpectTrue("strict-safe fallback evaluation promotes in place",
               ShouldPromoteFallbackEvaluation(true, true, true, true));
    ExpectTrue("unsafe fallback evaluation remains fallback",
               !ShouldPromoteFallbackEvaluation(true, true, true, false));
    ExpectTrue("invalid fallback evaluation cannot promote",
               !ShouldPromoteFallbackEvaluation(true, false, true, true));
    ExpectTrue("strict state is not a fallback promotion",
               !ShouldPromoteFallbackEvaluation(false, true, true, true));

    ExpectTrue("missing manual intent does not beat orbwalker",
               !ManualIntentWins(0, 41));
    ExpectTrue("newer manual intent beats orbwalker",
               ManualIntentWins(42, 41));
    ExpectTrue("equal manual intent beats orbwalker",
               ManualIntentWins(41, 41));
    ExpectTrue("newer orbwalker intent cannot beat active manual",
               ManualIntentWins(41, 42));

    MoveRouteEvaluation unsafeManualRoute;
    unsafeManualRoute.evaluated = true;
    unsafeManualRoute.valid = true;
    unsafeManualRoute.walkable = true;
    MoveRouteEvaluation safeManualRoute = unsafeManualRoute;
    safeManualRoute.strictSafe = true;
    MoveRouteEvaluation invalidManualRoute;
    invalidManualRoute.evaluated = true;
    MoveRouteEvaluation nonWalkableManualRoute;
    nonWalkableManualRoute.evaluated = true;
    nonWalkableManualRoute.valid = true;
    MoveRouteEvaluation unvalidatedManualRoute;

    ExpectTrue("active controller blocks invalid manual route",
               DecideManualRouteAction(true, invalidManualRoute) ==
                   ManualRouteAction::PreserveAndBlock);
    ExpectTrue("active controller blocks non-walkable manual route",
               DecideManualRouteAction(true, nonWalkableManualRoute) ==
                   ManualRouteAction::PreserveAndBlock);
    ExpectTrue("active controller blocks unvalidated manual route",
               DecideManualRouteAction(true, unvalidatedManualRoute) ==
                   ManualRouteAction::PreserveAndBlock);
    ExpectTrue("outside controller invalid manual preserves native handling",
               DecideManualRouteAction(false, invalidManualRoute) ==
                   ManualRouteAction::PreserveAndAllowNative);
    ExpectTrue("active controller releases only for strict-safe manual route",
               DecideManualRouteAction(true, safeManualRoute) ==
                   ManualRouteAction::AdoptSafe);
    ExpectTrue("validated unsafe manual route is deferred",
               DecideManualRouteAction(true, unsafeManualRoute) ==
                   ManualRouteAction::Defer);
    ExpectTrue("safe-manual adoption minimum is 250ms",
               SafeManualAdoptionWindowMs(-1) == 250);
    ExpectTrue("safe-manual adoption derives from ping",
               SafeManualAdoptionWindowMs(75) == 400);
    ExpectTrue("safe-manual adoption maximum is 1000ms",
               SafeManualAdoptionWindowMs(10000) == 1000);

    MoveIntentState intentState;
    const Vec2 firstManual(500.0f, 100.0f);
    const Vec2 secondManual(700.0f, 200.0f);
    const Vec2 orbDestination(900.0f, 300.0f);
    ExpectTrue("unsafe manual intent is accepted",
               intentState.RecordManual(
                   firstManual,
                   1000,
                   1,
                   unsafeManualRoute,
                   false,
                   50) == MoveIntentRecordResult::Deferred);
    ExpectTrue("unsafe manual intent owns deferred destination",
               intentState.HasDeferred() &&
               intentState.Deferred().Source() ==
                   MoveIntentSource::Manual &&
               intentState.Deferred().Generation() == 1);
    ExpectTrue("later unsafe orb intent cannot replace manual",
               intentState.Record(
                   orbDestination,
                   MoveIntentSource::Orbwalker,
                   1010,
                   2,
                   true) == MoveIntentRecordResult::Ignored);
    ExpectTrue("manual survives later unsafe orb step",
               intentState.Deferred().Position().Distance(firstManual) <
                   0.001f);
    ExpectTrue("later safe orb intent cannot clear manual",
               intentState.Record(
                   orbDestination,
                   MoveIntentSource::Orbwalker,
                   1020,
                   3,
                   false) == MoveIntentRecordResult::Ignored);
    ExpectTrue("manual survives later safe orb step",
               intentState.HasDeferred() &&
               intentState.Deferred().Position().Distance(firstManual) <
                   0.001f);

    ExpectTrue("newer manual replaces older manual",
               intentState.RecordManual(
                   secondManual,
                   1030,
                   4,
                   unsafeManualRoute,
                   true,
                   50) == MoveIntentRecordResult::Deferred);
    ExpectTrue("new manual owns destination and generation",
               intentState.Deferred().Position().Distance(secondManual) <
                   0.001f &&
               intentState.Deferred().Generation() == 4);
    ExpectTrue("older manual generation cannot replace newer manual",
               intentState.RecordManual(
                   firstManual,
                   1035,
                   3,
                   unsafeManualRoute,
                   true,
                   50) == MoveIntentRecordResult::Ignored &&
               intentState.Deferred().Position().Distance(secondManual) <
                   0.001f);

    MoveIntentState resumedManualState;
    resumedManualState.RecordManual(
        firstManual,
        1035,
        5,
        unsafeManualRoute,
        true,
        50);
    resumedManualState.CompleteDeferredResume();
    ExpectTrue("resumed manual releases manual ownership",
               !resumedManualState.HasManual() &&
               !resumedManualState.HasDeferred());

    ExpectTrue("observed path cannot replace active manual",
               intentState.Record(
                   Vec2(1000.0f, 400.0f),
                   MoveIntentSource::ObservedPath,
                   1040,
                   4,
                   true) == MoveIntentRecordResult::Ignored);
    intentState.Clear();
    ExpectTrue("observed path records only without manual",
               intentState.Record(
                   Vec2(1000.0f, 400.0f),
                   MoveIntentSource::ObservedPath,
                   1050,
                   4,
                   true) == MoveIntentRecordResult::Deferred);
    ExpectTrue("observed path owns fallback deferred destination",
               intentState.Deferred().Source() ==
                   MoveIntentSource::ObservedPath);

    MoveIntentState controllerIntentState;
    ExpectTrue("controller target is never recorded as user intent",
               controllerIntentState.Record(
                   Vec2(1100.0f, 500.0f),
                   MoveIntentSource::Controller,
                   1060,
                   5,
                   true) == MoveIntentRecordResult::Ignored &&
               !controllerIntentState.HasGoal() &&
               !controllerIntentState.HasDeferred());

    MoveIntentState safeManualState;
    const Vec2 safeManual(1200.0f, 600.0f);
    ExpectTrue("old automated deferred intent is recorded",
               safeManualState.Record(
                   Vec2(1150.0f, 550.0f),
                   MoveIntentSource::Orbwalker,
                   1065,
                   5,
                   true) == MoveIntentRecordResult::Deferred);
    ExpectTrue("safe manual command is adopted",
               safeManualState.RecordManual(
                   safeManual,
                   1070,
                   6,
                   safeManualRoute,
                   true,
                   75) == MoveIntentRecordResult::SafeManual);
    ExpectTrue("safe manual clears old deferred ownership",
               !safeManualState.HasDeferred());
    ExpectTrue("safe manual becomes current planner goal",
               safeManualState.HasGoal() &&
               safeManualState.Goal().Position().Distance(safeManual) <
                   0.001f);
    ExpectTrue("safe manual blocks stale controller target next frame",
               safeManualState.BlocksControllerTarget());
    ExpectTrue("safe manual records bounded adoption deadline",
               safeManualState.AdoptionDeadlineTick() == 1470);
    ExpectTrue("matching orb echo is not a second intent generation",
               safeManualState.IsManualEcho(safeManual, 1080) &&
               safeManualState.Manual().Generation() == 6);
    ExpectTrue("orb echo cannot replace safe manual planner goal",
               safeManualState.Record(
                   safeManual,
                   MoveIntentSource::Orbwalker,
                   1080,
                   7,
                   false) == MoveIntentRecordResult::Ignored &&
               safeManualState.Manual().Generation() == 6 &&
               safeManualState.Goal().Generation() == 6);
    ExpectTrue("observed native path adopts manual without losing goal",
               safeManualState.AdoptObservedPath(safeManual, 1090) &&
               !safeManualState.HasManual() &&
               safeManualState.HasGoal() &&
               safeManualState.Goal().Position().Distance(safeManual) <
                   0.001f);
    ExpectTrue("adopted manual path releases stale-target guard",
               !safeManualState.BlocksControllerTarget());

    MoveIntentState expiringAdoptionState;
    ExpectTrue("safe manual starts path-adoption ownership",
               expiringAdoptionState.RecordManual(
                   safeManual,
                   2000,
                   8,
                   safeManualRoute,
                   true,
                   75) == MoveIntentRecordResult::SafeManual);
    ExpectTrue("adoption remains before deadline without observation",
               !expiringAdoptionState.ExpireAdoption(2399) &&
               expiringAdoptionState.HasSafeManualAdoption());
    ExpectTrue("adoption expires at deadline without observation",
               expiringAdoptionState.ExpireAdoption(2400) &&
               !expiringAdoptionState.HasManual() &&
               !expiringAdoptionState.BlocksControllerTarget());
    ExpectTrue("orb input is accepted after adoption expiry",
               expiringAdoptionState.Record(
                   orbDestination,
                   MoveIntentSource::Orbwalker,
                   2400,
                   9,
                   false) == MoveIntentRecordResult::Accepted);

    MoveIntentState persistentUnsafeState;
    persistentUnsafeState.RecordManual(
        firstManual,
        3000,
        10,
        unsafeManualRoute,
        true,
        75);
    ExpectTrue("unsafe deferred manual survives adoption expiry time",
               !persistentUnsafeState.ExpireAdoption(10000) &&
               persistentUnsafeState.HasUnsafeManualDeferred() &&
               persistentUnsafeState.Deferred().Position().Distance(
                   firstManual) < 0.001f);
    ExpectTrue("orb remains suppressed by persistent unsafe manual",
               persistentUnsafeState.Record(
                   orbDestination,
                   MoveIntentSource::Orbwalker,
                   10000,
                   11,
                   false) == MoveIntentRecordResult::Ignored &&
               persistentUnsafeState.Deferred().Position().Distance(
                   firstManual) < 0.001f);

    MoveIntentState preservedUnsafeState;
    preservedUnsafeState.RecordManual(
        firstManual,
        11000,
        12,
        unsafeManualRoute,
        false,
        75);
    ExpectTrue("outside ownership invalid manual does not clear unsafe deferred",
               preservedUnsafeState.RecordManual(
                   secondManual,
                   11010,
                   13,
                   invalidManualRoute,
                   false,
                   75) == MoveIntentRecordResult::Ignored &&
               preservedUnsafeState.HasUnsafeManualDeferred() &&
               preservedUnsafeState.Deferred().Position().Distance(
                   firstManual) < 0.001f);

    MoveIntentState activeInvalidState;
    ExpectTrue("active invalid manual fails closed without adoption",
               activeInvalidState.RecordManual(
                   firstManual,
                   12000,
                   14,
                   invalidManualRoute,
                   true,
                   75) == MoveIntentRecordResult::Blocked &&
               !activeInvalidState.HasGoal() &&
               !activeInvalidState.HasDeferred() &&
               !activeInvalidState.HasSafeManualAdoption());

    LockedRouteStatus committed;
    committed.hasLock = true;
    committed.valid = true;
    committed.walkable = true;
    committed.pathSafe = true;
    committed.endpointSafe = true;
    ExpectTrue("safe strict lock remains committed regardless timer or score",
               KeepStrictRoute(committed));

    const bool rerouteStillRequired =
        DecideDeferredRoute(true, false) == DeferredRouteAction::Detour;
    const bool rerouteNowResumable =
        DecideDeferredRoute(true, true) == DeferredRouteAction::Resume;
    ExpectTrue("deferred resume keeps explicit twelve-unit tolerance",
               kDeferredResumeReachTolerance == 12.0f);
    ExpectTrue("unsafe deferred route never resumes due to proximity",
               rerouteStillRequired &&
                   IsMoveTargetReached(
                       1.0f,
                       kDeferredResumeReachTolerance,
                       false));
    ExpectTrue("strict-safe deferred route then uses resume tolerance",
               rerouteNowResumable &&
                   IsMoveTargetReached(
                       kDeferredResumeReachTolerance,
                       kDeferredResumeReachTolerance,
                       false));
    ExpectTrue("strict evade remains committed without deferred detour",
               ShouldCommitStrictState(true, false, false));
    ExpectTrue("required reroute remains committed",
               ShouldCommitStrictState(
                   false,
                   true,
                   rerouteStillRequired));
    ExpectTrue("resumable reroute releases obsolete detour commitment",
               !ShouldCommitStrictState(
                   false,
                   true,
                   !rerouteNowResumable));

    StrictCommitmentInput strictCommitment;
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.replanTimerExpired = true;
    ExpectTrue("replan timer expiry cannot replace committed strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    strictCommitment = {};
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.threatSerialChanged = true;
    ExpectTrue("serial-only revision cannot replace revalidated strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    strictCommitment = {};
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.materialClearanceGain = true;
    ExpectTrue("material clearance gain cannot replace committed strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    strictCommitment = {};
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.materialTimeGain = true;
    ExpectTrue("material time gain cannot replace committed strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    strictCommitment = {};
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.materialCursorGain = true;
    ExpectTrue("material cursor gain cannot replace committed strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    strictCommitment = {};
    strictCommitment.committedState = true;
    strictCommitment.route = committed;
    strictCommitment.targetLockExpired = true;
    ExpectTrue("target-lock expiry cannot replace committed strict target",
               ShouldRetainCommittedStrictTarget(strictCommitment));

    StrictCommitmentInput overlappingThreatRevision;
    overlappingThreatRevision.committedState = true;
    overlappingThreatRevision.route = committed;
    overlappingThreatRevision.threatSerialChanged = true;
    ExpectTrue("overlapping threat revision keeps revalidated safe target",
               ShouldRetainCommittedStrictTarget(
                   overlappingThreatRevision));
    overlappingThreatRevision.route.pathSafe = false;
    ExpectTrue("overlapping threat crossing route permits replacement",
               !ShouldRetainCommittedStrictTarget(
                   overlappingThreatRevision));

    return ZDEvadeTest::Finish("ZDEVADE CONTROLLER POLICY");
}
