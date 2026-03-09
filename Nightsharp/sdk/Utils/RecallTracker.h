#pragma once
#include "Enums.h"
#include "Game.h"
#include "GameObject.h"
#include "SpellBook.h"
#include "GameObjects.h"
#include <string>
#include <unordered_map>

// ============================================================================
// RecallTracker — Detect and track enemy recall progress
// Reference: EnsoulSharp.SDK/Core/Utils/RecallTracker.cs
//
// Detects recall by checking if enemy has "recall" buff or the active
// spell cast is a Recall-type spell. Tracks start time, duration, and
// completion status.
//
// Used by Awareness plugin to show recall bars + alerts.
// ============================================================================

namespace SDK {

    // ========================================================================
    // RecallStatus — Current recall state
    // ========================================================================
    enum class RecallStatus {
        Unknown,        // Not tracked yet
        Idle,           // Not recalling
        Recalling,      // Currently recalling
        Completed,      // Recall completed (arrived at base)
        Interrupted     // Recall was cancelled/interrupted
    };

    // ========================================================================
    // RecallInfo — Tracked recall state per enemy hero
    // ========================================================================
    struct RecallInfo {
        int            NetworkId   = 0;
        RecallStatus   Status      = RecallStatus::Unknown;
        float          StartTime   = 0.0f;   // Game time recall started
        float          Duration    = 0.0f;   // Total recall duration (8s normal, 4s baron)
        float          EndTime     = 0.0f;   // When recall should complete
        float          HealthPct   = 0.0f;   // HP% when started recalling
        std::string    RecallType;            // "recall" or "SuperRecall" (baron empowered)
        std::string    ChampionName;          // For display

        // Remaining time until recall completes
        float GetRemainingTime() const {
            if (Status != RecallStatus::Recalling) return 0.0f;
            float remaining = EndTime - Game::GetTime();
            return remaining > 0.0f ? remaining : 0.0f;
        }

        // Progress 0.0 to 1.0
        float GetProgress() const {
            if (Status != RecallStatus::Recalling || Duration <= 0.0f) return 0.0f;
            float elapsed = Game::GetTime() - StartTime;
            float progress = elapsed / Duration;
            if (progress < 0.0f) return 0.0f;
            if (progress > 1.0f) return 1.0f;
            return progress;
        }

        // Time since status last changed (for fading completed/interrupted notifications)
        float GetTimeSinceStatusChange() const {
            return Game::GetTime() - StartTime;
        }
    };

    // ========================================================================
    // RecallTracker
    // ========================================================================
    class RecallTracker {
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

                auto& info = s_entries[netId];
                info.NetworkId = netId;

                if (info.ChampionName.empty()) {
                    info.ChampionName = hero.GetChampionName();
                }

                // Check if hero is recalling by reading buff or spell cast
                bool isRecalling = IsHeroRecalling(hero);
                float recallDuration = GetRecallDuration(hero);

                switch (info.Status) {
                case RecallStatus::Unknown:
                case RecallStatus::Idle:
                case RecallStatus::Completed:
                case RecallStatus::Interrupted:
                    if (isRecalling) {
                        // Start new recall
                        info.Status = RecallStatus::Recalling;
                        info.StartTime = now;
                        info.Duration = recallDuration;
                        info.EndTime = now + recallDuration;
                        info.HealthPct = hero.GetHealth() / hero.GetMaxHealth();
                        info.RecallType = HasBaronBuff(hero) ? "SuperRecall" : "recall";
                    }
                    break;

                case RecallStatus::Recalling:
                    if (!isRecalling) {
                        // Recall ended — check if completed or interrupted
                        float elapsed = now - info.StartTime;
                        if (elapsed >= info.Duration - 0.5f) {
                            // Completed (allow 0.5s tolerance)
                            info.Status = RecallStatus::Completed;
                            info.StartTime = now; // for fade timer
                        } else {
                            // Interrupted
                            info.Status = RecallStatus::Interrupted;
                            info.StartTime = now; // for fade timer
                        }
                    } else if (hero.IsDead()) {
                        // Died while recalling
                        info.Status = RecallStatus::Interrupted;
                        info.StartTime = now;
                    }
                    break;
                }

                // Auto-clear old completed/interrupted after 5 seconds
                if ((info.Status == RecallStatus::Completed || info.Status == RecallStatus::Interrupted)
                    && (now - info.StartTime) > 5.0f) {
                    info.Status = RecallStatus::Idle;
                }
            }
        }

        // ====================================================================
        // Get recall info for a specific enemy
        // ====================================================================
        static const RecallInfo* GetInfo(int networkId) {
            auto it = s_entries.find(networkId);
            if (it != s_entries.end()) return &it->second;
            return nullptr;
        }

        // ====================================================================
        // Get all tracked entries
        // ====================================================================
        static const std::unordered_map<int, RecallInfo>& GetAllEntries() {
            return s_entries;
        }

        // ====================================================================
        // Get all currently recalling heroes
        // ====================================================================
        static std::vector<const RecallInfo*> GetActiveRecalls() {
            std::vector<const RecallInfo*> result;
            for (auto& [id, info] : s_entries) {
                if (info.Status == RecallStatus::Recalling)
                    result.push_back(&info);
            }
            return result;
        }

        // ====================================================================
        // Is anyone recalling?
        // ====================================================================
        static bool AnyRecalling() {
            for (auto& [id, info] : s_entries) {
                if (info.Status == RecallStatus::Recalling) return true;
            }
            return false;
        }

        // ====================================================================
        // Clear all tracking
        // ====================================================================
        static void Clear() {
            s_entries.clear();
        }

    private:
        // ====================================================================
        // Check if a hero is currently recalling
        // Method 1: Check for "recall" buff
        // Method 2: Check active spell cast name contains "Recall"
        // ====================================================================
        static bool IsHeroRecalling(const GameObject& hero) {
            // Method 1: Check buff
            BuffManager bm(hero.address);
            if (bm.HasBuff("recall") || bm.HasBuff("SuperRecall") || bm.HasBuff("OdinRecall"))
                return true;

            // Method 2: Check active spell cast
            SpellBook book(hero.address);
            if (book.IsValid()) {
                uintptr_t activeCast = book.GetActiveSpellCast();
                if (Globals::IsValidPtr(activeCast)) {
                    // Read spell name from active cast
                    uintptr_t spellInfo = Globals::Read<uintptr_t>(activeCast + Offset::SpellCastInfo::SpellData);
                    if (Globals::IsValidPtr(spellInfo)) {
                        SpellInfo si(spellInfo);
                        std::string name = si.GetName();
                        if (!name.empty()) {
                            // Check for various recall spell names
                            if (name.find("Recall") != std::string::npos ||
                                name.find("recall") != std::string::npos ||
                                name == "OdinRecall" ||
                                name == "SuperRecall") {
                                return true;
                            }
                        }
                    }
                }
            }
            return false;
        }

        // ====================================================================
        // Get recall duration based on baron buff
        // ====================================================================
        static float GetRecallDuration(const GameObject& hero) {
            if (HasBaronBuff(hero)) return 4.0f;  // Baron empowered recall
            return 8.0f;  // Normal recall
        }

        // ====================================================================
        // Check for Baron Nashor buff (empowered recall = 4s)
        // ====================================================================
        static bool HasBaronBuff(const GameObject& hero) {
            BuffManager bm(hero.address);
            return bm.HasBuff("exabornesbuff");
        }

        // ====================================================================
        // Storage
        // ====================================================================
        static inline std::unordered_map<int, RecallInfo> s_entries;
    };

} // namespace SDK
