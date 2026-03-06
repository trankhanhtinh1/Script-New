#pragma once
// ============================================================================
// LastCast.h — Port of EnsoulSharp.SDK LastCast.cs
// ============================================================================
// Extension for getting the last casted spell of a hero.
//
// Usage:
//   SDK::LastCast::Init();   // Call once (registers EventSystem callbacks)
//
//   // Get last casted spell of any hero
//   auto entry = SDK::LastCast::GetLastCastedSpell(enemy);
//   if (entry.IsValid) {
//       float elapsed = entry.TimeSinceCast(SDK::Game::GetTime());
//       printf("%s cast %s %.1fs ago\n", enemy.GetChampionName().c_str(),
//              entry.Name.c_str(), elapsed);
//   }
//
//   // Get last cast command sent by local player
//   auto pkt = SDK::LastCast::GetLastCastPacketSent();
//   if (pkt.IsValid) {
//       printf("I cast slot %d at %.1f\n", pkt.Slot, pkt.Tick);
//   }
// ============================================================================

#include "LastCastedSpellEntry.h"
#include "LastCastPacketSentEntry.h"
#include "EventSystem.h"
#include "Game.h"

#include <unordered_map>

namespace SDK {

class LastCast {
public:
    // ---- Initialize — register event callbacks ----
    static void Init() {
        if (s_initialized) return;
        s_initialized = true;

        // Subscribe to OnProcessSpellCast (fires for ALL heroes)
        EventSystem::OnProcessSpellCast([](const SpellCastArgs& args) {
            OnDoCast(args);
        });

        // Subscribe to OnStopCast (optional: mark entry as cancelled)
        EventSystem::OnStopCast([](const StopCastArgs& args) {
            OnStopCast(args);
        });
    }

    // ---- Get last casted spell for a specific hero (by NetworkId) ----
    static LastCastedSpellEntry GetLastCastedSpell(unsigned int netId) {
        auto it = s_castedSpells.find(netId);
        if (it != s_castedSpells.end()) {
            return it->second;
        }
        return LastCastedSpellEntry(); // Invalid/empty
    }

    // ---- Get last casted spell for a GameObject ----
    static LastCastedSpellEntry GetLastCastedSpell(const GameObject& hero) {
        return GetLastCastedSpell((unsigned int)hero.GetNetId());
    }

    // ---- Get last cast packet sent by local player ----
    static const LastCastPacketSentEntry& GetLastCastPacketSent() {
        return s_lastPacketSent;
    }

    // ---- Manually record a cast packet sent (for Orbwalker/SpellCaster) ----
    static void RecordCastPacketSent(int slot, unsigned int targetNetId = 0) {
        s_lastPacketSent = LastCastPacketSentEntry(slot, Game::GetTime(), targetNetId);
    }

    // ---- Check if hero has casted a spell within the last N seconds ----
    static bool HasCastedSpell(const GameObject& hero, float withinSeconds) {
        auto entry = GetLastCastedSpell(hero);
        if (!entry.IsValid) return false;
        return entry.TimeSinceCast(Game::GetTime()) <= withinSeconds;
    }

    // ---- Check if hero has casted a SPECIFIC spell within the last N seconds ----
    static bool HasCastedSpell(const GameObject& hero, const std::string& spellName,
                                float withinSeconds) {
        auto entry = GetLastCastedSpell(hero);
        if (!entry.IsValid) return false;
        if (entry.Name != spellName) return false;
        return entry.TimeSinceCast(Game::GetTime()) <= withinSeconds;
    }

    // ---- Get all tracked entries (readonly access) ----
    static const std::unordered_map<unsigned int, LastCastedSpellEntry>& GetAllEntries() {
        return s_castedSpells;
    }

    // ---- Clear all tracking data ----
    static void Clear() {
        s_castedSpells.clear();
        s_lastPacketSent = LastCastPacketSentEntry();
    }

private:
    // ---- Callback: OnDoCast (all heroes) ----
    static void OnDoCast(const SpellCastArgs& args) {
        if (!args.Sender.IsValid()) return;

        unsigned int netId = (unsigned int)args.Sender.GetNetId();
        if (netId == 0) return;

        // Build entry
        float now = Game::GetTime();
        float castDuration = 0.0f;

        // Estimate end time from SpellBook cooldown if available
        SpellBook sb(args.Sender.address);
        if (sb.IsValid()) {
            auto spell = sb.GetSpell((SpellSlotId)args.Slot);
            if (spell.IsValid()) {
                float totalCd = spell.GetTotalCooldown();
                if (totalCd > 0.0f) castDuration = totalCd;
            }
        }

        LastCastedSpellEntry entry(
            args.SpellName,
            (int)args.Slot,
            (unsigned int)args.TargetNetId,
            now,
            now + castDuration,
            args.StartPos,
            args.EndPos
        );

        s_castedSpells[netId] = entry;

        // If it's the local player, also record as packet sent
        auto& player = GameObjects::Player;
        if (player.IsValid() && (unsigned int)player.GetNetId() == netId) {
            s_lastPacketSent = LastCastPacketSentEntry(
                (int)args.Slot,
                now,
                (unsigned int)args.TargetNetId
            );
        }
    }

    // ---- Callback: OnStopCast ----
    static void OnStopCast(const StopCastArgs& args) {
        // When a spell cast is stopped/cancelled, we can optionally update the entry
        // For now we don't invalidate — the entry remains as "last cast attempted"
        (void)args;
    }

    static inline std::unordered_map<unsigned int, LastCastedSpellEntry> s_castedSpells;
    static inline LastCastPacketSentEntry s_lastPacketSent;
    static inline bool s_initialized = false;
};

} // namespace SDK
