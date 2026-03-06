#pragma once
#include "Enums.h"
#include "Game.h"
#include "GameObject.h"
#include "SpellBook.h"
#include "GameObjects.h"
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>

#undef min
#undef max

// ============================================================================
// SummonerTracker — Track enemy summoner spell cooldowns
// Reference: EnsoulSharp.SDK/Core/Utils/SummonerTracker.cs
//
// Tracks Flash, Ignite, Teleport, Smite, Heal, Exhaust, Barrier, etc.
// Used by Awareness plugin to display CD info above enemy HP bars.
//
// Design: Poll-based. Each frame, read summoner spells D/F from enemy
// SpellBook. When a spell goes on cooldown, record the absolute game time
// at which it will be ready.
// ============================================================================

namespace SDK {

    // ========================================================================
    // SummonerSpellInfo — Data about a summoner spell
    // ========================================================================
    struct SummonerSpellInfo {
        const char* InternalName;
        const char* DisplayName;
        float       BaseCooldown;      // Base CD in seconds (before CDR)
        ImU32       Color;             // Display color
    };

    // ========================================================================
    // SummonerSpellState — Tracked state per spell per hero
    // ========================================================================
    struct SummonerSpellState {
        std::string SpellName;          // Internal name (e.g. "SummonerFlash")
        float       ReadyAt   = 0.0f;   // Game time when ready
        float       TotalCD   = 0.0f;   // Total cooldown duration
        bool        IsUp      = true;    // Currently available
        bool        WasUp     = true;    // Previous frame state (for detecting cast)
    };

    // ========================================================================
    // SummonerTrackerEntry — Two summoner spells per hero
    // ========================================================================
    struct SummonerTrackerEntry {
        int                 NetworkId = 0;
        SummonerSpellState  Summoner1;      // D key
        SummonerSpellState  Summoner2;      // F key
    };

    // ========================================================================
    // SummonerTracker
    // ========================================================================
    class SummonerTracker {
    public:
        // ====================================================================
        // Update — call each frame
        // ====================================================================
        static void Update() {
            float now = Game::GetTime();

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid()) continue;
                int netId = hero.GetNetId();
                if (netId == 0) continue;

                auto& entry = s_entries[netId];
                entry.NetworkId = netId;

                // Read D and F spell slots
                SpellBook book(hero.address);
                if (!book.IsValid()) continue;

                UpdateSlot(book.D(), entry.Summoner1, now);
                UpdateSlot(book.F(), entry.Summoner2, now);
            }
        }

        // ====================================================================
        // Get entry for a specific hero (by network ID)
        // ====================================================================
        static const SummonerTrackerEntry* GetEntry(int networkId) {
            auto it = s_entries.find(networkId);
            if (it != s_entries.end()) return &it->second;
            return nullptr;
        }

        // ====================================================================
        // Get all tracked entries
        // ====================================================================
        static const std::unordered_map<int, SummonerTrackerEntry>& GetAllEntries() {
            return s_entries;
        }

        // ====================================================================
        // Get remaining CD for a specific hero's summoner spell
        // Returns 0 if ready, >0 if on CD
        // ====================================================================
        static float GetRemainingCD(int networkId, bool isSecondSlot) {
            auto* entry = GetEntry(networkId);
            if (!entry) return 0.0f;
            const auto& state = isSecondSlot ? entry->Summoner2 : entry->Summoner1;
            float remaining = state.ReadyAt - Game::GetTime();
            return remaining > 0.0f ? remaining : 0.0f;
        }

        // ====================================================================
        // Is a specific summoner spell ready?
        // ====================================================================
        static bool IsReady(int networkId, bool isSecondSlot) {
            return GetRemainingCD(networkId, isSecondSlot) <= 0.0f;
        }

        // ====================================================================
        // Check if enemy has Flash and if it's on CD
        // ====================================================================
        static bool HasFlash(int networkId) {
            auto* entry = GetEntry(networkId);
            if (!entry) return false;
            return (entry->Summoner1.SpellName == SummonerSpells::Flash ||
                    entry->Summoner2.SpellName == SummonerSpells::Flash);
        }

        static float GetFlashCD(int networkId) {
            auto* entry = GetEntry(networkId);
            if (!entry) return -1.0f;
            float now = Game::GetTime();
            if (entry->Summoner1.SpellName == SummonerSpells::Flash) {
                float r = entry->Summoner1.ReadyAt - now;
                return r > 0.0f ? r : 0.0f;
            }
            if (entry->Summoner2.SpellName == SummonerSpells::Flash) {
                float r = entry->Summoner2.ReadyAt - now;
                return r > 0.0f ? r : 0.0f;
            }
            return -1.0f;
        }

        // ====================================================================
        // Get display info for a summoner spell name
        // ====================================================================
        static const SummonerSpellInfo* GetSpellInfo(const std::string& name) {
            for (auto& info : s_spellInfoDB) {
                if (name == info.InternalName) return &info;
            }
            return nullptr;
        }

        // ====================================================================
        // Get display name for a summoner spell
        // ====================================================================
        static std::string GetDisplayName(const std::string& internalName) {
            auto* info = GetSpellInfo(internalName);
            if (info) return info->DisplayName;
            // Fallback: strip "Summoner" prefix
            if (internalName.size() > 8 && internalName.substr(0, 8) == "Summoner")
                return internalName.substr(8);
            return internalName;
        }

        // ====================================================================
        // Get color for summoner spell
        // ====================================================================
        static ImU32 GetSpellColor(const std::string& internalName) {
            auto* info = GetSpellInfo(internalName);
            if (info) return info->Color;
            return IM_COL32(200, 200, 200, 255); // Default gray
        }

        // ====================================================================
        // Format cooldown nicely: "3:20" or "45s" or "Ready"
        // ====================================================================
        static std::string FormatCD(float remainingCD) {
            if (remainingCD <= 0.0f) return "Ready";
            if (remainingCD >= 60.0f) {
                int min = (int)(remainingCD / 60.0f);
                int sec = (int)remainingCD % 60;
                char buf[16];
                snprintf(buf, sizeof(buf), "%d:%02d", min, sec);
                return buf;
            }
            char buf[8];
            snprintf(buf, sizeof(buf), "%.0fs", remainingCD);
            return buf;
        }

        // ====================================================================
        // Clear all tracking data
        // ====================================================================
        static void Clear() {
            s_entries.clear();
        }

    private:
        // ====================================================================
        // Update a single summoner slot
        // ====================================================================
        static void UpdateSlot(SpellSlot slot, SummonerSpellState& state, float now) {
            if (!slot.IsValid()) return;

            // Read spell name (only if not cached yet)
            if (state.SpellName.empty()) {
                state.SpellName = slot.GetName();
            }

            // Read cooldown from SpellSlot
            float readyAt = slot.GetReadyAt();
            float remainingCD = slot.GetRemainingCooldown();
            float totalCD = slot.GetTotalCooldown();

            bool isUp = (remainingCD <= 0.0f);

            // Detect cast event: was up last frame, now on CD
            if (state.WasUp && !isUp && readyAt > now) {
                state.ReadyAt = readyAt;
                state.TotalCD = totalCD > 0.0f ? totalCD : GetBaseCooldown(state.SpellName);
            }

            // If spell is on CD, update from game memory (more accurate)
            if (!isUp && readyAt > now) {
                state.ReadyAt = readyAt;
                if (totalCD > 0.0f) state.TotalCD = totalCD;
            }

            state.IsUp = isUp;
            state.WasUp = isUp;
        }

        // ====================================================================
        // Get base cooldown for summoner spell
        // ====================================================================
        static float GetBaseCooldown(const std::string& name) {
            auto* info = GetSpellInfo(name);
            if (info) return info->BaseCooldown;
            return 300.0f; // Default 5 min
        }

        // ====================================================================
        // Storage
        // ====================================================================
        static inline std::unordered_map<int, SummonerTrackerEntry> s_entries;

        // ====================================================================
        // Summoner Spell Info Database (S26 values)
        // ====================================================================
        static inline const std::vector<SummonerSpellInfo> s_spellInfoDB = {
            { "SummonerFlash",    "Flash",    300.0f, IM_COL32(255, 255,  50, 255) },  // Yellow
            { "SummonerDot",      "Ignite",   180.0f, IM_COL32(255,  80,  30, 255) },  // Red-orange
            { "SummonerTeleport", "TP",       360.0f, IM_COL32(100, 100, 255, 255) },  // Blue
            { "SummonerSmite",    "Smite",     15.0f, IM_COL32(180, 100, 255, 255) },  // Purple
            { "SummonerHeal",     "Heal",     240.0f, IM_COL32( 50, 255,  50, 255) },  // Green
            { "SummonerExhaust",  "Exhaust",  210.0f, IM_COL32(200, 180,  50, 255) },  // Gold
            { "SummonerBarrier",  "Barrier",  180.0f, IM_COL32(200, 200, 200, 255) },  // White/gray
            { "SummonerHaste",    "Ghost",    210.0f, IM_COL32(100, 200, 255, 255) },  // Cyan
            { "SummonerBoost",    "Cleanse",  210.0f, IM_COL32(100, 255, 200, 255) },  // Teal
            { "SummonerSnowball", "Snowball",  40.0f, IM_COL32(150, 200, 255, 255) },  // Light blue (ARAM)
        };
    };

} // namespace SDK
