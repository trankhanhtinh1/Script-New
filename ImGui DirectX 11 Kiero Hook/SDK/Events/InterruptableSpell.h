#pragma once
#include "EventSystem.h"
#include "GameObjects.h"
#include "SpellBook.h"
#include "libs/nlohmann/json.hpp"
#include <fstream>
#include <algorithm>
#include <unordered_map>

// ============================================================================
// InterruptableSpell — Detects enemy channeling/interruptable spells
// Reference: EnsoulSharp.SDK/Core/Events/InterruptableSpell.cs
//            + Resources/Data/InterruptableSpells.json
// ============================================================================

namespace SDK {

    // ========================================================================
    // Danger Level for interruptable spells
    // ========================================================================
    enum class InterruptDangerLevel {
        Low,
        Medium,
        High
    };

    // ========================================================================
    // Interruptable Spell Database Entry
    // ========================================================================
    struct InterruptableSpellEntry {
        std::string             ChampionName;
        SpellSlotId             Slot;
        InterruptDangerLevel    DangerLevel;
        bool                    MovementInterrupts; // can movement cancel it?
        std::string             Name;               // optional override spell name
    };

    // ========================================================================
    // Interruptable Target Event Args
    // ========================================================================
    struct InterruptableTargetEventArgs {
        GameObject              Sender;
        SpellSlotId             Slot;
        InterruptDangerLevel    DangerLevel;
        bool                    MovementInterrupts;
        std::string             SpellName;
        float                   EndTime;    // when the channel will finish
        float                   GameTime;   // when detected
    };

    // ========================================================================
    // Callback type
    // ========================================================================
    using OnInterruptableTargetFn = std::function<void(const InterruptableTargetEventArgs&)>;

    // ========================================================================
    // Global Interruptable Spell Entry (non-champion-specific, e.g. Teleport)
    // ========================================================================
    struct GlobalInterruptableSpellEntry {
        std::string             Name;               // spell name (lowercase, e.g. "summonerteleport")
        InterruptDangerLevel    DangerLevel;
        bool                    MovementInterrupts;
    };

    // ========================================================================
    // InterruptableSpell — Main class
    // ========================================================================
    class InterruptableSpell {
    public:
        // Register a callback
        static void OnInterruptableTarget(OnInterruptableTargetFn callback) {
            s_callbacks.push_back(callback);
        }

        // Initialize champion-specific from JSON
        static bool Init(const std::string& jsonPath) {
            return LoadFromJson(jsonPath);
        }

        // Initialize global interruptable spells from JSON
        static bool InitGlobal(const std::string& jsonPath) {
            return LoadGlobalFromJson(jsonPath);
        }

        // Initialize global with hardcoded fallback
        static void InitGlobalDefault() {
            if (!s_globalEntries.empty()) return;
            GlobalInterruptableSpellEntry e;
            e.Name = "summonerteleport";
            e.DangerLevel = InterruptDangerLevel::Medium;
            e.MovementInterrupts = true;
            s_globalEntries.push_back(e);
            BuildGlobalIndex();
        }

        // Initialize with hardcoded database
        static void InitDefault() {
            if (!s_entries.empty()) return;
            Add("Caitlyn",      SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("FiddleSticks", SpellSlotId::W, InterruptDangerLevel::Medium, true);
            Add("FiddleSticks", SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Galio",        SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Janna",        SpellSlotId::R, InterruptDangerLevel::Medium, true);
            Add("Jhin",         SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Karthus",      SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Katarina",     SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Lucian",       SpellSlotId::R, InterruptDangerLevel::High,   false);
            Add("Malzahar",     SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("MasterYi",     SpellSlotId::W, InterruptDangerLevel::Low,    true);
            Add("MissFortune",  SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Nunu",         SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Pantheon",     SpellSlotId::E, InterruptDangerLevel::Low,    true);
            Add("Pantheon",     SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Quinn",        SpellSlotId::R, InterruptDangerLevel::Medium, true);
            Add("Shen",         SpellSlotId::R, InterruptDangerLevel::Medium, true);
            Add("Sion",         SpellSlotId::Q, InterruptDangerLevel::Low,    true);
            Add("TahmKench",    SpellSlotId::R, InterruptDangerLevel::Medium, true);
            Add("TwistedFate",  SpellSlotId::R, InterruptDangerLevel::Medium, true);
            Add("Varus",        SpellSlotId::Q, InterruptDangerLevel::Medium, false);
            Add("Velkoz",       SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Vi",           SpellSlotId::Q, InterruptDangerLevel::Medium, false);
            Add("Warwick",      SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Xerath",       SpellSlotId::Q, InterruptDangerLevel::Medium, false);
            Add("Xerath",       SpellSlotId::R, InterruptDangerLevel::High,   true);
            // Newer champions
            Add("Samira",       SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Hwei",         SpellSlotId::R, InterruptDangerLevel::High,   true);
            Add("Briar",        SpellSlotId::W, InterruptDangerLevel::Low,    true);

            BuildIndex();
        }

        // Poll-based: call each frame to detect currently channeling enemies
        static void Update() {
            if (s_callbacks.empty()) return;

            float now = Game::GetTime();
            if (now <= 0.0f) return;

            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return;
            auto myTeam = localPlayer->GetTeam();

            for (auto& hero : GameObjects::EnemyHeroes) {
                if (!hero.IsValid() || !hero.IsAlive()) continue;

                int heroNetId = hero.GetNetId();
                SpellBook sb(hero.address);
                if (!sb.IsValid()) continue;

                uintptr_t activeCast = sb.GetActiveSpellCast();
                if (!Globals::IsValidPtr(activeCast)) {
                    // Not casting — clear state
                    s_castingState.erase(heroNetId);
                    continue;
                }

                // Read active spell slot
                int slotIdx = Globals::Read<int>(activeCast + Offset::SpellCastInfo::Slot);
                if (slotIdx < 0 || slotIdx > 3) {
                    s_castingState.erase(heroNetId);
                    continue; // Only care about QWER
                }

                // Read spell name first (needed for global check)
                std::string castSpellName;
                uintptr_t castSpellData = Globals::Read<uintptr_t>(activeCast + Offset::SpellCastInfo::SpellData);
                if (Globals::IsValidPtr(castSpellData)) {
                    char castNameBuf[128] = {};
                    Globals::ReadGameString(castSpellData + Offset::SpellBook::DataSpellName, castNameBuf, sizeof(castNameBuf));
                    castSpellName = castNameBuf;
                }

                // Check global interruptable spells first (e.g. Teleport)
                InterruptDangerLevel dangerLevel = InterruptDangerLevel::Low;
                bool movementInterrupts = true;
                bool isInterruptable = false;

                if (!castSpellName.empty()) {
                    auto* globalEntry = GetGlobalEntry(castSpellName);
                    if (globalEntry) {
                        dangerLevel = globalEntry->DangerLevel;
                        movementInterrupts = globalEntry->MovementInterrupts;
                        isInterruptable = true;
                    }
                }

                // Check champion-specific interruptable spells
                std::string champName = hero.GetChampionName();
                if (!isInterruptable) {
                    std::string key = champName + "_" + std::to_string(slotIdx);
                    auto it = s_champSlotIndex.find(key);
                    if (it == s_champSlotIndex.end()) continue;
                    dangerLevel = it->second->DangerLevel;
                    movementInterrupts = it->second->MovementInterrupts;
                    isInterruptable = true;
                }

                if (!isInterruptable) continue;

                // Only fire once per cast (deduplicate)
                auto& state = s_castingState[heroNetId];
                if (state.isFiring && state.slot == slotIdx) continue;

                state.isFiring = true;
                state.slot = slotIdx;

                // Build event
                InterruptableTargetEventArgs args;
                args.Sender             = hero;
                args.Slot               = (SpellSlotId)slotIdx;
                args.DangerLevel        = dangerLevel;
                args.MovementInterrupts = movementInterrupts;
                args.SpellName          = castSpellName;
                args.EndTime            = 0.0f; // Could read from channel duration
                args.GameTime           = now;

                for (auto& cb : s_callbacks) {
                    cb(args);
                }
            }
        }

        // Check if a champion + slot is interruptable
        static bool IsInterruptable(const std::string& champName, SpellSlotId slot) {
            std::string key = champName + "_" + std::to_string((int)slot);
            return s_champSlotIndex.find(key) != s_champSlotIndex.end();
        }

        // Check if a spell name is globally interruptable (e.g. Teleport)
        static bool IsGlobalInterruptable(const std::string& spellName) {
            std::string lower = spellName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return s_globalSpellIndex.find(lower) != s_globalSpellIndex.end();
        }

        // Get global interruptable spell data
        static const GlobalInterruptableSpellEntry* GetGlobalEntry(const std::string& spellName) {
            std::string lower = spellName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            auto it = s_globalSpellIndex.find(lower);
            if (it != s_globalSpellIndex.end()) return it->second;
            return nullptr;
        }

        // Get danger level
        static InterruptDangerLevel GetDangerLevel(const std::string& champName, SpellSlotId slot) {
            std::string key = champName + "_" + std::to_string((int)slot);
            auto it = s_champSlotIndex.find(key);
            if (it != s_champSlotIndex.end()) return it->second->DangerLevel;
            return InterruptDangerLevel::Low;
        }

        static const std::vector<InterruptableSpellEntry>& GetEntries() { return s_entries; }
        static const std::vector<GlobalInterruptableSpellEntry>& GetGlobalEntries() { return s_globalEntries; }

    private:
        static inline std::vector<OnInterruptableTargetFn>   s_callbacks;
        static inline std::vector<InterruptableSpellEntry>    s_entries;
        static inline std::vector<GlobalInterruptableSpellEntry> s_globalEntries;
        // Key: "ChampionName_SlotIndex"
        static inline std::unordered_map<std::string, const InterruptableSpellEntry*> s_champSlotIndex;
        // Key: lowercase spell name (e.g. "summonerteleport")
        static inline std::unordered_map<std::string, const GlobalInterruptableSpellEntry*> s_globalSpellIndex;

        struct CastState {
            bool isFiring = false;
            int  slot = -1;
        };
        static inline std::unordered_map<int, CastState> s_castingState;

        static void Add(const std::string& champ, SpellSlotId slot,
                         InterruptDangerLevel danger, bool movementInterrupts) {
            InterruptableSpellEntry e;
            e.ChampionName = champ;
            e.Slot = slot;
            e.DangerLevel = danger;
            e.MovementInterrupts = movementInterrupts;
            s_entries.push_back(e);
        }

        static void BuildIndex() {
            s_champSlotIndex.clear();
            for (auto& e : s_entries) {
                std::string key = e.ChampionName + "_" + std::to_string((int)e.Slot);
                s_champSlotIndex[key] = &e;
            }
        }

        static SpellSlotId ParseSlot(const std::string& s) {
            if (s == "Q") return SpellSlotId::Q;
            if (s == "W") return SpellSlotId::W;
            if (s == "E") return SpellSlotId::E;
            if (s == "R") return SpellSlotId::R;
            return SpellSlotId::Q;
        }

        static SpellSlotId ParseSlotFromInt(int i) {
            if (i >= 0 && i <= 3) return (SpellSlotId)i;
            return SpellSlotId::Q;
        }

        static InterruptDangerLevel ParseDanger(const std::string& s) {
            if (s == "High")   return InterruptDangerLevel::High;
            if (s == "Medium") return InterruptDangerLevel::Medium;
            return InterruptDangerLevel::Low;
        }

        static bool LoadFromJson(const std::string& jsonPath) {
            std::ifstream file(jsonPath);
            if (!file.is_open()) {
                InitDefault();
                return false;
            }

            try {
                nlohmann::json data = nlohmann::json::parse(file);
                s_entries.clear();

                // Format: { "ChampionName": [ { Slot, DangerLevel, MovementInterrupts, Name }, ... ] }
                for (auto& [champName, spells] : data.items()) {
                    if (!spells.is_array()) continue;
                    for (auto& sp : spells) {
                        InterruptableSpellEntry e;
                        e.ChampionName = champName;
                        e.DangerLevel = ParseDanger(sp.value("DangerLevel", "Low"));
                        e.MovementInterrupts = sp.value("MovementInterrupts", true);

                        // Slot can be string or int
                        if (sp.contains("Slot")) {
                            if (sp["Slot"].is_string()) {
                                e.Slot = ParseSlot(sp["Slot"].get<std::string>());
                            } else if (sp["Slot"].is_number()) {
                                e.Slot = ParseSlotFromInt(sp["Slot"].get<int>());
                            }
                        }

                        if (sp.contains("Name") && !sp["Name"].is_null()) {
                            e.Name = sp["Name"].get<std::string>();
                        }

                        s_entries.push_back(e);
                    }
                }

                BuildIndex();
                return true;
            } catch (...) {
                InitDefault();
                return false;
            }
        }

        // Build index for global spells
        static void BuildGlobalIndex() {
            s_globalSpellIndex.clear();
            for (auto& e : s_globalEntries) {
                std::string lower = e.Name;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                s_globalSpellIndex[lower] = &e;
            }
        }

        // Load GlobalInterruptableSpellsList.json
        // Format: [ { Name, DangerLevel, MovementInterrupts, Slot }, ... ]
        static bool LoadGlobalFromJson(const std::string& jsonPath) {
            std::ifstream file(jsonPath);
            if (!file.is_open()) {
                InitGlobalDefault();
                return false;
            }

            try {
                nlohmann::json data = nlohmann::json::parse(file);
                s_globalEntries.clear();

                if (!data.is_array()) {
                    InitGlobalDefault();
                    return false;
                }

                for (auto& item : data) {
                    GlobalInterruptableSpellEntry e;
                    e.Name = item.value("Name", "");
                    if (e.Name.empty()) continue;
                    std::transform(e.Name.begin(), e.Name.end(), e.Name.begin(), ::tolower);
                    e.DangerLevel = ParseDanger(item.value("DangerLevel", "Low"));
                    e.MovementInterrupts = item.value("MovementInterrupts", true);
                    s_globalEntries.push_back(e);
                }

                BuildGlobalIndex();
                return true;
            } catch (...) {
                InitGlobalDefault();
                return false;
            }
        }
    };

} // namespace SDK
