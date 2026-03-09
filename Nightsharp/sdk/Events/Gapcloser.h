#pragma once
#include "EventSystem.h"
#include "GameObjects.h"
#include "AiManager.h"
#include "libs/nlohmann/json.hpp"
#include <fstream>
#include <algorithm>
#include <unordered_map>

// ============================================================================
// Gapcloser — Detects enemy gapcloser/dash spells
// Reference: EnsoulSharp.SDK/Core/Events/Gapcloser.cs
//            + Resources/Data/Gapclosers.json
// ============================================================================

namespace SDK {

    // ========================================================================
    // Gapcloser Skill Type
    // ========================================================================
    enum class GapcloserSkillType {
        Skillshot,  // Dash in a direction (e.g. Lucian E, Graves E)
        Targeted    // Dash to a target (e.g. Jax Q, Irelia Q)
    };

    // ========================================================================
    // Gapcloser Database Entry
    // ========================================================================
    struct GapcloserEntry {
        std::string         ChampionName;
        std::string         SpellName;   // lowercase
        SpellSlotId         Slot;
        GapcloserSkillType  SkillType;
        bool                Invert;      // true = dash AWAY from cast direction (e.g. Caitlyn E)
    };

    // ========================================================================
    // Gapcloser Event Args — fired when an enemy gapcloser is detected
    // ========================================================================
    struct GapcloserEventArgs {
        GameObject          Sender;
        SpellSlotId         Slot;
        GapcloserSkillType  SkillType;
        std::string         SpellName;
        Vec3                StartPos;
        Vec3                EndPos;
        float               Speed;
        bool                IsInverted;     // dash goes opposite direction
        float               GameTime;
    };

    // ========================================================================
    // Callback type
    // ========================================================================
    using OnGapcloserFn = std::function<void(const GapcloserEventArgs&)>;

    // ========================================================================
    // Gapcloser — Main class
    // ========================================================================
    class Gapcloser {
    public:
        // Register a callback for gapcloser events
        static void OnGapcloser(OnGapcloserFn callback) {
            s_callbacks.push_back(callback);
        }

        // Initialize from JSON file
        static bool Init(const std::string& jsonPath) {
            return LoadFromJson(jsonPath);
        }

        // Initialize with hardcoded database (fallback if no JSON)
        static void InitDefault() {
            if (!s_entries.empty()) return;
            // Add common gapclosers hardcoded
            Add("Aatrox",    "aatroxe",            SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Ahri",      "ahritumble",         SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Akali",     "akalie",             SpellSlotId::E, GapcloserSkillType::Skillshot, true);
            Add("Akali",     "akalir",             SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Akali",     "akalirb",            SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Alistar",   "headbutt",           SpellSlotId::W, GapcloserSkillType::Targeted,  false);
            Add("Caitlyn",   "caitlynentrapment",  SpellSlotId::E, GapcloserSkillType::Skillshot, true);
            Add("Camille",   "camilleedash2",      SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Corki",     "carpetbomb",         SpellSlotId::W, GapcloserSkillType::Skillshot, false);
            Add("Diana",     "dianateleport",      SpellSlotId::R, GapcloserSkillType::Targeted,  false);
            Add("Ekko",      "ekkoeattack",        SpellSlotId::E, GapcloserSkillType::Targeted,  false);
            Add("Fiora",     "fioraq",             SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Fizz",      "fizzq",              SpellSlotId::Q, GapcloserSkillType::Targeted,  false);
            Add("Galio",     "galioe",             SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Gragas",    "gragase",            SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Graves",    "gravesmove",         SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Irelia",    "ireliaq",            SpellSlotId::Q, GapcloserSkillType::Targeted,  false);
            Add("JarvanIV",  "jarvanivcataclysm",  SpellSlotId::R, GapcloserSkillType::Targeted,  false);
            Add("Jax",       "jaxleapstrike",      SpellSlotId::Q, GapcloserSkillType::Targeted,  false);
            Add("Kassadin",  "riftwalk",           SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Katarina",  "katarinae",          SpellSlotId::E, GapcloserSkillType::Targeted,  false);
            Add("Kayn",      "kaynq",              SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Khazix",    "khazixe",            SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Khazix",    "khazixelong",        SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Kindred",   "kindredq",           SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("LeeSin",    "blindmonkqtwo",      SpellSlotId::Q, GapcloserSkillType::Targeted,  false);
            Add("Lucian",    "luciane",            SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Malphite",  "ufslash",            SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("MasterYi",  "alphastrike",        SpellSlotId::Q, GapcloserSkillType::Targeted,  false);
            Add("Pantheon",  "pantheonw",          SpellSlotId::W, GapcloserSkillType::Targeted,  false);
            Add("Poppy",     "poppye",             SpellSlotId::E, GapcloserSkillType::Targeted,  false);
            Add("Pyke",      "pykee",              SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Rakan",     "rakanw",             SpellSlotId::W, GapcloserSkillType::Skillshot, false);
            Add("Sejuani",   "sejuaniq",           SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Shen",      "shene",              SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Tristana",  "tristanaw",          SpellSlotId::W, GapcloserSkillType::Skillshot, false);
            Add("Vayne",     "vaynetumble",        SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Vi",        "viq",                SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Yasuo",     "yasuodashwrapper",   SpellSlotId::E, GapcloserSkillType::Targeted,  false);
            Add("Zac",       "zace",               SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            // New champions
            Add("Yone",      "yoneq3",             SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Yone",      "yoner",              SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Viego",     "viegow",             SpellSlotId::W, GapcloserSkillType::Skillshot, false);
            Add("Viego",     "viegor",             SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Gwen",      "gwene",              SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Briar",     "briare",             SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Briar",     "briarr",             SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Ambessa",   "ambessaq",           SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("Ambessa",   "ambessae",           SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Ambessa",   "ambessar",           SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("KSante",    "ksanteq3",           SpellSlotId::Q, GapcloserSkillType::Skillshot, false);
            Add("KSante",    "ksanter",            SpellSlotId::R, GapcloserSkillType::Targeted,  false);
            Add("Sett",      "settr",              SpellSlotId::R, GapcloserSkillType::Targeted,  false);
            Add("Samira",    "samirae",            SpellSlotId::E, GapcloserSkillType::Targeted,  false);
            Add("Kaisa",     "kaisar",             SpellSlotId::R, GapcloserSkillType::Skillshot, false);
            Add("Sylas",     "sylase",             SpellSlotId::E, GapcloserSkillType::Skillshot, false);
            Add("Sylas",     "sylasw",             SpellSlotId::W, GapcloserSkillType::Targeted,  false);

            BuildIndex();
        }

        // Call in EventSystem::Update or after OnProcessSpellCast
        static void OnSpellCastDetected(const SpellCastArgs& args) {
            if (s_callbacks.empty()) return;
            if (!args.Sender.IsValid()) return;

            // Only care about enemy heroes
            auto* localPlayer = &GameObjects::Player;
            if (!localPlayer->IsValid()) return;
            if (args.Sender.GetTeam() == localPlayer->GetTeam()) return;

            // Check if spell name matches a gapcloser
            std::string lowerName = args.SpellName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

            auto it = s_spellIndex.find(lowerName);
            if (it == s_spellIndex.end()) return;

            const GapcloserEntry& entry = *it->second;

            // Build event args
            GapcloserEventArgs gapArgs;
            gapArgs.Sender    = args.Sender;
            gapArgs.Slot      = entry.Slot;
            gapArgs.SkillType = entry.SkillType;
            gapArgs.SpellName = args.SpellName;
            gapArgs.StartPos  = args.StartPos;
            gapArgs.EndPos    = args.EndPos;
            gapArgs.IsInverted = entry.Invert;
            gapArgs.GameTime  = args.CastTime;

            // Estimate dash speed from AiManager
            AiManager ai(args.Sender.address);
            gapArgs.Speed = ai.IsDashing() ? ai.GetDashSpeed() : 0.0f;

            // Fire all callbacks
            for (auto& cb : s_callbacks) {
                cb(gapArgs);
            }
        }

        // Get all gapcloser entries
        static const std::vector<GapcloserEntry>& GetEntries() { return s_entries; }

        // Check if a spell name is a gapcloser
        static bool IsGapcloser(const std::string& spellName) {
            std::string lower = spellName;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            return s_spellIndex.find(lower) != s_spellIndex.end();
        }

    private:
        static inline std::vector<OnGapcloserFn>    s_callbacks;
        static inline std::vector<GapcloserEntry>    s_entries;
        static inline std::unordered_map<std::string, const GapcloserEntry*> s_spellIndex;

        static void Add(const std::string& champ, const std::string& spell,
                         SpellSlotId slot, GapcloserSkillType type, bool invert) {
            GapcloserEntry e;
            e.ChampionName = champ;
            e.SpellName = spell;
            e.Slot = slot;
            e.SkillType = type;
            e.Invert = invert;
            s_entries.push_back(e);
        }

        static void BuildIndex() {
            s_spellIndex.clear();
            for (auto& e : s_entries) {
                std::string lower = e.SpellName;
                std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                s_spellIndex[lower] = &e;
            }
        }

        static SpellSlotId ParseSlot(const std::string& s) {
            if (s == "Q") return SpellSlotId::Q;
            if (s == "W") return SpellSlotId::W;
            if (s == "E") return SpellSlotId::E;
            if (s == "R") return SpellSlotId::R;
            return SpellSlotId::Q;
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

                // Format: { "ChampionName": [ { Slot, SpellName, SkillType, Invert }, ... ] }
                for (auto& [champName, spells] : data.items()) {
                    if (!spells.is_array()) continue;
                    for (auto& sp : spells) {
                        GapcloserEntry e;
                        e.ChampionName = champName;
                        e.SpellName = sp.value("SpellName", "");
                        e.Slot = ParseSlot(sp.value("Slot", "Q"));
                        std::string skillType = sp.value("SkillType", "Skillshot");
                        e.SkillType = (skillType == "Targeted") ?
                            GapcloserSkillType::Targeted : GapcloserSkillType::Skillshot;
                        e.Invert = sp.value("Invert", false);

                        // Lowercase spell name for index
                        std::transform(e.SpellName.begin(), e.SpellName.end(),
                                       e.SpellName.begin(), ::tolower);
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
    };

} // namespace SDK
