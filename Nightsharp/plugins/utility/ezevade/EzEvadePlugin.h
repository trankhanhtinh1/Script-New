#pragma once
// ============================================================================
// EzEvadePlugin.h — Plugin wrapper for EzEvade (C++ port of ezEvade)
// Connects EzEvade::Evade + SpellDetector + SpellDrawer + SpecialSpells
// into the NightSharp PluginManager lifecycle.
//
// Reference: ezEvade/Evade.cs constructor + Game_OnGameUpdate + Drawing_OnDraw
// ============================================================================

#include "../../IPlugin.h"
#include "../../../sdk/EzEvade/Core/Evade.h"
#include "../../../sdk/EzEvade/Core/EvadeState.h"
#include "../../../sdk/EzEvade/Spells/SpellDetector.h"
#include "../../../sdk/EzEvade/Spells/SpellDrawer.h"
#include "../../../sdk/EzEvade/Spells/SpellData.h"
#include "../../../sdk/EzEvade/Helpers/ObjectCache.h"
#include "../../../sdk/EzEvade/Utils/EvadeUtils.h"
#include "../../../sdk/EzEvade/EvadeSpells/EvadeSpell.h"
#include "../../../sdk/EzEvade/EvadeSpells/EvadeSpellData.h"
#include "../../../sdk/EzEvade/EvadeSpells/EvadeSpellDatabase.h"

// SpecialSpells — all champion handlers
#include "../../../sdk/EzEvade/SpecialSpells/AllChampions.h"
#include "../../../sdk/EzEvade/SpecialSpells/Ahri.h"
#include "../../../sdk/EzEvade/SpecialSpells/Ashe.h"
#include "../../../sdk/EzEvade/SpecialSpells/Azir.h"
#include "../../../sdk/EzEvade/SpecialSpells/Darius.h"
#include "../../../sdk/EzEvade/SpecialSpells/Ekko.h"
#include "../../../sdk/EzEvade/SpecialSpells/Fizz.h"
#include "../../../sdk/EzEvade/SpecialSpells/Graves.h"
#include "../../../sdk/EzEvade/SpecialSpells/Heimerdinger.h"
#include "../../../sdk/EzEvade/SpecialSpells/JarvanIV.h"
#include "../../../sdk/EzEvade/SpecialSpells/Jayce.h"
#include "../../../sdk/EzEvade/SpecialSpells/Jinx.h"
#include "../../../sdk/EzEvade/SpecialSpells/Lucian.h"
#include "../../../sdk/EzEvade/SpecialSpells/Lulu.h"
#include "../../../sdk/EzEvade/SpecialSpells/Lux.h"
#include "../../../sdk/EzEvade/SpecialSpells/Malzahar.h"
#include "../../../sdk/EzEvade/SpecialSpells/Orianna.h"
#include "../../../sdk/EzEvade/SpecialSpells/Sion.h"
#include "../../../sdk/EzEvade/SpecialSpells/Syndra.h"
#include "../../../sdk/EzEvade/SpecialSpells/Taric.h"
#include "../../../sdk/EzEvade/SpecialSpells/Twitch.h"
#include "../../../sdk/EzEvade/SpecialSpells/Viktor.h"
#include "../../../sdk/EzEvade/SpecialSpells/Xerath.h"
#include "../../../sdk/EzEvade/SpecialSpells/Yasuo.h"
#include "../../../sdk/EzEvade/SpecialSpells/Yorick.h"
#include "../../../sdk/EzEvade/SpecialSpells/Zed.h"
#include "../../../sdk/EzEvade/SpecialSpells/Ziggs.h"
#include "../../../sdk/EzEvade/SpecialSpells/Zilean.h"

#include "../../../sdk/GameObjects/GameObjects.h"
#include "../../../sdk/GameObjects/Missile.h"
#include "../../../sdk/Game.h"
#include "../../../sdk/UI/MenuUI.h"
#include "../../../sdk/Events/EventSystem.h"

#include "../../../imgui/imgui.h"
#include <memory>
#include <vector>

namespace Plugins {

    // Simple debug log for EzEvade event tracing
    struct EzEvadeDebug {
        static inline std::vector<std::string> lastSpellEvents;
        static void LogSpell(const std::string& name) {
            if (lastSpellEvents.size() >= 5)
                lastSpellEvents.erase(lastSpellEvents.begin());
            lastSpellEvents.push_back(name);
        }
    };

    class EzEvadePlugin : public IPlugin {
    public:
        const char* GetName()     const override { return "EzEvade"; }
        const char* GetAuthor()   const override { return "ezEvade (C++ port)"; }
        PluginCategory GetCategory() const override { return PluginCategory::Utility; }

        // =================================================================
        // OnLoad — C# Evade() constructor + SpellDetector init
        // =================================================================
        void OnLoad() override {
            using namespace EzEvade;

            // 1. Create the MenuUI menu (same style as OrbwalkerPlugin)
            CreateMenu();

            // 2. Initialize ObjectCache (hero cache, turrets, etc.)
            ObjectCache::Initialize();
            ObjectCache::myHeroCache.UpdateInfo();

            // 3. Initialize Evade core (sets up defaults into menuCache)
            m_evade.Initialize();

            // 4. Initialize SpellDetector (loads spell database, channeled spells)
            SpellDetector::Initialize();

            // 5. Initialize EvadeSpell database (blinks, dashes, shields, etc.)
            EvadeSpell::evadeSpells = GetEvadeSpellDatabase();

            // 6. Initialize SpellDrawer
            SpellDrawer::Initialize();

            // 7. Load SpecialSpells for enemy champions
            LoadSpecialSpells();

            // 8. Register EventSystem hooks — CRITICAL for spell detection
            RegisterEventHooks();

            m_initialized = true;
        }

        // =================================================================
        // OnUnload — cleanup
        // =================================================================
        void OnUnload() override {
            using namespace EzEvade;

            // Clear spell data
            SpellDetector::spells.clear();
            SpellDetector::drawSpells.clear();
            SpellDetector::OnProcessSpecialSpell.clear();

            m_specialSpells.clear();
            m_initialized = false;

            // Remove menu
            if (m_menu) {
                SDK::MenuUI::Menu::Remove("EzEvade");
                m_menu.reset();
            }
        }

        // =================================================================
        // OnUpdate — C# Game_OnGameUpdate
        //   Called every script tick (~45 FPS)
        // =================================================================
        void OnUpdate() override {
            if (!m_initialized) return;

            // Update game ping cache
            EzEvade::ObjectCache::gamePing = (float)SDK::Game::GetPing();

            // Update hero cache
            EzEvade::ObjectCache::myHeroCache.UpdateInfo();

            // Run SpellDetector tick (detect new spells, expire old ones)
            EzEvade::SpellDetector::OnGameUpdate();

            // Run Evade main update (dodge logic, movement blocking, etc.)
            EzEvade::Evade::OnGameUpdate();
        }

        // =================================================================
        // OnRender — C# Drawing_OnDraw
        //   Called every present frame (full FPS)
        // =================================================================
        void OnRender() override {
            if (!m_initialized) return;

            // Draw spell indicators
            EzEvade::SpellDrawer::OnDraw();

            // Draw evade status text (ON/OFF indicator)
            EzEvade::SpellDrawer::DrawEvadeStatus();

            // Debug info overlay (read from menu)
            DrawDebugOverlay();
        }

        // =================================================================
        // OnMenu — Uses SDK::MenuUI system (same style as Orbwalker)
        // =================================================================
        void OnMenu() override {
            if (m_menu) m_menu->Draw();
        }

    private:
        EzEvade::Evade m_evade;
        bool m_initialized = false;
        std::shared_ptr<SDK::MenuUI::Menu> m_menu;
        std::vector<std::unique_ptr<EzEvade::SpecialSpells::ChampionPlugin>> m_specialSpells;

        // =================================================================
        // CreateMenu — Build MenuUI menu (replaces raw ImGui::TreeNode)
        //   Mirrors the C# Evade menu structure with proper MenuUI items
        //   that ObjectCache::GetBool/GetSlider can access
        // =================================================================
        void CreateMenu() {
            m_menu = SDK::MenuUI::Menu::Create("EzEvade", "EzEvade - Skillshot Dodging");

            // --- General ---
            auto general = m_menu->AddSubMenu("General", "General");
            auto* dodge = general->Add<SDK::MenuUI::MenuBool>("DodgeSkillShots", "Dodge Skillshots", true);
            general->Add<SDK::MenuUI::MenuBool>("ActivateEvadeSpells", "Use Evade Spells", true);
            general->Add<SDK::MenuUI::MenuBool>("DodgeDangerous", "Dodge Only Dangerous", false);
            general->Add<SDK::MenuUI::MenuBool>("DodgeFOWSpells", "Dodge FOW Spells", true);
            general->Add<SDK::MenuUI::MenuBool>("DodgeCircularSpells", "Dodge Circular Spells", true);

            // --- Advanced ---
            auto advanced = m_menu->AddSubMenu("Advanced", "Advanced");
            advanced->Add<SDK::MenuUI::MenuBool>("HigherPrecision", "Higher Precision", false);
            advanced->Add<SDK::MenuUI::MenuBool>("RecalculatePosition", "Recalculate Position", true);
            advanced->Add<SDK::MenuUI::MenuBool>("ContinueMovement", "Continue Movement", true);
            advanced->Add<SDK::MenuUI::MenuBool>("CalculateWindupDelay", "Calculate Windup Delay", true);
            advanced->Add<SDK::MenuUI::MenuBool>("CheckSpellCollision", "Check Spell Collision", false);
            advanced->Add<SDK::MenuUI::MenuBool>("PreventDodgingUnderTower", "Prevent Dodge Under Tower", false);
            advanced->Add<SDK::MenuUI::MenuBool>("PreventDodgingNearEnemy", "Prevent Dodge Near Enemy", true);
            advanced->Add<SDK::MenuUI::MenuBool>("AdvancedSpellDetection", "Advanced Spell Detection", false);
            advanced->Add<SDK::MenuUI::MenuBool>("ClickRemove", "Click Remove", true);
            advanced->Add<SDK::MenuUI::MenuBool>("ClickOnlyOnce", "Click Only Once", true);
            advanced->Add<SDK::MenuUI::MenuBool>("EnableEvadeDistance", "Enable Evade Distance", false);
            advanced->Add<SDK::MenuUI::MenuBool>("FastMovementBlock", "Fast Movement Block", false);
            advanced->Add<SDK::MenuUI::MenuSlider>("TickLimiter", "Tick Limiter", 100, 0, 500);
            advanced->Add<SDK::MenuUI::MenuSlider>("SpellDetectionTime", "Spell Detection Time", 0, 0, 1000);
            advanced->Add<SDK::MenuUI::MenuSlider>("DodgeInterval", "Dodge Interval", 0, 0, 2000);
            advanced->Add<SDK::MenuUI::MenuSlider>("FastEvadeActivationTime", "Fast Evade Activation (ms)", 65, 0, 500);
            advanced->Add<SDK::MenuUI::MenuSlider>("SpellActivationTime", "Spell Activation (ms)", 400, 0, 1000);
            advanced->Add<SDK::MenuUI::MenuSlider>("RejectMinDistance", "Reject Min Distance", 10, 0, 100);

            // --- Buffers ---
            auto buffers = m_menu->AddSubMenu("Buffers", "Buffers");
            buffers->Add<SDK::MenuUI::MenuSlider>("ExtraPingBuffer", "Extra Ping Buffer", 65, 0, 200);
            buffers->Add<SDK::MenuUI::MenuSlider>("ExtraCPADistance", "Extra CPA Distance", 10, 0, 100);
            buffers->Add<SDK::MenuUI::MenuSlider>("ExtraSpellRadius", "Extra Spell Radius", 0, 0, 100);
            buffers->Add<SDK::MenuUI::MenuSlider>("ExtraEvadeDistance", "Extra Evade Distance", 100, 0, 300);
            buffers->Add<SDK::MenuUI::MenuSlider>("ExtraAvoidDistance", "Extra Avoid Distance", 50, 0, 200);
            buffers->Add<SDK::MenuUI::MenuSlider>("ReactionTime", "Reaction Time (ms)", 0, 0, 500);
            buffers->Add<SDK::MenuUI::MenuSlider>("MinComfortZone", "Min Comfort Zone", 550, 0, 1000);

            // --- Keys ---
            auto keys = m_menu->AddSubMenu("Keys", "Keybinds");
            keys->Add<SDK::MenuUI::MenuKeyBind>("DodgeDangerousKey", "Dodge Dangerous Key", 0, SDK::MenuUI::KeyBindType::Press);
            keys->Add<SDK::MenuUI::MenuBool>("DodgeDangerousKeyEnabled", "Dodge Dangerous Key Enabled", false);
            keys->Add<SDK::MenuUI::MenuKeyBind>("DodgeComboKey", "Dodge Only On Combo Key", 0, SDK::MenuUI::KeyBindType::Press);
            keys->Add<SDK::MenuUI::MenuBool>("DodgeOnlyOnComboKeyEnabled", "Dodge Only On Combo Enabled", false);
            keys->Add<SDK::MenuUI::MenuKeyBind>("DontDodgeKey", "Don't Dodge Key", 0, SDK::MenuUI::KeyBindType::Press);
            keys->Add<SDK::MenuUI::MenuBool>("DontDodgeKeyEnabled", "Don't Dodge Key Enabled", false);

            // --- Drawing ---
            auto draw = m_menu->AddSubMenu("Drawing", "Drawing");
            draw->Add<SDK::MenuUI::MenuBool>("DrawSkillShots", "Draw Skillshots", true);
            draw->Add<SDK::MenuUI::MenuBool>("ShowStatus", "Show Status", true);
            draw->Add<SDK::MenuUI::MenuBool>("DrawSpellPos", "Draw Spell Position", false);
            draw->Add<SDK::MenuUI::MenuBool>("DrawEvadePosition", "Draw Evade Position", false);

            // --- Debug ---
            auto debug = m_menu->AddSubMenu("Debug", "Debug Info");
            debug->Add<SDK::MenuUI::MenuBool>("ShowDebugOverlay", "Show Debug Overlay", false);

            // --- Register all menu items into ObjectCache::menuCache ---
            RegisterMenuCache(m_menu.get());
        }

        // =================================================================
        // RegisterMenuCache — recursively register all menu items into
        //   ObjectCache::menuCache so GetBool/GetSlider/SetBool/SetSlider
        //   can find them by name
        // =================================================================
        void RegisterMenuCache(SDK::MenuUI::Menu* menu) {
            if (!menu) return;

            // Set the root menu pointer
            EzEvade::ObjectCache::menuCache.menu = menu;

            // Recursively add all items
            EzEvade::ObjectCache::menuCache.AddMenuToCache(menu);
        }

        // =================================================================
        // RegisterEventHooks — Connect SDK EventSystem to EzEvade
        //   This bridges the gap between the SDK's event polling system
        //   and EzEvade's SpellDetector callbacks.
        //   Without this, SpellDetector never receives missile/spell events
        //   and cannot detect any skillshots!
        // =================================================================
        void RegisterEventHooks() {
            using namespace EzEvade;

            // Hook: OnProcessSpellCast → SpellDetector::OnProcessSpell
            // When an enemy hero casts a spell, forward to SpellDetector
            SDK::EventSystem::OnProcessSpellCast([](const SDK::SpellCastArgs& args) {
                // Only track enemy hero spells (not auto attacks)
                if (args.IsAutoAttack) return;
                if (!args.Sender.IsValid() || !args.Sender.IsHero()) return;
                // Check if enemy (IsEnemy requires a reference target)
                if (!args.Sender.IsEnemy(SDK::GameObjects::Player)) return;

                // Create a temporary pointer for the hero (SDK uses value types)
                // SpellDetector::OnProcessSpell expects a pointer
                SDK::GameObject sender = args.Sender;
                EzEvadeDebug::LogSpell("[Cast] " + args.SpellName);
                EzEvade::SpellDetector::OnProcessSpell(&sender, args.StartPos, args.EndPos, args.SpellName);
            });

            // Hook: OnMissileCreated → SpellDetector::OnMissileCreate
            // When a missile is created in the game, forward to SpellDetector
            SDK::EventSystem::OnMissileCreated([](const SDK::MissileArgs& args) {
                if (!args.MissileObj.IsValid()) return;

                // Get caster info
                int casterNetId = args.CasterNetId;
                bool casterVisible = true; // Assume visible if we can see the missile

                // Check if caster is visible
                for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                    if (hero.GetNetId() == casterNetId) {
                        casterVisible = hero.IsVisible();
                        break;
                    }
                }

                // Convert Missile to GameObject for SpellDetector
                SDK::GameObject missileObj(args.MissileObj.address);
                EzEvadeDebug::LogSpell("[Missile] " + args.SpellName);
                EzEvade::SpellDetector::OnMissileCreate(missileObj, args.StartPos, args.EndPos,
                    args.SpellName, casterNetId, casterVisible);
            });

            // Hook: OnMissileDeleted → SpellDetector::OnMissileDelete
            // When a missile is destroyed, remove tracking
            SDK::EventSystem::OnMissileDeleted([](const SDK::MissileArgs& args) {
                if (!args.MissileObj.IsValid()) return;
                int missileNetId = args.MissileObj.GetNetworkId();
                if (missileNetId != 0) {
                    EzEvade::SpellDetector::OnMissileDelete(missileNetId);
                }
            });

            // Hook: OnProcessSpellCast → Evade::OnProcessSpell
            // Also forward to Evade for cast-time tracking
            SDK::EventSystem::OnProcessSpellCast([](const SDK::SpellCastArgs& args) {
                if (!args.Sender.IsValid() || !args.Sender.IsHero()) return;

                // Forward hero spell casts to Evade for wind-up tracking
                auto& player = SDK::GameObjects::Player;
                if (args.Sender.GetNetId() == player.GetNetId()) {
                    SDK::GameObject sender = args.Sender;
                    EzEvade::Evade::OnProcessSpell(&sender, args.SpellName,
                        (int)args.Slot, 0.0f);
                }
            });
        }

        // =================================================================
        // DrawDebugOverlay — show debug info on screen
        // =================================================================
        void DrawDebugOverlay() {
            auto* debugMenu = m_menu ? m_menu->GetSubMenu("Debug") : nullptr;
            if (!debugMenu) return;
            auto* showDbg = debugMenu->Get<SDK::MenuUI::MenuBool>("ShowDebugOverlay");
            if (!showDbg || !showDbg->Enabled) return;

            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (!dl) return;

            float y = 200.0f;
            auto text = [&](const char* fmt, ...) {
                char buf[256];
                va_list args;
                va_start(args, fmt);
                vsnprintf(buf, sizeof(buf), fmt, args);
                va_end(args);
                dl->AddText(ImVec2(11, y + 1), IM_COL32(0, 0, 0, 200), buf);
                dl->AddText(ImVec2(10, y), IM_COL32(0, 255, 200, 255), buf);
                y += 16;
            };

            text("[EzEvade Debug]");
            text("Active Spells: %d", (int)EzEvade::SpellDetector::spells.size());
            text("Draw Spells: %d", (int)EzEvade::SpellDetector::drawSpells.size());
            text("Detected Spells: %d", (int)EzEvade::SpellDetector::detectedSpells.size());
            text("isDodging: %s", EzEvade::EvadeState::isDodging ? "YES" : "NO");
            text("Game Ping: %.0f ms", EzEvade::ObjectCache::gamePing);
            text("SpecialSpells: %d", (int)m_specialSpells.size());
            text("SpellDB (process): %d", (int)EzEvade::SpellDetector::onProcessSpells.size());
            text("SpellDB (missile): %d", (int)EzEvade::SpellDetector::onMissileSpells.size());
            text("DodgeSkillShots: %s", EzEvade::ObjectCache::GetBool("DodgeSkillShots") ? "ON" : "OFF");
            text("DrawSkillShots: %s", EzEvade::ObjectCache::GetBool("DrawSkillShots") ? "ON" : "OFF");
            text("Hero Cache: pos(%.0f, %.0f)", 
                EzEvade::ObjectCache::myHeroCache.serverPos2D.x,
                EzEvade::ObjectCache::myHeroCache.serverPos2D.y);

            // Show last spell events from EventSystem hooks
            text("Last Events: %d", (int)EzEvadeDebug::lastSpellEvents.size());
            for (size_t i = 0; i < EzEvadeDebug::lastSpellEvents.size() && i < 5; i++) {
                text("  -> %s", EzEvadeDebug::lastSpellEvents[i].c_str());
            }
        }

        // =================================================================
        // LoadSpecialSpells — register champion-specific spell handlers
        //   C# original: uses reflection to scan enemy champions and load
        //   matching ChampionPlugin implementations
        // =================================================================
        void LoadSpecialSpells() {
            using namespace EzEvade::SpecialSpells;

            // Always load general handlers
            m_specialSpells.push_back(std::make_unique<AllChampions>());

            // Scan enemy heroes and load matching handlers
            for (auto& hero : SDK::GameObjects::EnemyHeroes) {
                std::string champName = hero.GetChampionName();

                if      (champName == "Ahri")         m_specialSpells.push_back(std::make_unique<Ahri>());
                else if (champName == "Ashe")         m_specialSpells.push_back(std::make_unique<Ashe>());
                else if (champName == "Azir")         m_specialSpells.push_back(std::make_unique<Azir>());
                else if (champName == "Darius")       m_specialSpells.push_back(std::make_unique<Darius>());
                else if (champName == "Ekko")         m_specialSpells.push_back(std::make_unique<Ekko>());
                else if (champName == "Fizz")         m_specialSpells.push_back(std::make_unique<Fizz>());
                else if (champName == "Graves")       m_specialSpells.push_back(std::make_unique<Graves>());
                else if (champName == "Heimerdinger") m_specialSpells.push_back(std::make_unique<Heimerdinger>());
                else if (champName == "JarvanIV")     m_specialSpells.push_back(std::make_unique<JarvanIV>());
                else if (champName == "Jayce")        m_specialSpells.push_back(std::make_unique<Jayce>());
                else if (champName == "Jinx")         m_specialSpells.push_back(std::make_unique<Jinx>());
                else if (champName == "Lucian")       m_specialSpells.push_back(std::make_unique<Lucian>());
                else if (champName == "Lulu")         m_specialSpells.push_back(std::make_unique<Lulu>());
                else if (champName == "Lux")          m_specialSpells.push_back(std::make_unique<Lux>());
                else if (champName == "Malzahar")     m_specialSpells.push_back(std::make_unique<Malzahar>());
                else if (champName == "Orianna")      m_specialSpells.push_back(std::make_unique<Orianna>());
                else if (champName == "Sion")         m_specialSpells.push_back(std::make_unique<Sion>());
                else if (champName == "Syndra")       m_specialSpells.push_back(std::make_unique<Syndra>());
                else if (champName == "Taric")        m_specialSpells.push_back(std::make_unique<Taric>());
                else if (champName == "Twitch")       m_specialSpells.push_back(std::make_unique<Twitch>());
                else if (champName == "Viktor")       m_specialSpells.push_back(std::make_unique<Viktor>());
                else if (champName == "Xerath")       m_specialSpells.push_back(std::make_unique<Xerath>());
                else if (champName == "Yasuo")        m_specialSpells.push_back(std::make_unique<Yasuo>());
                else if (champName == "Yorick")       m_specialSpells.push_back(std::make_unique<Yorick>());
                else if (champName == "Zed")          m_specialSpells.push_back(std::make_unique<Zed>());
                else if (champName == "Ziggs")        m_specialSpells.push_back(std::make_unique<Ziggs>());
                else if (champName == "Zilean")       m_specialSpells.push_back(std::make_unique<Zilean>());
            }

            // Call LoadSpecialSpell on every spell in the database
            for (auto& entry : EzEvade::SpellDetector::onProcessSpells) {
                EzEvade::SpellData& sd = entry.second;
                for (auto& plugin : m_specialSpells) {
                    plugin->LoadSpecialSpell(sd);
                }
            }
        }
    };

} // namespace Plugins
