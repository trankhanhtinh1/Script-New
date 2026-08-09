#pragma once

#include "AzirSoldierRules.h"

#include "../../../sdk/Events/Events.h"
#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/Enumerations/ChampionId.h"

#include <cstdint>
#include <vector>

namespace OrbwalkerKuro::AzirSoldierSupport {

inline std::vector<::SDK::AIMinionClient> GetAzirSandSoldiers(
    const ::SDK::AIHeroClient& player
) {
    std::vector<::SDK::AIMinionClient> result;
    result.reserve(4);
    if (!player.IsValid() || player.IsDead() ||
        ::SDK::ChampionIdFromName(player.CharacterName().c_str()) !=
            ::SDK::ChampionId::Azir) {
        return result;
    }

    const ::SDK::GameObjectTeam playerTeam = player.Team();
    const auto appendSoldiers = [&](const auto& candidates) {
        for (const auto& minion : candidates) {
            // Sand soldiers are intentionally reported by League as
            // untargetable. Targetability belongs to the attack target, not to
            // the soldier that receives Azir's attack command.
            if (!minion.IsValid() || minion.IsDead() ||
                minion.Team() != playerTeam) {
                continue;
            }
            if (!AzirSoldierRules::IsSandSoldierName(minion.CharacterName()) &&
                !AzirSoldierRules::IsSandSoldierName(minion.Name())) {
                continue;
            }

            const std::uint32_t networkId = minion.CachedNetworkId();
            bool duplicate = false;
            if (networkId != 0) {
                for (const auto& existing : result) {
                    if (existing.CachedNetworkId() == networkId) {
                        duplicate = true;
                        break;
                    }
                }
            }
            if (!duplicate) {
                result.push_back(minion);
            }
        }
    };

    // IsPet() is checked before the special-minion name table by the facade,
    // so AzirSoldier may legitimately live in either list across patches.
    appendSoldiers(::SDK::GameObjects::AllySpecialMinions());
    appendSoldiers(::SDK::GameObjects::AllyPets());
    return result;
}

} // namespace OrbwalkerKuro::AzirSoldierSupport
