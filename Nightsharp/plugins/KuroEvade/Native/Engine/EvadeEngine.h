#pragma once

// Runtime port of Program.cs movement ownership and danger transitions.
// Detection/drawing live outside this class and continue while Enabled is off;
// this class only owns movement/cast intervention while evasion is enabled.

#include "EvadeSpell.h"
#include "Evader.h"
#include "../Helpers/Helpers.h"

#include "../../../../Core/CoreControl.h"
#include "../../../../SDK/SDK.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <utility>
#include <string>
#include <vector>

namespace Plugins::KuroEvade {

enum class SourceEvadeState {
    Idle,
    Dodging,
    RecoveringPath,
};

enum class SourcePendingOrderType {
    None,
    Move,
    Attack,
};

class SourceEvadeEngine final {
public:
    using ConfigResolver = SourceEvadeSpell::ConfigResolver;
    using AllyShieldResolver = SourceEvadeSpell::AllyShieldResolver;

    bool Tick(const EvadeSettings& settings,
              SourceSkillshotList& skillshots,
              int& lastDodgeTick,
              const ConfigResolver& configResolver,
              const AllyShieldResolver& allyShieldResolver,
              char* lastEvent,
              std::size_t lastEventSize) {
        const int now = SDK::Variables::TickCount();
        const auto player = GameObjects::Player();

        // The detector and drawer are intentionally not touched here. Turning
        // Evade off means no movement, no cast and no command interception.
        if (!settings.Enabled) {
            Reset(false);
            ClearBlockedCommand();
            return false;
        }
        const bool preserveTransientState =
            now < m_spellOnlyUntilTick ||
            m_state != SourceEvadeState::Idle;
        if (!CanRun(player, settings, preserveTransientState)) {
            Reset(false);
            ClearBlockedCommand();
            return false;
        }
        m_waitingForWindup = false;
        m_windupRemainingMs = 0;
        m_canFinishCurrentAttack = false;
        m_canStartNewAttack = false;
        m_currentDangerLevel = 0;
        m_currentDangerousThreat = false;

        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float speed = std::max(50.0f, player.MoveSpeed());
        // FocusOnEvade commits the intent present when the evade begins. A
        // merely hovered cursor can otherwise flip two equally safe side exits
        // every replan. Non-focus mode deliberately retains live steering.
        const Vec2 liveGoal = ResolveGoalPosition(player);
        if (!settings.FocusOnEvade ||
            m_state == SourceEvadeState::Idle ||
            !m_hasCommittedGoal || m_committedGoal.IsZero()) {
            m_committedGoal = liveGoal;
            m_hasCommittedGoal = !liveGoal.IsZero();
        }
        const Vec2 goal = m_hasCommittedGoal ? m_committedGoal : liveGoal;
        m_observedPath = BuildPath(player, goal);

        int currentDanger = 0;
        float lowestHitTime = FLT_MAX;
        SourceSkillshotList incomingThreats;
        int baselineThreats = SourceEvader::CountPathThreats(
            m_observedPath, settings.CrossingTimeOffset, speed, 0,
            radius, skillshots, settings, &currentDanger,
            &lowestHitTime, &incomingThreats);
        bool directDanger = false;
        SourceSkillshotList directThreats;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!SourceEvader::ShouldConsider(skillshot, settings)) continue;
            if (!skillshot->ContainsStatic(hero, radius, settings)) continue;

            directDanger = true;
            directThreats.push_back(skillshot);
            skillshot->EvadeTime = 0.0f;
            skillshot->SpellHitTime = skillshot->HitTime(hero, settings);
        }

        if (m_state != SourceEvadeState::Idle && !goal.IsZero() &&
            hero.DistanceSqr(goal) > radius * radius) {
            // The observed server path becomes our safe dodge route after the
            // first move order. Keep testing the committed user route as well;
            // otherwise that safe path would make the engine release while the
            // same missile still blocks the user's original direction. Merge
            // both sets because overlapping spells may threaten different paths.
            int goalDanger = 0;
            float goalLowestHitTime = FLT_MAX;
            SourceSkillshotList goalThreats;
            SourceEvader::CountPathThreats(
                { hero, goal }, settings.CrossingTimeOffset, speed, 0,
                radius, skillshots, settings, &goalDanger,
                &goalLowestHitTime, &goalThreats);
            currentDanger = std::max(currentDanger, goalDanger);
            lowestHitTime = std::min(lowestHitTime, goalLowestHitTime);
            for (const SourceSkillshotPtr& threat : goalThreats) {
                if (std::find(incomingThreats.begin(), incomingThreats.end(),
                              threat) == incomingThreats.end()) {
                    incomingThreats.push_back(threat);
                    ++baselineThreats;
                }
            }
        }

        // Static containment and the time-aware server path are complementary.
        // In particular, the path produced by our previous evade order may be
        // safe while the hero is still inside the missile corridor. Merge the
        // two views so that baseline=0 cannot cancel that valid escape route.
        for (const SourceSkillshotPtr& skillshot : directThreats) {
            if (std::find(incomingThreats.begin(), incomingThreats.end(),
                          skillshot) == incomingThreats.end()) {
                incomingThreats.push_back(skillshot);
                ++baselineThreats;
            }
            currentDanger = std::max(
                currentDanger, SourceEvader::DangerValue(*skillshot));
            lowestHitTime = std::min(
                lowestHitTime, skillshot->HitTime(hero, settings));
        }
        const bool pathDanger = baselineThreats > 0;
        int baselineDangerScore = 0;
        for (const SourceSkillshotPtr& skillshot : incomingThreats) {
            if (!skillshot) continue;
            const int threatDanger = SourceEvader::DangerValue(*skillshot);
            baselineDangerScore += threatDanger;
            m_currentDangerLevel = std::max(
                m_currentDangerLevel, threatDanger);
            m_currentDangerousThreat = m_currentDangerousThreat ||
                skillshot->Data.IsDangerous;
        }

        if (directDanger || pathDanger) {
            m_safeSinceTick = 0;
        }

        // Once the solver deliberately releases an unavoidable collision, keep
        // that decision stable through its impact window. Tick previously
        // ignored this guard even though ShouldBlockMove honoured it, allowing
        // evade and the orbwalker to reclaim movement on alternating frames.
        if (m_unavoidableReleaseUntilTick > 0 &&
            now >= m_unavoidableReleaseUntilTick) {
            m_unavoidableReleaseUntilTick = 0;
            m_unavoidableThreats.clear();
        }
        if ((directDanger || pathDanger) &&
            now < m_unavoidableReleaseUntilTick) {
            const bool hasNewThreat = std::any_of(
                incomingThreats.begin(), incomingThreats.end(),
                [&](const SourceSkillshotPtr& threat) {
                    return std::find(
                        m_unavoidableThreats.begin(),
                        m_unavoidableThreats.end(), threat) ==
                        m_unavoidableThreats.end();
                });
            const float recheckDistance = std::max(60.0f, radius + 20.0f);
            const bool positionChanged =
                !m_unavoidableOrigin.IsZero() &&
                hero.DistanceSqr(m_unavoidableOrigin) >=
                    recheckDistance * recheckDistance &&
                now - m_lastUnavoidableProbeTick >= 120;
            if (!hasNewThreat && !positionChanged) {
                SetLastEvent(lastEvent, lastEventSize,
                             "unavoidable: waiting for impact");
                return false;
            }
            // A newly detected spell or a meaningful player displacement is a
            // new decision. The distance/cooldown pair prevents the old
            // unavoidable result from reclaiming movement every frame.
            m_lastUnavoidableProbeTick = now;
            m_unavoidableReleaseUntilTick = 0;
            m_unavoidableThreats.clear();
        }

        // Preserve ownership while an evade spell is resolving. Dashes,
        // untargetability and spell shields are expected transient states of
        // those spells and must not reset the engine on the following frame.
        if (now < m_spellOnlyUntilTick) {
            m_unavoidableReleaseUntilTick = 0;
            m_state = SourceEvadeState::Dodging;
            const int resolvingWindup = SDK::Orbwalker::IsAutoAttacking()
                ? std::max(0, SDK::Orbwalker::AttackCastDelayRemaining())
                : 0;
            if (m_spellOnlyStopsMovement && resolvingWindup <= 0 &&
                player.IsMoving() && now - m_lastStopTick >= 180 &&
                CoreControl::StopMoving(true)) {
                m_lastStopTick = now;
                lastDodgeTick = now;
            }
            SetLastEvent(lastEvent, lastEventSize, "evade spell resolving");
            return true;
        }

        if (!directDanger && !pathDanger) {
            // Ally shielding remains independent, but it must not consume the
            // shared cast window before the local coverage solver can protect
            // the player from an incoming threat.
            SourceEvadeSpell::TryShieldAllies(
                settings, player, skillshots, configResolver,
                allyShieldResolver, lastEvent, lastEventSize);
            const bool wasActive = m_state != SourceEvadeState::Idle;
            if (wasActive) {
                if (m_safeSinceTick <= 0) {
                    m_safeSinceTick = now;
                }
                const int confirmationWindow = std::clamp(
                    std::max(0, SDK::Game::Ping()) / 2 + 45, 55, 110);
                if (now - m_safeSinceTick < confirmationWindow) {
                    const float releaseDistance =
                        std::max(80.0f, radius + 35.0f);
                    if (m_hasMoveTarget &&
                        hero.DistanceSqr(m_moveTarget) >
                            releaseDistance * releaseDistance &&
                        now - m_lastActionTick >= 110 &&
                        CoreControl::IssueMove(
                            Vec3::From2D(
                                m_moveTarget, player.ServerPosition().y),
                            true)) {
                        m_lastIssuedTarget = m_moveTarget;
                        m_lastActionTick = now;
                        lastDodgeTick = now;
                    }
                    SetLastEvent(lastEvent, lastEventSize,
                                 "confirming safe exit");
                    return true;
                }
            }
            Reset(false);
            if (wasActive) SetLastEvent(lastEvent, lastEventSize, "safe");
            ContinueBlockedCommand(player, skillshots, settings);
            return false;
        }

        SourcePositionInfo currentTargetInfo;
        if (m_hasMoveTarget) {
            currentTargetInfo = SourceEvader::EvaluatePosition(
                player, m_moveTarget, goal, skillshots, settings);
        }
        const bool targetSafetyRegressed =
            m_state != SourceEvadeState::Idle &&
            (m_plan.Found || m_targetUnsafeSinceTick > 0) &&
            m_hasMoveTarget && currentTargetInfo.Navigable &&
            currentTargetInfo.PathThreatCount > 0;
        if (targetSafetyRegressed) {
            if (m_targetUnsafeSinceTick <= 0) {
                m_targetUnsafeSinceTick = now;
            }
        } else if (!m_hasMoveTarget || !currentTargetInfo.Navigable ||
                   currentTargetInfo.PathThreatCount == 0) {
            m_targetUnsafeSinceTick = 0;
        }
        const int targetSafetyDebounceMs = std::clamp(
            std::max(0, SDK::Game::Ping()) / 2 + 25, 40, 80);
        const bool toleratingTargetRegression = targetSafetyRegressed &&
            now - m_targetUnsafeSinceTick < targetSafetyDebounceMs;
        const bool committedSafeTarget =
            m_state != SourceEvadeState::Idle &&
            m_hasMoveTarget && currentTargetInfo.Navigable &&
            currentTargetInfo.PathThreatCount == 0;
        const bool targetStillUsable = m_hasMoveTarget &&
            currentTargetInfo.Navigable &&
            (currentTargetInfo.PathThreatCount < baselineThreats ||
             committedSafeTarget || toleratingTargetRegression);
        const bool routeNeedsReplan =
            !targetStillUsable || toleratingTargetRegression;
        const bool shouldReplan =
            (!m_holdingPosition && routeNeedsReplan) ||
            now - m_lastPlanTick >= settings.EvadePointChangeInterval;
        bool keepingRegressedTarget = false;
        if (shouldReplan) {
            // Candidate routes are new orders and therefore start after the
            // client/server command delay. The observed path and an already
            // issued target intentionally remain delay-free.
            const int orderDelay = CommandDelayMs();
            SourceEvadePlan nextPlan = SourceEvader::FindBestPosition(
                player, goal, skillshots, settings, true, orderDelay);
            m_lastPlanTick = now;
            if (nextPlan.HasCandidate) {
                const bool targetLockActive = m_lastTargetSwitchTick > 0 &&
                    now - m_lastTargetSwitchTick < std::max(
                        160, settings.EvadePointChangeInterval);
                SourcePositionInfo chosen = SelectStableTarget(
                    nextPlan, targetLockActive,
                    player, hero, goal, skillshots, settings, orderDelay);
                const bool switched = !m_hasMoveTarget ||
                    m_moveTarget.DistanceSqr(chosen.Position) > 65.0f * 65.0f;
                keepingRegressedTarget =
                    toleratingTargetRegression && !switched;
                m_plan = std::move(nextPlan);
                m_plan.Best = chosen;
                m_plan.HasCandidate = chosen.Navigable;
                m_plan.Found = chosen.Navigable &&
                    chosen.PathThreatCount == 0;
                m_plan.UsedFallback = !m_plan.Found;
                m_moveTarget = chosen.Position;
                m_hasMoveTarget = m_plan.HasCandidate;
                if (switched) {
                    m_lastTargetSwitchTick = now;
                    m_targetUnsafeSinceTick = 0;
                }
            } else if (toleratingTargetRegression &&
                       currentTargetInfo.Navigable) {
                // Candidate generation can flicker for one frame at a moving
                // collision boundary. Keep the previously safe issued route
                // during the short debounce instead of releasing movement.
                m_plan = {};
                m_plan.Best = currentTargetInfo;
                m_plan.HasCandidate = true;
                m_plan.Found = false;
                m_plan.UsedFallback = true;
                m_moveTarget = currentTargetInfo.Position;
                m_hasMoveTarget = true;
                keepingRegressedTarget = true;
            } else {
                m_plan = std::move(nextPlan);
                m_moveTarget = {};
                m_hasMoveTarget = false;
            }
        } else if (targetStillUsable) {
            // Threat timing advances every frame even while target hysteresis
            // keeps the same point. Refresh coverage so spell-vs-walk choices
            // never use the previous plan tick's remaining-hit count.
            m_plan.Best = currentTargetInfo;
            m_plan.HasCandidate = true;
            m_plan.Found = currentTargetInfo.Navigable &&
                currentTargetInfo.PathThreatCount == 0;
            m_plan.UsedFallback = !m_plan.Found;
        }

        // Preserve an auto attack only when the exact delayed movement path is
        // still safe. AttackCastDelayRemaining is supplied by OrbwalkerKuro
        // and is expressed in milliseconds; the small latency budget covers
        // the order reaching the server after the projectile/cast is released.
        const bool windingUp = SDK::Orbwalker::IsAutoAttacking();
        const int windupRemaining = windingUp
            ? std::max(0, SDK::Orbwalker::AttackCastDelayRemaining())
            : 0;
        const int windupLatency = std::clamp(
            std::max(0, SDK::Game::Ping()) / 6, 0, 25);
        const int delayedMovement = windupRemaining + windupLatency;
        const int prospectiveAttackWindup = std::clamp(
            static_cast<int>(
                std::max(0.0f, SDK::AttackCastDelay(player)) * 1000.0f) +
                windupLatency,
            100, 900);
        if (ShouldHoldPosition(
                directDanger, pathDanger, now, player, hero, goal, radius,
                skillshots, settings)) {
            m_unavoidableReleaseUntilTick = 0;
            const bool enteringHold = !m_holdingPosition;
            m_holdingPosition = true;
            if (enteringHold || m_holdStartTick <= 0) {
                m_holdStartTick = now;
            }
            m_state = SourceEvadeState::RecoveringPath;
            m_hasMoveTarget = false;
            m_moveTarget = {};
            m_canStartNewAttack = SourceEvader::IsSafeForDuration(
                hero, prospectiveAttackWindup + 25,
                radius, skillshots, settings);

            // The coordinator exposes SafePositionHold so OrbwalkerKuro can
            // acquire a target and attack from this safe pocket. Never send
            // Stop during the attack windup.
            if (windupRemaining <= 0 &&
                (enteringHold ||
                 (player.IsMoving() && now - m_lastStopTick >= 180))) {
                if (CoreControl::StopMoving(true)) {
                    m_lastStopTick = now;
                    lastDodgeTick = now;
                }
            }
            SetLastEvent(lastEvent, lastEventSize,
                windupRemaining > 0
                    ? "holding position: finish attack"
                    : "holding position: orbwalker can attack");
            return true;
        }
        if (m_holdingPosition) {
            // A hold may end because its timer elapsed, the point became
            // unsafe, or a valid escape opened. In every case, do not re-enter
            // it during the same continuous threat episode; otherwise a
            // flickering boundary produces Stop -> Move -> Stop oscillation.
            m_pathHoldExhausted = true;
        }
        m_holdingPosition = false;
        m_holdStartTick = 0;

        if (m_plan.Found && m_hasMoveTarget) {
            const bool prospectiveRouteSafe = SourceEvader::IsSafePath(
                { hero, m_moveTarget },
                std::clamp(settings.EvadingFirstTimeOffset, 0, 500),
                speed, prospectiveAttackWindup, radius,
                skillshots, settings).IsSafe;
            const bool prospectiveReleaseMargin = lowestHitTime == FLT_MAX ||
                lowestHitTime >
                    static_cast<float>(prospectiveAttackWindup + 20);
            m_canStartNewAttack =
                prospectiveRouteSafe && prospectiveReleaseMargin;
        }

        bool mustCancelWindup = false;
        if (windupRemaining > 0 && m_plan.Found && m_hasMoveTarget) {
            const bool delayedRouteSafe = SourceEvader::IsSafePath(
                { hero, m_moveTarget },
                std::clamp(settings.EvadingFirstTimeOffset, 0, 500),
                speed, delayedMovement, radius,
                skillshots, settings).IsSafe;
            const float hitTimeAtHero = lowestHitTime;
            const bool hasReleaseMargin = hitTimeAtHero == FLT_MAX ||
                hitTimeAtHero > static_cast<float>(delayedMovement + 20);
            const bool dangerAllowsAttack =
                currentDanger <= settings.AllowAutoAttackDangerLevel;
            m_canFinishCurrentAttack = dangerAllowsAttack &&
                delayedRouteSafe && hasReleaseMargin;
            m_windupRemainingMs = windupRemaining;

            if (m_canFinishCurrentAttack) {
                m_unavoidableReleaseUntilTick = 0;
                m_state = directDanger
                    ? SourceEvadeState::Dodging
                    : SourceEvadeState::RecoveringPath;
                m_waitingForWindup = true;
                // OrbwalkerKuro already owns the attack windup. OnBeforeMove
                // and the shared coordinator block competing movement until
                // the safe release point.
                SetLastEvent(lastEvent, lastEventSize, "holding attack windup");
                return true;
            }
            mustCancelWindup = true;
        } else if (windupRemaining > 0) {
            m_windupRemainingMs = windupRemaining;
            mustCancelWindup = true;
        }

        const float dangerWindow = lowestHitTime == FLT_MAX
            ? 1000.0f
            : lowestHitTime;
        const bool movementImprovesOutcome = m_plan.HasCandidate &&
            (m_plan.Best.PathThreatCount < baselineThreats ||
             (m_plan.Best.PathThreatCount == baselineThreats &&
              (m_plan.Best.PathDangerLevel < currentDanger ||
               (m_plan.Best.PathDangerLevel == currentDanger &&
                (m_plan.Best.PathDangerScore < baselineDangerScore ||
                 (m_plan.Best.PathDangerScore == baselineDangerScore &&
                  lowestHitTime < FLT_MAX &&
                  m_plan.Best.EarliestPathHitTime >
                      lowestHitTime + 80.0f))))));

        // Walking is preferred when it produces a completely safe route. If
        // even the best walking route still gets hit, score every ready evade
        // spell and cast the one that leaves the fewest threats. This also
        // handles 2 -> 0 and 2 -> 1 coverage instead of all-or-nothing logic.
        if (!m_plan.Found) {
            const EvadeSpellUseResult spell = SourceEvadeSpell::TryUseBest(
                settings, player,
                m_plan.HasCandidate ? m_plan.Best.Position : goal,
                baselineThreats, std::max(1, currentDanger), dangerWindow,
                incomingThreats, skillshots, configResolver,
                allyShieldResolver, lastEvent, lastEventSize);
            if (spell.Used) {
                m_unavoidableReleaseUntilTick = 0;
                m_state = SourceEvadeState::Dodging;
                lastDodgeTick = now;
                if (!spell.MovementSpeed) {
                    const int spellSettleMargin = settings.SmoothEvadeSpell
                        ? 160
                        : 80;
                    m_spellOnlyUntilTick = now + std::clamp(
                        static_cast<int>(dangerWindow) + spellSettleMargin,
                        300, 1200);
                    m_spellOnlyStopsMovement = !spell.Displacement;
                    if (m_spellOnlyStopsMovement && windupRemaining <= 0 &&
                        player.IsMoving() && CoreControl::StopMoving(true)) {
                        m_lastStopTick = now;
                    }
                }
                return true;
            }
        }

        const bool committedSafeRoute =
            m_state != SourceEvadeState::Idle &&
            m_hasMoveTarget &&
            (m_plan.Found || keepingRegressedTarget);
        if (!m_plan.HasCandidate ||
            (!movementImprovesOutcome && !committedSafeRoute)) {
            // No action changes the outcome. Holding movement/attacks here only
            // harms the player, so publish Idle and bypass move interception
            // through the impact window.
            int releaseWindow = std::clamp(
                static_cast<int>(dangerWindow) + 180, 220, 1400);
            for (const SourceSkillshotPtr& threat : incomingThreats) {
                if (!threat || !threat->IsActive(now)) continue;
                // Persistent zones report HitTime=0 after impact. Their
                // remaining active lifetime is the real risk window and keeps
                // an unavoidable release from expiring on the following frame.
                releaseWindow = std::max(releaseWindow, std::clamp(
                    threat->EndTick() - now + 100, 220, 1400));
            }
            Reset(false);
            ClearBlockedCommand();
            m_unavoidableReleaseUntilTick = now + releaseWindow;
            m_unavoidableThreats = incomingThreats;
            m_unavoidableOrigin = hero;
            m_lastUnavoidableProbeTick = now;
            SetLastEvent(lastEvent, lastEventSize,
                         "unavoidable: orbwalker released");
            return false;
        }

        m_unavoidableReleaseUntilTick = 0;
        if (!m_plan.Found) {
            m_state = SourceEvadeState::RecoveringPath;
        }

        m_state = directDanger
            ? SourceEvadeState::Dodging
            : SourceEvadeState::RecoveringPath;

        // Config.LowEvadeSmooth intentionally delays walking until impact is
        // close. CoreControl still applies the process-wide 45 ms MoveTo gate.
        if (settings.LowEvadeSmooth && !mustCancelWindup &&
            dangerWindow > 500.0f) {
            const int smoothDelay = std::clamp(
                static_cast<int>(dangerWindow) - 350, 0, 120);
            if (smoothDelay > 0 && SourceEvader::IsSafePath(
                    { hero, m_moveTarget },
                    std::clamp(settings.EvadingFirstTimeOffset, 0, 500),
                    speed, smoothDelay, radius,
                    skillshots, settings).IsSafe) {
                // The delayed-path model assumes no movement before the new
                // order. Stop the previous server path so that assumption is
                // actually true instead of letting the hero drift into danger.
                if (player.IsMoving() && now - m_lastStopTick >= 180 &&
                    CoreControl::StopMoving(true)) {
                    m_lastStopTick = now;
                    lastDodgeTick = now;
                }
                SetLastEvent(lastEvent, lastEventSize, "delaying walking safely");
                return true;
            }
        }

        const bool targetChanged = m_lastIssuedTarget.IsZero() ||
            m_lastIssuedTarget.DistanceSqr(m_moveTarget) > 65.0f * 65.0f;
        // The process-wide gate remains 45 ms, while this engine refreshes an
        // unchanged target at 110 ms to reduce command noise and visible zigzag.
        if (targetChanged || now - m_lastActionTick >= 110) {
            if (CoreControl::IssueMove(
                    Vec3::From2D(m_moveTarget, player.ServerPosition().y), true)) {
                m_lastIssuedTarget = m_moveTarget;
                m_lastActionTick = now;
                lastDodgeTick = now;
                SetLastEvent(lastEvent, lastEventSize,
                    mustCancelWindup
                        ? "cancel windup evade"
                        : directDanger ? "walking evade" : "safe path detour");
            }
        }
        return true;
    }

    bool ShouldBlockMove(const Vec2& destination,
                         const EvadeSettings& settings,
                         const SourceSkillshotList& skillshots,
                         bool remember = true,
                         int targetNetworkId = 0) {
        const auto player = GameObjects::Player();
        const int now = SDK::Variables::TickCount();
        if (!settings.Enabled ||
            !CanRun(player, settings,
                    now < m_spellOnlyUntilTick ||
                    m_state != SourceEvadeState::Idle) ||
            destination.IsZero()) {
            return false;
        }
        if (now < m_unavoidableReleaseUntilTick) {
            // The coverage solver proved that no available action lowers the
            // hit count. Do not let this interceptor re-lock OrbwalkerKuro or
            // another script while the engine itself is deliberately Idle.
            return false;
        }

        const Vec2 hero = player.ServerPosition().To2D();
        if (m_holdingPosition ||
            (m_waitingForWindup && m_windupRemainingMs > 0)) {
            // Do not let another script/orbwalker issue a move that cancels an
            // attack which the time model has explicitly decided to finish.
            if (remember) {
                RememberBlockedCommand(destination, targetNetworkId);
            }
            return true;
        }
        int commandDanger = 0;
        const bool dangerous = SourceEvader::CountPathThreats(
            { hero, destination },
            std::clamp(settings.CrossingTimeOffset, 0, 500),
            std::max(50.0f, player.MoveSpeed()), CommandDelayMs(),
            player.BoundingRadius(), skillshots, settings,
            &commandDanger) > 0;
        if (m_state != SourceEvadeState::Idle) {
            // A real right-click is a stronger intent signal than a hovered
            // cursor. Let it steer to a verified safe route even in focus
            // mode, while orbwalker-generated moves remain locked unless live
            // steering was explicitly selected.
            const bool acceptsSteering = settings.UseCurrentPath &&
                (remember || !settings.FocusOnEvade);
            const bool attackAllowed = targetNetworkId == 0 ||
                std::max(m_currentDangerLevel, commandDanger) <=
                    settings.AllowAutoAttackDangerLevel;
            if (acceptsSteering && attackAllowed && !dangerous &&
                SourceEvader::IsSafePoint(
                    destination, player.BoundingRadius(), skillshots, settings)) {
                ClearBlockedCommand();
                m_moveTarget = destination;
                m_hasMoveTarget = true;
                m_lastIssuedTarget = destination;
                m_lastActionTick = now;
                if (remember) {
                    m_committedGoal = destination;
                    m_hasCommittedGoal = true;
                }
                return false;
            }
            if (remember) {
                RememberBlockedCommand(destination, targetNetworkId);
            }
            return true;
        }
        if (dangerous && remember) {
            RememberBlockedCommand(destination, targetNetworkId);
        } else if (remember) {
            ClearBlockedCommand();
        }
        return dangerous;
    }

    // Any new orbwalker move is a newer user intent than a raw right-click
    // retained during evasion. It must supersede that pending command so an
    // old click cannot win the first global move-order window after handoff.
    void SupersedeBlockedCommand() {
        ClearBlockedCommand();
    }

    void Shutdown() {
        Reset(true);
        ClearBlockedCommand();
    }

    bool HasMoveTarget() const { return m_hasMoveTarget; }
    const Vec2& MoveTarget() const { return m_moveTarget; }
    bool IsWaitingForWindup() const { return m_waitingForWindup; }
    bool IsHoldingPosition() const { return m_holdingPosition; }
    int WindupRemainingMs() const { return m_windupRemainingMs; }
    bool CanFinishCurrentAttack() const { return m_canFinishCurrentAttack; }
    bool CanStartNewAttack() const { return m_canStartNewAttack; }
    int CurrentDangerLevel() const { return m_currentDangerLevel; }
    bool HasDangerousThreat() const { return m_currentDangerousThreat; }
    bool IsMovementBlocking() const {
        return m_state == SourceEvadeState::RecoveringPath;
    }
    bool IsIntervening() const { return m_state != SourceEvadeState::Idle; }
    SourceEvadeState State() const { return m_state; }
    const SourceEvadePlan& LastPlan() const { return m_plan; }
    const std::vector<Vec2>& ObservedPath() const { return m_observedPath; }

private:
    SourceEvadeState m_state = SourceEvadeState::Idle;
    SourceEvadePlan m_plan;
    Vec2 m_moveTarget;
    Vec2 m_lastIssuedTarget;
    Vec2 m_committedGoal;
    Vec2 m_unavoidableOrigin;
    bool m_hasMoveTarget = false;
    bool m_hasCommittedGoal = false;
    bool m_waitingForWindup = false;
    bool m_canFinishCurrentAttack = false;
    bool m_canStartNewAttack = false;
    bool m_holdingPosition = false;
    bool m_spellOnlyStopsMovement = false;
    bool m_currentDangerousThreat = false;
    bool m_pathHoldExhausted = false;
    int m_windupRemainingMs = 0;
    int m_currentDangerLevel = 0;
    int m_lastPlanTick = 0;
    int m_lastTargetSwitchTick = 0;
    int m_lastStopTick = 0;
    int m_lastActionTick = 0;
    int m_spellOnlyUntilTick = 0;
    int m_unavoidableReleaseUntilTick = 0;
    int m_lastUnavoidableProbeTick = 0;
    int m_safeSinceTick = 0;
    int m_targetUnsafeSinceTick = 0;
    int m_holdStartTick = 0;
    std::vector<Vec2> m_observedPath;
    SourceSkillshotList m_unavoidableThreats;

    struct PendingCommand {
        SourcePendingOrderType Type = SourcePendingOrderType::None;
        Vec2 Position;
        int TargetNetworkId = 0;
        int Tick = 0;
    };
    PendingCommand m_pendingCommand;

    static bool CanRun(const SDK::AIHeroClient& player,
                       const EvadeSettings& settings,
                       bool preserveTransientState = false) {
        if (!player.IsValid() || player.IsDead()) {
            return false;
        }
        if (!preserveTransientState &&
            (player.IsInvulnerable() || !player.IsTargetable() ||
             Helpers::IsSpellShielded(player) || player.IsDashing())) {
            return false;
        }
        if (settings.DisableEvadeForOlafR) {
            const std::string playerName = player.CharacterName();
            const SDK::ChampionId playerChampionId =
                SDK::ChampionIdFromName(playerName.c_str());
            if (playerChampionId == SDK::ChampionId::Olaf &&
                player.HasBuff("OlafRagnarok")) {
                return false;
            }
        }
        return true;
    }

    static Vec2 ResolveGoalPosition(const SDK::AIHeroClient& player) {
        const Vec2 cursor = SDK::Game::CursorPos().To2D();
        if (!cursor.IsZero() && cursor.IsValid()) return cursor;
        const std::vector<Vec3> path = player.Path();
        if (!path.empty() && !path.back().IsZero()) return path.back().To2D();
        return player.ServerPosition().To2D();
    }

    static int CommandDelayMs() {
        // Use one latency model for planning, manual interception and replay.
        // The old 90 ms cap under-modelled high-ping orders.
        return std::clamp(
            std::max(0, SDK::Game::Ping()) / 2 + 15, 15, 180);
    }

    static std::vector<Vec2> BuildPath(const SDK::AIHeroClient& player,
                                       const Vec2& fallback) {
        std::vector<Vec2> result{ player.ServerPosition().To2D() };
        for (const Vec3& point : player.Path()) {
            const Vec2 value = point.To2D();
            if (!value.IsZero() && result.back().DistanceSqr(value) > 4.0f) {
                result.push_back(value);
            }
        }
        if (result.size() == 1 && (player.IsMoving() || player.HasPath())) {
            Vec2 actualEnd = player.PathEnd().To2D();
            if (actualEnd.IsZero() || !actualEnd.IsValid()) {
                actualEnd = fallback;
            }
            if (!actualEnd.IsZero() &&
                result.front().DistanceSqr(actualEnd) > 4.0f) {
                result.push_back(actualEnd);
            }
        }
        return result;
    }

    SourcePositionInfo SelectStableTarget(
            const SourceEvadePlan& nextPlan,
            bool targetLockActive,
            const SDK::AIHeroClient& player,
            const Vec2& hero,
            const Vec2& goal,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings,
            int movementDelay) const {
        SourcePositionInfo chosen = nextPlan.Best;
        if (!m_hasMoveTarget) {
            return chosen;
        }

        const SourcePositionInfo old = SourceEvader::EvaluatePosition(
            player, m_moveTarget, goal, skillshots, settings);
        const float radius = std::max(1.0f, player.BoundingRadius());
        if (!old.Navigable || old.DistanceToPlayer <= radius + 15.0f) {
            return chosen;
        }
        // Never smooth across a coverage regression. This comparison remains
        // valid for partial routes such as 2 -> 1 as well as fully safe ones.
        if (old.PathThreatCount < chosen.PathThreatCount) {
            return old;
        }
        if (old.PathThreatCount > chosen.PathThreatCount) {
            return chosen;
        }
        if (old.PathDangerLevel != chosen.PathDangerLevel) {
            return old.PathDangerLevel < chosen.PathDangerLevel
                ? old : chosen;
        }
        if (old.PathDangerScore != chosen.PathDangerScore) {
            return old.PathDangerScore < chosen.PathDangerScore
                ? old : chosen;
        }
        if (std::abs(old.EarliestPathHitTime -
                     chosen.EarliestPathHitTime) > 80.0f) {
            return old.EarliestPathHitTime > chosen.EarliestPathHitTime
                ? old : chosen;
        }
        if (old.SafePoint != chosen.SafePoint) {
            return old.SafePoint ? old : chosen;
        }
        if (old.DangerCount != chosen.DangerCount) {
            return old.DangerCount < chosen.DangerCount ? old : chosen;
        }
        if (old.DangerLevel != chosen.DangerLevel) {
            return old.DangerLevel < chosen.DangerLevel ? old : chosen;
        }

        // Ring topology is stronger than smoothing: never smooth an outer
        // escape back into the inner pocket, and immediately accept an outer
        // upgrade when one becomes reachable.
        const bool outerUpgrade =
            chosen.OuterRingExits > old.OuterRingExits ||
            chosen.InnerRingShelters < old.InnerRingShelters;
        const bool oldTopologyBetter =
            old.OuterRingExits > chosen.OuterRingExits ||
            old.InnerRingShelters < chosen.InnerRingShelters;
        if (oldTopologyBetter &&
            old.DistanceToPlayer <= chosen.DistanceToPlayer + 120.0f) {
            return old;
        }
        if (outerUpgrade &&
            chosen.DistanceToPlayer <= old.DistanceToPlayer + 120.0f) {
            return chosen;
        }

        const Vec2 oldDirection = (old.Position - hero).Normalized();
        const Vec2 newDirection = (chosen.Position - hero).Normalized();
        if (oldDirection.IsZero() || newDirection.IsZero()) {
            return chosen;
        }
        const float alignment = oldDirection.Dot(newDirection);
        const float scoreGain = old.Score - chosen.Score;
        const bool urgentClearance =
            (old.Clearance < 20.0f &&
             chosen.Clearance > old.Clearance + 35.0f) ||
            (old.WallClearance < radius + 20.0f &&
             chosen.WallClearance > old.WallClearance + 45.0f);
        const float requiredGain = targetLockActive ? 110.0f : 40.0f;
        const bool sharpTurn = alignment < 0.70f;
        if (!urgentClearance &&
            (scoreGain < requiredGain ||
             (sharpTurn && scoreGain < 170.0f))) {
            return old;
        }

        // Low-pass the direction, then run the blended point through the same
        // point/path/NavMesh checks. Unsafe interpolation is never accepted.
        if (alignment > -0.10f) {
            const float alpha = targetLockActive ? 0.25f : 0.40f;
            const Vec2 blendedDirection =
                (oldDirection * (1.0f - alpha) +
                 newDirection * alpha).Normalized();
            const float blendedDistance = std::max(
                radius + 35.0f,
                old.DistanceToPlayer * (1.0f - alpha) +
                    chosen.DistanceToPlayer * alpha);
            const Vec2 blended = hero + blendedDirection * blendedDistance;
            SourcePositionInfo smooth = SourceEvader::EvaluatePosition(
                player, blended, goal, skillshots, settings, movementDelay);
            if (smooth.Navigable &&
                smooth.PathThreatCount <= chosen.PathThreatCount &&
                smooth.SafePoint &&
                smooth.DistanceToPlayer > radius + 30.0f &&
                smooth.OuterRingExits >= chosen.OuterRingExits &&
                smooth.InnerRingShelters <= chosen.InnerRingShelters) {
                chosen = smooth;
            }
        }
        return chosen;
    }

    bool ShouldHoldPosition(
            bool directDanger,
            bool pathDanger,
            int now,
            const SDK::AIHeroClient& player,
            const Vec2& hero,
            const Vec2& goal,
            float radius,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) {
        if (directDanger || !pathDanger ||
            m_pathHoldExhausted ||
            !SourceEvader::IsSafePoint(
                hero, radius, skillshots, settings)) {
            return false;
        }

        const int holdWindow = std::clamp(
            settings.CrossingTimeOffset + std::max(0, SDK::Game::Ping()),
            240, 520);
        if (!SourceEvader::IsSafeForDuration(
                hero, holdWindow, radius, skillshots, settings)) {
            return false;
        }

        // A path-only hold is intentionally bounded. This covers a missile
        // crossing the future route in open ground without getting stuck
        // behind a long-lived zone forever.
        const int maxHoldMs = std::clamp(
            settings.PathOnlyHoldMaxMs, 80, 500);
        if (m_holdingPosition && m_holdStartTick > 0 &&
            now - m_holdStartTick >= maxHoldMs) {
            return false;
        }

        const float height = player.ServerPosition().y;
        const SourceGeometry::NavigationProbe wall =
            SourceGeometry::ProbeNavigation(
                hero, height, std::max(140.0f, radius + 100.0f));
        const bool nearWall = wall.BlockedRays > 0 &&
            wall.Clearance < radius + 75.0f;
        // A validated outer-ring escape has topology priority over waiting;
        // otherwise the safe hold itself could trap the hero in the ring's
        // inner pocket and reduce every later evade direction.
        if (m_plan.Found && m_plan.Best.OuterRingExits > 0) {
            return false;
        }

        // Once a safe hold is selected, keep it until the obstructing
        // skillshot passes or the current point stops being safe. This avoids
        // alternating between stop and two equally scored side exits.
        if (m_holdingPosition) {
            return true;
        }
        if (!m_plan.Found) {
            return settings.PreferPathHold || nearWall;
        }

        Vec2 intendedDirection;
        if (m_observedPath.size() >= 2) {
            intendedDirection = (m_observedPath[1] - hero).Normalized();
        }
        if (intendedDirection.IsZero()) {
            intendedDirection = (goal - hero).Normalized();
        }
        const Vec2 evadeDirection =
            (m_plan.Best.Position - hero).Normalized();
        if (intendedDirection.IsZero() || evadeDirection.IsZero()) {
            return true;
        }

        const bool sharpDetour =
            intendedDirection.Dot(evadeDirection) < 0.30f;
        const bool crampedExit =
            m_plan.Best.WallClearance < radius + 30.0f;
        const bool headsIntoWall = !wall.EscapeDirection.IsZero() &&
            evadeDirection.Dot(wall.EscapeDirection) < -0.15f;
        const bool longDetour =
            m_plan.Best.DistanceToPlayer > radius + 150.0f;
        return nearWall
            ? sharpDetour || crampedExit || headsIntoWall
            : settings.PreferPathHold && (sharpDetour || longDetour);
    }

    void Reset(bool full) {
        m_state = SourceEvadeState::Idle;
        m_hasMoveTarget = false;
        m_waitingForWindup = false;
        m_canFinishCurrentAttack = false;
        m_canStartNewAttack = false;
        m_holdingPosition = false;
        m_spellOnlyStopsMovement = false;
        m_currentDangerousThreat = false;
        m_pathHoldExhausted = false;
        m_windupRemainingMs = 0;
        m_currentDangerLevel = 0;
        m_moveTarget = {};
        m_lastIssuedTarget = {};
        m_committedGoal = {};
        m_unavoidableOrigin = {};
        m_hasCommittedGoal = false;
        m_plan = {};
        m_observedPath.clear();
        m_spellOnlyUntilTick = 0;
        m_unavoidableReleaseUntilTick = 0;
        m_lastUnavoidableProbeTick = 0;
        m_safeSinceTick = 0;
        m_targetUnsafeSinceTick = 0;
        m_holdStartTick = 0;
        m_unavoidableThreats.clear();
        if (full) {
            m_lastPlanTick = 0;
            m_lastTargetSwitchTick = 0;
            m_lastStopTick = 0;
            m_lastActionTick = 0;
        }
    }

    void RememberBlockedCommand(const Vec2& position,
                                int targetNetworkId) {
        m_pendingCommand.Type = targetNetworkId != 0
            ? SourcePendingOrderType::Attack
            : SourcePendingOrderType::Move;
        m_pendingCommand.Position = position;
        m_pendingCommand.TargetNetworkId = targetNetworkId;
        m_pendingCommand.Tick = SDK::Variables::TickCount();
    }

    void ClearBlockedCommand() {
        m_pendingCommand = {};
    }

    void ContinueBlockedCommand(const SDK::AIHeroClient& player,
                                const SourceSkillshotList& skillshots,
                                const EvadeSettings& settings) {
        if (!settings.Enabled || !player.IsValid() ||
            m_pendingCommand.Type == SourcePendingOrderType::None ||
            m_pendingCommand.Position.IsZero() ||
            m_pendingCommand.Tick <= 0) return;
        const int elapsed = SDK::Variables::TickCount() -
            m_pendingCommand.Tick;
        const int replayTtl = std::clamp(
            std::max(450, std::max(0, SDK::Game::Ping()) * 2 + 120),
            450, 800);
        if (elapsed < CommandDelayMs() || elapsed > replayTtl) {
            if (elapsed > replayTtl) ClearBlockedCommand();
            return;
        }

        if (m_pendingCommand.Type == SourcePendingOrderType::Attack) {
            const SDK::AIBaseClient target =
                SDK::ObjectManager::GetUnitByNetworkId<SDK::AIBaseClient>(
                    m_pendingCommand.TargetNetworkId);
            if (!target.IsValid() || target.IsDead() ||
                !target.IsTargetable() ||
                (!target.IsEnemy() &&
                 target.Team() != SDK::GameObjectTeam::Neutral)) {
                ClearBlockedCommand();
                return;
            }
            const float attackRange = std::max(0.0f, player.AttackRange()) +
                player.BoundingRadius() + target.BoundingRadius() + 75.0f;
            if (player.ServerPosition().To2D().DistanceSqr(
                    target.ServerPosition().To2D()) >
                attackRange * attackRange) {
                // Never turn an old click into a fresh chase after evasion.
                ClearBlockedCommand();
                return;
            }
            if (CoreControl::IssueAttack(
                    target.Address(), target.ServerPosition(), true)) {
                ClearBlockedCommand();
            }
            return;
        }

        const Vec2 hero = player.ServerPosition().To2D();
        if (!SourceEvader::PathIsDangerous(
                hero, m_pendingCommand.Position,
                std::max(50.0f, player.MoveSpeed()),
                player.BoundingRadius(), skillshots, settings,
                CommandDelayMs()) &&
            CoreControl::IssueMove(
                Vec3::From2D(
                    m_pendingCommand.Position, player.ServerPosition().y),
                true)) {
            ClearBlockedCommand();
        }
    }

    static void SetLastEvent(char* buffer,
                             std::size_t bufferSize,
                             const char* text) {
        if (buffer && bufferSize > 0) {
            strncpy_s(buffer, bufferSize, text ? text : "", _TRUNCATE);
        }
    }
};

} // namespace Plugins::KuroEvade
