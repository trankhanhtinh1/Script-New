#pragma once

#include "EvadeHelper.h"
#include "EvadeSettings.h"
#include "EvadeSpell.h"
#include "SpellDetector.h"

#include "../../../Core/CoreControl.h"
#include "../../../SDK/SDK.h"

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
            RestoreOrbwalkerMove();
            return false;
        }

        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        EvadeSettings effectiveSettings = settings;
        effectiveSettings.CurrentWindupDelay = settings.CalculateWindupDelay
            ? static_cast<float>(std::max(0, m_lastWindupEndTick - SDK::Variables::TickCount()))
            : 0.0f;
        EvadeHelper helper(effectiveSettings);

        if (!helper.IsEndangered(player, heroPos, boundingRadius, skillshots)) {
            m_hasLastPosition = false;
            RestoreOrbwalkerMove();
            return false;
        }

        const int now = SDK::Variables::TickCount();
        if (settings.DodgeInterval > 0 && now - lastDodgeTick < settings.DodgeInterval) {
            return false;
        }

        const int currentDanger = helper.HighestDangerLevelAt(heroPos, boundingRadius, skillshots);
        const float lowestHitTime = helper.LowestHitTimeAt(heroPos, boundingRadius, skillshots);
        Vec2 best;
        if (helper.FindBestPosition(player, heroPos, boundingRadius, skillshots, best)) {
            if (effectiveSettings.UseEvadeSpells &&
                EvadeSpell::TryUseBest(effectiveSettings, player, best, true, currentDanger, lowestHitTime,
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
            CoreControl::IssueMove(Vec3::From2D(best, planeY), true);
            m_lastPosition = best;
            m_hasLastPosition = true;
            lastDodgeTick = now;
            SetLastEvent(lastEvent, lastEventSize, "dodging");
            return true;
        }

        if (effectiveSettings.UseEvadeSpells &&
            EvadeSpell::TryUseBest(effectiveSettings, player, heroPos, false, currentDanger, lowestHitTime,
                                   skillshots, evadeSpellConfig, lastEvent, lastEventSize)) {
            DisableOrbwalkerMove();
            lastDodgeTick = now;
            return true;
        }

        SetLastEvent(lastEvent, lastEventSize, "no safe position");
        RestoreOrbwalkerMove();
        return false;
    }

private:
    int m_lastWindupEndTick = 0;
    Vec2 m_lastPosition;
    bool m_hasLastPosition = false;

    static void SetLastEvent(char* lastEvent, std::size_t lastEventSize, const char* text) {
        if (!lastEvent || lastEventSize == 0) {
            return;
        }
        strncpy_s(lastEvent, lastEventSize, text ? text : "", _TRUNCATE);
    }
};

} // namespace Plugins::KuroEvade
