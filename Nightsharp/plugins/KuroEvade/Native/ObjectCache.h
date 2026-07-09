#pragma once

#include "HeroInfo.h"

#include "../../../SDK/SDK.h"

#include <unordered_map>

namespace Plugins::KuroEvade {

struct ObjectCache final {
    static std::unordered_map<int, SDK::AITurretClient>& Turrets() {
        static std::unordered_map<int, SDK::AITurretClient> turrets;
        return turrets;
    }

    static HeroInfo& PlayerCache() {
        static HeroInfo info(SDK::ObjectManager::Player());
        return info;
    }

    static float& GamePing() {
        static float ping = 0.0f;
        return ping;
    }

    static void Init() {
        Refresh();
    }

    static void Refresh() {
        GamePing() = static_cast<float>(SDK::Game::Ping());
        PlayerCache().UpdateInfo();

        auto& turrets = Turrets();
        for (const auto& turret : SDK::GameObjects::EnemyTurrets()) {
            if (turret.IsValid() && !turret.IsDead()) {
                turrets[turret.NetworkId()] = turret;
            }
        }
        for (auto it = turrets.begin(); it != turrets.end();) {
            if (!it->second.IsValid() || it->second.IsDead()) {
                it = turrets.erase(it);
            } else {
                ++it;
            }
        }
    }
};

} // namespace Plugins::KuroEvade
