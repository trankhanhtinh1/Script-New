#pragma once

#include "EvadeCommandEngine.h"
#include "EvadePlanner.h"
#include "../Detection/ThreatDetector.h"
#include "../EvadeSpells/EvadeSpellEngine.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace ZDEvade {

struct EvadeRuntimeConfig {
    bool enabled = true;
    bool walkingEnabled = true;
    bool evadeSpellsEnabled = true;
    bool leastDangerFallback = true;
    int minimumDanger = 1;
    int evadeSpellMinimumDanger = 3;
    float evadeSpellMarginThresholdMs = 45.0f;
    const ThreatRuleMap* threatRules = nullptr;
    const EvadeSpellRuleMap* evadeSpellRules = nullptr;
    int moveIntervalMs = 75;
    int moveRefreshMs = 260;
    int replanIntervalMs = 70;
    int fallbackReplanIntervalMs = 45;
    EvadeSettings planner;
};

class EvadeController {
public:
    void Reset() {
        command.Reset();
        ResetLocalState();
    }

    void ResetForExternalOwner() {
        command.AbandonControlForExternalOwner();
        ResetLocalState();
    }

private:
    void ResetLocalState() {
        spellEngine.Reset();
        ClearSpellHold();
        state = EvadeControllerState::Idle;
        locked = {};
        pendingTarget = {};
        pendingTargetState = EvadeControllerState::Assessing;
        pendingTargetManualEpoch = 0;
        pendingTargetThreatSetFingerprint = 0;
        pendingTargetSyntheticExtension = false;
        pendingTargetRetryPending = false;
        lastPlan = {};
        lastThreatSerial = -1;
        lastPlanTick = 0;
        lastPlannedThreatSetFingerprint = 0;
        hasPlannedThreatSetFingerprint = false;
        noPlanRetry = {};
        continuousChallenger = {};
        committedRoute = {};
        lastTargetSwitchTick = 0;
        degradationCommitUntilTick = 0;
        releaseControlUntilTick = 0;
        releaseThreatSerial = -1;
        moveRequestGeneration = 0;
        manualRequestGeneration = 0;
        releaseMoveRequestGeneration = 0;
        lockedManualEpoch = 0;
        handledAcquisitionSerial = -1;
        acquisitionMoveRetryPending = false;
        waitingForWindup = false;
        holdingPosition = false;
        releaseHolding = false;
        moveIntents.Clear();
    }

public:
    void Update(const EvadeRuntimeConfig& config) {
        const int now = SDK::Variables::TickCount();
        const auto player = SDK::ObjectManager::Player();
        const std::vector<Threat> snapshot = ThreatDetector::Snapshot();
        if (!config.enabled || (!config.walkingEnabled && !config.evadeSpellsEnabled) ||
            !player.IsValid() || player.IsDead()) {
            Reset();
            return;
        }
        if (!config.walkingEnabled) moveIntents.Clear();
        waitingForWindup = false;
        moveIntents.ExpireAdoption(now);

        const std::vector<Threat> threats = FilterThreats(
            snapshot,
            config.minimumDanger,
            player.HealthPercent(),
            config.threatRules,
            now);
        const std::uint64_t threatSetFingerprint =
            ThreatFingerprintOf(threats);
        const bool threatSetChangedSincePlan =
            hasPlannedThreatSetFingerprint &&
            lastPlannedThreatSetFingerprint !=
                threatSetFingerprint;
        if (continuousChallenger.active &&
            continuousChallenger.threatSetFingerprint !=
                threatSetFingerprint) {
            continuousChallenger = {};
        }
        const Vec2 heroPos = player.ServerPosition().To2D();
        const Vec2 currentIntentGoal = moveIntents.HasGoal()
            ? moveIntents.Goal().Position()
            : SDK::Game::CursorPos().To2D();
        const float heroRadius = SanitizeHeroRadius(player.BoundingRadius());
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const bool releaseControlActive =
            state != EvadeControllerState::Idle;
        const bool pathAcquisitionDanger =
            EvadeGeometry::HeroThreatenedNow(
            threats,
            heroPos,
            heroRadius,
            ControlThreatBuffer(
                false,
                config.planner.pathBuffer,
                config.planner.releaseBuffer),
            now,
            config.planner.maxThreatHorizonMs);
        const bool releaseMarginDanger =
            EvadeGeometry::HeroThreatenedNow(
                threats,
                heroPos,
                heroRadius,
                ControlThreatBuffer(
                    true,
                    config.planner.pathBuffer,
                    config.planner.releaseBuffer),
                now,
                config.planner.maxThreatHorizonMs);
        const bool exactDanger = EvadeGeometry::HeroThreatenedNow(
            threats,
            heroPos,
            heroRadius,
            0.0f,
            now,
            config.planner.maxThreatHorizonMs);
        const bool directDanger = EvadeGeometry::HeroThreatenedNow(
            threats,
            heroPos,
            heroRadius,
            0.0f,
            now,
            0.0f);
        const bool navInterventionArmed =
            IsNavigationInterventionArmed(
                releaseControlActive,
                exactDanger,
                pathAcquisitionDanger,
                releaseMarginDanger);

        CandidateEvaluation currentRoute;
        CandidateEvaluation observedNativeRoute;
        bool nativeRouteAvailable = false;
        bool unsafeCurrentPath = false;
        bool actionableObservedRouteThreat = false;
        std::vector<Vec2> observedPath;
        if (config.walkingEnabled && player.HasPath()) {
            observedPath = BuildCurrentPath(player, player.PathEnd().To2D());
            if (observedPath.size() >= 2) {
                currentRoute = EvadeGeometry::EvaluatePathCandidate(
                    observedPath,
                    PlannerCandidateSource::Cursor,
                    -1,
                    currentIntentGoal,
                    player.ServerPosition().y,
                    moveSpeed,
                    heroRadius,
                    now,
                    config.planner,
                    threats);
                const ObservedRouteEvaluation observedEvaluation = {
                    true,
                    currentRoute.valid,
                    currentRoute.walkable,
                    currentRoute.pathSafe,
                    currentRoute.endpointSafe,
                };
                const ExternalMoveRouteEvaluation observedMoveRoute = {
                    currentRoute.valid,
                    currentRoute.walkable,
                    currentRoute.pathSafe,
                    currentRoute.endpointSafe,
                    currentRoute.strictSafe,
                    currentRoute.reenteredDanger,
                    currentRoute.enteredNewThreat,
                };
                actionableObservedRouteThreat =
                    observedPath.size() >= 2 &&
                    ExternalMoveRouteHasActionableThreat(
                        observedMoveRoute);
                unsafeCurrentPath =
                    IsObservedRouteUnsafe(
                        observedPath.size(),
                        observedEvaluation,
                        navInterventionArmed) ||
                    actionableObservedRouteThreat;
                observedNativeRoute = currentRoute;
                nativeRouteAvailable =
                    currentRoute.valid &&
                    currentRoute.walkable;
            }
            moveIntents.AdoptObservedPath(
                player.PathEnd().To2D(),
                now);
        }
        if (holdingPosition && !unsafeCurrentPath) {
            const Vec2 heldGoal = moveIntents.HasDeferred()
                ? moveIntents.Deferred().Position()
                : currentIntentGoal;
            if (heldGoal.IsValid() && !heldGoal.IsZero() &&
                heroPos.Distance(heldGoal) > heroRadius) {
                observedPath = {heroPos, heldGoal};
                currentRoute = EvadeGeometry::EvaluateCandidate(
                    heldGoal,
                    PlannerCandidateSource::Cursor,
                    -1,
                    heroPos,
                    heldGoal,
                    player.ServerPosition().y,
                    moveSpeed,
                    heroRadius,
                    now,
                    config.planner,
                    threats);
                const ObservedRouteEvaluation heldEvaluation = {
                    true,
                    currentRoute.valid,
                    currentRoute.walkable,
                    currentRoute.pathSafe,
                    currentRoute.endpointSafe,
                };
                const ExternalMoveRouteEvaluation heldMoveRoute = {
                    currentRoute.valid,
                    currentRoute.walkable,
                    currentRoute.pathSafe,
                    currentRoute.endpointSafe,
                    currentRoute.strictSafe,
                    currentRoute.reenteredDanger,
                    currentRoute.enteredNewThreat,
                };
                const bool heldRouteThreat =
                    observedPath.size() >= 2 &&
                    ExternalMoveRouteHasActionableThreat(
                        heldMoveRoute);
                unsafeCurrentPath =
                    IsObservedRouteUnsafe(
                        observedPath.size(),
                        heldEvaluation,
                        navInterventionArmed) ||
                    heldRouteThreat;
                actionableObservedRouteThreat =
                    actionableObservedRouteThreat ||
                    heldRouteThreat;
            }
        }
        const bool actionableThreatContext =
            navInterventionArmed || actionableObservedRouteThreat;
        if (actionableThreatContext &&
            moveIntents.HasDeferred()) {
            moveIntents.Clear();
        }
        const ThreatFreeActionDecision threatFreeUpdate =
            DecideThreatFreeAction(
                actionableThreatContext,
                ThreatFreeDecisionSite::Update);
        if (threatFreeUpdate.applies) {
            if (threatFreeUpdate.clearIntents)
                moveIntents.Clear();
            releaseControlUntilTick = 0;
            holdingPosition = false;
            releaseHolding = false;
            waitingForWindup = false;
            if (threatFreeUpdate.releaseControl)
                HandleRelease();
            return;
        }
        CandidateEvaluation deferredRoute;
        if (moveIntents.HasDeferred()) {
            deferredRoute = EvadeGeometry::EvaluateCandidate(
                moveIntents.Deferred().Position(),
                PlannerCandidateSource::Cursor,
                -1,
                heroPos,
                moveIntents.Deferred().Position(),
                player.ServerPosition().y,
                moveSpeed,
                heroRadius,
                now,
                config.planner,
                threats);
        }
        const bool directDeferredSafe =
            moveIntents.HasDeferred() &&
            deferredRoute.valid &&
            deferredRoute.walkable &&
            deferredRoute.strictSafe;
        const DeferredRouteAction deferredAction = DecideDeferredRoute(
            moveIntents.HasDeferred(),
            directDeferredSafe);
        const bool rerouteRequired =
            deferredAction == DeferredRouteAction::Detour;
        const bool resumeDeferred =
            deferredAction == DeferredRouteAction::Resume;
        const bool strictEndpointReached =
            (state == EvadeControllerState::StrictEvade ||
             state == EvadeControllerState::ReroutingPath) &&
            locked.valid &&
            locked.strictSafe &&
            !locked.position.IsZero() &&
            IsRouteTargetReached(
                heroPos.Distance(locked.position),
                config.planner.endpointBuffer,
                exactDanger);
        ReleaseHysteresisInput hysteresisInput;
        hysteresisInput.controlActive = releaseControlActive;
        hysteresisInput.pathAcquisitionDanger =
            pathAcquisitionDanger;
        hysteresisInput.releaseMarginDanger = releaseMarginDanger;
        hysteresisInput.exactDanger = exactDanger;
        hysteresisInput.currentPathUnsafe = unsafeCurrentPath;
        hysteresisInput.strictEndpointReached =
            strictEndpointReached;
        const ReleaseHysteresisAction hysteresisAction =
            DecideReleaseHysteresis(hysteresisInput);
        const bool endangered =
            hysteresisAction != ReleaseHysteresisAction::Release;

        const CandidateEvaluation holdRoute =
            EvadeGeometry::EvaluateStationaryCandidate(
                heroPos,
                currentIntentGoal,
                player.ServerPosition().y,
                heroRadius,
                now,
                config.planner,
                threats);
        const float routeTemporalResolutionMs =
            std::max(25.0f, config.planner.temporalStepMs);
        const bool baselineUsesNative =
            nativeRouteAvailable &&
            ThreatCoverageNoWorseAtResolution(
                CoverageOf(observedNativeRoute),
                CoverageOf(holdRoute),
                routeTemporalResolutionMs);
        CandidateEvaluation baselineRoute = baselineUsesNative
            ? observedNativeRoute
            : holdRoute;
        ThreatCoverage baselineCoverage = CoverageOf(baselineRoute);

        const int serial = ThreatDetector::ChangeSerial();
        const bool observedInvulnerable = player.IsInvulnerable();
        const bool observedUntargetable = !player.IsTargetable();
        bool spellHoldActive = spellHoldUntilTick > now;
        const bool spellHoldActivationReached =
            spellHoldActivationTick > 0 &&
            now >= spellHoldActivationTick;
        if (ShouldClearEstimatedHold(
                spellHoldProtection,
                spellHoldActive,
                spellHoldActivationReached,
                observedInvulnerable,
                observedUntargetable)) {
            ClearSpellHold();
            spellHoldActive = false;
        }
        const bool verifiedSuppressingHold =
            VerifiedHoldMaySuppressPlanning(
                spellHoldProtection,
                spellHoldActive,
                observedInvulnerable,
                observedUntargetable);
        const FirstActionableAcquisitionPolicy acquisition =
            DecideFirstActionableAcquisition({
                command.ControlActive(),
                exactDanger,
                pathAcquisitionDanger,
                directDanger,
                actionableObservedRouteThreat,
                verifiedSuppressingHold,
                serial,
                handledAcquisitionSerial,
            });
        AcquisitionSerialCommit acquisitionSerialCommit = {
            &handledAcquisitionSerial,
            serial,
            acquisition.recordSerialAfterProcessing,
        };
        if (acquisition.firstActionableAcquisition) {
            releaseControlUntilTick = 0;
            noPlanRetry = {};
            continuousChallenger = {};
            acquisitionMoveRetryPending = false;
        }
        if (releaseControlUntilTick > now) {
            ReleaseDecisionInput cooldownDecision;
            cooldownDecision.currentThreatSerial = serial;
            cooldownDecision.releaseThreatSerial = releaseThreatSerial;
            cooldownDecision.currentMoveRequestGeneration =
                moveRequestGeneration;
            cooldownDecision.releaseMoveRequestGeneration =
                releaseMoveRequestGeneration;
            if (MustBreakReleaseCooldown(cooldownDecision)) {
                releaseControlUntilTick = 0;
            } else if (
                !resumeDeferred &&
                (endangered || unsafeCurrentPath)) {
                const bool unsafeMovementActive =
                    unsafeCurrentPath && player.IsMoving();
                if (unsafeMovementActive &&
                    !command.ControlActive() &&
                    !command.BeginControl()) {
                    return;
                }
                const StopIssueResult stopResult = unsafeMovementActive
                    ? command.StopUnsafeMovement()
                    : StopIssueResult::Failed;
                if (!CanReleaseAfterStop(
                        unsafeMovementActive,
                        stopResult)) {
                    command.BeginControl();
                    return;
                }
                HandleRelease();
                return;
            }
        }
        if (!endangered && !unsafeCurrentPath) releaseControlUntilTick = 0;
        if (hysteresisAction ==
                ReleaseHysteresisAction::HoldAtStrictEndpoint &&
            !resumeDeferred) {
            releaseHolding = true;
            holdingPosition = false;
            if (!command.BeginControl(true)) return;
            if (player.IsMoving())
                command.StopUnsafeMovement(180);
            return;
        }
        releaseHolding = false;

        if (unsafeCurrentPath)
            moveIntents.Clear();

        const Vec2 plannerGoal = moveIntents.HasDeferred()
            ? moveIntents.Deferred().Position()
            : moveIntents.HasGoal()
                ? moveIntents.Goal().Position()
                : SDK::Game::CursorPos().To2D();
        const Vec2 lockedTarget = locked.position;
        CandidateEvaluation currentLocked;
        const LockedRouteValidation lockedValidation =
            ValidateLocked(
            player,
            threats,
            config,
            now,
            baselineCoverage,
            routeTemporalResolutionMs,
            &currentLocked);
        const bool lockedHardValid =
            lockedValidation.hardValid;
        const bool lockedSafetyValid =
            lockedValidation.safety != LockedRouteSafety::Unsafe;
        const bool lockedPhysicallyReached =
            lockedValidation.reached;
        CommittedRoutePolicyInput validationCommitmentInput;
        validationCommitmentInput.commitment = committedRoute;
        validationCommitmentInput.threatSetFingerprint =
            threatSetFingerprint;
        validationCommitmentInput.manualEpoch =
            manualRequestGeneration;
        validationCommitmentInput.threatSetEmpty = threats.empty();
        validationCommitmentInput.currentHardValid =
            lockedHardValid;
        validationCommitmentInput.currentNoWorse =
            lockedSafetyValid;
        validationCommitmentInput.currentReached =
            lockedPhysicallyReached;
        committedRoute = DecideCommittedRoute(
            validationCommitmentInput).commitment;
        const bool enforceCommittedBranch =
            committedRoute.active &&
            committedRoute.threatSetFingerprint ==
                threatSetFingerprint &&
            committedRoute.manualEpoch ==
                manualRequestGeneration &&
            lockedHardValid;
        const bool retainExactCommittedTarget =
            enforceCommittedBranch &&
            lockedSafetyValid &&
            !lockedPhysicallyReached;
        if (!lockedSafetyValid ||
            lockedManualEpoch != manualRequestGeneration) {
            continuousChallenger = {};
        }
        if (enforceCommittedBranch)
            continuousChallenger = {};
        if (ShouldClearPendingTargetForExactCommitment(
                retainExactCommittedTarget,
                pendingTargetRetryPending)) {
            ClearPendingTarget();
        }
        bool pendingTargetUsable = false;
        if (pendingTarget.valid) {
            const Vec2 proposedPosition = pendingTarget.position;
            const bool pendingEpochValid =
                pendingTargetManualEpoch == manualRequestGeneration;
            const bool pendingThreatSetValid =
                pendingTargetThreatSetFingerprint ==
                    threatSetFingerprint;
            const bool pendingBranchValid =
                !enforceCommittedBranch ||
                SameCommittedRouteBranch(
                    committedRoute,
                    pendingTarget.sourceThreatId,
                    pendingTarget.stabilityBranchKey,
                    proposedPosition - heroPos);
            CandidateEvaluation refreshedPending =
                EvadeGeometry::EvaluateCandidate(
                    proposedPosition,
                    pendingTarget.source,
                    pendingTarget.sourceThreatId,
                    heroPos,
                    plannerGoal,
                    player.ServerPosition().y,
                    moveSpeed,
                    heroRadius,
                    now,
                    config.planner,
                    threats);
            refreshedPending.position = proposedPosition;
            CarryStabilityBranchKey(
                refreshedPending,
                pendingTarget);
            refreshedPending.enemyDistance =
                pendingTarget.enemyDistance;
            refreshedPending.turretPenalty =
                pendingTarget.turretPenalty;
            const LockedRouteValidation pendingValidation =
                ClassifyLockedRoute({
                    CoverageOf(refreshedPending),
                    baselineCoverage,
                    true,
                    refreshedPending.valid,
                    refreshedPending.walkable,
                    IsRouteTargetReached(
                        heroPos.Distance(proposedPosition),
                        config.planner.endpointBuffer,
                        exactDanger),
                    false,
                    refreshedPending.strictSafe,
                    routeTemporalResolutionMs,
                    refreshedPending.startThreatIdentities.Size() > 0,
                    refreshedPending.exitedStartEnvelope,
                });
            if (ShouldRetainRefreshedPendingTarget(
                    pendingEpochValid,
                    pendingThreatSetValid,
                    pendingBranchValid,
                    pendingValidation.safety)) {
                pendingTarget = refreshedPending;
                pendingTargetUsable =
                    pendingTarget.strictSafe ||
                    config.leastDangerFallback;
                pendingTargetState = pendingTarget.strictSafe
                    ? rerouteRequired
                        ? EvadeControllerState::ReroutingPath
                        : EvadeControllerState::StrictEvade
                    : EvadeControllerState::FallbackEvade;
            } else {
                ClearPendingTarget();
            }
        }
        const bool strictStateBeforeValidation =
            state == EvadeControllerState::StrictEvade ||
            state == EvadeControllerState::ReroutingPath;
        if (strictStateBeforeValidation &&
            lockedValidation.safety ==
                LockedRouteSafety::FallbackNoWorse) {
            locked = currentLocked;
            locked.position = lockedTarget;
            locked.strictSafe = false;
            lastPlan.selected = locked;
            lastPlan.found = true;
            lastPlan.strictSafe = false;
            state = EvadeControllerState::FallbackEvade;
            degradationCommitUntilTick = SaturatingTickAdd(
                now,
                DegradationCommitWindowMs(
                    config.planner.targetLockMs));
        }
        const bool promotedFallback = ShouldPromoteFallbackEvaluation(
            state == EvadeControllerState::FallbackEvade,
            lockedHardValid && currentLocked.valid,
            currentLocked.walkable,
            currentLocked.strictSafe);
        if (promotedFallback) {
            locked = currentLocked;
            locked.position = lockedTarget;
            lastPlan.selected = locked;
            lastPlan.found = true;
            lastPlan.strictSafe = true;
            state = EvadeControllerState::StrictEvade;
            degradationCommitUntilTick = 0;
        }
        if ((state == EvadeControllerState::StrictEvade ||
             state == EvadeControllerState::ReroutingPath) &&
            lockedValidation.safety ==
                LockedRouteSafety::StrictSafe) {
            ClearPendingTarget();
            pendingTargetUsable = false;
        }
        const bool lockedImprovesCoverage = lockedHardValid &&
            MateriallyImprovesThreatCoverage(
                CoverageOf(currentLocked),
                baselineCoverage,
                routeTemporalResolutionMs);
        const bool lockedNoWorseCoverage = lockedHardValid &&
            ThreatCoverageNoWorseAtResolution(
                CoverageOf(currentLocked),
                baselineCoverage,
                routeTemporalResolutionMs);
        const bool committedState = ShouldCommitStrictState(
            state == EvadeControllerState::StrictEvade,
            state == EvadeControllerState::ReroutingPath,
            rerouteRequired);
        const EvadeControllerState committedControllerState =
            rerouteRequired ? EvadeControllerState::ReroutingPath : state;
        StrictCommitmentInput commitment;
        commitment.committedState = committedState;
        commitment.deferredResumeReady = resumeDeferred;
        commitment.route = {
                locked.valid,
                lockedValidation.hardValid,
                lockedValidation.hardValid,
                currentLocked.pathSafe && currentLocked.timingSafe,
                currentLocked.endpointSafe,
                lockedPhysicallyReached,
            };
        commitment.replanTimerExpired =
            lastPlanTick == 0 ||
            TickDifference(now, lastPlanTick) >=
                std::max(20, config.replanIntervalMs);
        commitment.threatSerialChanged = serial != lastThreatSerial;
        commitment.threatSerialChanged =
            commitment.threatSerialChanged ||
            threatSetChangedSincePlan;
        commitment.targetLockExpired =
            lastTargetSwitchTick <= 0 ||
            TickDifference(now, lastTargetSwitchTick) >= std::max(
                config.planner.targetLockMs,
                config.replanIntervalMs);
        const bool committedStrictLock =
            ShouldRetainCommittedStrictTarget(commitment);
        if (committedStrictLock) {
            const Vec2 committedTarget = locked.position;
            locked = currentLocked;
            locked.position = committedTarget;
            lastPlan.selected = locked;
            lastPlan.found = true;
            lastPlan.strictSafe = true;
            state = committedControllerState;
            lastThreatSerial = serial;
        }

        if (ShouldExecuteReleaseOrDeferredResume(
                resumeDeferred,
                endangered) &&
            !rerouteRequired &&
            !committedStrictLock) {
            HandleRelease();
            holdingPosition = false;
            if (resumeDeferred) {
                if (!command.BeginControl(true)) return;
                const Vec2 resumeDestination =
                    moveIntents.Deferred().Position();
                const MoveIssueResult resumeResult = command.MoveTo(
                    player,
                    resumeDestination,
                    config.moveIntervalMs,
                    config.moveRefreshMs,
                    kDeferredResumeReachTolerance,
                    false);
                if (resumeResult == MoveIssueResult::Issued ||
                    resumeResult == MoveIssueResult::AlreadyFollowing) {
                    moveIntents.CompleteDeferredResume();
                }
                command.EndControl();
            } else if (unsafeCurrentPath) {
                if (!command.BeginControl(true)) return;
                command.StopUnsafeMovement();
                command.EndControl();
            }
            return;
        }
        state = state == EvadeControllerState::Idle
            ? EvadeControllerState::Assessing
            : state;
        if (verifiedSuppressingHold) {
            ClearPendingTarget();
            command.BeginControl();
            return;
        }

        int planInterval = state == EvadeControllerState::FallbackEvade
            ? config.fallbackReplanIntervalMs
            : config.replanIntervalMs;
        if ((state == EvadeControllerState::StrictEvade ||
             state == EvadeControllerState::ReroutingPath) &&
            lockedSafetyValid)
            planInterval = std::clamp(planInterval * 3, 120, 240);
        if (state == EvadeControllerState::FallbackEvade &&
            lastPlan.selected.firstCollisionTimeMs != FLT_MAX && lastPlanTick > 0) {
            const float remaining = lastPlan.selected.firstCollisionTimeMs -
                static_cast<float>(TickDifference(now, lastPlanTick));
            planInterval = std::min(
                planInterval,
                std::max(20, static_cast<int>(std::round(remaining * 0.2f))));
        }
        const bool planningDue = ShouldReplanRoute(
            lockedSafetyValid,
            now,
            lastPlanTick,
            planInterval,
            serial,
            lastThreatSerial,
            manualRequestGeneration,
            noPlanRetry);
        const bool shouldReplan = acquisition.forceReplan ||
            (!promotedFallback &&
             !pendingTargetUsable &&
             (planningDue || lockedPhysicallyReached));
        bool releasedReachedBranch = false;
        bool reachedAlternateAvailable = false;

        const bool fallbackLockWasActive =
            state == EvadeControllerState::FallbackEvade;
        if (shouldReplan) {
            const bool threatSetChangedForPlan =
                threatSetChangedSincePlan;
            const EvadeControllerState stateBeforePlanning = state;
            state = EvadeControllerState::Assessing;
            PlannerResult plan = config.walkingEnabled
                ? EvadePlanner::FindBest(
                    player,
                    threats,
                    config.planner,
                    plannerGoal)
                : PlannerResult{};
            lastPlanTick = now;
            lastThreatSerial = serial;
            lastPlannedThreatSetFingerprint =
                threatSetFingerprint;
            hasPlannedThreatSetFingerprint = true;
            bool outerCommitmentApplied = false;
            bool outerCommitmentProposes = false;
            bool outerCommitmentSyntheticExtension = false;
            if (enforceCommittedBranch) {
                const CommittedRouteIdentity enforcedIdentity =
                    committedRoute;
                const CommittedBranchCandidate branchChoice =
                    FindCommittedBranchCandidate(
                        plan,
                        currentLocked,
                        baselineCoverage,
                        player,
                        plannerGoal,
                        threats,
                        config,
                        now,
                        lockedPhysicallyReached);
                const CandidateEvaluation& branchCandidate =
                    branchChoice.evaluation;
                const bool sameCommittedPhysicalSide =
                    branchCandidate.valid &&
                    SameCommittedRouteBranch(
                        committedRoute,
                        branchCandidate.sourceThreatId,
                        branchCandidate.stabilityBranchKey,
                        branchCandidate.position - heroPos);
                const bool shortenCommittedFallback =
                    !currentLocked.strictSafe &&
                    ShouldReplaceCommittedFallback(
                        StableMetrics(currentLocked),
                        StableMetrics(branchCandidate),
                        sameCommittedPhysicalSide,
                        routeTemporalResolutionMs);
                CommittedRoutePolicyInput outerInput;
                outerInput.commitment = committedRoute;
                outerInput.threatSetFingerprint =
                    threatSetFingerprint;
                outerInput.manualEpoch =
                    manualRequestGeneration;
                outerInput.currentHardValid =
                    lockedHardValid;
                outerInput.currentNoWorse =
                    lockedSafetyValid &&
                    !shortenCommittedFallback;
                outerInput.currentReached =
                    lockedPhysicallyReached;
                outerInput.candidateAvailable =
                    branchCandidate.valid &&
                    branchCandidate.walkable &&
                    CandidateHasRequiredStartEnvelopeExit(
                        branchCandidate.startThreatIdentities.Size() > 0,
                        branchCandidate.exitedStartEnvelope);
                outerInput.candidateStartsInThreat =
                    branchCandidate.startThreatIdentities.Size() > 0;
                outerInput.candidateExitedStartEnvelope =
                    branchCandidate.exitedStartEnvelope;
                outerInput.candidateSourceThreatId =
                    branchCandidate.sourceThreatId;
                outerInput.candidateStabilityBranchKey =
                    branchCandidate.stabilityBranchKey;
                outerInput.candidateDirection =
                    branchCandidate.position - heroPos;
                outerInput.candidateSyntheticExtension =
                    branchChoice.syntheticExtension;
                outerInput.reachedExtensionEvaluated =
                    branchChoice.extensionAttempted;
                const CommittedRouteDecision outerDecision =
                    DecideCommittedRoute(outerInput);
                committedRoute = outerDecision.commitment;
                outerCommitmentProposes =
                    outerDecision.action ==
                        CommittedRouteAction::ProposeSameBranch ||
                    outerDecision.action ==
                        CommittedRouteAction::
                            ProposeSameDirectionExtension;
                outerCommitmentSyntheticExtension =
                    outerDecision.action ==
                    CommittedRouteAction::
                        ProposeSameDirectionExtension;
                releasedReachedBranch =
                    outerDecision.action ==
                        CommittedRouteAction::
                            ReleaseReachedBranch;
                if (releasedReachedBranch) {
                    const CandidateEvaluation alternate =
                        FindAlternateBranchCandidate(
                            plan,
                            enforcedIdentity,
                            baselineCoverage,
                            player,
                            config);
                    reachedAlternateAvailable =
                        alternate.valid &&
                        alternate.walkable;
                    if (reachedAlternateAvailable) {
                        plan.selected = alternate;
                        plan.found = true;
                        plan.strictSafe =
                            alternate.strictSafe;
                    } else {
                        plan = {};
                    }
                } else if (outerCommitmentProposes) {
                    plan.selected = branchCandidate;
                    plan.found = true;
                    plan.strictSafe =
                        branchCandidate.strictSafe;
                } else {
                    plan.selected = currentLocked;
                    plan.selected.position = lockedTarget;
                    plan.found = lockedHardValid;
                    plan.strictSafe =
                        plan.found &&
                        currentLocked.strictSafe;
                }
                outerCommitmentApplied =
                    !releasedReachedBranch;
                continuousChallenger = {};
            }
            if (!outerCommitmentApplied &&
                !releasedReachedBranch &&
                plan.found &&
                lockedSafetyValid) {
                const bool sameManualEpoch =
                    lockedManualEpoch == manualRequestGeneration;
                const bool targetLockActive = sameManualEpoch &&
                    lastTargetSwitchTick > 0 &&
                    TickDifference(now, lastTargetSwitchTick) < std::max(
                        config.planner.targetLockMs,
                        config.replanIntervalMs);
                const bool degradationCommitActive =
                    sameManualEpoch &&
                    degradationCommitUntilTick > now;
                if (sameManualEpoch) {
                    plan.selected = SelectStableTarget(
                        plan.selected,
                        currentLocked,
                        targetLockActive,
                        degradationCommitActive,
                        player,
                        plannerGoal,
                        threats,
                        config,
                        exactDanger);
                }
                plan.found = plan.selected.valid && plan.selected.walkable;
                plan.strictSafe = plan.found && plan.selected.strictSafe;
            }
            UnavoidableDecisionInput unavoidableInput;
            unavoidableInput.holdCoverage = CoverageOf(holdRoute);
            unavoidableInput.nativeCoverage =
                CoverageOf(observedNativeRoute);
            unavoidableInput.nativeAvailable = nativeRouteAvailable;
            unavoidableInput.candidateCoverage =
                CoverageOf(plan.selected);
            unavoidableInput.candidateAvailable =
                plan.found &&
                (plan.strictSafe || config.leastDangerFallback);
            unavoidableInput.candidateValid =
                plan.selected.valid;
            unavoidableInput.candidateWalkable =
                plan.selected.walkable;
            unavoidableInput.candidateStrictSafe =
                plan.strictSafe;
            unavoidableInput.candidateMakesProgress =
                MakesDeterministicProgress(
                    plan.selected,
                    baselineRoute,
                    config.planner);
            unavoidableInput.candidateStartsInThreat =
                plan.selected.startThreatIdentities.Size() > 0;
            unavoidableInput.candidateExitedStartEnvelope =
                plan.selected.exitedStartEnvelope;
            unavoidableInput.candidateEnteredNewThreat =
                plan.selected.enteredNewThreat;
            unavoidableInput.candidateReenteredDanger =
                plan.selected.reenteredDanger;
            unavoidableInput.fallbackLockActive =
                fallbackLockWasActive &&
                !releasedReachedBranch;
            unavoidableInput.lockCoverage =
                CoverageOf(currentLocked);
            unavoidableInput.lockValid = lockedHardValid;
            unavoidableInput.lockWalkable =
                currentLocked.walkable;
            unavoidableInput.lockReached =
                !locked.position.IsZero() &&
                IsRouteTargetReached(
                    heroPos.Distance(locked.position),
                    config.planner.endpointBuffer,
                    exactDanger);
            unavoidableInput.currentManualEpoch =
                manualRequestGeneration;
            unavoidableInput.lockManualEpoch =
                lockedManualEpoch;
            const UnavoidableDecision unavoidable =
                DecideUnavoidableAction(unavoidableInput);
            bool commitPlanInPlace = false;
            if (outerCommitmentApplied) {
                continuousChallenger = {};
                const bool targetChanged =
                    outerCommitmentProposes &&
                    (!locked.valid ||
                     locked.position.DistanceSqr(
                         plan.selected.position) > 0.25f);
                if (targetChanged) {
                    pendingTarget = plan.selected;
                    pendingTargetManualEpoch =
                        manualRequestGeneration;
                    pendingTargetThreatSetFingerprint =
                        threatSetFingerprint;
                    pendingTargetSyntheticExtension =
                        outerCommitmentSyntheticExtension;
                    pendingTargetRetryPending = false;
                    pendingTargetState = plan.strictSafe
                        ? rerouteRequired
                            ? EvadeControllerState::ReroutingPath
                            : EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                    pendingTargetUsable =
                        pendingTarget.strictSafe ||
                        config.leastDangerFallback;
                    state = stateBeforePlanning;
                } else {
                    const Vec2 retainedTarget =
                        outerCommitmentProposes
                        ? plan.selected.position
                        : lockedTarget;
                    locked = outerCommitmentProposes
                        ? plan.selected
                        : currentLocked;
                    locked.position = retainedTarget;
                    lockedManualEpoch =
                        manualRequestGeneration;
                    plan.selected = locked;
                    plan.found = locked.valid &&
                        locked.walkable;
                    plan.strictSafe =
                        plan.found && locked.strictSafe;
                    state = plan.strictSafe
                        ? rerouteRequired
                            ? EvadeControllerState::ReroutingPath
                            : EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                    commitPlanInPlace = true;
                }
            } else if (unavoidable.retainLockedFallback) {
                continuousChallenger = {};
                const Vec2 retainedTarget = locked.position;
                locked = currentLocked;
                locked.position = retainedTarget;
                plan.selected = locked;
                plan.found = true;
                plan.strictSafe = locked.strictSafe;
                state = EvadeControllerState::FallbackEvade;
                commitPlanInPlace = true;
            } else if (
                unavoidable.action ==
                    UnavoidableAction::MoveFallback &&
                plan.found) {
                const bool committedIdentityChanged =
                    committedRoute.active &&
                    !SameCommittedRouteBranch(
                        committedRoute,
                        plan.selected.sourceThreatId,
                        plan.selected.stabilityBranchKey,
                        plan.selected.position - heroPos);
                const bool committedTargetChanged =
                    committedRoute.active &&
                    locked.position.DistanceSqr(
                        plan.selected.position) > 0.25f;
                const bool committedThreatSetChanged =
                    committedRoute.active &&
                    committedRoute.threatSetFingerprint !=
                        threatSetFingerprint;
                const bool switched = !locked.valid ||
                    committedIdentityChanged ||
                    committedTargetChanged ||
                    committedThreatSetChanged ||
                    locked.position.DistanceSqr(plan.selected.position) >
                        config.planner.targetSwitchDistance *
                        config.planner.targetSwitchDistance;
                if (switched) {
                    const bool requiresHysteresis =
                        RequiresContinuousSwitchHysteresis(
                            lockedSafetyValid &&
                                !releasedReachedBranch,
                            plan.strictSafe &&
                                !currentLocked.strictSafe,
                            HasDiscreteThreatCoverageImprovement(
                                CoverageOf(plan.selected),
                                CoverageOf(currentLocked)),
                            lockedManualEpoch !=
                                manualRequestGeneration,
                            threatSetChangedForPlan);
                    const ContinuousChallengerDecision
                        challengerDecision =
                            AdvanceContinuousChallenger(
                                continuousChallenger,
                                heroPos,
                                plan.selected.position,
                                static_cast<int>(
                                    plan.selected.source),
                                plan.selected.sourceThreatId,
                                plan.selected.stabilityBranchKey,
                                manualRequestGeneration,
                                threatSetFingerprint,
                                now,
                                moveSpeed,
                                requiresHysteresis);
                    continuousChallenger =
                        challengerDecision.state;
                    if (requiresHysteresis &&
                        !challengerDecision.switchReady) {
                        const Vec2 retainedTarget =
                            locked.position;
                        locked = currentLocked;
                        locked.position = retainedTarget;
                        plan.selected = locked;
                        plan.found = true;
                        plan.strictSafe =
                            locked.strictSafe;
                        state = stateBeforePlanning;
                        commitPlanInPlace = true;
                    } else {
                        continuousChallenger = {};
                        pendingTarget = plan.selected;
                        pendingTargetManualEpoch =
                            manualRequestGeneration;
                        pendingTargetThreatSetFingerprint =
                            threatSetFingerprint;
                        pendingTargetSyntheticExtension = false;
                        pendingTargetRetryPending = false;
                        pendingTargetState = plan.strictSafe
                            ? rerouteRequired
                                ? EvadeControllerState::ReroutingPath
                                : EvadeControllerState::StrictEvade
                            : EvadeControllerState::FallbackEvade;
                        pendingTargetUsable =
                            pendingTarget.strictSafe ||
                            config.leastDangerFallback;
                        state = lockedSafetyValid
                            ? stateBeforePlanning
                            : EvadeControllerState::Assessing;
                    }
                } else {
                    continuousChallenger = {};
                    locked = plan.selected;
                    lockedManualEpoch = manualRequestGeneration;
                    state = plan.strictSafe
                        ? rerouteRequired
                            ? EvadeControllerState::ReroutingPath
                            : EvadeControllerState::StrictEvade
                        : EvadeControllerState::FallbackEvade;
                    commitPlanInPlace = true;
                }
            } else {
                continuousChallenger = {};
                if (lockedSafetyValid &&
                    !releasedReachedBranch) {
                    locked = currentLocked;
                    locked.position = lockedTarget;
                    state = stateBeforePlanning;
                } else {
                    locked = {};
                    ClearPendingTarget();
                    state = EvadeControllerState::Assessing;
                }
            }
            if (commitPlanInPlace) lastPlan = plan;
        } else if (lockedSafetyValid &&
                   (lockedImprovesCoverage ||
                    lockedNoWorseCoverage)) {
            locked = currentLocked;
            locked.position = lockedTarget;
            lastPlan.selected = locked;
            lastPlan.found = true;
            lastPlan.strictSafe = locked.strictSafe;
        }

        if (releasedReachedBranch &&
            !reachedAlternateAvailable) {
            ClearPendingTarget();
            locked = {};
            lastPlan = {};
            state = EvadeControllerState::Assessing;
        }

        const bool retainedFallbackLock =
            state == EvadeControllerState::FallbackEvade &&
            lockedSafetyValid &&
            lockedManualEpoch == manualRequestGeneration &&
            ThreatCoverageNoWorseAtResolution(
                CoverageOf(locked),
                baselineCoverage,
                routeTemporalResolutionMs);
        const bool newlyAdmissibleLock =
            locked.valid &&
            locked.walkable &&
            (MateriallyImprovesThreatCoverage(
                 CoverageOf(locked),
                 baselineCoverage,
                 routeTemporalResolutionMs) ||
             (EquivalentThreatCoverageAtResolution(
                  CoverageOf(locked),
                  baselineCoverage,
                  routeTemporalResolutionMs) &&
              MakesDeterministicProgress(
                  locked,
                  baselineRoute,
                  config.planner)));
        const bool outerCommittedHardRoute =
            committedRoute.active &&
            committedRoute.threatSetFingerprint ==
                threatSetFingerprint &&
            committedRoute.manualEpoch ==
                manualRequestGeneration &&
            lockedHardValid &&
            lockedSafetyValid &&
            locked.valid &&
            locked.walkable;
        const bool hasUsableCommittedPlan =
            config.walkingEnabled &&
            !moveIntents.BlocksControllerTarget() &&
            lastPlan.found &&
            locked.valid &&
            locked.walkable &&
            CandidateHasRequiredStartEnvelopeExit(
                locked.startThreatIdentities.Size() > 0,
                locked.exitedStartEnvelope) &&
            (committedStrictLock ||
             outerCommittedHardRoute ||
             retainedFallbackLock ||
             newlyAdmissibleLock) &&
            (locked.strictSafe || config.leastDangerFallback);
        const bool hasUsablePendingPlan =
            config.walkingEnabled &&
            !moveIntents.BlocksControllerTarget() &&
            pendingTargetUsable &&
            pendingTarget.valid &&
            pendingTarget.walkable &&
            CandidateHasRequiredStartEnvelopeExit(
                pendingTarget.startThreatIdentities.Size() > 0,
                pendingTarget.exitedStartEnvelope);
        bool hasUsableLockedPlan =
            hasUsableCommittedPlan ||
            hasUsablePendingPlan;
        const CandidateEvaluation& actionRoute =
            hasUsablePendingPlan ? pendingTarget : locked;
        if (shouldReplan) {
            noPlanRetry = hasUsableLockedPlan
                ? NoPlanRetrySchedule{}
                : acquisition.firstActionableAcquisition
                    ? NoPlanRetrySchedule{
                        true,
                        SaturatingTickAdd(
                            now,
                            acquisition.noPlanRetryDelayMs),
                        serial,
                        manualRequestGeneration,
                    }
                    : ScheduleNoPlanRetry(
                        now,
                        baselineRoute.firstCollisionTimeMs,
                        config.planner.minimumTimeMarginMs,
                        serial,
                        manualRequestGeneration);
        } else if (hasUsableLockedPlan) {
            noPlanRetry = {};
        }
        const bool pathDanger = unsafeCurrentPath || rerouteRequired;
        if (config.walkingEnabled &&
            !acquisition.skipComfortHold &&
            !hasUsablePendingPlan &&
            !baselineUsesNative &&
            ShouldHoldPosition(
                directDanger,
                pathDanger,
                player,
                heroPos,
                plannerGoal,
                heroRadius,
                observedPath,
                threats,
                config,
                hasUsableLockedPlan)) {
            holdingPosition = true;
            state = EvadeControllerState::ReroutingPath;
            if (!command.BeginControl(true)) {
                holdingPosition = false;
                return;
            }
            const int windupRemaining = SDK::Orbwalker::IsAutoAttacking()
                ? std::max(0, SDK::Orbwalker::AttackCastDelayRemaining())
                : 0;
            if (windupRemaining <= 0 && player.IsMoving())
                command.StopUnsafeMovement(180);
            return;
        }
        holdingPosition = false;

        const int windupRemaining = SDK::Orbwalker::IsAutoAttacking()
            ? std::max(0, SDK::Orbwalker::AttackCastDelayRemaining())
            : 0;
        if (!acquisition.skipWindupPreservation &&
            windupRemaining > 0 &&
            hasUsableLockedPlan &&
            actionRoute.strictSafe) {
            const int windupLatency = std::clamp(
                std::max(0, SDK::Game::Ping()) / 6,
                0,
                25);
            EvadeSettings delayed = config.planner;
            delayed.inputDelayMs += static_cast<float>(
                windupRemaining + windupLatency);
            CandidateEvaluation delayedRoute = EvadeGeometry::EvaluateCandidate(
                actionRoute.position,
                actionRoute.source,
                actionRoute.sourceThreatId,
                heroPos,
                plannerGoal,
                player.ServerPosition().y,
                moveSpeed,
                heroRadius,
                now,
                delayed,
                threats);
            if (delayedRoute.valid && delayedRoute.walkable &&
                delayedRoute.strictSafe &&
                ImprovesThreatCoverage(
                    CoverageOf(delayedRoute),
                    baselineCoverage)) {
                if (!command.BeginControl(true)) return;
                waitingForWindup = true;
                return;
            }
        }

        const bool weakWalkingPlan =
            !hasUsableLockedPlan ||
            !actionRoute.strictSafe ||
            actionRoute.timeMarginMs <
                config.evadeSpellMarginThresholdMs ||
            actionRoute.minimumClearance <
                std::max(0.0f, config.planner.preferredClearance * 0.5f);
        if (endangered &&
            config.evadeSpellsEnabled &&
            weakWalkingPlan &&
            !(acquisition.firstActionableAcquisition &&
              !hasUsableLockedPlan)) {
            if (!command.BeginControl()) return;
            const EvadeSpellCastResult spell = spellEngine.TryUse(
                player,
                threats,
                config.planner,
                config.evadeSpellMinimumDanger,
                &lastPlan,
                config.evadeSpellRules);
            if (spell.casted) {
                ClearPendingTarget();
                committedRoute = {};
                locked = spell.destination;
                locked.source = PlannerCandidateSource::EvadeSpell;
                locked.strictSafe = true;
                lastPlan.found = true;
                lastPlan.strictSafe = true;
                lastPlan.selected = locked;
                state = EvadeControllerState::StrictEvade;
                spellHoldUntilTick = spell.holdUntilTick;
                spellHoldActivationTick = spell.protectionActivationTick;
                spellHoldProtection = spell.protection;
                return;
            }
        }

        if (!hasUsableLockedPlan) {
            acquisitionMoveRetryPending = false;
            if (baselineUsesNative &&
                !acquisition.stopImmediatelyWithoutPlan) {
                holdingPosition = false;
                HandleRelease();
                return;
            }
            state = EvadeControllerState::FallbackEvade;
            if (!command.BeginControl(true)) {
                holdingPosition = false;
                return;
            }
            const bool playerMoving = player.IsMoving();
            const StopIssueResult stopResult = playerMoving
                ? command.StopUnsafeMovement()
                : StopIssueResult::Failed;
            const NoPlanHoldAction holdAction =
                DecideNoPlanHoldAction(playerMoving, stopResult);
            holdingPosition =
                holdAction == NoPlanHoldAction::Hold;
            return;
        }

        if (!command.BeginControl()) return;
        const bool issuingPendingTarget = hasUsablePendingPlan;
        const bool emergencyMoveCadence =
            acquisition.firstActionableAcquisition ||
            acquisitionMoveRetryPending ||
            pendingTargetRetryPending;
        const MoveIssueResult moveResult = command.MoveTo(
            player,
            actionRoute.position,
            emergencyMoveCadence
                ? acquisition.moveMinimumIntervalMs
                : config.moveIntervalMs,
            config.moveRefreshMs,
            EndpointReachTolerance(config.planner.endpointBuffer),
            exactDanger);
        if (emergencyMoveCadence) {
            acquisitionMoveRetryPending =
                moveResult == MoveIssueResult::Throttled;
        }
        if (issuingPendingTarget) {
            const TargetCommitDecision targetCommit =
                DecideTargetCommit(
                    moveResult,
                    lockedHardValid &&
                        committedRoute.active);
            if (targetCommit.commitProposed) {
                const CandidateEvaluation acceptedTarget =
                    pendingTarget;
                const EvadeControllerState acceptedState =
                    pendingTargetState;
                CommittedRoutePolicyInput acceptedInput;
                acceptedInput.commitment = committedRoute;
                acceptedInput.threatSetFingerprint =
                    pendingTargetThreatSetFingerprint;
                acceptedInput.manualEpoch =
                    pendingTargetManualEpoch;
                acceptedInput.candidateAvailable =
                    CandidateHasRequiredStartEnvelopeExit(
                        acceptedTarget.startThreatIdentities.Size() > 0,
                        acceptedTarget.exitedStartEnvelope);
                acceptedInput.candidateStartsInThreat =
                    acceptedTarget.startThreatIdentities.Size() > 0;
                acceptedInput.candidateExitedStartEnvelope =
                    acceptedTarget.exitedStartEnvelope;
                acceptedInput.candidateSourceThreatId =
                    acceptedTarget.sourceThreatId;
                acceptedInput.candidateStabilityBranchKey =
                    acceptedTarget.stabilityBranchKey;
                acceptedInput.candidateDirection =
                    acceptedTarget.position - heroPos;
                acceptedInput.candidateSyntheticExtension =
                    pendingTargetSyntheticExtension;
                CommittedRouteDecision acceptedDecision;
                acceptedDecision.commitment = committedRoute;
                acceptedDecision.action =
                    pendingTargetSyntheticExtension
                    ? CommittedRouteAction::
                        ProposeSameDirectionExtension
                    : SameCommittedRouteBranch(
                        committedRoute,
                        acceptedTarget.sourceThreatId,
                        acceptedTarget.stabilityBranchKey,
                        acceptedInput.candidateDirection)
                    ? CommittedRouteAction::ProposeSameBranch
                    : CommittedRouteAction::ProposeBranchSwitch;
                committedRoute = CommitProposedRoute(
                    acceptedDecision,
                    acceptedInput);
                locked = acceptedTarget;
                lockedManualEpoch = pendingTargetManualEpoch;
                lastPlan.selected = locked;
                lastPlan.found = true;
                lastPlan.strictSafe = locked.strictSafe;
                lastTargetSwitchTick = now;
                degradationCommitUntilTick = 0;
                state = acceptedState;
                ClearPendingTarget();
                hasUsableLockedPlan = true;
            } else if (targetCommit.retryProposed) {
                pendingTargetRetryPending = true;
                hasUsableLockedPlan =
                    targetCommit.retainCommitted &&
                    hasUsableCommittedPlan;
                if (!hasUsableLockedPlan)
                    command.StopUnsafeMovement();
            } else if (!targetCommit.retryProposed) {
                ClearPendingTarget();
                hasUsableLockedPlan =
                    targetCommit.retainCommitted &&
                    hasUsableCommittedPlan;
                if (!hasUsableLockedPlan) {
                    lastPlanTick = 0;
                    command.StopUnsafeMovement();
                }
            }
        } else if (MoveResultInvalidatesLock(moveResult)) {
            locked.valid = false;
            committedRoute = {};
            lastPlanTick = 0;
            hasUsableLockedPlan = false;
            command.StopUnsafeMovement();
        }
        if (ShouldCancelUnsafeMovement(
                config.walkingEnabled && command.ControlActive(),
                hasUsableLockedPlan)) {
            command.StopUnsafeMovement();
        }
    }

    EvadeControllerState State() const { return state; }
    bool IsEvading() const {
        return state == EvadeControllerState::StrictEvade ||
               state == EvadeControllerState::FallbackEvade ||
               state == EvadeControllerState::ReroutingPath ||
               state == EvadeControllerState::Assessing;
    }
    const CandidateEvaluation& Locked() const { return locked; }
    const PlannerResult& LastPlan() const { return lastPlan; }
    const EvadeCommandEngine& Command() const { return command; }
    bool IsWaitingForWindup() const { return waitingForWindup; }
    bool IsHoldingPosition() const {
        return holdingPosition || releaseHolding;
    }

    bool HandleMoveRequest(const Vec2& destination,
                           MoveIntentSource source,
                           const EvadeRuntimeConfig& config) {
        const int now = SDK::Variables::TickCount();
        moveIntents.ExpireAdoption(now);
        if (source == MoveIntentSource::Controller) return false;
        if (!config.enabled || !config.walkingEnabled) {
            moveIntents.Clear();
            return false;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            moveIntents.Clear();
            return false;
        }
        const std::vector<Threat> threats = FilterThreats(
            ThreatDetector::Snapshot(),
            config.minimumDanger,
            player.HealthPercent(),
            config.threatRules,
            now);
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float heroRadius =
            SanitizeHeroRadius(player.BoundingRadius());
        const bool controllerOwnsMovement =
            command.ControlActive() || IsEvading();
        const bool pathAcquisitionDanger =
            EvadeGeometry::HeroThreatenedNow(
                threats,
                heroPos,
                heroRadius,
                ControlThreatBuffer(
                    false,
                    config.planner.pathBuffer,
                    config.planner.releaseBuffer),
                now,
                config.planner.maxThreatHorizonMs);
        const bool releaseMarginDanger =
            EvadeGeometry::HeroThreatenedNow(
                threats,
                heroPos,
                heroRadius,
                ControlThreatBuffer(
                    true,
                    config.planner.pathBuffer,
                    config.planner.releaseBuffer),
                now,
                config.planner.maxThreatHorizonMs);
        const bool exactDanger =
            EvadeGeometry::HeroThreatenedNow(
                threats,
                heroPos,
                heroRadius,
                0.0f,
                now,
                config.planner.maxThreatHorizonMs);
        const bool navInterventionArmed =
            IsNavigationInterventionArmed(
                controllerOwnsMovement,
                exactDanger,
                pathAcquisitionDanger,
                releaseMarginDanger);

        CandidateEvaluation movement;
        const bool hasValidDestination =
            destination.IsValid() && !destination.IsZero();
        if (hasValidDestination) {
            movement = EvadeGeometry::EvaluateCandidate(
                destination,
                PlannerCandidateSource::Cursor,
                -1,
                heroPos,
                destination,
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                heroRadius,
                now,
                config.planner,
                threats);
        }
        const CandidateEvaluation stationaryHold =
            EvadeGeometry::EvaluateStationaryCandidate(
                heroPos,
                destination,
                player.ServerPosition().y,
                heroRadius,
                now,
                config.planner,
                threats);
        const bool startsInThreat =
            movement.startThreatIdentities.Size() > 0;
        const bool coverageNoWorseThanHold =
            ThreatCoverageNoWorseAtResolution(
                CoverageOf(movement),
                CoverageOf(stationaryHold),
                std::max(
                    25.0f,
                    config.planner.temporalStepMs)) &&
            movement.dangerExposureMs <=
                stationaryHold.dangerExposureMs + 0.01f;
        const bool makesExitProgress =
            startsInThreat &&
            movement.exitedStartEnvelope &&
            movement.endpointSafe &&
            std::isfinite(movement.exitDistance) &&
            movement.exitDistance <= movement.travelDistance + 0.5f;
        ExternalMoveRouteEvaluation requestRoute;
        requestRoute.valid = movement.valid;
        requestRoute.walkable = movement.walkable;
        requestRoute.pathSafe = movement.pathSafe;
        requestRoute.endpointSafe = movement.endpointSafe;
        requestRoute.strictSafe = movement.strictSafe;
        requestRoute.reenteredDanger =
            movement.reenteredDanger;
        requestRoute.enteredNewThreat =
            movement.enteredNewThreat;
        requestRoute.startsInThreat = startsInThreat;
        requestRoute.coverageNoWorseThanHold =
            coverageNoWorseThanHold;
        requestRoute.makesExitProgress =
            makesExitProgress;
        const bool actionableThreatContext =
            navInterventionArmed ||
            ExternalMoveRouteHasActionableThreat(requestRoute);
        const ExternalMoveDecision moveDecision =
            DecideExternalMove({
                source,
                controllerOwnsMovement,
                actionableThreatContext,
                requestRoute,
            });
        const bool consumeRequest =
            moveDecision.consume || !moveDecision.allowNative;
        const ThreatFreeActionDecision threatFreeMove =
            DecideThreatFreeAction(
                actionableThreatContext,
                ThreatFreeDecisionSite::MoveRequest);
        if (threatFreeMove.applies) {
            if (threatFreeMove.clearIntents)
                moveIntents.Clear();
            releaseControlUntilTick = 0;
            holdingPosition = false;
            releaseHolding = false;
            waitingForWindup = false;
            if (threatFreeMove.releaseControl)
                HandleRelease();
            return consumeRequest;
        }
        if (moveDecision.discardBlockedIntent) {
            moveIntents.Clear();
            return consumeRequest;
        }

        if (!hasValidDestination || !moveDecision.adoptGoal)
            return consumeRequest;
        if (source == MoveIntentSource::Orbwalker &&
            moveIntents.HasManual()) {
            if (moveIntents.IsManualEcho(destination, now))
                return consumeRequest ||
                    moveIntents.Manual().SafetyBlocked();
            return true;
        }
        if (source == MoveIntentSource::ObservedPath &&
            moveIntents.HasManual()) {
            return false;
        }
        const RequestGenerationState nextGenerations =
            AdvanceRequestGenerations(
                {
                    moveRequestGeneration,
                    manualRequestGeneration,
                },
                source);
        moveRequestGeneration =
            nextGenerations.moveRequestGeneration;
        manualRequestGeneration =
            nextGenerations.manualRequestGeneration;
        if (source == MoveIntentSource::Manual) {
            acquisitionMoveRetryPending = false;
            ClearPendingTarget();
        }
        if (releaseControlUntilTick > now) {
            ReleaseDecisionInput cooldownDecision;
            cooldownDecision.currentThreatSerial =
                ThreatDetector::ChangeSerial();
            cooldownDecision.releaseThreatSerial = releaseThreatSerial;
            cooldownDecision.currentMoveRequestGeneration =
                moveRequestGeneration;
            cooldownDecision.releaseMoveRequestGeneration =
                releaseMoveRequestGeneration;
            if (MustBreakReleaseCooldown(cooldownDecision))
                releaseControlUntilTick = 0;
        }
        if (source == MoveIntentSource::Manual) {
            MoveRouteEvaluation route;
            route.evaluated = true;
            route.valid = movement.valid;
            route.walkable = movement.walkable;
            route.strictSafe = moveDecision.adoptGoal;
            const MoveIntentRecordResult manualResult =
                moveIntents.RecordManual(
                    destination,
                    now,
                    moveRequestGeneration,
                    route,
                    controllerOwnsMovement,
                    SDK::Game::Ping());
            if (manualResult == MoveIntentRecordResult::SafeManual) {
                ReleaseForSafeManual();
                return false;
            }
            return manualResult == MoveIntentRecordResult::Deferred ||
                manualResult == MoveIntentRecordResult::Blocked;
        }
        const MoveIntentRecordResult recordResult = moveIntents.Record(
            destination,
            source,
            now,
            moveRequestGeneration,
            false);
        return consumeRequest ||
            recordResult == MoveIntentRecordResult::Deferred;
    }

private:
    EvadeControllerState state = EvadeControllerState::Idle;
    EvadeCommandEngine command;
    EvadeSpellEngine spellEngine;
    CandidateEvaluation locked;
    CandidateEvaluation pendingTarget;
    EvadeControllerState pendingTargetState =
        EvadeControllerState::Assessing;
    std::uint64_t pendingTargetManualEpoch = 0;
    std::uint64_t pendingTargetThreatSetFingerprint = 0;
    bool pendingTargetSyntheticExtension = false;
    bool pendingTargetRetryPending = false;
    PlannerResult lastPlan;
    int lastThreatSerial = -1;
    int lastPlanTick = 0;
    std::uint64_t lastPlannedThreatSetFingerprint = 0;
    bool hasPlannedThreatSetFingerprint = false;
    NoPlanRetrySchedule noPlanRetry;
    ContinuousChallengerState continuousChallenger;
    CommittedRouteIdentity committedRoute;
    int lastTargetSwitchTick = 0;
    int degradationCommitUntilTick = 0;
    int spellHoldUntilTick = 0;
    int spellHoldActivationTick = 0;
    HoldProtectionKind spellHoldProtection = HoldProtectionKind::None;
    int releaseControlUntilTick = 0;
    int releaseThreatSerial = -1;
    std::uint64_t moveRequestGeneration = 0;
    std::uint64_t manualRequestGeneration = 0;
    std::uint64_t releaseMoveRequestGeneration = 0;
    std::uint64_t lockedManualEpoch = 0;
    int handledAcquisitionSerial = -1;
    bool acquisitionMoveRetryPending = false;
    bool waitingForWindup = false;
    bool holdingPosition = false;
    bool releaseHolding = false;
    MoveIntentState moveIntents;

    struct AcquisitionSerialCommit {
        int* handledSerial = nullptr;
        int serial = -1;
        bool pending = false;

        ~AcquisitionSerialCommit() {
            if (pending && handledSerial)
                *handledSerial = serial;
        }
    };

    static std::vector<Threat> FilterThreats(const std::vector<Threat>& input,
                                             int minimumDanger,
                                             float healthPercent,
                                             const ThreatRuleMap* rules,
                                             int now) {
        std::vector<Threat> output;
        output.reserve(input.size());
        for (const auto& threat : input) {
            if (threat.IsExpiredAt(now)) continue;
            Threat filtered = threat;
            bool hasRule = false;
            if (rules) {
                const auto rule = rules->find(threat.SpellName());
                if (rule != rules->end()) {
                    hasRule = true;
                    if (!rule->second.enabled || healthPercent > rule->second.dodgeHealthPercent) continue;
                    filtered.dangerOverride = std::clamp(rule->second.danger, 1, 4);
                }
            }
            if (!hasRule && threat.DefaultOff()) continue;
            if (filtered.Danger() >= std::max(1, minimumDanger)) output.push_back(std::move(filtered));
        }
        return output;
    }

    static std::uint64_t ThreatFingerprintOf(
        const std::vector<Threat>& threats) {
        return StableSemanticThreatSetFingerprint(threats);
    }

    static std::vector<Vec2> BuildCurrentPath(const SDK::AIHeroClient& player,
                                              const Vec2& fallbackEnd) {
        const Vec2 start = player.ServerPosition().To2D();
        const auto& cachedWaypoints = player.CachedWaypoints();
        std::vector<Vec2> waypoints;
        waypoints.reserve(cachedWaypoints.size());
        for (const auto& waypoint : cachedWaypoints) {
            waypoints.push_back(waypoint.To2D());
        }
        return NormalizeObservedWaypoints(
            start,
            waypoints,
            fallbackEnd);
    }

    static ThreatCoverage CoverageOf(const CandidateEvaluation& evaluation) {
        return {
            evaluation.collisionCount,
            evaluation.endpointDanger,
            evaluation.pathDanger,
            evaluation.maxDanger,
            evaluation.dangerExposureMs,
            evaluation.firstCollisionTimeMs,
            evaluation.summedExposureDanger,
        };
    }

    static bool MakesDeterministicProgress(
        const CandidateEvaluation& candidate,
        const CandidateEvaluation& baseline,
        const EvadeSettings& settings) {
        (void)settings;
        if (!candidate.valid ||
            !candidate.walkable ||
            candidate.travelDistance < 1.0f ||
            candidate.enteredNewThreat ||
            candidate.reenteredDanger) {
            return false;
        }
        if (candidate.startThreatIdentities.Size() > 0 &&
            !candidate.exitedStartEnvelope) {
            return false;
        }
        const float baselineExit =
            (baseline.startThreatIdentities.Size() == 0 ||
             baseline.exitedStartEnvelope) &&
                std::isfinite(baseline.exitDistance)
            ? baseline.exitDistance
            : std::numeric_limits<float>::infinity();
        return std::isfinite(candidate.exitDistance) &&
            candidate.exitDistance + 0.5f < baselineExit;
    }

    static StableRouteMetrics StableMetrics(
        const CandidateEvaluation& evaluation) {
        return {
            CoverageOf(evaluation),
            evaluation.strictSafe,
            evaluation.exitedStartEnvelope,
            evaluation.minimumClearance,
            evaluation.timeMarginMs,
            evaluation.exitDistance,
            evaluation.travelDistance,
            evaluation.cursorDistance,
            evaluation.turretPenalty,
        };
    }

    struct CommittedBranchCandidate {
        CandidateEvaluation evaluation;
        bool syntheticExtension = false;
        bool extensionAttempted = false;
    };

    CandidateEvaluation FindAlternateBranchCandidate(
        const PlannerResult& plan,
        const CommittedRouteIdentity& releasedIdentity,
        const ThreatCoverage& baselineCoverage,
        const SDK::AIHeroClient& player,
        const EvadeRuntimeConfig& config) const {
        CandidateEvaluation best;
        const Vec2 hero = player.ServerPosition().To2D();
        const float temporalResolutionMs = std::max(
            25.0f,
            config.planner.temporalStepMs);
        for (const CandidateEvaluation& candidate :
             plan.candidates) {
            if (!candidate.valid ||
                !candidate.walkable ||
                !CandidateHasRequiredStartEnvelopeExit(
                    candidate.startThreatIdentities.Size() > 0,
                    candidate.exitedStartEnvelope) ||
                candidate.position.IsZero() ||
                (!candidate.strictSafe &&
                 !config.leastDangerFallback) ||
                (!candidate.strictSafe &&
                 !ThreatCoverageNoWorseAtResolution(
                     CoverageOf(candidate),
                     baselineCoverage,
                     temporalResolutionMs)) ||
                SameCommittedRouteBranch(
                    releasedIdentity,
                    candidate.sourceThreatId,
                    candidate.stabilityBranchKey,
                    candidate.position - hero) ||
                IsMoveTargetReached(
                    hero.Distance(candidate.position),
                    EndpointReachTolerance(
                        config.planner.endpointBuffer),
                    false)) {
                continue;
            }
            const bool better =
                !best.valid ||
                (candidate.strictSafe && !best.strictSafe) ||
                (candidate.strictSafe == best.strictSafe &&
                 (MateriallyImprovesThreatCoverage(
                      CoverageOf(candidate),
                      CoverageOf(best),
                      temporalResolutionMs) ||
                  (EquivalentThreatCoverageAtResolution(
                       CoverageOf(candidate),
                       CoverageOf(best),
                       temporalResolutionMs) &&
                   candidate.travelDistance <
                       best.travelDistance)));
            if (better) best = candidate;
        }
        return best;
    }

    CommittedBranchCandidate FindCommittedBranchCandidate(
        const PlannerResult& plan,
        const CandidateEvaluation& current,
        const ThreatCoverage& baselineCoverage,
        const SDK::AIHeroClient& player,
        const Vec2& goal,
        const std::vector<Threat>& threats,
        const EvadeRuntimeConfig& config,
        int now,
        bool reached) const {
        CommittedBranchCandidate result;
        const Vec2 hero = player.ServerPosition().To2D();
        const auto consider = [&](const CandidateEvaluation& candidate,
                                  bool syntheticExtension) {
            if (!candidate.valid ||
                !candidate.walkable ||
                !CandidateHasRequiredStartEnvelopeExit(
                    candidate.startThreatIdentities.Size() > 0,
                    candidate.exitedStartEnvelope) ||
                candidate.position.IsZero() ||
                (!candidate.strictSafe &&
                 !config.leastDangerFallback) ||
                !SameCommittedRouteBranch(
                    committedRoute,
                    candidate.sourceThreatId,
                    candidate.stabilityBranchKey,
                    candidate.position - hero)) {
                return;
            }
            const float temporalResolutionMs = std::max(
                25.0f,
                config.planner.temporalStepMs);
            if (!candidate.strictSafe &&
                !ThreatCoverageNoWorseAtResolution(
                    CoverageOf(candidate),
                    baselineCoverage,
                    temporalResolutionMs)) {
                return;
            }
            if (reached &&
                IsMoveTargetReached(
                    hero.Distance(candidate.position),
                    EndpointReachTolerance(
                        config.planner.endpointBuffer),
                    false)) {
                return;
            }
            const CandidateEvaluation& best =
                result.evaluation;
            const bool equivalentCoverage =
                best.valid &&
                EquivalentThreatCoverageAtResolution(
                    CoverageOf(candidate),
                    CoverageOf(best),
                    temporalResolutionMs);
            const bool betterTrueExit =
                equivalentCoverage &&
                candidate.exitedStartEnvelope &&
                (!best.exitedStartEnvelope ||
                 candidate.exitDistance + 0.5f <
                     best.exitDistance);
            const bool equalTrueExit =
                equivalentCoverage &&
                candidate.exitedStartEnvelope ==
                    best.exitedStartEnvelope &&
                (!candidate.exitedStartEnvelope ||
                 std::fabs(
                     candidate.exitDistance -
                     best.exitDistance) <= 0.5f);
            const bool better =
                !best.valid ||
                (candidate.strictSafe && !best.strictSafe) ||
                (candidate.strictSafe == best.strictSafe &&
                 (MateriallyImprovesThreatCoverage(
                      CoverageOf(candidate),
                      CoverageOf(best),
                      std::max(
                          25.0f,
                          config.planner.temporalStepMs)) ||
                  betterTrueExit ||
                  (equalTrueExit &&
                   candidate.travelDistance <
                       best.travelDistance)));
            if (better) {
                result.evaluation = candidate;
                result.syntheticExtension =
                    syntheticExtension;
            }
        };
        if (!reached) {
            for (const CandidateEvaluation& candidate :
                 plan.candidates) {
                consider(candidate, false);
            }
            return result;
        }
        result.extensionAttempted = true;
        if (committedRoute.normalizedDirection.IsZero())
            return result;

        const float heroRadius =
            SanitizeHeroRadius(player.BoundingRadius());
        const Vec2 extensionTarget =
            BuildCommittedRouteExtensionTarget(
                hero,
                committedRoute,
                heroRadius,
                config.planner.endpointBuffer,
                config.planner.ringStep,
                config.planner.maxSearchRadius);
        if (extensionTarget.IsZero()) return result;
        CandidateEvaluation extension =
            EvadeGeometry::EvaluateCandidate(
                extensionTarget,
                current.source,
                committedRoute.sourceThreatId,
                hero,
                goal,
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                heroRadius,
                now,
                config.planner,
                threats);
        extension.sourceThreatId =
            committedRoute.sourceThreatId;
        extension.stabilityBranchKey =
            committedRoute.stabilityBranchKey;
        extension.enemyDistance = current.enemyDistance;
        extension.turretPenalty = current.turretPenalty;
        consider(extension, true);
        return result;
    }

    CandidateEvaluation SelectStableTarget(
        const CandidateEvaluation& proposed,
        const CandidateEvaluation& current,
        bool targetLockActive,
        bool degradationCommitActive,
        const SDK::AIHeroClient& player,
        const Vec2& goal,
        const std::vector<Threat>& threats,
        const EvadeRuntimeConfig& config,
        bool exactDanger) const {
        CandidateEvaluation chosen = proposed;
        if (!current.valid || !current.walkable || current.position.IsZero())
            return chosen;
        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = SanitizeHeroRadius(player.BoundingRadius());
        if (IsRouteTargetReached(
                hero.Distance(current.position),
                config.planner.endpointBuffer,
                exactDanger)) {
            return chosen;
        }
        const Vec2 oldDirection = (current.position - hero).Normalized();
        const Vec2 newDirection = (chosen.position - hero).Normalized();
        if (oldDirection.IsZero() || newDirection.IsZero()) return chosen;
        const float alignment = oldDirection.Dot(newDirection);
        if (KeepStableRoute(
                StableMetrics(current),
                StableMetrics(chosen),
                alignment,
                targetLockActive,
                degradationCommitActive)) {
            return current;
        }
        if (alignment <= -0.10f) return chosen;

        const float alpha = std::clamp(
            targetLockActive
                ? config.planner.lockedDirectionBlend
                : config.planner.directionBlend,
            0.0f,
            1.0f);
        const Vec2 blendedDirection =
            (oldDirection * (1.0f - alpha) + newDirection * alpha).Normalized();
        const float blendedDistance =
            current.travelDistance * (1.0f - alpha) +
            chosen.travelDistance * alpha;
        CandidateEvaluation blended = EvadeGeometry::EvaluateCandidate(
            hero + blendedDirection * blendedDistance,
            chosen.source,
            chosen.sourceThreatId,
            hero,
            goal,
            player.ServerPosition().y,
            std::max(50.0f, player.MoveSpeed()),
            radius,
            SDK::Variables::TickCount(),
            config.planner,
            threats);
        if (!blended.valid || !blended.walkable ||
            hero.Distance(blended.position) <= radius ||
            ImprovesThreatCoverage(
                CoverageOf(chosen),
                CoverageOf(blended)) ||
            (chosen.strictSafe && !blended.strictSafe)) {
            return chosen;
        }
        blended.enemyDistance =
            current.enemyDistance * (1.0f - alpha) +
            chosen.enemyDistance * alpha;
        blended.turretPenalty =
            current.turretPenalty * (1.0f - alpha) +
            chosen.turretPenalty * alpha;
        CarryStabilityBranchKey(blended, chosen);
        return blended;
    }

    bool ShouldHoldPosition(
        bool directDanger,
        bool pathDanger,
        const SDK::AIHeroClient& player,
        const Vec2& hero,
        const Vec2& goal,
        float radius,
        const std::vector<Vec2>& observedPath,
        const std::vector<Threat>& threats,
        const EvadeRuntimeConfig& config,
        bool hasUsablePlan) const {
        if (directDanger || !pathDanger) return false;
        const int holdWindow = std::clamp(
            SaturatingDurationAdd(
                ClampTickOffset(std::round(config.planner.inputDelayMs)),
                std::max(0, SDK::Game::Ping())),
            config.planner.nearWallHoldMinimumMs,
            std::max(
                config.planner.nearWallHoldMinimumMs,
                config.planner.nearWallHoldMaximumMs));
        if (EvadeGeometry::HeroThreatenedNow(
                threats,
                hero,
                radius,
                config.planner.endpointBuffer,
                SDK::Variables::TickCount(),
                static_cast<float>(holdWindow))) {
            return false;
        }
        const float probeDistance = std::max(140.0f, radius + 100.0f);
        const NavigationProbe wall = ProbeNavigation(
            hero,
            player.ServerPosition().y,
            probeDistance);
        if (wall.blockedRays <= 0 || wall.clearance >= radius + 75.0f)
            return false;
        if (holdingPosition) return true;
        if (!hasUsablePlan || !locked.strictSafe) return true;

        Vec2 intendedDirection;
        if (observedPath.size() >= 2)
            intendedDirection = (observedPath[1] - hero).Normalized();
        if (intendedDirection.IsZero())
            intendedDirection = (goal - hero).Normalized();
        const Vec2 evadeDirection = (locked.position - hero).Normalized();
        if (intendedDirection.IsZero() || evadeDirection.IsZero()) return true;
        const NavigationProbe targetWall = ProbeNavigation(
            locked.position,
            player.ServerPosition().y,
            probeDistance);
        const bool sharpDetour =
            intendedDirection.Dot(evadeDirection) < 0.30f;
        const bool crampedExit =
            targetWall.clearance < radius + 30.0f;
        const bool headsIntoWall = !wall.escapeDirection.IsZero() &&
            evadeDirection.Dot(wall.escapeDirection) < -0.15f;
        return sharpDetour || crampedExit || headsIntoWall;
    }

    LockedRouteValidation ValidateLocked(
        const SDK::AIHeroClient& player,
        const std::vector<Threat>& threats,
        const EvadeRuntimeConfig& config,
        int now,
        const ThreatCoverage& baselineCoverage,
        float temporalResolutionMs,
        CandidateEvaluation* evaluationOut) const {
        const bool hasValidLock =
            locked.valid &&
            locked.position.IsValid() &&
            !locked.position.IsZero();
        const float targetDistance = hasValidLock
            ? player.ServerPosition().To2D().Distance(
                locked.position)
            : FLT_MAX;
        const LockedRouteValidation endpointValidation =
            ClassifyLockedEndpointBoundary({
                baselineCoverage,
                hasValidLock,
                locked.walkable,
                targetDistance,
                EndpointReachTolerance(
                    config.planner.endpointBuffer),
                temporalResolutionMs,
                locked.startThreatIdentities.Size() > 0,
                locked.exitedStartEnvelope,
            });
        if (endpointValidation.reached) {
            CandidateEvaluation current;
            current.position = locked.position;
            current.source = locked.source;
            current.sourceThreatId =
                locked.sourceThreatId;
            current.stabilityBranchKey =
                locked.stabilityBranchKey;
            current.valid = true;
            current.walkable = true;
            current.endpointDanger =
                baselineCoverage.endpointDanger;
            current.pathDanger =
                baselineCoverage.pathDanger;
            current.maxDanger =
                baselineCoverage.maxDanger;
            current.collisionCount =
                baselineCoverage.collisionCount;
            current.firstCollisionTimeMs =
                baselineCoverage.firstCollisionTimeMs;
            current.dangerExposureMs =
                baselineCoverage.dangerExposureMs;
            current.summedExposureDanger =
                baselineCoverage.summedExposureDanger;
            current.travelDistance = targetDistance;
            current.arrivalTimeMs = 0.0f;
            current.enemyDistance = locked.enemyDistance;
            current.turretPenalty = locked.turretPenalty;
            current.rejectReason =
                PlannerRejectReason::None;
            if (evaluationOut) *evaluationOut = current;
            return endpointValidation;
        }
        if (!hasValidLock || !locked.walkable)
            return endpointValidation;
        const bool followsLockedPath = player.HasPath() &&
            player.PathEnd().To2D().Distance(locked.position) <= 80.0f;
        const std::vector<Vec2> currentPath = followsLockedPath
            ? BuildCurrentPath(player, locked.position)
            : std::vector<Vec2>();
        CandidateEvaluation current = currentPath.size() >= 2
            ? EvadeGeometry::EvaluatePathCandidate(
                currentPath,
                locked.source,
                locked.sourceThreatId,
                SDK::Game::CursorPos().To2D(),
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                SanitizeHeroRadius(player.BoundingRadius()),
                now,
                config.planner,
                threats,
                nullptr,
                locked.stabilityBranchKey)
            : EvadeGeometry::EvaluateCandidate(
                locked.position,
                locked.source,
                locked.sourceThreatId,
                player.ServerPosition().To2D(),
                SDK::Game::CursorPos().To2D(),
                player.ServerPosition().y,
                std::max(50.0f, player.MoveSpeed()),
                SanitizeHeroRadius(player.BoundingRadius()),
                now,
                config.planner,
                threats);
        current.enemyDistance = locked.enemyDistance;
        current.turretPenalty = locked.turretPenalty;
        CarryStabilityBranchKey(current, locked);
        current.position = locked.position;
        if (evaluationOut) *evaluationOut = current;
        return ClassifyLockedRoute({
            CoverageOf(current),
            baselineCoverage,
            true,
            current.valid,
            current.walkable,
            false,
            false,
            current.strictSafe,
            temporalResolutionMs,
            current.startThreatIdentities.Size() > 0,
            current.exitedStartEnvelope,
        });
    }

    void ClearPendingTarget() {
        pendingTarget = {};
        pendingTargetState = EvadeControllerState::Assessing;
        pendingTargetManualEpoch = 0;
        pendingTargetThreatSetFingerprint = 0;
        pendingTargetSyntheticExtension = false;
        pendingTargetRetryPending = false;
        continuousChallenger = {};
    }

    void HandleRelease() {
        ClearSpellHold();
        if (state == EvadeControllerState::Idle && !command.ControlActive()) return;
        command.EndControl();
        state = EvadeControllerState::Idle;
        locked = {};
        committedRoute = {};
        ClearPendingTarget();
        lastPlan = {};
        noPlanRetry = {};
        acquisitionMoveRetryPending = false;
        lastPlannedThreatSetFingerprint = 0;
        hasPlannedThreatSetFingerprint = false;
        lockedManualEpoch = 0;
        degradationCommitUntilTick = 0;
        waitingForWindup = false;
        holdingPosition = false;
        releaseHolding = false;
        lastThreatSerial = ThreatDetector::ChangeSerial();
    }

    void ReleaseForSafeManual() {
        ClearSpellHold();
        command.EndControl();
        state = EvadeControllerState::Idle;
        locked = {};
        committedRoute = {};
        ClearPendingTarget();
        lastPlan = {};
        lastPlanTick = 0;
        lastPlannedThreatSetFingerprint = 0;
        hasPlannedThreatSetFingerprint = false;
        noPlanRetry = {};
        acquisitionMoveRetryPending = false;
        lockedManualEpoch = 0;
        lastTargetSwitchTick = 0;
        degradationCommitUntilTick = 0;
        releaseControlUntilTick = 0;
        releaseThreatSerial = -1;
        releaseMoveRequestGeneration = moveRequestGeneration;
        waitingForWindup = false;
        holdingPosition = false;
        releaseHolding = false;
        lastThreatSerial = ThreatDetector::ChangeSerial();
    }

    void ClearSpellHold() {
        spellHoldUntilTick = 0;
        spellHoldActivationTick = 0;
        spellHoldProtection = HoldProtectionKind::None;
    }
};

}
