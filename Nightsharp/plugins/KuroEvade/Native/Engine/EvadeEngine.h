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
        const auto player = SDK::ObjectManager::Player();

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

        SourceEvadeSpell::TryShieldAllies(
            settings, player, skillshots, configResolver,
            allyShieldResolver, lastEvent, lastEventSize);

        const Vec2 hero = player.ServerPosition().To2D();
        const float radius = std::max(1.0f, player.BoundingRadius());
        const float speed = std::max(50.0f, player.MoveSpeed());
        const Vec2 desired = settings.FocusOnEvade
            ? hero
            : ResolveDesiredPosition(player);
        m_observedPath = BuildPath(player, desired);

        int currentDanger = 0;
        float lowestHitTime = FLT_MAX;
        bool directDanger = false;
        for (const SourceSkillshotPtr& skillshot : skillshots) {
            if (!SourceEvader::ShouldConsider(skillshot, settings)) continue;
            if (settings.OnlyEvadeWhileCan &&
                !skillshot->CanHeroEvade(player, settings)) continue;
            if (!skillshot->ContainsStatic(hero, radius, settings)) continue;

            directDanger = true;
            currentDanger = std::max(
                currentDanger, SourceEvader::DangerValue(*skillshot));
            lowestHitTime = std::min(
                lowestHitTime, skillshot->HitTime(hero, settings));
            skillshot->EvadeTime = 0.0f;
            skillshot->SpellHitTime = lowestHitTime;
        }

        const bool pathDanger = !SourceEvader::IsSafePath(
            m_observedPath, settings.CrossingTimeOffset, speed, 0,
            radius, skillshots, settings).IsSafe;
        if (!directDanger && !pathDanger) {
            const bool wasActive = m_state != SourceEvadeState::Idle;
            Reset(false);
            if (wasActive) SetLastEvent(lastEvent, lastEventSize, "safe");
            ContinueBlockedCommand(player, skillshots, settings);
            return false;
        }

        const bool targetStillSafe = m_hasMoveTarget &&
            SourceEvader::IsSafePoint(
                m_moveTarget, radius, skillshots, settings) &&
            !SourceEvader::PathIsDangerous(
                hero, m_moveTarget, speed, radius, skillshots, settings);
        if (!targetStillSafe ||
            now - m_lastPlanTick >= settings.EvadePointChangeInterval) {
            m_plan = SourceEvader::FindBestPosition(
                player, desired, skillshots, settings, false);
            m_lastPlanTick = now;
            if (m_plan.Found) {
                m_moveTarget = m_plan.Best.Position;
                m_hasMoveTarget = true;
            } else {
                m_moveTarget = {};
                m_hasMoveTarget = false;
            }
        }

        const float dangerWindow = lowestHitTime == FLT_MAX
            ? 1000.0f
            : lowestHitTime;
        const bool shouldTrySpell = settings.SmoothEvadeSpell || !m_plan.Found;
        if (shouldTrySpell && SourceEvadeSpell::TryUseBest(
                settings, player,
                m_plan.Found ? m_plan.Best.Position : desired,
                m_plan.Found, std::max(1, currentDanger), dangerWindow,
                skillshots, configResolver, allyShieldResolver,
                lastEvent, lastEventSize)) {
            m_state = SourceEvadeState::Dodging;
            TakeOrbwalkerControl();
            if (!settings.SmoothEvadeSpell || !m_plan.Found) {
                return true;
            }
        }

        if (!m_plan.Found) {
            m_state = SourceEvadeState::RecoveringPath;
            TakeOrbwalkerControl();
            SetLastEvent(lastEvent, lastEventSize, "no safe position");
            return true;
        }

        m_state = directDanger
            ? SourceEvadeState::Dodging
            : SourceEvadeState::RecoveringPath;
        TakeOrbwalkerControl();

        // Config.LowEvadeSmooth intentionally delays walking until impact is
        // close. CoreControl still applies the process-wide 45 ms MoveTo gate.
        if (settings.LowEvadeSmooth && dangerWindow > 500.0f) {
            SetLastEvent(lastEvent, lastEventSize, "delaying walking");
            return true;
        }

        const bool targetChanged = m_lastIssuedTarget.IsZero() ||
            m_lastIssuedTarget.DistanceSqr(m_moveTarget) > 25.0f * 25.0f;
        if (targetChanged || now - m_lastActionTick >= 45) {
            if (CoreControl::IssueMove(
                    Vec3::From2D(m_moveTarget, player.ServerPosition().y), true)) {
                m_lastIssuedTarget = m_moveTarget;
                m_lastActionTick = now;
                lastDodgeTick = now;
                SetLastEvent(lastEvent, lastEventSize,
                    directDanger ? "walking evade" : "safe path detour");
            }
        }
        return true;
    }

    bool ShouldBlockMove(const Vec2& destination,
                         const EvadeSettings& settings,
                         const SourceSkillshotList& skillshots,
                         bool remember = true) {
        const auto player = SDK::ObjectManager::Player();
        if (!settings.Enabled || !CanRun(player, settings) ||
            destination.IsZero()) {
            return false;
        }

        const Vec2 hero = player.ServerPosition().To2D();
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
    bool IsWaitingForWindup() const { return false; }
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
    int m_lastPlanTick = 0;
    int m_lastActionTick = 0;
    int m_blockedMoveTick = 0;
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

    static Vec2 ResolveDesiredPosition(const SDK::AIHeroClient& player) {
        const std::vector<Vec3> path = player.Path();
        if (!path.empty() && !path.back().IsZero()) return path.back().To2D();
        return SDK::Game::CursorPos().To2D();
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
        if (result.size() == 1 && !fallback.IsZero()) result.push_back(fallback);
        return result;
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
        m_moveTarget = {};
        m_lastIssuedTarget = {};
        m_plan = {};
        m_observedPath.clear();
        RestoreOrbwalkerMove();
        if (full) {
            m_lastPlanTick = 0;
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
