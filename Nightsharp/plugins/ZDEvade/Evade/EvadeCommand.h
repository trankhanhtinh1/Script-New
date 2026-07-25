#pragma once

#include "../../../SDK/SDK.h"
#include "../../../Core/CoreControl.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../Core/CoreNavGrid.h"
#include "../../../SDK/Wrappers/Orbwalking/Orbwalker.h"
#include "../Detection/Threat.h"

namespace ZDEvade {

struct EvadeCommand {
    enum class Order {
        None,
        MoveTo,
        Attack,
        CastSpell
    };

    Order order = Order::None;
    Vec2 targetPosition = {};
    int timestamp = 0;
    bool isProcessed = false;

    EvadeCommand() = default;
};

struct EvadeCommandManager {
    static inline EvadeCommand lastEvadeCommand = {};
    static inline EvadeCommand lastBlockedUserMoveTo = {};
    static inline Vec2 lastMoveToPosition = {};
    static inline int lastMoveTick = 0;
    static inline int lastMoveAttemptTick = 0;
    static inline Vec2 lastMoveAttemptPosition = {};
    static inline bool lastMoveSucceeded = false;
    static inline bool orbwalkerWasMoveEnabled = true;
    static inline bool orbwalkerWasAttackEnabled = true;
    static inline bool orbwalkerMoveDisabled = false;
    static inline bool orbwalkerAttackDisabled = false;
    static constexpr int kMinMoveInterval = 90;
    static constexpr int kForceMoveInterval = 35;
    static constexpr int kSameMoveRefreshInterval = 350;
    static constexpr float kSameMoveDistanceSqr = 2500.0f;
    static constexpr float kNearMoveDistanceSqr = 10000.0f;

    static void DisableOrbwalkerMove() {
        if (orbwalkerMoveDisabled) return;
        orbwalkerWasMoveEnabled = SDK::Orbwalker::MoveEnabled();
        SDK::Orbwalker::MoveEnabled(false);
        orbwalkerMoveDisabled = true;
    }

    static void DisableOrbwalkerAttack() {
        if (orbwalkerAttackDisabled) return;
        orbwalkerWasAttackEnabled = SDK::Orbwalker::AttackEnabled();
        SDK::Orbwalker::AttackEnabled(false);
        orbwalkerAttackDisabled = true;
    }

    static void RestoreOrbwalkerMove() {
        if (!orbwalkerMoveDisabled) return;
        SDK::Orbwalker::MoveEnabled(orbwalkerWasMoveEnabled);
        orbwalkerMoveDisabled = false;
    }

    static void RestoreOrbwalkerAttack() {
        if (!orbwalkerAttackDisabled) return;
        SDK::Orbwalker::AttackEnabled(orbwalkerWasAttackEnabled);
        orbwalkerAttackDisabled = false;
    }

    static void RestoreOrbwalker() {
        RestoreOrbwalkerMove();
        RestoreOrbwalkerAttack();
    }

    static void ApplyStrictEvadeControl() {
        DisableOrbwalkerMove();
        DisableOrbwalkerAttack();
    }

    static bool MoveTo(const Vec2& pos, bool force = false) {
        const int now = SDK::Variables::TickCount();
        if (CoreEvadeState::StrictEvadeActive) DisableOrbwalkerMove();

        const float targetDeltaSqr = lastMoveToPosition.DistanceSqr(pos);
        const float attemptDeltaSqr = lastMoveAttemptPosition.DistanceSqr(pos);
        const bool samePosition = lastMoveSucceeded && targetDeltaSqr < kSameMoveDistanceSqr;
        const bool nearPosition = lastMoveSucceeded && targetDeltaSqr < kNearMoveDistanceSqr;
        const bool sameAttempt = lastMoveAttemptTick > 0 && attemptDeltaSqr < kNearMoveDistanceSqr;
        const std::int64_t elapsed = TickDifference(now, lastMoveTick);
        const std::int64_t attemptElapsed =
            TickDifference(now, lastMoveAttemptTick);

        if (sameAttempt && attemptElapsed < (force ? kForceMoveInterval : kMinMoveInterval)) return lastMoveSucceeded;

        if (lastMoveSucceeded) {
            if (force) {
                if (nearPosition && elapsed < kForceMoveInterval) return true;
            } else {
                if (samePosition && elapsed < kSameMoveRefreshInterval) return true;
                if (elapsed < kMinMoveInterval) return true;
                if (samePosition) return true;
            }
        }

        lastMoveAttemptTick = now;
        lastMoveAttemptPosition = pos;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            lastMoveSucceeded = false;
            return false;
        }

        const float planeY = player.ServerPosition().y;
        if (!CoreControl::IssueMove(Vec3::From2D(pos, planeY), true)) {
            lastMoveSucceeded = false;
            return false;
        }

        lastEvadeCommand.order = EvadeCommand::Order::MoveTo;
        lastEvadeCommand.targetPosition = pos;
        lastEvadeCommand.timestamp = now;
        lastEvadeCommand.isProcessed = false;

        lastMoveToPosition = pos;
        lastMoveTick = now;
        lastMoveSucceeded = true;
        return true;
    }

    static void BlockUserMoveTo(const Vec2& pos) {
        lastBlockedUserMoveTo.order = EvadeCommand::Order::MoveTo;
        lastBlockedUserMoveTo.targetPosition = pos;
        lastBlockedUserMoveTo.timestamp = SDK::Variables::TickCount();
        lastBlockedUserMoveTo.isProcessed = false;
    }

    static bool HasPendingBlockedMove() {
        return !lastBlockedUserMoveTo.isProcessed;
    }

    static void MarkBlockedMoveProcessed() {
        lastBlockedUserMoveTo.isProcessed = true;
    }

};

} // namespace ZDEvade
