#pragma once

// ============================================================================
// SpellDataInstClient — runtime wrapper around a single spell slot
// ============================================================================
// Represents one (owner, SpellSlot) pair and projects the raw
// `CoreSpellBook::SlotRef` reads as ergonomic accessors. Zero state beyond the
// two identity fields — every call resolves the current slot snapshot on
// demand so the wrapper stays consistent with hook-driven cooldown / level
// updates.
//
// Used by `SpellbookClient::GetSpell()` and by `AIBaseClient::GetSpell()` as
// the single return type for "what is this slot doing right now?" queries.
// ============================================================================

#include "../../core/CoreAPI.h"
#include "../Enumerations/SpellSlot.h"

#include <string>

namespace SDK {

class SpellDataInstClient {
public:
    SpellDataInstClient() = default;
    SpellDataInstClient(uintptr_t owner, SpellSlot slot)
        : m_owner(owner), m_slot(slot) {}

    bool IsValid() const {
        return GetSlot().IsValid();
    }

    SpellSlot Slot() const {
        return m_slot;
    }

    int Level() const {
        return GetSlot().GetLevel();
    }

    int Ammo() const {
        return GetSlot().GetAmmo();
    }

    int Stacks() const {
        return GetSlot().GetStacks();
    }

    float Cooldown() const {
        return GetSlot().GetCooldown();
    }

    float TotalCooldown() const {
        return GetSlot().GetTotalCooldown();
    }

    float RemainingCooldown(float gameTime = CoreAPI::Game::GetTime()) const {
        return GetSlot().GetRemainingCooldown(gameTime);
    }

    float ManaCost() const {
        return GetSlot().GetManaCost();
    }

    float CastRange() const {
        return GetSlot().GetCastRange(Level());
    }

    float Width() const {
        return GetSlot().GetLineWidth();
    }

    float MissileSpeed() const {
        return GetSlot().GetMissileSpeed();
    }

    int CastType() const {
        return GetSlot().GetCastType();
    }

    std::string Name() const {
        char buf[128] = {};
        return GetSlot().ReadSpellName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    std::string ScriptName() const {
        char buf[128] = {};
        return GetSlot().ReadScriptName(buf, sizeof(buf)) ? std::string(buf) : std::string();
    }

    CoreAPI::SpellBook::SpellState State(float gameTime = CoreAPI::Game::GetTime()) const {
        return CoreAPI::SpellBook::GetSpellState(m_owner, static_cast<int>(m_slot), gameTime);
    }

    bool IsReady(float gameTime = CoreAPI::Game::GetTime()) const {
        return State(gameTime) == CoreSpellBook::State_Ready;
    }

private:
    CoreSpellBook::SlotRef GetSlot() const {
        return CoreAPI::SpellBook::GetSlot(m_owner, static_cast<int>(m_slot));
    }

    uintptr_t m_owner = 0;
    SpellSlot m_slot = SpellSlot::Unknown;
};

} // namespace SDK
