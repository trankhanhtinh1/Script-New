#pragma once

#include "../../libs/nlohmann/json.hpp"
#include "../Core/Game.h"
#include "../Core/Objects.h"
#include "../GameObjects/ObjectManager.h"

#include <string>

namespace SDK::Extensions {

    template <typename T>
    inline T FromJson(const std::string& json) {
        if (json.empty()) {
            return T{};
        }
        return nlohmann::json::parse(json).template get<T>();
    }

    template <typename T>
    inline std::string ToJson(const T& object) {
        return nlohmann::json(object).dump();
    }

    inline bool IsReady(const SpellDataInstClient& spell, int t = 0) {
        if (!spell.IsValid()) {
            return false;
        }

        const float gameTime = Game::Time();
        const auto state = spell.State(gameTime);
        if (state == CoreSpellBook::State_Ready) {
            return true;
        }

        return state == CoreSpellBook::State_Cooldown &&
               spell.RemainingCooldown(gameTime) <= (static_cast<float>(t) / 1000.0f);
    }

    inline bool IsReady(SpellSlot slot, int t = 0) {
        const auto player = ObjectManager::Player();
        return player.IsValid() && IsReady(player.Spellbook().GetSpell(slot), t);
    }

} // namespace SDK::Extensions

#include "Enumerable.h"
#include "Unit.h"
