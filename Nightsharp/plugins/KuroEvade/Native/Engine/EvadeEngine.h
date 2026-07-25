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
#include <cstddef>
#include <cstring>
#include <utility>
#include <vector>

namespace Plugins::KuroEvade {

enum class SourceEvadeState {
    Idle,
    Dodging,
    RecoveringPath,
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
        if (!CanRun(player, settings)) {
            Reset(false);
            ClearBlockedCommand();
            return false;
        }
        m_waitingForWindup = false;
        m_windupRemainingMs = 0;
        m_canFinishCurrentAttack = false;

        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float speed = std::max(50.0f, player.MoveSpeed());
        // Cursor is always the planner goal. FocusOnEvade only reduces its
        // weight so safety/short exits dominate; it no longer replaces the
        // goal with the hero position. Danger is still tested against the
        // actual server path, not an imaginary path to a merely hovered mouse.
        const Vec2 goal = ResolveGoalPosition(player);
        m_observedPath = BuildPath(player, goal);

        int currentDanger = 0;
        float lowestHitTime = FLT_MAX;
        SourceSkillshotList incomingThreats;
        int baselineThreats = SourceEvader::CountPathThreats(
            m_observedPath, settings.CrossingTimeOffset, speed, 0,
            radius, skillshots, settings, &currentDanger,
            &lowestHitTime, &incomingThreats);
        bool directDanger = false;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!SourceEvader::ShouldConsider(skillshot, settings)) continue;
            if (!skillshot->ContainsStatic(hero, radius, settings)) continue;

            directDanger = true;
            skillshot->EvadeTime = 0.0f;
            skillshot->SpellHitTime = skillshot->HitTime(hero, settings);
        }

        bool pathDanger = baselineThreats > 0;
        if (m_holdingPosition && !pathDanger && !goal.IsZero() &&
            hero.DistanceSqr(goal) > radius * radius) {
            // Stop clears the server path. While already holding, continue to
            // test the user's goal route so the engine does not release for a
            // frame, move again, then stop again on the same blocked corridor.
            baselineThreats = SourceEvader::CountPathThreats(
                { hero, goal }, settings.CrossingTimeOffset, speed, 0,
                radius, skillshots, settings, &currentDanger,
                &lowestHitTime, &incomingThreats);
            pathDanger = baselineThreats > 0;
        }
        if (!directDanger && !pathDanger) {
            // Ally shielding remains independent, but it must not consume the
            // shared cast window before the local coverage solver can protect
            // the player from an incoming threat.
            SourceEvadeSpell::TryShieldAllies(
                settings, player, skillshots, configResolver,
                allyShieldResolver, lastEvent, lastEventSize);
            const bool wasActive = m_state != SourceEvadeState::Idle;
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
        const bool targetStillUsable = m_hasMoveTarget &&
            currentTargetInfo.Navigable &&
            currentTargetInfo.PathThreatCount < baselineThreats;
        const bool shouldReplan =
            (!m_holdingPosition && !targetStillUsable) ||
            now - m_lastPlanTick >= settings.EvadePointChangeInterval;
        if (shouldReplan) {
            SourceEvadePlan nextPlan = SourceEvader::FindBestPosition(
                player, goal, skillshots, settings, true);
            m_lastPlanTick = now;
            if (nextPlan.HasCandidate) {
                const bool targetLockActive = m_lastTargetSwitchTick > 0 &&
                    now - m_lastTargetSwitchTick < std::max(
                        160, settings.EvadePointChangeInterval);
                SourcePositionInfo chosen = SelectStableTarget(
                    nextPlan, targetStillUsable, targetLockActive,
                    player, hero, goal, skillshots, settings);
                const bool switched = !m_hasMoveTarget ||
                    m_moveTarget.DistanceSqr(chosen.Position) > 40.0f * 40.0f;
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
                }
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

        if (ShouldHoldPosition(
                directDanger, pathDanger, player, hero, goal, radius,
                skillshots, settings)) {
            m_unavoidableReleaseUntilTick = 0;
            const bool enteringHold = !m_holdingPosition;
            m_holdingPosition = true;
            m_state = SourceEvadeState::RecoveringPath;
            m_hasMoveTarget = false;
            m_moveTarget = {};

            // Only movement is disabled. AttackEnabled is deliberately left
            // untouched so OrbwalkerKuro can acquire a target and attack from
            // this safe pocket. Never send Stop during the attack windup.
            TakeOrbwalkerControl();
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
        m_holdingPosition = false;

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
            m_canFinishCurrentAttack = delayedRouteSafe && hasReleaseMargin;
            m_windupRemainingMs = windupRemaining;

            if (m_canFinishCurrentAttack) {
                m_unavoidableReleaseUntilTick = 0;
                m_state = directDanger
                    ? SourceEvadeState::Dodging
                    : SourceEvadeState::RecoveringPath;
                m_waitingForWindup = true;
                // OrbwalkerKuro already owns the attack windup and will not
                // move early. Keep it enabled while OnBeforeMove blocks any
                // competing movement order until the safe release point.
                RestoreOrbwalkerMove();
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
        const int movementThreats = m_plan.HasCandidate
            ? m_plan.Best.PathThreatCount
            : baselineThreats;
        const int movementBenefit = std::max(
            0, baselineThreats - movementThreats);

        // A non-movement evade spell owns the current collision window. Do not
        // fall back to a partial walking route on the next frame while the
        // shield, wall, blink or invulnerability is still resolving.
        if (now < m_spellOnlyUntilTick) {
            m_unavoidableReleaseUntilTick = 0;
            m_state = SourceEvadeState::Dodging;
            TakeOrbwalkerControl();
            if (m_spellOnlyStopsMovement && windupRemaining <= 0 &&
                player.IsMoving() && now - m_lastStopTick >= 180 &&
                CoreControl::StopMoving(true)) {
                m_lastStopTick = now;
                lastDodgeTick = now;
            }
            SetLastEvent(lastEvent, lastEventSize, "evade spell resolving");
            return true;
        }

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
                TakeOrbwalkerControl();
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

        if (!m_plan.HasCandidate || movementBenefit <= 0) {
            // No action changes the outcome. Holding movement/attacks here only
            // harms the player, so publish Idle and bypass move interception
            // through the impact window.
            Reset(false);
            ClearBlockedCommand();
            m_unavoidableReleaseUntilTick = now + std::clamp(
                static_cast<int>(dangerWindow) + 180, 220, 1400);
            SetLastEvent(lastEvent, lastEventSize,
                         "unavoidable: orbwalker released");
            return false;
        }

        m_unavoidableReleaseUntilTick = 0;
        if (!m_plan.Found) {
            m_state = SourceEvadeState::RecoveringPath;
            TakeOrbwalkerControl();
        }

        m_state = directDanger
            ? SourceEvadeState::Dodging
            : SourceEvadeState::RecoveringPath;
        TakeOrbwalkerControl();

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
                SetLastEvent(lastEvent, lastEventSize, "delaying walking safely");
                return true;
            }
        }

        const bool targetChanged = m_lastIssuedTarget.IsZero() ||
            m_lastIssuedTarget.DistanceSqr(m_moveTarget) > 40.0f * 40.0f;
        // The process-wide gate remains 45 ms, while this engine refreshes an
        // unchanged target at 90 ms to reduce command noise and visible zigzag.
        if (targetChanged || now - m_lastActionTick >= 90) {
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
                         bool remember = true) {
        const auto player = GameObjects::Player();
        if (!settings.Enabled || !CanRun(player, settings) ||
            destination.IsZero()) {
            return false;
        }
        if (SDK::Variables::TickCount() < m_unavoidableReleaseUntilTick) {
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
            return true;
        }
        const bool dangerous = SourceEvader::PathIsDangerous(
            hero, destination, std::max(50.0f, player.MoveSpeed()),
            player.BoundingRadius(), skillshots, settings);
        if (m_state != SourceEvadeState::Idle) {
            if (settings.UseCurrentPath && !settings.FocusOnEvade && !dangerous &&
                SourceEvader::IsSafePoint(
                    destination, player.BoundingRadius(), skillshots, settings)) {
                m_moveTarget = destination;
                m_hasMoveTarget = true;
                return false;
            }
            if (remember) RememberBlockedCommand(destination);
            return true;
        }
        if (dangerous && remember) RememberBlockedCommand(destination);
        return dangerous;
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
    bool IsMovementBlocking() const {
        return m_state == SourceEvadeState::RecoveringPath;
    }
    bool IsIntervening() const { return m_state != SourceEvadeState::Idle; }
    SourceEvadeState State() const { return m_state; }
    const SourceEvadePlan& LastPlan() const { return m_plan; }
    const std::vector<Vec2>& ObservedPath() const { return m_observedPath; }

    static void RestoreOrbwalkerMove() {
        if (!s_orbwalkerDisabled) return;
        SDK::Orbwalker::MoveEnabled(s_orbwalkerWasMoveEnabled);
        s_orbwalkerDisabled = false;
    }

private:
    SourceEvadeState m_state = SourceEvadeState::Idle;
    SourceEvadePlan m_plan;
    Vec2 m_moveTarget;
    Vec2 m_lastIssuedTarget;
    Vec2 m_blockedMovePos;
    bool m_hasMoveTarget = false;
    bool m_waitingForWindup = false;
    bool m_canFinishCurrentAttack = false;
    bool m_holdingPosition = false;
    bool m_spellOnlyStopsMovement = false;
    int m_windupRemainingMs = 0;
    int m_lastPlanTick = 0;
    int m_lastTargetSwitchTick = 0;
    int m_lastStopTick = 0;
    int m_lastActionTick = 0;
    int m_blockedMoveTick = 0;
    int m_spellOnlyUntilTick = 0;
    int m_unavoidableReleaseUntilTick = 0;
    std::vector<Vec2> m_observedPath;

    static inline bool s_orbwalkerWasMoveEnabled = true;
    static inline bool s_orbwalkerDisabled = false;

    static bool CanRun(const SDK::AIHeroClient& player,
                       const EvadeSettings& settings) {
        if (!player.IsValid() || player.IsDead() || player.IsInvulnerable() ||
            !player.IsTargetable() || Helpers::IsSpellShielded(player) ||
            player.IsDashing()) {
            return false;
        }
        if (settings.DisableEvadeForOlafR &&
            _stricmp(player.CharacterName().c_str(), "Olaf") == 0 &&
            player.HasBuff("OlafRagnarok")) {
            return false;
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
            bool oldTargetUsable,
            bool targetLockActive,
            const SDK::AIHeroClient& player,
            const Vec2& hero,
            const Vec2& goal,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) const {
        SourcePositionInfo chosen = nextPlan.Best;
        if (!m_hasMoveTarget || !oldTargetUsable) {
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

        // Ring topology is stronger than smoothing: never smooth an outer
        // escape back into the inner pocket, and immediately accept an outer
        // upgrade when one becomes reachable.
        const bool outerUpgrade =
            chosen.OuterRingExits > old.OuterRingExits ||
            chosen.InnerRingShelters < old.InnerRingShelters;
        if (old.OuterRingExits > chosen.OuterRingExits ||
            old.InnerRingShelters < chosen.InnerRingShelters) {
            return old;
        }
        if (outerUpgrade) {
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
            const float blendedDistance =
                old.DistanceToPlayer * (1.0f - alpha) +
                chosen.DistanceToPlayer * alpha;
            const Vec2 blended = hero + blendedDirection * blendedDistance;
            SourcePositionInfo smooth = SourceEvader::EvaluatePosition(
                player, blended, goal, skillshots, settings);
            if (smooth.Navigable &&
                smooth.PathThreatCount <= chosen.PathThreatCount &&
                (!chosen.SafePoint || smooth.SafePoint) &&
                smooth.DistanceToPlayer > radius &&
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
            const SDK::AIHeroClient& player,
            const Vec2& hero,
            const Vec2& goal,
            float radius,
            const SourceSkillshotList& skillshots,
            const EvadeSettings& settings) const {
        if (directDanger || !pathDanger ||
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

        const float height = player.ServerPosition().y;
        const SourceGeometry::NavigationProbe wall =
            SourceGeometry::ProbeNavigation(
                hero, height, std::max(140.0f, radius + 100.0f));
        const bool nearWall = wall.BlockedRays > 0 &&
            wall.Clearance < radius + 75.0f;
        if (!nearWall) {
            return false;
        }

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
            return true;
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
        return sharpDetour || crampedExit || headsIntoWall;
    }

    static void TakeOrbwalkerControl() {
        if (s_orbwalkerDisabled) return;
        s_orbwalkerWasMoveEnabled = SDK::Orbwalker::MoveEnabled();
        SDK::Orbwalker::MoveEnabled(false);
        s_orbwalkerDisabled = true;
    }

    void Reset(bool full) {
        m_state = SourceEvadeState::Idle;
        m_hasMoveTarget = false;
        m_waitingForWindup = false;
        m_canFinishCurrentAttack = false;
        m_holdingPosition = false;
        m_spellOnlyStopsMovement = false;
        m_windupRemainingMs = 0;
        m_moveTarget = {};
        m_lastIssuedTarget = {};
        m_plan = {};
        m_observedPath.clear();
        m_spellOnlyUntilTick = 0;
        m_unavoidableReleaseUntilTick = 0;
        RestoreOrbwalkerMove();
        if (full) {
            m_lastPlanTick = 0;
            m_lastTargetSwitchTick = 0;
            m_lastStopTick = 0;
            m_lastActionTick = 0;
        }
    }

    void RememberBlockedCommand(const Vec2& position) {
        m_blockedMovePos = position;
        m_blockedMoveTick = SDK::Variables::TickCount();
    }

    void ClearBlockedCommand() {
        m_blockedMovePos = {};
        m_blockedMoveTick = 0;
    }

    void ContinueBlockedCommand(const SDK::AIHeroClient& player,
                                const SourceSkillshotList& skillshots,
                                const EvadeSettings& settings) {
        if (!settings.Enabled || !player.IsValid() ||
            m_blockedMovePos.IsZero() || m_blockedMoveTick <= 0) return;
        const int elapsed = SDK::Variables::TickCount() - m_blockedMoveTick;
        if (elapsed < std::max(0, SDK::Game::Ping()) || elapsed > 1000) {
            if (elapsed > 1000) ClearBlockedCommand();
            return;
        }
        const Vec2 hero = player.ServerPosition().To2D();
        if (!SourceEvader::PathIsDangerous(
                hero, m_blockedMovePos, std::max(50.0f, player.MoveSpeed()),
                player.BoundingRadius(), skillshots, settings) &&
            CoreControl::IssueMove(
                Vec3::From2D(m_blockedMovePos, player.ServerPosition().y), true)) {
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
