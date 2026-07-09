#pragma once

#include "../../SDK/SDK.h"
#include "../../Core/CoreControl.h"
#include "../../Core/CoreNavGrid.h"
#include "../../SDK/Wrappers/Orbwalking/Orbwalker.h"

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
    static inline bool orbwalkerDisabled = false;
    static constexpr int kMinMoveInterval = 90;
    static constexpr int kForceMoveInterval = 140;
    static constexpr int kSameMoveRefreshInterval = 350;
    static constexpr float kSameMoveDistanceSqr = 2500.0f;
    static constexpr float kNearMoveDistanceSqr = 10000.0f;

    static void DisableOrbwalkerMove() {
        if (orbwalkerDisabled) return;
        orbwalkerWasMoveEnabled = SDK::Orbwalker::MoveEnabled();
        SDK::Orbwalker::MoveEnabled(false);
        orbwalkerDisabled = true;
    }

    static void RestoreOrbwalkerMove() {
        if (!orbwalkerDisabled) return;
        SDK::Orbwalker::MoveEnabled(orbwalkerWasMoveEnabled);
        orbwalkerDisabled = false;
    }

    static bool MoveTo(const Vec2& pos, bool force = false) {
        const int now = SDK::Variables::TickCount();
        DisableOrbwalkerMove();

        const float targetDeltaSqr = lastMoveToPosition.DistanceSqr(pos);
        const float attemptDeltaSqr = lastMoveAttemptPosition.DistanceSqr(pos);
        const bool samePosition = lastMoveSucceeded && targetDeltaSqr < kSameMoveDistanceSqr;
        const bool nearPosition = lastMoveSucceeded && targetDeltaSqr < kNearMoveDistanceSqr;
        const bool sameAttempt = lastMoveAttemptTick > 0 && attemptDeltaSqr < kNearMoveDistanceSqr;
        const int elapsed = now - lastMoveTick;
        const int attemptElapsed = now - lastMoveAttemptTick;

        if (sameAttempt && attemptElapsed < kMinMoveInterval) return lastMoveSucceeded;

        if (lastMoveSucceeded) {
            if (samePosition && elapsed < kSameMoveRefreshInterval) return true;
            if (force && nearPosition && elapsed < kForceMoveInterval) return true;
            if (!force && elapsed < kMinMoveInterval) return true;
            if (!force && samePosition) return true;
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
