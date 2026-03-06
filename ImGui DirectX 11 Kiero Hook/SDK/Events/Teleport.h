#pragma once
// ============================================================================
// Teleport.h — Teleport / Channel Tracking System
// ============================================================================
// Reference: EnsoulSharp.SDK/Core/Events/Teleport.cs
//
// Detects and tracks all forms of teleportation and channeled relocations:
//   - Summoner Teleport (TP)
//   - Shen R (Stand United)
//   - Twisted Fate R (Destiny / Gate)
//   - Pantheon R (Grand Starfall)
//   - Ryze R (Realm Warp)
//   - Tahm Kench R (Devour + teleport)
//   - Hexgate (Hextech gates on Summoner's Rift)
//   - Recall (normal, baron-empowered)
//
// Usage:
//   SDK::Teleport::Init();            // Once during init
//   SDK::Teleport::Update();          // Each frame
//
//   // Check specific hero:
//   auto info = SDK::Teleport::GetInfo(enemyNetId);
//   if (info.Status == TeleportStatus::Start) { ... }
//
//   // Register callback:
//   SDK::Teleport::OnTeleport([](const TeleportEventArgs& args) {
//       printf("%s is using %s!\n", args.ChampionName.c_str(),
//              TeleportTypeToString(args.Type));
//   });
// ============================================================================

#include "Enums.h"
#include "Game.h"
#include "GameObject.h"
#include "SpellBook.h"
#include "BuffManager.h"
#include "GameObjects.h"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <algorithm>

namespace SDK {

    // ========================================================================
    // TeleportType — What kind of teleport/channel is occurring
    // ========================================================================
    enum class TeleportType {
        Unknown,
        Recall,             // Normal recall (B key)
        SuperRecall,        // Baron-empowered recall (faster)
        Teleport,           // Summoner spell Teleport
        ShenR,              // Shen R (Stand United)
        TwistedFateR,       // TF R (Destiny / Gate)
        PantheonR,          // Pantheon R (Grand Starfall)
        RyzeR,              // Ryze R (Realm Warp)
        TahmKenchR,         // Tahm Kench R (Abyssal Voyage)
        Hexgate,            // Hextech Hexgate
        GalefoceR,          // (future items/abilities with teleport)
    };

    // ========================================================================
    // TeleportStatus — Current state of the teleport
    // ========================================================================
    enum class TeleportStatus {
        Unknown,
        Start,              // Channel started
        Abort,              // Channel was interrupted / cancelled
        Finish              // Channel completed, hero teleported
    };

    // ========================================================================
    // TeleportEventArgs — Event data passed to callbacks
    // ========================================================================
    struct TeleportEventArgs {
        unsigned int    NetworkId       = 0;
        std::string     ChampionName;
        TeleportType    Type            = TeleportType::Unknown;
        TeleportStatus  Status          = TeleportStatus::Unknown;
        float           Duration        = 0.0f;   // Total channel time
        float           StartTime       = 0.0f;   // Game time when started
        float           EndTime         = 0.0f;   // Expected completion time
        Vec3            StartPosition;             // Where hero is channeling from
        Vec3            TargetPosition;            // Where hero is teleporting to (if known)
    };

    // ========================================================================
    // TeleportInfo — Per-hero tracking state
    // ========================================================================
    struct TeleportInfo {
        unsigned int    NetworkId       = 0;
        std::string     ChampionName;
        TeleportType    Type            = TeleportType::Unknown;
        TeleportStatus  Status          = TeleportStatus::Unknown;
        float           Duration        = 0.0f;
        float           StartTime       = 0.0f;
        float           EndTime         = 0.0f;
        Vec3            StartPosition;
        Vec3            TargetPosition;

        // Helpers
        float GetRemainingTime() const {
            if (Status != TeleportStatus::Start) return 0.0f;
            float remaining = EndTime - Game::GetTime();
            return remaining > 0.0f ? remaining : 0.0f;
        }

        float GetProgress() const {
            if (Duration <= 0.0f || Status != TeleportStatus::Start) return 0.0f;
            float elapsed = Game::GetTime() - StartTime;
            return std::min(1.0f, std::max(0.0f, elapsed / Duration));
        }

        bool IsActive() const { return Status == TeleportStatus::Start; }
        bool IsCompleted() const { return Status == TeleportStatus::Finish; }
        bool IsAborted() const { return Status == TeleportStatus::Abort; }
    };

    // ========================================================================
    // Teleport — Main tracker class
    // ========================================================================
    class Teleport {
    public:
        using TeleportCallback = std::function<void(const TeleportEventArgs&)>;

        // ====================================================================
        // Init — register callbacks
        // ====================================================================
        static void Init() {
            if (s_initialized) return;
            s_initialized = true;
        }

        // ====================================================================
        // Update — call each frame
        // ====================================================================
        static void Update() {
            float now = Game::GetTime();

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid()) continue;

                unsigned int netId = (unsigned int)hero.GetNetId();
                if (netId == 0) continue;

                auto& info = s_teleports[netId];
                info.NetworkId = netId;
                info.ChampionName = hero.GetChampionName();

                // Detect teleport/recall from buffs and active spells
                TeleportType detectedType = DetectTeleportType(hero);

                if (detectedType != TeleportType::Unknown) {
                    // Channel detected
                    if (info.Status != TeleportStatus::Start) {
                        // NEW channel start
                        info.Type = detectedType;
                        info.Status = TeleportStatus::Start;
                        info.Duration = GetChannelDuration(detectedType);
                        info.StartTime = now;
                        info.EndTime = now + info.Duration;
                        info.StartPosition = hero.GetPosition();
                        info.TargetPosition = Vec3(); // Unknown for most

                        FireEvent(info, TeleportStatus::Start);
                    }
                    // Else: already tracking, check for completion
                    else if (now >= info.EndTime) {
                        info.Status = TeleportStatus::Finish;
                        FireEvent(info, TeleportStatus::Finish);
                    }
                }
                else {
                    // No channel detected
                    if (info.Status == TeleportStatus::Start) {
                        // Was channeling but stopped → either finished or aborted
                        if (now >= info.EndTime - 0.1f) {
                            // Close enough to end time → completed
                            info.Status = TeleportStatus::Finish;
                            FireEvent(info, TeleportStatus::Finish);
                        } else {
                            // Interrupted early
                            info.Status = TeleportStatus::Abort;
                            FireEvent(info, TeleportStatus::Abort);
                        }
                    }
                    // Reset after event fired
                    if (info.Status == TeleportStatus::Finish ||
                        info.Status == TeleportStatus::Abort) {
                        // Keep status for 1 second so consumers can read it
                        if (now - info.EndTime > 1.0f && now - info.StartTime > info.Duration + 1.0f) {
                            info.Status = TeleportStatus::Unknown;
                        }
                    }
                }
            }

            // Also track ally heroes (for Shen R partner, etc.)
            for (auto& hero : GameObjects::AllyHeroes) {
                if (!hero.IsValid()) continue;
                if (hero.address == GameObjects::Player.address) continue;

                unsigned int netId = (unsigned int)hero.GetNetId();
                if (netId == 0) continue;

                auto& info = s_teleports[netId];
                info.NetworkId = netId;
                info.ChampionName = hero.GetChampionName();

                TeleportType detectedType = DetectTeleportType(hero);

                if (detectedType != TeleportType::Unknown) {
                    if (info.Status != TeleportStatus::Start) {
                        info.Type = detectedType;
                        info.Status = TeleportStatus::Start;
                        info.Duration = GetChannelDuration(detectedType);
                        info.StartTime = now;
                        info.EndTime = now + info.Duration;
                        info.StartPosition = hero.GetPosition();
                        FireEvent(info, TeleportStatus::Start);
                    } else if (now >= info.EndTime) {
                        info.Status = TeleportStatus::Finish;
                        FireEvent(info, TeleportStatus::Finish);
                    }
                } else {
                    if (info.Status == TeleportStatus::Start) {
                        if (now >= info.EndTime - 0.1f) {
                            info.Status = TeleportStatus::Finish;
                            FireEvent(info, TeleportStatus::Finish);
                        } else {
                            info.Status = TeleportStatus::Abort;
                            FireEvent(info, TeleportStatus::Abort);
                        }
                    }
                    if (info.Status == TeleportStatus::Finish ||
                        info.Status == TeleportStatus::Abort) {
                        if (now - info.StartTime > info.Duration + 1.0f) {
                            info.Status = TeleportStatus::Unknown;
                        }
                    }
                }
            }
        }

        // ====================================================================
        // Get tracking info for a specific hero
        // ====================================================================
        static TeleportInfo GetInfo(unsigned int netId) {
            auto it = s_teleports.find(netId);
            if (it != s_teleports.end()) return it->second;
            return TeleportInfo();
        }

        static TeleportInfo GetInfo(const GameObject& hero) {
            return GetInfo((unsigned int)hero.GetNetId());
        }

        // ====================================================================
        // Get all active teleports
        // ====================================================================
        static std::vector<TeleportInfo> GetActiveTeleports() {
            std::vector<TeleportInfo> result;
            for (auto& [id, info] : s_teleports) {
                if (info.IsActive()) result.push_back(info);
            }
            return result;
        }

        // ====================================================================
        // Is any enemy teleporting?
        // ====================================================================
        static bool IsAnyEnemyTeleporting() {
            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid()) continue;
                auto info = GetInfo(hero);
                if (info.IsActive()) return true;
            }
            return false;
        }

        // ====================================================================
        // Register callback (EnsoulSharp: Teleport.OnTeleport += handler)
        // ====================================================================
        static void OnTeleport(TeleportCallback callback) {
            s_callbacks.push_back(callback);
        }

        // ====================================================================
        // Clear all tracking
        // ====================================================================
        static void Clear() {
            s_teleports.clear();
        }

        // ====================================================================
        // Utility: TeleportType to string
        // ====================================================================
        static const char* TypeToString(TeleportType type) {
            switch (type) {
            case TeleportType::Recall:        return "Recall";
            case TeleportType::SuperRecall:   return "Super Recall";
            case TeleportType::Teleport:      return "Teleport";
            case TeleportType::ShenR:         return "Shen R";
            case TeleportType::TwistedFateR:  return "TF R";
            case TeleportType::PantheonR:     return "Pantheon R";
            case TeleportType::RyzeR:         return "Ryze R";
            case TeleportType::TahmKenchR:    return "Tahm Kench R";
            case TeleportType::Hexgate:       return "Hexgate";
            default:                          return "Unknown";
            }
        }

        static const char* StatusToString(TeleportStatus status) {
            switch (status) {
            case TeleportStatus::Start:  return "Channeling";
            case TeleportStatus::Abort:  return "Aborted";
            case TeleportStatus::Finish: return "Completed";
            default:                     return "Unknown";
            }
        }

    private:
        // ====================================================================
        // Detect what type of teleport/channel a hero is currently doing
        // ====================================================================
        static TeleportType DetectTeleportType(const GameObject& hero) {
            BuffManager buffs(hero.address);

            // ---- Summoner Teleport ----
            if (buffs.HasBuff("summonerteleport") ||
                buffs.HasBuff("SummonerTeleport") ||
                buffs.HasBuff("teleport_target"))
                return TeleportType::Teleport;

            // ---- Recall (normal + baron) ----
            if (buffs.HasBuff("SuperRecall") || buffs.HasBuff("OdinSuperRecall"))
                return TeleportType::SuperRecall;

            if (buffs.HasBuff("recall") || buffs.HasBuff("OdinRecall"))
                return TeleportType::Recall;

            // ---- Shen R (Stand United) ----
            if (buffs.HasBuff("ShenRChannel") ||
                buffs.HasBuff("shenrchannelbuff") ||
                buffs.HasBuff("ShenStandUnited"))
                return TeleportType::ShenR;

            // ---- Twisted Fate R (Destiny / Gate) ----
            if (buffs.HasBuff("gate") ||
                buffs.HasBuff("TwistedFateR") ||
                buffs.HasBuff("Destiny"))
                return TeleportType::TwistedFateR;

            // ---- Pantheon R (Grand Starfall) ----
            if (buffs.HasBuff("PantheonRChannel") ||
                buffs.HasBuff("PantheonR") ||
                buffs.HasBuff("pantheonrjump"))
                return TeleportType::PantheonR;

            // ---- Ryze R (Realm Warp) ----
            if (buffs.HasBuff("RyzeR") ||
                buffs.HasBuff("ryzerportalchannel"))
                return TeleportType::RyzeR;

            // ---- Tahm Kench R (Abyssal Voyage) ----
            if (buffs.HasBuff("TahmKenchNewR") ||
                buffs.HasBuff("tahmkenchr") ||
                buffs.HasBuff("TahmKenchRChannel"))
                return TeleportType::TahmKenchR;

            // ---- Hexgate ----
            if (buffs.HasBuff("S12_Map_Hexgate_Teleporting") ||
                buffs.HasBuff("hexgateteleport") ||
                buffs.HasBuff("HexgateBuff"))
                return TeleportType::Hexgate;

            // ---- Active spell cast fallback ----
            SpellBook sb(hero.address);
            if (sb.IsValid()) {
                uintptr_t activeCast = sb.GetActiveSpellCast();
                if (Globals::IsValidPtr(activeCast)) {
                    uintptr_t spellData = Globals::Read<uintptr_t>(
                        activeCast + Offset::SpellCastInfo::SpellData);
                    if (Globals::IsValidPtr(spellData)) {
                        SpellInfo si(spellData);
                        std::string name = si.GetName();
                        if (!name.empty()) {
                            // Case-insensitive matching
                            std::string lower = name;
                            for (auto& c : lower) c = (char)tolower(c);

                            if (lower.find("summonerteleport") != std::string::npos)
                                return TeleportType::Teleport;
                            if (lower == "recall" || lower == "odinrecall")
                                return TeleportType::Recall;
                            if (lower == "superrecall" || lower == "odinsuperrecall")
                                return TeleportType::SuperRecall;
                            if (lower.find("shenr") != std::string::npos)
                                return TeleportType::ShenR;
                            if (lower.find("gate") != std::string::npos && lower.find("hex") == std::string::npos)
                                return TeleportType::TwistedFateR;
                            if (lower.find("pantheonr") != std::string::npos)
                                return TeleportType::PantheonR;
                            if (lower.find("ryzer") != std::string::npos)
                                return TeleportType::RyzeR;
                            if (lower.find("tahmkench") != std::string::npos && lower.find("r") != std::string::npos)
                                return TeleportType::TahmKenchR;
                            if (lower.find("hexgate") != std::string::npos)
                                return TeleportType::Hexgate;
                        }
                    }
                }
            }

            return TeleportType::Unknown;
        }

        // ====================================================================
        // Get standard channel duration for teleport type
        // ====================================================================
        static float GetChannelDuration(TeleportType type) {
            switch (type) {
            case TeleportType::Recall:        return 8.0f;    // Normal recall
            case TeleportType::SuperRecall:   return 4.0f;    // Baron recall
            case TeleportType::Teleport:      return 4.0f;    // TP (S26: 4 seconds)
            case TeleportType::ShenR:         return 3.0f;    // Shen R
            case TeleportType::TwistedFateR:  return 1.5f;    // TF R (Gate channel)
            case TeleportType::PantheonR:     return 2.0f;    // Pantheon R
            case TeleportType::RyzeR:         return 2.0f;    // Ryze R
            case TeleportType::TahmKenchR:    return 1.0f;    // Tahm Kench R (S26)
            case TeleportType::Hexgate:       return 1.5f;    // Hexgate
            default:                          return 4.0f;
            }
        }

        // ====================================================================
        // Fire event to all registered callbacks
        // ====================================================================
        static void FireEvent(const TeleportInfo& info, TeleportStatus status) {
            TeleportEventArgs args;
            args.NetworkId = info.NetworkId;
            args.ChampionName = info.ChampionName;
            args.Type = info.Type;
            args.Status = status;
            args.Duration = info.Duration;
            args.StartTime = info.StartTime;
            args.EndTime = info.EndTime;
            args.StartPosition = info.StartPosition;
            args.TargetPosition = info.TargetPosition;

            for (auto& cb : s_callbacks) {
                try { cb(args); } catch (...) {}
            }
        }

        // ---- State ----
        static inline std::unordered_map<unsigned int, TeleportInfo> s_teleports;
        static inline std::vector<TeleportCallback> s_callbacks;
        static inline bool s_initialized = false;
    };

} // namespace SDK
