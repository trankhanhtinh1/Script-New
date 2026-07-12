#pragma once

#include "EvadeHelper.h"
#include "EvadeSettings.h"
#include "EvadeSpell.h"
#include "SpellDetector.h"

#include "../../../Core/CoreControl.h"
#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Plugins::KuroEvade {

class Evade {
public:
    void OnProcessSpell(const SDK::Events::ProcessSpellEventArgs& args) {
        const auto player = SDK::ObjectManager::Player();
        const bool isPlayer = player.IsValid() &&
            (static_cast<int>(args.Sender.NetworkId) == player.NetworkId() ||
             args.Sender.Ptr == player.Address());
        if (!isPlayer || args.IsAutoAttack) {
            return;
        }

        const float remainingMs =
            (player.Spellbook().CastEndTime() - SDK::Game::Time()) * 1000.0f;
        const float attackWindupMs = SDK::AttackWindup(player) * 1000.0f;
        if (remainingMs <= 0.0f || std::fabs(remainingMs - attackWindupMs) <= 1.0f) {
            return;
        }

        m_lastWindupEndTick = SDK::Variables::TickCount() +
            static_cast<int>(remainingMs) - SDK::Game::Ping() / 2;
    }

    static inline bool s_orbwalkerWasMoveEnabled = true;
    static inline bool s_orbwalkerDisabled = false;

    static void DisableOrbwalkerMove() {
        if (s_orbwalkerDisabled) return;
        s_orbwalkerWasMoveEnabled = SDK::Orbwalker::MoveEnabled();
        SDK::Orbwalker::MoveEnabled(false);
        s_orbwalkerDisabled = true;
    }

    static void RestoreOrbwalkerMove() {
        if (!s_orbwalkerDisabled) return;
        SDK::Orbwalker::MoveEnabled(s_orbwalkerWasMoveEnabled);
        s_orbwalkerDisabled = false;
    }

    bool Tick(const EvadeSettings& settings,
              SpellDetector::SkillshotList& skillshots,
              int& lastDodgeTick,
              const EvadeSpell::ConfigResolver& evadeSpellConfig,
              char* lastEvent,
              std::size_t lastEventSize) {
        if (!settings.Enabled || !settings.DodgeKeyActive) {
            m_hasLastPosition = false;
            m_waitingForWindup = false;
            m_isMovementBlocking = false;
            RestoreOrbwalkerMove();
            return false;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            RestoreOrbwalkerMove();
            return false;
        }

        if (player.IsDashing() || player.IsInvulnerable() || player.Spellbook().IsChanneling()) {
            RestoreOrbwalkerMove();
            return false;
        }

        if (player.HealthPercent() > settings.DodgeHp) {
            RestoreOrbwalkerMove();
            return false;
        }

        if (skillshots.empty()) {
            m_waitingForWindup = false;
            RestoreOrbwalkerMove();
            if (m_isMovementBlocking) {
                (void)CoreControl::IssueMove(
                    Vec3::From2D(m_blockedMovePos, player.ServerPosition().y), true);
                m_isMovementBlocking = false;
            }
            return false;
        }

        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        const int now = SDK::Variables::TickCount();
        const int trackedWindupRemaining = std::max(0, m_lastWindupEndTick - now);
        const bool isAutoAttacking = SDK::Orbwalker::IsAutoAttacking();
        const int orbwalkerWindupRemaining = isAutoAttacking
            ? SDK::Orbwalker::AttackCastDelayRemaining()
            : 0;
        EvadeSettings delayedSettings = settings;
        delayedSettings.CurrentWindupDelay = settings.CalculateWindupDelay
            ? static_cast<float>(std::max(trackedWindupRemaining, orbwalkerWindupRemaining))
            : 0.0f;
        const bool canFinishAttack = delayedSettings.CurrentWindupDelay > 1.0f &&
            isAutoAttacking;
        EvadeSettings immediateSettings = delayedSettings;
        immediateSettings.CurrentWindupDelay = 0.0f;
        EvadeHelper delayedHelper(delayedSettings);
        EvadeHelper immediateHelper(immediateSettings);

        const float delayMs = settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping()) +
            std::max(0.0f, delayedSettings.CurrentWindupDelay);

        if (!delayedHelper.IsEndangered(player, heroPos, boundingRadius, skillshots)) {
            if (m_hasLastPosition && m_waitingForWindup && canFinishAttack) {
                const float speed = std::max(50.0f, player.MoveSpeed());
                if (!delayedHelper.CheckMovePath(
                        m_lastPosition, delayMs, heroPos, boundingRadius,
                        skillshots, speed)) {
                    DisableOrbwalkerMove();
                    SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                    return true;
                }
            }
            m_hasLastPosition = false;
            m_waitingForWindup = false;
            const auto path = player.Path();

            if (m_isMovementBlocking) {
                const float speed = std::max(50.0f, player.MoveSpeed());
                const float immediateDelay = settings.ExtraDelay +
                    static_cast<float>(SDK::Game::Ping());
                if (canFinishAttack) {
                    if (!delayedHelper.CheckMovePath(
                            m_blockedMovePos, delayMs, heroPos,
                            boundingRadius, skillshots, speed)) {
                        DisableOrbwalkerMove();
                        SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                        return true;
                    }
                    Vec2 delayedDetour;
                    if (delayedHelper.FindBestPositionForPath(
                            player, heroPos, boundingRadius, m_blockedMovePos,
                            skillshots, delayedDetour)) {
                        m_lastPosition = delayedDetour;
                        m_hasLastPosition = true;
                        m_waitingForWindup = true;
                        DisableOrbwalkerMove();
                        SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                        return true;
                    }
                }
                if (!immediateHelper.CheckMovePath(
                        m_blockedMovePos, immediateDelay, heroPos,
                        boundingRadius, skillshots, speed)) {
                    RestoreOrbwalkerMove();
                    if (CoreControl::IssueMove(
                            Vec3::From2D(m_blockedMovePos, player.ServerPosition().y), true)) {
                        m_isMovementBlocking = false;
                        m_blockedMoveTick = now;
                        SetLastEvent(lastEvent, lastEventSize, "resume blocked move");
                        return false;
                    }
                }

                if (now - m_blockedMoveTick >= 80) {
                    Vec2 detour;
                    if (immediateHelper.FindBestPositionForPath(
                            player, heroPos, boundingRadius, m_blockedMovePos,
                            skillshots, detour)) {
                        DisableOrbwalkerMove();
                        if (CoreControl::IssueMove(
                                Vec3::From2D(detour, player.ServerPosition().y), true)) {
                            m_lastPosition = detour;
                            m_hasLastPosition = true;
                            m_blockedMoveTick = now;
                            SetLastEvent(lastEvent, lastEventSize, "wall/threat detour");
                        }
                        return true;
                    }
                }

                DisableOrbwalkerMove();
                (void)CoreControl::HoldPosition(true);
                return true;
            }

            if (!path.empty()) {
                const Vec2 movePos = path.back().To2D();
                const float speed = std::max(50.0f, player.MoveSpeed());
                if (delayedHelper.CheckMovePath(
                        path, delayMs, heroPos, boundingRadius, skillshots, speed)) {
                    Vec2 best;
                    bool plannedAfterWindup = canFinishAttack &&
                        delayedHelper.FindBestPositionForPath(
                            player, heroPos, boundingRadius, movePos, skillshots, best);
                    bool found = plannedAfterWindup;
                    if (!found) {
                        found = immediateHelper.FindBestPositionForPath(
                            player, heroPos, boundingRadius, movePos, skillshots, best);
                    }
                    if (found) {
                        if (plannedAfterWindup) {
                            m_lastPosition = best;
                            m_hasLastPosition = true;
                            m_waitingForWindup = true;
                            DisableOrbwalkerMove();
                            SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                            return true;
                        }
                        DisableOrbwalkerMove();
                        const float planeY = player.ServerPosition().y;
                        if (!CoreControl::IssueMove(Vec3::From2D(best, planeY), true)) {
                            SetLastEvent(lastEvent, lastEventSize, "dodge order failed");
                            return true;
                        }
                        m_lastPosition = best;
                        m_hasLastPosition = true;
                        m_waitingForWindup = false;
                        lastDodgeTick = now;
                        m_isMovementBlocking = true;
                        m_blockedMovePos = movePos;
                        m_blockedMoveTick = now;
                        SetLastEvent(lastEvent, lastEventSize, "movement block");
                        return true;
                    }

                    // The current native path is unsafe and no traversable
                    // detour exists yet.  Stop at the current safe point and
                    // keep the original command for receding-horizon retry.
                    DisableOrbwalkerMove();
                    (void)CoreControl::HoldPosition(true);
                    m_isMovementBlocking = true;
                    m_blockedMovePos = movePos;
                    m_blockedMoveTick = now;
                    SetLastEvent(lastEvent, lastEventSize, "hold blocked path");
                    return true;
                }
            }
            RestoreOrbwalkerMove();
            return false;
        }

        if (settings.DodgeInterval > 0 && now - lastDodgeTick < settings.DodgeInterval) {
            DisableOrbwalkerMove();
            return true;
        }

        const float immediateDelayMs = settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping());
        const float speed = std::max(50.0f, player.MoveSpeed());
        if (m_hasLastPosition && m_waitingForWindup && canFinishAttack) {
            if (!delayedHelper.CheckMovePath(
                    m_lastPosition, delayMs, heroPos, boundingRadius,
                    skillshots, speed)) {
                DisableOrbwalkerMove();
                SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                return true;
            }
            m_waitingForWindup = false;
        }
        if (m_hasLastPosition && !m_waitingForWindup && !canFinishAttack &&
            !immediateHelper.CheckMovePath(
                m_lastPosition, immediateDelayMs, heroPos, boundingRadius,
                skillshots, speed)) {
            DisableOrbwalkerMove();
            const auto currentPath = player.Path();
            if (currentPath.empty() ||
                currentPath.back().To2D().Distance(m_lastPosition) >= 5.0f) {
                (void)CoreControl::IssueMove(
                    Vec3::From2D(m_lastPosition, player.ServerPosition().y), true);
            }
            return true;
        }

        const int currentDanger = immediateHelper.HighestDangerLevelAt(
            heroPos, boundingRadius, skillshots);
        const float lowestHitTime = immediateHelper.LowestHitTimeAt(
            heroPos, boundingRadius, skillshots);
        Vec2 best;
        bool plannedAfterWindup = canFinishAttack &&
            delayedHelper.FindBestPosition(
                player, heroPos, boundingRadius, skillshots, best);
        bool found = plannedAfterWindup;
        if (!found) {
            found = immediateHelper.FindBestPosition(
                player, heroPos, boundingRadius, skillshots, best);
        }

        if (found) {
            if (plannedAfterWindup) {
                m_lastPosition = best;
                m_hasLastPosition = true;
                m_waitingForWindup = true;
                DisableOrbwalkerMove();
                SetLastEvent(lastEvent, lastEventSize, "wait attack windup");
                return true;
            }

            if (immediateSettings.UseEvadeSpells &&
                EvadeSpell::TryUseBest(immediateSettings, player, best, true, currentDanger, lowestHitTime,
                                       skillshots, evadeSpellConfig, lastEvent, lastEventSize)) {
                DisableOrbwalkerMove();
                lastDodgeTick = now;
                return true;
            }

            if (settings.ClickOnlyOnce && m_hasLastPosition &&
                m_lastPosition.Distance(best) < 5.0f) {
                const auto path = player.Path();
                if (!path.empty() &&
                    path.back().To2D().Distance(best) < 5.0f) {
                    DisableOrbwalkerMove();
                    return true;
                }
            }

            DisableOrbwalkerMove();
            const float planeY = player.ServerPosition().y;
            if (!CoreControl::IssueMove(Vec3::From2D(best, planeY), true)) {
                SetLastEvent(lastEvent, lastEventSize, "dodge order failed");
                return true;
            }
            m_lastPosition = best;
            m_hasLastPosition = true;
            m_waitingForWindup = false;
            lastDodgeTick = now;
            SetLastEvent(lastEvent, lastEventSize, "dodging");
            return true;
        }

        if (immediateSettings.UseEvadeSpells &&
            EvadeSpell::TryUseBest(immediateSettings, player, heroPos, false, currentDanger, lowestHitTime,
                                   skillshots, evadeSpellConfig, lastEvent, lastEventSize)) {
            DisableOrbwalkerMove();
            lastDodgeTick = now;
            return true;
        }

        Vec2 survivalPosition;
        if (immediateHelper.FindBestPosition(
                player, heroPos, boundingRadius, skillshots,
                survivalPosition, true)) {
            DisableOrbwalkerMove();
            if (CoreControl::IssueMove(
                    Vec3::From2D(survivalPosition, player.ServerPosition().y), true)) {
                m_lastPosition = survivalPosition;
                m_hasLastPosition = true;
                m_waitingForWindup = false;
                lastDodgeTick = now;
                SetLastEvent(lastEvent, lastEventSize, "least-danger fallback");
                return true;
            }
        }

        SetLastEvent(lastEvent, lastEventSize, "no safe position");
        DisableOrbwalkerMove();
        return true;
    }

    bool HasMoveTarget() const {
        return m_hasLastPosition && !m_lastPosition.IsZero();
    }

    const Vec2& MoveTarget() const {
        return m_lastPosition;
    }

    bool IsWaitingForWindup() const {
        return m_waitingForWindup;
    }

    bool IsMovementBlocking() const {
        return m_isMovementBlocking;
    }

private:
    int m_lastWindupEndTick = 0;
    Vec2 m_lastPosition;
    bool m_hasLastPosition = false;
    bool m_waitingForWindup = false;
    bool m_isMovementBlocking = false;
    Vec2 m_blockedMovePos = {};
    int m_blockedMoveTick = 0;

    static void SetLastEvent(char* lastEvent, std::size_t lastEventSize, const char* text) {
        if (!lastEvent || lastEventSize == 0) {
            return;
        }
        strncpy_s(lastEvent, lastEventSize, text ? text : "", _TRUNCATE);
    }
};

} // namespace Plugins::KuroEvade
