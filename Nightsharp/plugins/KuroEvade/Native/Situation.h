#pragma once

#include "EvadeSettings.h"
#include "ObjectCache.h"
#include "EvadeUtils.h"

#include "../../../SDK/SDK.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <string>

namespace Plugins::KuroEvade {

struct Situation final {
    static bool CheckTeam(const SDK::GameObject& unit, bool devMode = false) {
        const auto player = SDK::ObjectManager::Player();
        return (unit.IsValid() && player.IsValid() && unit.Team() != player.Team()) || devMode;
    }

    static bool CheckEmitterTeam(const SDK::EffectEmitter& emitter, bool devMode = false) {
        if (!emitter.IsValid()) {
            return false;
        }
        std::string name = EvadeUtils::GetObjectName(emitter);
        std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return name.find("red") != std::string::npos ||
               ((name.find("green") != std::string::npos || name.find("ally") != std::string::npos) && devMode) ||
               (name.find("green") == std::string::npos && name.find("ally") == std::string::npos);
    }

    static const char* EmitterColor(bool devMode = false) {
        return devMode ? "green" : "red";
    }

    static const char* EmitterTeam(bool devMode = false) {
        return devMode ? "ally" : "enemy";
    }

    static float DistanceToEnemyChampion(const Vec2& pos) {
        float best = FLT_MAX;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (enemy.IsValid() && !enemy.IsDead() && enemy.IsVisible()) {
                best = std::min(best, enemy.ServerPosition().To2D().Distance(pos));
            }
        }
        return best;
    }

    static float DistanceToEnemyTurret(const Vec2& pos) {
        ObjectCache::Refresh();
        float best = FLT_MAX;
        for (const auto& entry : ObjectCache::Turrets()) {
            const auto& turret = entry.second;
            if (turret.IsValid() && !turret.IsDead() && turret.IsEnemy()) {
                best = std::min(best, turret.Position().To2D().Distance(pos));
            }
        }
        return best;
    }

    static bool IsNearEnemy(const Vec2& pos,
                            const EvadeSettings& settings,
                            bool alreadyNear = true) {
        if (!settings.PreventEnemy) {
            return false;
        }

        const float current = DistanceToEnemyChampion(ObjectCache::PlayerCache().serverPos2D);
        const float candidate = DistanceToEnemyChampion(pos);
        if (current < settings.MinComfortZone) {
            return alreadyNear && current > candidate;
        }
        return candidate < settings.MinComfortZone;
    }

    static bool IsUnderTurret(const Vec2& pos,
                              const EvadeSettings& settings,
                              bool checkEnemy = true) {
        if (!settings.PreventTower) {
            return false;
        }
        const float radius = 875.0f + ObjectCache::PlayerCache().boundingRadius;
        return (!checkEnemy || true) && DistanceToEnemyTurret(pos) <= radius;
    }

    static bool HasSpellShield(const SDK::AIHeroClient& unit) {
        if (!unit.IsValid()) {
            return false;
        }
        return SDK::HasBuffOfType(unit, SDK::BuffType::SpellShield) ||
               SDK::HasBuffOfType(unit, SDK::BuffType::SpellImmunity);
    }

    static bool ChampionSpecificChecks(const SDK::AIHeroClient& player) {
        return !player.IsValid() || player.HasBuff("SionR");
    }

    static bool CommonChecks(const SDK::AIHeroClient& player) {
        return !player.IsValid() ||
               player.IsDead() ||
               player.IsInvulnerable() ||
               !player.IsTargetable() ||
               HasSpellShield(player) ||
               ChampionSpecificChecks(player) ||
               player.IsDashing();
    }

    static bool ShouldDodge(const EvadeSettings& settings) {
        const auto player = SDK::ObjectManager::Player();
        return settings.Enabled && settings.DodgeKeyActive && !CommonChecks(player);
    }

    static bool ShouldUseEvadeSpell(const EvadeSettings& settings) {
        const auto player = SDK::ObjectManager::Player();
        return settings.Enabled && settings.UseEvadeSpells && !CommonChecks(player);
    }
};

} // namespace Plugins::KuroEvade
