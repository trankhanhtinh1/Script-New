#pragma once

#include "../../../SDK/SDK.h"
#include "../../../Core/CoreBuffs.h"

namespace ZDEvade {

struct Situation {
    static bool ShouldDodge(bool dodgeEnabled, bool isChanneling, bool isDodgeDangerousOnly) {
        if (!dodgeEnabled) {
            return false;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }

        if (CommonChecks(isChanneling)) {
            return false;
        }

        return true;
    }

    static bool ShouldUseEvadeSpell(bool evadeSpellsEnabled, bool isChanneling) {
        if (!evadeSpellsEnabled) {
            return false;
        }

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return false;
        }

        if (CommonChecks(isChanneling)) {
            return false;
        }

        return true;
    }

    static bool CommonChecks(bool isChanneling) {
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return true;
        }

        if (isChanneling) {
            return true;
        }

        if (player.IsDead()) {
            return true;
        }

        if (player.IsInvulnerable()) {
            return true;
        }

        if (!player.IsTargetable()) {
            return true;
        }

        if (player.IsDashing()) {
            return true;
        }

        if (HasSpellShield(player)) {
            return true;
        }

        return false;
    }

    static bool HasSpellShield(const SDK::AIHeroClient& player) {
        // Check for spell shield buffs
        if (CoreBuffs::HasBuffType(player.Address(), static_cast<int>(SDK::BuffType::SpellShield))) {
            return true;
        }
        if (CoreBuffs::HasBuffType(player.Address(), static_cast<int>(SDK::BuffType::SpellImmunity))) {
            return true;
        }
        return false;
    }
};

} // namespace ZDEvade
