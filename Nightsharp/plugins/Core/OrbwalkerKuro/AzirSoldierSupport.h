#pragma once

#include "AzirSoldierRules.h"

#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Enumerations/ChampionId.h"

#include <vector>

namespace OrbwalkerKuro::AzirSoldierSupport {

inline std::vector<::SDK::AIMinionClient> GetAzirSandSoldiers(
    const ::SDK::AIHeroClient& player
) {
    std::vector<::SDK::AIMinionClient> result;
    if (!player.IsValid() || player.IsDead() ||
        ::SDK::ChampionIdFromName(player.CharacterName().c_str()) !=
            ::SDK::ChampionId::Azir) {
        return result;
    }

    const ::SDK::GameObjectTeam playerTeam = player.Team();
    for (const auto& minion : ::SDK::GameObjects::AllySpecialMinions()) {
        if (!minion.IsValid() || minion.IsDead() ||
            minion.Team() != playerTeam) {
            continue;
        }
        if (AzirSoldierRules::IsSandSoldierName(minion.CharacterName()) ||
            AzirSoldierRules::IsSandSoldierName(minion.Name())) {
            result.push_back(minion);
        }
    }
    return result;
}

} // namespace OrbwalkerKuro::AzirSoldierSupport
