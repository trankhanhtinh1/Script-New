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
        lastPlan = {};
        lastThreatSerial = -1;
        lastPlanTick = 0;
        lastPlannedThreatSetFingerprint = 0;
        hasPlannedThreatSetFingerprint = false;
        noPlanRetry = {};
        continuousChallenger = {};
        lastTargetSwitchTick = 0;
        degradationCommitUntilTick = 0;
        releaseControlUntilTick = 0;
        releaseThreatSerial = -1;
        moveRequestGeneration = 0;
        manualRequestGeneration = 0;
        releaseMoveRequestGeneration = 0;
        lockedManualEpoch = 0;
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
        const float heroRadius = std::max(10.0f, player.BoundingRadius());
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
                unsafeCurrentPath = IsObservedRouteUnsafe(
                    observedPath.size(),
                    observedEvaluation,
                    navInterventionArmed);
                actionableObservedRouteThreat =
                    IsObservedThreatRouteUnsafe(
                        observedPath.size(),
                        observedEvaluation);
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
                unsafeCurrentPath = IsObservedRouteUnsafe(
                    observedPath.size(),
                    heldEvaluation,
                    navInterventionArmed);
                actionableObservedRouteThreat =
                    actionableObservedRouteThreat ||
                    IsObservedThreatRouteUnsafe(
                        observedPath.size(),
                        heldEvaluation);
            }
        }
        const bool actionableThreatContext =
            navInterventionArmed || actionableObservedRouteThreat;
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
            IsRouteTargetReached(heroPos.Distance(locked.position));
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

        if (unsafeCurrentPath && !moveIntents.HasDeferred()) {
            const Vec2 pathEnd = player.PathEnd().To2D();
            if (pathEnd.IsValid() && !pathEnd.IsZero()) {
                const bool matchesCommandTarget =
                    command.ControlActive() &&
                    !command.LastTarget().IsZero() &&
                    pathEnd.DistanceSqr(command.LastTarget()) <= 3600.0f;
                const bool matchesLockedTarget =
                    !locked.position.IsZero() &&
                    pathEnd.DistanceSqr(locked.position) <= 6400.0f;
                if (!matchesCommandTarget && !matchesLockedTarget) {
                    moveIntents.Record(
                        pathEnd,
                        MoveIntentSource::ObservedPath,
                        now,
                        moveRequestGeneration,
                        true);
                }
            }
        }

        const Vec2 plannerGoal = moveIntents.HasDeferred()
            ? moveIntents.Deferred().Position()
            : moveIntents.HasGoal()
                ? moveIntents.Goal().Position()
                : SDK::Game::CursorPos().To2D();
        CandidateEvaluation currentLocked;
        const bool lockedHardValid = ValidateLocked(
            player,
            threats,
            config,
            now,
            &currentLocked);
        const Vec2 lockedTarget = locked.position;
        const LockedRouteValidation lockedValidation =
            ClassifyLockedRoute({
                CoverageOf(currentLocked),
                baselineCoverage,
                locked.valid,
                lockedHardValid && currentLocked.valid,
                currentLocked.walkable,
                !lockedTarget.IsZero() &&
                    IsRouteTargetReached(
                        heroPos.Distance(lockedTarget)),
                false,
                currentLocked.strictSafe,
                routeTemporalResolutionMs,
            });
        const bool lockedSafetyValid =
            lockedValidation.safety != LockedRouteSafety::Unsafe;
        if (!lockedSafetyValid ||
            lockedManualEpoch != manualRequestGeneration) {
            continuousChallenger = {};
        }
        bool pendingTargetUsable = false;
        if (pendingTarget.valid) {
            const Vec2 proposedPosition = pendingTarget.position;
            const bool pendingEpochValid =
                pendingTargetManualEpoch == manualRequestGeneration;
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
                        heroPos.Distance(proposedPosition)),
                    false,
                    refreshedPending.strictSafe,
                    routeTemporalResolutionMs,
                });
            if (pendingEpochValid &&
                pendingValidation.safety !=
                    LockedRouteSafety::Unsafe) {
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
                IsRouteTargetReached(
                    heroPos.Distance(locked.position)),
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
                    config.moveRefreshMs);
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
        const bool shouldReplan = !committedStrictLock &&
            !promotedFallback &&
            !pendingTargetUsable &&
            planningDue;

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
            if (plan.found && lockedSafetyValid) {
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
                        config);
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
            unavoidableInput.fallbackLockActive =
                fallbackLockWasActive;
            unavoidableInput.lockCoverage =
                CoverageOf(currentLocked);
            unavoidableInput.lockValid = lockedHardValid;
            unavoidableInput.lockWalkable =
                currentLocked.walkable;
            unavoidableInput.lockReached =
                !locked.position.IsZero() &&
                IsRouteTargetReached(
                    heroPos.Distance(locked.position));
            unavoidableInput.currentManualEpoch =
                manualRequestGeneration;
            unavoidableInput.lockManualEpoch =
                lockedManualEpoch;
            const UnavoidableDecision unavoidable =
                DecideUnavoidableAction(unavoidableInput);
            bool commitPlanInPlace = false;
            if (unavoidable.retainLockedFallback) {
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
                const bool switched = !locked.valid ||
                    locked.position.DistanceSqr(plan.selected.position) >
                        config.planner.targetSwitchDistance *
                        config.planner.targetSwitchDistance;
                if (switched) {
                    const bool requiresHysteresis =
                        RequiresContinuousSwitchHysteresis(
                            lockedSafetyValid,
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
                if (lockedSafetyValid) {
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
        const bool hasUsableCommittedPlan =
            config.walkingEnabled &&
            !moveIntents.BlocksControllerTarget() &&
            lastPlan.found &&
            locked.valid &&
            locked.walkable &&
            (committedStrictLock ||
             retainedFallbackLock ||
             newlyAdmissibleLock) &&
            (locked.strictSafe || config.leastDangerFallback);
        const bool hasUsablePendingPlan =
            config.walkingEnabled &&
            !moveIntents.BlocksControllerTarget() &&
            pendingTargetUsable &&
            pendingTarget.valid &&
            pendingTarget.walkable;
        bool hasUsableLockedPlan =
            hasUsableCommittedPlan ||
            hasUsablePendingPlan;
        const CandidateEvaluation& actionRoute =
            hasUsablePendingPlan ? pendingTarget : locked;
        if (shouldReplan) {
            noPlanRetry = hasUsableLockedPlan
                ? NoPlanRetrySchedule{}
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
        if (windupRemaining > 0 &&
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
        if (endangered && config.evadeSpellsEnabled && weakWalkingPlan) {
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
            if (baselineUsesNative) {
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
        const MoveIssueResult moveResult = command.MoveTo(
            player,
            actionRoute.position,
            config.moveIntervalMs,
            config.moveRefreshMs);
        if (issuingPendingTarget) {
            const TargetCommitDecision targetCommit =
                DecideTargetCommit(
                    moveResult,
                    lockedSafetyValid);
            if (targetCommit.commitProposed) {
                const CandidateEvaluation acceptedTarget =
                    pendingTarget;
                const EvadeControllerState acceptedState =
                    pendingTargetState;
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
            std::max(10.0f, player.BoundingRadius());
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
        const ObservedRouteEvaluation requestRoute = {
            hasValidDestination,
            movement.valid,
            movement.walkable,
            movement.pathSafe,
            movement.endpointSafe,
        };
        const bool actionableThreatContext =
            navInterventionArmed ||
            IsObservedThreatRouteUnsafe(2, requestRoute);
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
            return !threatFreeMove.allowNativeInput;
        }

        if (!hasValidDestination) {
            return source == MoveIntentSource::Manual &&
                controllerOwnsMovement;
        }
        if (source == MoveIntentSource::Orbwalker &&
            moveIntents.HasManual()) {
            if (moveIntents.IsManualEcho(destination, now))
                return moveIntents.Manual().SafetyBlocked();
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
        if (source == MoveIntentSource::Manual)
            ClearPendingTarget();
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
            route.strictSafe = movement.strictSafe;
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
        const bool unsafe = movement.valid && movement.walkable &&
            (!movement.pathSafe || !movement.endpointSafe);
        const bool safetyBlocked = unsafe ||
            holdingPosition ||
            releaseHolding ||
            waitingForWindup;
        const MoveIntentRecordResult recordResult = moveIntents.Record(
            destination,
            source,
            now,
            moveRequestGeneration,
            safetyBlocked);
        return recordResult == MoveIntentRecordResult::Deferred;
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
    PlannerResult lastPlan;
    int lastThreatSerial = -1;
    int lastPlanTick = 0;
    std::uint64_t lastPlannedThreatSetFingerprint = 0;
    bool hasPlannedThreatSetFingerprint = false;
    NoPlanRetrySchedule noPlanRetry;
    ContinuousChallengerState continuousChallenger;
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
    bool waitingForWindup = false;
    bool holdingPosition = false;
    bool releaseHolding = false;
    MoveIntentState moveIntents;

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
        std::vector<int> ids;
        ids.reserve(threats.size());
        for (const Threat& threat : threats)
            ids.push_back(threat.id);
        return StableThreatSetFingerprint(ids);
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
        if (!candidate.valid ||
            !candidate.walkable ||
            candidate.travelDistance < 1.0f) {
            return false;
        }
        const float baselineExit = std::isfinite(baseline.exitDistance)
            ? baseline.exitDistance
            : settings.maxSearchRadius;
        return std::isfinite(candidate.exitDistance) &&
            candidate.exitDistance + 0.5f < baselineExit;
    }

    static StableRouteMetrics StableMetrics(
        const CandidateEvaluation& evaluation) {
        return {
            CoverageOf(evaluation),
            evaluation.strictSafe,
            evaluation.minimumClearance,
            evaluation.timeMarginMs,
            evaluation.exitDistance,
            evaluation.travelDistance,
            evaluation.cursorDistance,
            evaluation.turretPenalty,
        };
    }

    CandidateEvaluation SelectStableTarget(
        const CandidateEvaluation& proposed,
        const CandidateEvaluation& current,
        bool targetLockActive,
        bool degradationCommitActive,
        const SDK::AIHeroClient& player,
        const Vec2& goal,
        const std::vector<Threat>& threats,
        const EvadeRuntimeConfig& config) const {
        CandidateEvaluation chosen = proposed;
        if (!current.valid || !current.walkable || current.position.IsZero())
            return chosen;
        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(10.0f, player.BoundingRadius());
        if (IsRouteTargetReached(
                hero.Distance(current.position))) {
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

    bool ValidateLocked(const SDK::AIHeroClient& player,
                        const std::vector<Threat>& threats,
                        const EvadeRuntimeConfig& config,
                        int now,
                        CandidateEvaluation* evaluationOut) const {
        if (!locked.valid || !locked.walkable || locked.position.IsZero()) return false;
        if (IsRouteTargetReached(
                player.ServerPosition().To2D().Distance(
                    locked.position))) {
            return false;
        }
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
                std::max(10.0f, player.BoundingRadius()),
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
                std::max(10.0f, player.BoundingRadius()),
                now,
                config.planner,
                threats);
        current.enemyDistance = locked.enemyDistance;
        current.turretPenalty = locked.turretPenalty;
        CarryStabilityBranchKey(current, locked);
        current.position = locked.position;
        if (evaluationOut) *evaluationOut = current;
        return current.valid && current.walkable;
    }

    void ClearPendingTarget() {
        pendingTarget = {};
        pendingTargetState = EvadeControllerState::Assessing;
        pendingTargetManualEpoch = 0;
        continuousChallenger = {};
    }

    void HandleRelease() {
        ClearSpellHold();
        if (state == EvadeControllerState::Idle && !command.ControlActive()) return;
        command.EndControl();
        state = EvadeControllerState::Idle;
        locked = {};
        ClearPendingTarget();
        lastPlan = {};
        noPlanRetry = {};
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
        ClearPendingTarget();
        lastPlan = {};
        lastPlanTick = 0;
        lastPlannedThreatSetFingerprint = 0;
        hasPlannedThreatSetFingerprint = false;
        noPlanRetry = {};
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
