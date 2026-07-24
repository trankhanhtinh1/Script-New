#pragma once

#include "../../../Core/CoreControl.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../SDK/SDK.h"
#include "../../../SDK/Wrappers/Orbwalking/Orbwalker.h"
#include "../Detection/Threat.h"
#include "EvadeMoveResultAdapter.h"

#include <algorithm>

namespace ZDEvade {

class EvadeCommandEngine {
public:
    ~EvadeCommandEngine() {
        EndControl();
        if (ownerToken) {
            CoreEvadeState::ReleaseOwner(ownerToken);
            ownerToken = {};
        }
    }

    EvadeCommandEngine() = default;
    EvadeCommandEngine(const EvadeCommandEngine&) = delete;
    EvadeCommandEngine& operator=(const EvadeCommandEngine&) = delete;

    void Reset(int stopThrottleSafeGraceMs = 180) {
        const int retainedStopTick = StopThrottleTickAfterReset(
            StopThrottleResetMode::FullReset,
            SDK::Variables::TickCount(),
            lastStopTick,
            stopThrottleSafeGraceMs);
        EndControl();
        ResetMovementState();
        lastStopTick = retainedStopTick;
    }

    bool BeginControl(bool allowAttacks = false) {
        const int now = SDK::Variables::TickCount();
        const int comboUntilTick = allowAttacks
            ? 0
            : SaturatingTickAdd(now, 150);
        if (!SetOwnedControlState(
                true,
                !allowAttacks,
                comboUntilTick)) {
            return false;
        }

        if (!controlActive) {
            controlledOrbwalker = SDK::Orbwalker::Implementation();
            if (controlledOrbwalker) {
                previousMoveEnabled = controlledOrbwalker->MoveEnabled();
            }
            const AttackControlDecision attack = DecideAttackControl(
                false,
                previousAttackEnabled,
                controlledOrbwalker
                    ? controlledOrbwalker->AttackEnabled()
                    : false,
                allowAttacks);
            previousAttackEnabled = attack.baselineAttackEnabled;
        }

        imposedMoveEnabled = false;
        imposedAttackEnabled = DecideAttackControl(
            true,
            previousAttackEnabled,
            controlledOrbwalker &&
                    SDK::Orbwalker::Implementation() == controlledOrbwalker
                ? controlledOrbwalker->AttackEnabled()
                : previousAttackEnabled,
            allowAttacks).imposedAttackEnabled;
        if (controlledOrbwalker &&
            SDK::Orbwalker::Implementation() == controlledOrbwalker) {
            controlledOrbwalker->MoveEnabled(imposedMoveEnabled);
            controlledOrbwalker->AttackEnabled(imposedAttackEnabled);
        }

        controlActive = true;
        preservingAttacks = imposedAttackEnabled;
        return true;
    }

    void EndControl() {
        ExitControl(LegacyControlExitMode::NormalRestore);
    }

    void AbandonControlForExternalOwner() {
        ExitControl(LegacyControlExitMode::ExternalOwnerHandoff);
    }

    StopIssueResult StopUnsafeMovement(int minimumIntervalMs = 50) {
        const int now = SDK::Variables::TickCount();
        if (lastStopTick > 0 &&
            TickDifference(now, lastStopTick) <
                std::max(0, minimumIntervalMs)) {
            return StopIssueResult::Throttled;
        }
        if (!CoreControl::StopMoving(true)) return StopIssueResult::Failed;
        lastStopTick = now;
        return StopIssueResult::Issued;
    }

    MoveIssueResult MoveTo(const SDK::AIHeroClient& player,
                           const Vec2& destination,
                           int minimumIntervalMs,
                           int refreshIntervalMs,
                           float targetReachTolerance,
                           bool currentPositionExactDanger) {
        if (!player.IsValid() ||
            !destination.IsValid() ||
            destination.IsZero()) {
            return MoveIssueResult::HardFailure;
        }
        const int now = SDK::Variables::TickCount();
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float planeY = player.ServerPosition().y;
        const Vec3 destination3 = Vec3::From2D(destination, planeY);
        if (!heroPos.IsValid() ||
            heroPos.IsZero() ||
            !CoreNavGrid::IsWalkable(destination3)) {
            return MoveIssueResult::HardFailure;
        }
        const float distance = heroPos.Distance(destination);

        const bool targetChanged = lastTarget.IsZero() ||
            lastTarget.DistanceSqr(destination) > 2025.0f;
        const Vec2 pathEnd = player.PathEnd().To2D();
        const bool pathMatches = player.HasPath() &&
            pathEnd.IsValid() &&
            !pathEnd.IsZero() &&
            pathEnd.DistanceSqr(destination) <= 3600.0f;
        if (!targetChanged &&
            distance + 12.0f < lastProgressDistance) {
            lastProgressDistance = distance;
            lastProgressTick = now;
        }

        if (IsMoveTargetReached(
                distance,
                targetReachTolerance,
                currentPositionExactDanger)) {
            if (targetChanged) {
                lastTarget = destination;
                lastProgressDistance = distance;
                lastProgressTick = now;
                lastSuccessTick = 0;
                consecutiveMoveFailures = 0;
            }
            consecutiveMoveFailures = NextMoveFailureStreak(
                MoveIssueResult::AlreadyFollowing,
                consecutiveMoveFailures);
            return MoveIssueResult::AlreadyFollowing;
        }

        const bool stuck = !targetChanged &&
            lastProgressTick > 0 &&
            TickDifference(now, lastProgressTick) >= 240;
        const MoveCadenceAction cadence = DecideMoveCadence(
            pathMatches,
            stuck,
            targetChanged,
            now,
            lastSuccessTick,
            lastAttemptTick,
            minimumIntervalMs,
            refreshIntervalMs);
        if (cadence == MoveCadenceAction::AlreadyFollowing) {
            consecutiveMoveFailures = NextMoveFailureStreak(
                MoveIssueResult::AlreadyFollowing,
                consecutiveMoveFailures);
            return MoveIssueResult::AlreadyFollowing;
        }
        if (cadence == MoveCadenceAction::Throttled) {
            return MoveIssueResult::Throttled;
        }

        lastAttemptTick = now;
        const MoveFailureClassification issue =
            AdaptCoreMoveIssueResult(
                CoreControl::IssueMoveDetailed(destination3, true),
                consecutiveMoveFailures);
        consecutiveMoveFailures = issue.consecutiveFailures;
        if (issue.result == MoveIssueResult::Issued) {
            if (targetChanged) {
                lastTarget = destination;
                lastProgressDistance = distance;
                lastProgressTick = now;
            }
            lastSuccessTick = now;
            if (stuck) {
                lastProgressTick = now;
                lastProgressDistance = distance;
            }
        }
        return issue.result;
    }

    bool ControlActive() const { return controlActive; }
    bool PreservingAttacks() const { return preservingAttacks; }
    int LastSuccessTick() const { return lastSuccessTick; }
    const Vec2& LastTarget() const { return lastTarget; }

private:
    bool controlActive = false;
    bool preservingAttacks = false;
    CoreEvadeState::OwnerToken ownerToken = {};
    SDK::IOrbwalker* controlledOrbwalker = nullptr;
    bool previousMoveEnabled = false;
    bool previousAttackEnabled = false;
    bool imposedMoveEnabled = false;
    bool imposedAttackEnabled = false;
    int lastAttemptTick = 0;
    int lastSuccessTick = 0;
    int lastProgressTick = 0;
    int lastStopTick = 0;
    int consecutiveMoveFailures = 0;
    float lastProgressDistance = 0.0f;
    Vec2 lastTarget = {};

    void ExitControl(LegacyControlExitMode mode) {
        const bool ownerStateReleaseSucceeded =
            ownerToken &&
            CoreEvadeState::SetOwnerState(
                ownerToken, false, false, 0);
        if (!ownerStateReleaseSucceeded) {
            ownerToken = {};
            ClearLocalControlBookkeeping();
            return;
        }
        const bool otherOwnerActiveAfterRelease =
            CoreEvadeState::StrictEvadeActive;
        if (!controlActive) {
            ClearLocalControlBookkeeping();
            return;
        }

        const bool sameOrbwalkerImplementation =
            SDK::Orbwalker::Implementation() == controlledOrbwalker;
        const LegacyControlRestoreDecision restore =
            DecideLegacyControlRestore(
                {
                    true,
                    sameOrbwalkerImplementation,
                    sameOrbwalkerImplementation && controlledOrbwalker
                        ? controlledOrbwalker->MoveEnabled()
                        : imposedMoveEnabled,
                    imposedMoveEnabled,
                    sameOrbwalkerImplementation && controlledOrbwalker
                        ? controlledOrbwalker->AttackEnabled()
                        : imposedAttackEnabled,
                    imposedAttackEnabled,
                    true,
                    true,
                    ownerStateReleaseSucceeded,
                    otherOwnerActiveAfterRelease,
                },
                mode);
        if (controlledOrbwalker && restore.restoreMoveEnabled) {
            controlledOrbwalker->MoveEnabled(previousMoveEnabled);
        }
        if (controlledOrbwalker && restore.restoreAttackEnabled) {
            controlledOrbwalker->AttackEnabled(previousAttackEnabled);
        }

        ClearLocalControlBookkeeping();
    }

    bool SetOwnedControlState(bool active,
                              bool blockAttacks,
                              int comboUntilTick) {
        if (CoreEvadeState::SetOwnerState(
                ownerToken,
                active,
                blockAttacks,
                comboUntilTick)) {
            return true;
        }

        ownerToken = CoreEvadeState::AcquireOwner();
        return ownerToken &&
            CoreEvadeState::SetOwnerState(
                ownerToken,
                active,
                blockAttacks,
                comboUntilTick);
    }

    void ClearLocalControlBookkeeping() {
        const int retainedStopTick = StopThrottleTickAfterReset(
            StopThrottleResetMode::ImmediateRelease,
            0,
            lastStopTick);
        controlActive = false;
        preservingAttacks = false;
        controlledOrbwalker = nullptr;
        previousMoveEnabled = false;
        previousAttackEnabled = false;
        imposedMoveEnabled = false;
        imposedAttackEnabled = false;
        ResetMovementState();
        lastStopTick = retainedStopTick;
    }

    void ResetMovementState() {
        preservingAttacks = false;
        lastAttemptTick = 0;
        lastSuccessTick = 0;
        lastProgressTick = 0;
        lastStopTick = 0;
        consecutiveMoveFailures = 0;
        lastProgressDistance = 0.0f;
        lastTarget = {};
    }
};

}
