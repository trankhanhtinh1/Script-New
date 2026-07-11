#pragma once

#include "../../../Core/CoreControl.h"
#include "../../../Core/CoreEvadeState.h"
#include "../../../SDK/SDK.h"
#include "../../../SDK/Wrappers/Orbwalking/Orbwalker.h"

#include <algorithm>

namespace ZDEvade {

class EvadeCommandEngine {
public:
    void BeginControl() {
        const int now = SDK::Variables::TickCount();
        if (!controlActive) {
            previousMoveEnabled = SDK::Orbwalker::MoveEnabled();
            previousAttackEnabled = SDK::Orbwalker::AttackEnabled();
            controlActive = true;
        }
        SDK::Orbwalker::MoveEnabled(false);
        SDK::Orbwalker::AttackEnabled(false);
        CoreEvadeState::SetStrictEvadeActive(true);
        CoreEvadeState::BlockComboUntil(now + 150);
    }

    void EndControl() {
        const int now = SDK::Variables::TickCount();
        if (controlActive) {
            SDK::Orbwalker::MoveEnabled(previousMoveEnabled);
            SDK::Orbwalker::AttackEnabled(previousAttackEnabled);
        }
        controlActive = false;
        CoreEvadeState::SetStrictEvadeActive(false);
        CoreEvadeState::ClearComboBlock(now);
        ResetMovementState();
    }

    void StopUnsafeMovement() {
        const int now = SDK::Variables::TickCount();
        if (lastStopTick > 0 && now - lastStopTick < 50) return;
        if (CoreControl::StopMoving(true)) lastStopTick = now;
    }

    bool MoveTo(const SDK::AIHeroClient& player,
                const Vec2& destination,
                int minimumIntervalMs,
                int refreshIntervalMs) {
        if (!player.IsValid() || !destination.IsValid() || destination.IsZero()) return false;
        const int now = SDK::Variables::TickCount();
        const Vec2 heroPos = player.ServerPosition().To2D();
        const float distance = heroPos.Distance(destination);
        if (distance < 12.0f) return true;

        const bool targetChanged = lastTarget.IsZero() || lastTarget.DistanceSqr(destination) > 2025.0f;
        const Vec2 pathEnd = player.PathEnd().To2D();
        const bool pathMatches = !pathEnd.IsZero() && pathEnd.DistanceSqr(destination) <= 3600.0f;

        if (targetChanged) {
            lastTarget = destination;
            lastProgressDistance = distance;
            lastProgressTick = now;
        } else if (distance + 12.0f < lastProgressDistance) {
            lastProgressDistance = distance;
            lastProgressTick = now;
        }

        const bool stuck = lastProgressTick > 0 && now - lastProgressTick >= 240;
        const int minimumInterval = std::max(20, minimumIntervalMs);
        const int refreshInterval = std::max(minimumInterval, refreshIntervalMs);
        if (lastAttemptTick > 0 && now - lastAttemptTick < minimumInterval) return lastSucceeded;
        if (!targetChanged && pathMatches && !stuck && lastSuccessTick > 0 &&
            now - lastSuccessTick < refreshInterval) return true;

        lastAttemptTick = now;
        const float planeY = player.ServerPosition().y;
        const bool issued = CoreControl::IssueMove(Vec3::From2D(destination, planeY), true);
        lastSucceeded = issued;
        if (issued) {
            lastSuccessTick = now;
            if (stuck) {
                lastProgressTick = now;
                lastProgressDistance = distance;
            }
        }
        return issued;
    }

    bool ControlActive() const { return controlActive; }
    int LastSuccessTick() const { return lastSuccessTick; }
    const Vec2& LastTarget() const { return lastTarget; }

private:
    bool controlActive = false;
    bool previousMoveEnabled = true;
    bool previousAttackEnabled = true;
    bool lastSucceeded = false;
    int lastAttemptTick = 0;
    int lastSuccessTick = 0;
    int lastProgressTick = 0;
    int lastStopTick = 0;
    float lastProgressDistance = 0.0f;
    Vec2 lastTarget = {};

    void ResetMovementState() {
        lastSucceeded = false;
        lastAttemptTick = 0;
        lastSuccessTick = 0;
        lastProgressTick = 0;
        lastStopTick = 0;
        lastProgressDistance = 0.0f;
        lastTarget = {};
    }
};

}
