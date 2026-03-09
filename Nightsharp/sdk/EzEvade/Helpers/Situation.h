#pragma once
#include "sdk/SDK.h"
#include "sdk/EzEvade/Helpers/EvadeRuntimeState.h"
#include <algorithm>
#include <cfloat>
#include <cstring>
#include <string.h>
#include <memory>
#include <string>

namespace EzEvade {
namespace Situation {

inline std::shared_ptr<SDK::MenuUI::Menu> s_menu;

inline void SetMenu(const std::shared_ptr<SDK::MenuUI::Menu>& menu) {
    s_menu = menu;
}

inline bool MenuBool(const char* key, bool fallback = false) {
    if (!s_menu) return fallback;
    return s_menu->GetBoolValue(key, fallback);
}

inline bool MenuKey(const char* key, bool fallback = false) {
    if (!s_menu) return fallback;
    return s_menu->GetKeyBindValue(key, fallback);
}

inline bool CheckTeam(const SDK::GameObject& unit) {
    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid() || !unit.IsValid()) return false;
    return unit.GetTeam() != me.GetTeam() || EvadeRuntimeState::DevModeOn;
}

inline float GetDistanceToEnemyChampions(const Vec2& pos) {
    float minDist = FLT_MAX;
    for (const auto& hero : SDK::GameObjects::EnemyHeroes) {
        if (!hero.IsValid() || !hero.IsAlive() || !hero.IsVisible()) continue;
        minDist = std::min(minDist, hero.GetServerPosition().To2D().Distance(pos));
    }
    return minDist;
}

inline bool IsNearEnemy(const Vec2& pos, float distance, bool alreadyNear = true) {
    (void)alreadyNear;
    if (!MenuBool("PreventDodgingNearEnemy", true)) {
        return false;
    }

    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid()) return false;

    const float curDist = GetDistanceToEnemyChampions(me.GetServerPosition().To2D());
    const float posDist = GetDistanceToEnemyChampions(pos);

    if (curDist < distance) {
        return curDist > posDist;
    }
    return posDist < distance;
}

inline bool IsUnderTurret(const Vec2& pos, bool checkEnemy = true) {
    if (!MenuBool("PreventDodgingUnderTower", false)) {
        return false;
    }

    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid()) return false;

    const float turretRange = 875.0f + me.GetBoundingRadius();
    for (const auto& turret : SDK::GameObjects::AllTurrets) {
        if (!turret.IsValid() || !turret.IsAlive()) continue;
        if (checkEnemy && turret.GetTeam() == me.GetTeam()) continue;

        if (turret.GetPosition().To2D().Distance(pos) <= turretRange) {
            return true;
        }
    }
    return false;
}

inline bool ChampionSpecificChecks() {
    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid()) return true;

    if (me.GetChampionName() == "Sion" && me.HasBuff("SionR")) {
        return true;
    }
    return false;
}

inline bool HasSpellShield(const SDK::GameObject& unit) {
    if (!unit.IsValid()) return false;

    if (unit.HasBuffOfType(SDK::BuffType::SpellShield)) {
        return true;
    }

    const auto last = SDK::LastCast::GetLastCastedSpell(unit);
    if (!last.IsValid) {
        return false;
    }

    const float now = SDK::Game::GetTime();
    const float sinceCastMs = (now - last.StartTime) * 1000.0f;
    if (sinceCastMs < 0.0f || sinceCastMs > 300.0f) {
        return false;
    }

    const std::string spellName = last.Name;
    if (_stricmp(spellName.c_str(), "SivirE") == 0) return true;
    if (_stricmp(spellName.c_str(), "BlackShield") == 0) return true;
    if (_stricmp(spellName.c_str(), "NocturneShit") == 0) return true;
    return false;
}

inline bool CommonChecks() {
    const auto& me = SDK::GameObjects::Player;
    if (!me.IsValid()) return true;

    const bool comboOnlyEnabled = MenuBool("DodgeOnlyOnComboKeyEnabled", false);
    const bool comboKeyActive = MenuKey("DodgeComboKey", false);

    return EvadeRuntimeState::IsChanneling
        || (comboOnlyEnabled && !comboKeyActive)
        || me.IsDead()
        || me.IsInvulnerable()
        || !me.IsTargetable()
        || HasSpellShield(me)
        || ChampionSpecificChecks()
        || me.IsDashing()
        || EvadeRuntimeState::HasGameEnded;
}

inline bool ShouldDodge() {
    const bool dontDodgeEnabled = MenuBool("DontDodgeKeyEnabled", false);
    const bool dontDodgeActive = MenuKey("DontDodgeKey", false);
    if (dontDodgeEnabled && dontDodgeActive) {
        return false;
    }

    if (!MenuKey("DodgeSkillShots", true) || CommonChecks()) {
        return false;
    }

    return true;
}

inline bool ShouldUseEvadeSpell() {
    const bool dontDodgeEnabled = MenuBool("DontDodgeKeyEnabled", false);
    const bool dontDodgeActive = MenuKey("DontDodgeKey", false);
    if (dontDodgeEnabled && dontDodgeActive) {
        return false;
    }

    if (!MenuKey("ActivateEvadeSpells", true)
        || CommonChecks()
        || (EvadeRuntimeState::LastWindupTime - (float)SDK::Game::GetTickCount()) > 0.0f) {
        return false;
    }

    return true;
}

inline bool IsDodgeDangerousEnabled() {
    if (MenuBool("DodgeDangerous", false)) {
        return true;
    }

    if (MenuBool("DodgeDangerousKeyEnabled", false)) {
        if (MenuKey("DodgeDangerousKey", false) || MenuKey("DodgeDangerousKey2", false)) {
            return true;
        }
    }

    return false;
}

} // namespace Situation
} // namespace EzEvade
