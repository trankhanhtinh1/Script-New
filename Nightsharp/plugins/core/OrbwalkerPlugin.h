#pragma once
#include "../IPlugin.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/Core/Objects.h"
#include "../../sdk/Core/Game.h"
#include "../../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../../sdk/Wrappers/Orbwalking/OrbwalkerBase.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../sdk/Math/HealthPrediction.h"
#include "../../sdk/Math/Prediction/Health.h"
#include "../../imgui/imgui.h"
#include "../../menu/MenuUI.h"
#include "../../menu/PluginRegistry.h"
#include "../../core/CoreAPI.h"
#include "../../core/CoreControl.h"
#include "../../core/CrashTelemetry.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <string>
#include <unordered_set>
#include <vector>
#include <unordered_map>

// ============================================================================
// OrbwalkerPlugin — advanced orbwalker ported from ImpulseAIO NewOrbwalker.cs
// Core plugin: handles attack + move cycle with proper timing
// Overrides built-in SDK::Orbwalker when loaded
// ============================================================================

namespace Plugins {

class OrbwalkerPlugin : public IPlugin {
public:
    const char* GetName() const override { return "Orbwalker 2.0"; }
    const char* GetInternalId() const override { return "core_orbwalker"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    // ── Debug logging ──
    static void DebugLog(const char* text) {
        if (!text || !*text) return;
        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_orbplugin_debug.txt",
            FILE_APPEND_DATA, FILE_SHARE_READ, nullptr,
            OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (hFile == INVALID_HANDLE_VALUE) return;
        DWORD w = 0;
        WriteFile(hFile, text, (DWORD)lstrlenA(text), &w, nullptr);
        CloseHandle(hFile);
    }
    static void DebugStage(const char* stage) {
        s_debugStage = stage ? stage : "?";
        CrashTelemetry::SetStage(stage);
    }
    static inline const char* s_debugStage = "idle";

    // ====================================================================
    // Lifecycle
    // ====================================================================
    void OnLoad() override {
        m_menu = SDK::Menu::Create("orbwalker_plugin", "Orbwalker 2.0");
        if (!m_menu) return;

        // Attackable sub-menu
        auto* attackable = m_menu->AddSubMenu("attackable", "Attackable Unit");
        attackable->Add<SDK::MenuBool>("barrels", "GP Barrels", true);
        attackable->Add<SDK::MenuBool>("junglePlant", "Jungle Plant", false);
        attackable->Add<SDK::MenuBool>("specialMinions", "Pets", true);
        attackable->Add<SDK::MenuBool>("wards", "Wards", true);
        attackable->Add<SDK::MenuBool>("inhibitor", "Inhibitor", true);
        attackable->Add<SDK::MenuBool>("nexus", "Nexus", true);

        // Prioritize sub-menu
        auto* prioritize = m_menu->AddSubMenu("prioritize", "Prioritize");
        prioritize->Add<SDK::MenuBool>("farmOverHarass", "Farm Over Harass", true);
        prioritize->Add<SDK::MenuBool>("specialMinion", "Special Minion (Barrels/Wards)", false);
        prioritize->Add<SDK::MenuBool>("smallJungle", "Small Jungle", false);
        prioritize->Add<SDK::MenuBool>("turret", "Turret", true);

        // Settings sub-menu
        auto* settings = m_menu->AddSubMenu("settings", "Orbwalker Settings");
        settings->Add<SDK::MenuSlider>("extraHold", "Extra Hold Position", 50, 0, 250);
        settings->Add<SDK::MenuBool>("moveRandom", "Randomize Movement", false);
        settings->Add<SDK::MenuSlider>("windupDelay", "Extra Windup Delay (ms)", 30, 0, 250);
        settings->Add<SDK::MenuBool>("limitAttack", "Don't Kite if AS > 2.5", false);
        settings->Add<SDK::MenuBool>("missileCheck", "Use Missile Checks", true);
        settings->Add<SDK::MenuBool>("calcItemDamage", "Calculate Item Damage", true);
        settings->Add<SDK::MenuBool>("yasuoWallCheck", "Yasuo WindWall Check", true);
        settings->Add<SDK::MenuBool>("highOrb", "High Frequency Walk", false);
        settings->Add<SDK::MenuBool>("calculateRunaway", "Calculate Runaway Distance", false);
        settings->Add<SDK::MenuSlider>("maxMoveDistance", "Max Move Distance", 0, 0, 1500);
        settings->Add<SDK::MenuSlider>("moveDelay", "Move Delay (ms)", 50, 0, 500);

        // Farm sub-menu
        auto* farm = m_menu->AddSubMenu("farm", "Farm");
        farm->Add<SDK::MenuSlider>("farmDelay", "Farm Delay", 30, 0, 200);
        farm->Add<SDK::MenuSlider>("fastFarmDelay", "Fast Farm Delay", 0, 0, 1000);
        farm->Add<SDK::MenuList>("turretFarm", "Turret Farm",
            std::vector<std::string>{"Enabled", "Off"}, 0);
        farm->Add<SDK::MenuSlider>("turretFarmMaxLevel", "Turret Farm Max Level", 18, 1, 18);
        farm->Add<SDK::MenuBool>("shouldWait", "Wait for Last Hit", true);

        // Misc sub-menu
        auto* misc = m_menu->AddSubMenu("misc", "Misc");
        misc->Add<SDK::MenuBool>("drawChaseRange", "Draw Chase Range", false);
        misc->Add<SDK::MenuBool>("showFakeClick", "Show Fake Click", false);
        misc->Add<SDK::MenuSlider>("forceChaseRange", "Force Chase Extra Range", 0, 0, 500);
        misc->Add<SDK::MenuKeyBind>("findKey", "Force Chase Key", 'F', SDK::KeyBindType::Press);

        // Drawing sub-menu
        auto* draw = m_menu->AddSubMenu("drawing", "Drawing");
        draw->Add<SDK::MenuBool>("drawAttackRange", "Draw Attack Range", true);
        draw->Add<SDK::MenuBool>("drawHoldPosition", "Draw Hold Position", false);
        draw->Add<SDK::MenuBool>("drawKillableMinion", "Draw Killable Minion", false);
        draw->Add<SDK::MenuBool>("drawActiveMode", "Draw Active Mode", true);
        draw->Add<SDK::MenuBool>("drawNonKillable", "Draw Non-Killable Minion", false);

        // Keybinds
        m_menu->Add<SDK::MenuSeparator>("sep_keys", "--- Keybinds ---");
        m_menu->Add<SDK::MenuKeyBind>("combo", "Combo", VK_SPACE, SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("flee", "Flee", 'Z', SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("comboNoMove", "Combo (No Move)", 0, SDK::KeyBindType::Press);

        // === Override built-in SDK Orbwalker ===
        DisableSDKOrbwalker();
    }

    void OnUnload() override {
        // === Restore built-in SDK Orbwalker ===
        RestoreSDKOrbwalker();
        m_menu = nullptr;
    }

    // ====================================================================
    // Per-frame logic
    // ====================================================================
    void OnUpdate() override {
      __try {
        OnUpdateImpl();
      }
      __except (OrbPluginCrashHandler(GetExceptionInformation())) {
        return;
      }
    }

    static LONG WINAPI OrbPluginCrashHandler(EXCEPTION_POINTERS* ep) {
        char buf[512] = {};
        std::snprintf(buf, sizeof(buf),
            "[OrbPlugin] CRASH at stage='%s' code=0x%08X addr=%p\r\n",
            s_debugStage,
            ep && ep->ExceptionRecord ? ep->ExceptionRecord->ExceptionCode : 0,
            ep && ep->ExceptionRecord ? (void*)ep->ExceptionRecord->ExceptionAddress : nullptr);
        DebugLog(buf);
        CrashTelemetry::SetStage(buf);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    void OnUpdateImpl() {
        DebugStage("OrbPlugin::OnUpdate::Enter");
        if (!m_menu) return;

        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;

        // Poll AA state every frame (detect actual AA start + missile launch)
        DebugStage("OrbPlugin::PollAttackState");
        PollAttackState();

        if (!SDK::Game::ShouldProcessInput()) return;

        DebugStage("OrbPlugin::GetActiveMode");
        m_activeMode = GetActiveMode();
        SDK::Orbwalker::Instance().ActiveMode = m_activeMode;
        if (m_activeMode == SDK::OrbwalkerMode::None) return;

        // Update ForceChase state
        m_forceChaseActive = false;
        if (m_activeMode == SDK::OrbwalkerMode::Combo) {
            auto* misc = m_menu->GetSubMenu("misc");
            if (misc) {
                bool findKey = misc->GetKeyBindValue("findKey", false);
                int chaseRange = misc->GetSliderValue("forceChaseRange", 0);
                if (findKey && chaseRange > 0) {
                    m_forceChaseActive = true;
                    m_forceChaseExtraRange = (float)chaseRange;
                }
            }
        }

        DebugStage("OrbPlugin::GetTarget");
        SDK::AIBaseClient target = GetTarget();

        DebugStage("OrbPlugin::Orbwalk");
        Orbwalk(target);

        DebugStage("OrbPlugin::OnUpdate::Done");
    }

    // ====================================================================
    // Drawing
    // ====================================================================
    void OnRender() override {
      __try {
        OnRenderImpl();
      }
      __except (OrbPluginCrashHandler(GetExceptionInformation())) {
        return;
      }
    }

    void OnRenderImpl() {
        if (!m_menu) return;
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;

        auto* drawMenu = m_menu->GetSubMenu("drawing");
        if (!drawMenu) return;

        // Draw attack range
        if (drawMenu->GetBoolValue("drawAttackRange", true)) {
            SDK::Drawing::DrawCircle(player.Position(),
                player.AttackRange() + player.BoundingRadius(), IM_COL32(200, 150, 200, 150), 1.5f);
        }

        // Draw hold position
        if (drawMenu->GetBoolValue("drawHoldPosition", false)) {
            auto* settings = m_menu->GetSubMenu("settings");
            int holdDist = settings ? settings->GetSliderValue("extraHold", 50) : 50;
            SDK::Drawing::DrawCircle(player.Position(),
                player.BoundingRadius() + (float)holdDist, IM_COL32(128, 0, 200, 100), 1.0f);
        }

        // TODO: Killable/Non-killable minion drawing removed — will be rewritten with farm logic

        // Draw chase range
        auto* miscMenu = m_menu->GetSubMenu("misc");
        if (miscMenu) {
            int chaseRange = miscMenu->GetSliderValue("forceChaseRange", 0);
            if (miscMenu->GetBoolValue("drawChaseRange", false) && chaseRange > 0) {
                float totalRange = player.AttackRange() + player.BoundingRadius() + (float)chaseRange;
                if (m_forceChaseActive) {
                    float t = fmodf(SDK::Game::Time() * 2.0f, 1.0f);
                    int r = (int)(sinf(t * 6.2832f) * 127 + 128);
                    int g = (int)(sinf(t * 6.2832f + 2.094f) * 127 + 128);
                    int b = (int)(sinf(t * 6.2832f + 4.189f) * 127 + 128);
                    SDK::Drawing::DrawCircle(player.Position(), totalRange, IM_COL32(r, g, b, 200), 2.5f);
                } else {
                    SDK::Drawing::DrawCircle(player.Position(), totalRange, IM_COL32(100, 200, 255, 120), 1.0f);
                }
            }
        }

        // Draw active mode indicator
        if (drawMenu->GetBoolValue("drawActiveMode", true) && m_activeMode != SDK::OrbwalkerMode::None) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (dl) {
                const char* modeText = nullptr;
                ImU32 modeColor = IM_COL32(255, 255, 255, 220);
                switch (m_activeMode) {
                case SDK::OrbwalkerMode::Combo:   modeText = "Combo"; modeColor = IM_COL32(255, 100, 100, 220); break;
                case SDK::OrbwalkerMode::Harass:  modeText = "Harass"; modeColor = IM_COL32(255, 200, 100, 220); break;
                case SDK::OrbwalkerMode::LastHit: modeText = "LastHit"; modeColor = IM_COL32(100, 255, 100, 220); break;
                case SDK::OrbwalkerMode::Clear:   modeText = "LaneClear"; modeColor = IM_COL32(100, 200, 255, 220); break;
                case SDK::OrbwalkerMode::Flee:    modeText = "Flee"; modeColor = IM_COL32(200, 200, 200, 220); break;
                default: break;
                }
                if (modeText) {
                    ImVec2 displaySize = ImGui::GetIO().DisplaySize;
                    ImVec2 textSize = ImGui::CalcTextSize(modeText);
                    float x = displaySize.x / 2 - textSize.x / 2;
                    float y = displaySize.y - 80;
                    dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 180), modeText);
                    dl->AddText(ImVec2(x, y), modeColor, modeText);
                }
            }
        }
    }

    // ====================================================================
    // Menu
    // ====================================================================
    void OnMenu() override {
        // Menu is drawn by the MenuUI system
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    // ====================================================================
    // Public API (for scripts)
    // ====================================================================
    SDK::OrbwalkerMode GetActiveMode() const {
        if (!m_menu) return SDK::OrbwalkerMode::None;

        if (m_menu->GetKeyBindValue("combo", false)) return SDK::OrbwalkerMode::Combo;
        if (m_menu->GetKeyBindValue("comboNoMove", false)) return SDK::OrbwalkerMode::Combo;
        if (m_menu->GetKeyBindValue("flee", false)) return SDK::OrbwalkerMode::Flee;
        // TODO: V/C/X/A farm modes removed — will be rewritten

        return SDK::OrbwalkerMode::None;
    }

    bool IsComboNoMove() const {
        if (!m_menu) return false;
        return m_menu->GetKeyBindValue("comboNoMove", false);
    }

    bool IsItemDamageCalculationEnabled() const {
        if (!m_menu) return true;
        auto* settings = m_menu->GetSubMenu("settings");
        return settings ? settings->GetBoolValue("calcItemDamage", true) : true;
    }

    // ====================================================================
    // Timing
    // ====================================================================
    bool CanAttack(float extraWindup = 0.0f) const {
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return false;

        // Check buffs that prevent attacking
        if (player.HasBuff("tahmkenchwhasdevouredtarget")) return false;

        // Champion-specific checks
        std::string champName = player.CharacterName();
        if (champName == "Jhin" && player.HasBuff("JhinPassiveReload")) return false;
        if (champName == "Graves") {
            if (!player.HasBuff("GravesBasicAttackAmmo1") &&
                !player.HasBuff("GravesBasicAttackAmmo2")) {
                return false;
            }
        }

        float now = SDK::Game::Time();
        float delay = player.AttackDelay();
        return now >= m_lastAttackCommandTime + delay;
    }

    bool CanMove(float extraWindup = 0.0f) const {
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return false;

        // BlockOrders — hard block after attack
        float now = SDK::Game::Time();
        if (now < m_blockOrdersUntil) return false;

        std::string champName = player.CharacterName();
        if (champName == "Kalista") return true;

        if (champName == "Rengar" && player.HasBuff("RengarR")) {
            extraWindup += 0.05f;
        }

        float windup = player.AttackCastDelay();

        // Missile launched — allow movement immediately
        auto* settings = m_menu ? m_menu->GetSubMenu("settings") : nullptr;
        bool missileCheck = settings ? settings->GetBoolValue("missileCheck", true) : true;
        if (m_missileLaunched && missileCheck) return true;

        float windupDelayMs = settings ? (float)(settings->GetSliderValue("windupDelay", 80)) : 80.0f;
        float windupBuffer = windupDelayMs / 1000.0f;

        // Use confirmed AA start time (from polling) if available, otherwise order time
        float attackStartTime = (m_confirmedAttackStartTime > 0.0f)
            ? m_confirmedAttackStartTime
            : m_lastAttackCommandTime;

        return now >= attackStartTime + windup + windupBuffer + extraWindup;
    }

    void ResetAutoAttackTimer() {
        m_lastAttackCommandTime = 0.0f;
        m_confirmedAttackStartTime = 0.0f;
        m_missileLaunched = false;
        m_blockOrdersUntil = 0.0f;
        SDK::Orbwalker::Instance().LastAutoAttackTick = 0;
        SDK::Orbwalker::Instance().MissileLaunched = false;
    }

    SDK::OrbwalkerMode ActiveMode() const { return m_activeMode; }

    bool ShouldWait(float range) {
        // TODO: farm logic removed — will be rewritten
        (void)range;
        return false;
    }

    bool BlockAttack() const { return m_blockAttack; }
    bool BlockMove() const { return m_blockMove; }
    void SetBlockAttack(bool block) { m_blockAttack = block; }
    void SetBlockMove(bool block) { m_blockMove = block; }

private:
    // ====================================================================
    // State
    // ====================================================================
    SDK::Menu* m_menu = nullptr;
    SDK::OrbwalkerMode m_activeMode = SDK::OrbwalkerMode::None;
    float m_lastAttackCommandTime = 0.0f;
    float m_lastMoveTime = 0.0f;
    int m_autoAttackCounter = 0;
    bool m_afterAttackFired = true;
    bool m_forceChaseActive = false;
    float m_forceChaseExtraRange = 0.0f;
    bool m_blockAttack = false;
    bool m_blockMove = false;
    SDK::Vector3 m_lastMoveDir = SDK::Vector3(0, 0, 0);
    SDK::AIBaseClient m_lastTarget;

    // AA polling state (detect actual AA start, not order send time)
    bool m_wasWindingUp = false;
    float m_confirmedAttackStartTime = 0.0f;
    bool m_missileLaunched = false;
    float m_blockOrdersUntil = 0.0f;

    // SDK Orbwalker override state
    int m_sdkOrbRegistryIndex = -1;
    bool m_sdkOrbWasLoaded = true;

    // ====================================================================
    // SDK Orbwalker Override: Disable/Restore
    // ====================================================================
    void DisableSDKOrbwalker() {
        // 1. Disable via menu (prevents Orbwalker::Update from running its loop)
        auto* sdkMenu = SDK::Orbwalker::GetMenu();
        if (sdkMenu) {
            auto* enabled = sdkMenu->Get<SDK::MenuBool>("enabledOption");
            if (enabled) {
                enabled->Enabled = false;
            }
        }

        // 2. Hide from PluginRegistry sidebar
        m_sdkOrbRegistryIndex = PluginRegistry::FindByInternalId("orbwalker");
        if (m_sdkOrbRegistryIndex >= 0) {
            m_sdkOrbWasLoaded = PluginRegistry::Plugins[m_sdkOrbRegistryIndex].Loaded;
            PluginRegistry::Plugins[m_sdkOrbRegistryIndex].Loaded = false;
        }
    }

    void RestoreSDKOrbwalker() {
        // 1. Re-enable via menu
        auto* sdkMenu = SDK::Orbwalker::GetMenu();
        if (sdkMenu) {
            auto* enabled = sdkMenu->Get<SDK::MenuBool>("enabledOption");
            if (enabled) {
                enabled->Enabled = true;
            }
        }

        // 2. Restore PluginRegistry visibility
        if (m_sdkOrbRegistryIndex >= 0) {
            PluginRegistry::Plugins[m_sdkOrbRegistryIndex].Loaded = m_sdkOrbWasLoaded;
            m_sdkOrbRegistryIndex = -1;
        }
    }

    // ====================================================================
    // Poll-based AA detection (replaces reliance on SDK PollAutoAttackState)
    // Detects actual game AA start and missile launch via IsWindingUp()
    // ====================================================================
    void PollAttackState() {
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        const bool isWindingUp = player.IsWindingUp();
        const float now = SDK::Game::Time();

        // Phase 1: Detect AA START — transition: NOT winding up → IS winding up
        if (isWindingUp && !m_wasWindingUp) {
            m_confirmedAttackStartTime = now;
            m_missileLaunched = false;
            SDK::Orbwalker::Instance().MissileLaunched = false;
        }

        // Phase 2: Detect MISSILE LAUNCH — transition: WAS winding up → NOT winding up
        if (!isWindingUp && m_wasWindingUp && !m_missileLaunched) {
            m_missileLaunched = true;
            SDK::Orbwalker::Instance().MissileLaunched = true;

            if (m_lastTarget.IsValid()) {
                SDK::OrbwalkingActionArgs afterArgs;
                afterArgs.Target = m_lastTarget;
                afterArgs.Sender = player;
                afterArgs.Type = SDK::OrbwalkingType::AfterAttack;
                afterArgs.Process = true;
                SDK::Orbwalker::Instance().InvokeAction(afterArgs);
            }
        }

        // Fallback: if windup time exceeded but still no missile detected
        if (!m_missileLaunched && m_confirmedAttackStartTime > 0.0f) {
            float windup = player.AttackCastDelay();
            if (now >= m_confirmedAttackStartTime + windup + 0.05f) {
                m_missileLaunched = true;
                SDK::Orbwalker::Instance().MissileLaunched = true;
            }
        }

        m_wasWindingUp = isWindingUp;
    }

    // ====================================================================
    // GetProjectileSpeed
    // ====================================================================
    static float GetProjectileSpeed(const SDK::AIBaseClient& unit) {
        if (unit.IsMelee()) return FLT_MAX;

        std::string name = unit.CharacterName();
        if (name.empty()) return 2000.0f;

        static const std::unordered_map<std::string, float> speedMap = {
            {"Jinx", 2750.0f}, {"Kayle", 2000.0f}, {"Viktor", 2300.0f}, {"Neeko", 1500.0f},
            {"Jayce", 2500.0f}, {"Nidalee", 1750.0f}, {"Elise", 1600.0f},
            {"Ivern", 1600.0f}, {"Poppy", 1600.0f}, {"Thresh", 1800.0f}, {"Rakan", 1800.0f},
            {"Aphelios", 2100.0f}, {"Caitlyn", 2500.0f}, {"Ezreal", 2000.0f},
            {"Ashe", 2000.0f}, {"Varus", 2000.0f}, {"KogMaw", 1800.0f}, {"Twitch", 2500.0f},
            {"Tristana", 2250.0f}, {"Lucian", 2800.0f}, {"Vayne", 2000.0f}, {"Draven", 1600.0f},
            {"Jhin", 2600.0f}, {"MissFortune", 2000.0f}, {"Kalista", 2400.0f}, {"Sivir", 1750.0f},
            {"Xayah", 2075.0f}, {"Kaisa", 2000.0f}, {"Senna", 20000.0f}, {"Samira", 2600.0f},
            {"Zeri", 2600.0f}, {"Nilah", FLT_MAX}, {"Smolder", 2500.0f},
            {"Teemo", 1500.0f}, {"Azir", FLT_MAX}, {"Orianna", 1450.0f}, {"Syndra", 1800.0f},
            {"Lux", 1600.0f}, {"Ahri", 1750.0f}, {"Annie", 1500.0f}, {"Brand", 1600.0f},
            {"Cassiopeia", 1500.0f}, {"Velkoz", 1600.0f}, {"Xerath", 2050.0f}, {"Ziggs", 1500.0f},
            {"Zyra", 1700.0f}, {"Lulu", 1450.0f}, {"Nami", 1500.0f}, {"Sona", 1500.0f},
            {"Soraka", 1500.0f}, {"Janna", 1600.0f}, {"Yuumi", 1500.0f}, {"Seraphine", 1500.0f},
            {"Heimerdinger", 1500.0f}, {"Kennen", 1600.0f}, {"Quinn", 2000.0f}, {"Kindred", 2000.0f},
            {"Graves", FLT_MAX},
        };

        auto it = speedMap.find(name);
        if (it != speedMap.end()) return it->second;
        return 2000.0f;
    }

    static bool HasJaxCounterStrike(const SDK::AIBaseClient& target) {
        if (!target.IsValid() || !target.IsHero()) return false;
        return target.HasBuff("JaxCounterStrike");
    }

    // ====================================================================
    // Target selection
    // ====================================================================
    SDK::AIBaseClient GetTarget() {
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return SDK::AIBaseClient();

        if (m_activeMode == SDK::OrbwalkerMode::None || m_activeMode == SDK::OrbwalkerMode::Flee)
            return SDK::AIBaseClient();

        float range = player.AttackRange() + player.BoundingRadius();
        if (m_forceChaseActive)
            range += m_forceChaseExtraRange;

        // Combo: hero target only
        auto hero = GetBestHeroTarget(range);
        if (hero.IsValid()) return hero;

        // TODO: farm/harass/lasthit target logic removed — will be rewritten

        return SDK::AIBaseClient();
    }

    SDK::AIBaseClient GetBestHeroTarget(float range) {
        auto player = SDK::ObjectManager::Player();

        // Check TargetSelector forced target
        auto forced = SDK::TargetSelector::GetForcedTarget();
        if (forced.IsValid() && forced.IsAlive() && forced.IsVisible()) {
            if (player.Distance(forced) <= range + forced.BoundingRadius()) {
                if (!HasJaxCounterStrike(forced))
                    return forced;
            }
        }

        auto target = SDK::TargetSelector::GetTarget(range);
        if (target.IsValid() && HasJaxCounterStrike(target)) {
            auto targets = SDK::TargetSelector::GetTargets(range);
            for (auto& t : targets) {
                if (!HasJaxCounterStrike(t))
                    return t;
            }
            return SDK::AIBaseClient();
        }

        return target;
    }

    // TODO: Farm functions removed (GetSpecialMinion, AnalyzeLaneFarm, TurretFarm,
    // GetBestJungleTarget, DetectNonKillableMinions, timing helpers) — will be rewritten

    // ====================================================================
    // Orbwalk — Attack + Move cycle
    // ====================================================================
    void Orbwalk(SDK::AIBaseClient& target) {
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;

        float now = SDK::Game::Time();

        // AFTER ATTACK EVENT
        DebugStage("OrbPlugin::Orbwalk::AfterAttackCheck");
        if (!m_afterAttackFired &&
            m_lastAttackCommandTime > 0.0f &&
            CanMove() &&
            m_lastTarget.IsValid()) {
            DebugStage("OrbPlugin::Orbwalk::AfterAttackInvoke");
            SDK::OrbwalkingActionArgs afterArgs;
            afterArgs.Target = m_lastTarget;
            afterArgs.Sender = player;
            afterArgs.Type = SDK::OrbwalkingType::AfterAttack;
            afterArgs.Process = true;
            SDK::Orbwalker::Instance().InvokeAction(afterArgs);
            m_afterAttackFired = true;
        }

        // ATTACK PHASE
        DebugStage("OrbPlugin::Orbwalk::AttackCheck");
        if (CanAttack() && !m_blockAttack && target.IsValid() && player.InAutoAttackRange(target)) {
            SDK::OrbwalkingActionArgs beforeArgs;
            beforeArgs.Target = target;
            beforeArgs.Sender = player;
            beforeArgs.Type = SDK::OrbwalkingType::BeforeAttack;
            beforeArgs.Process = true;
            SDK::Orbwalker::Instance().InvokeAction(beforeArgs);

            if (beforeArgs.Process) {
                // TargetSwitch event
                if (m_lastTarget.IsValid() && m_lastTarget.NetworkId() != target.NetworkId()) {
                    SDK::OrbwalkingActionArgs switchArgs;
                    switchArgs.Target = target;
                    switchArgs.Sender = player;
                    switchArgs.Type = SDK::OrbwalkingType::TargetSwitch;
                    switchArgs.Process = true;
                    SDK::Orbwalker::Instance().InvokeAction(switchArgs);
                }

                Attack(target);

                SDK::OrbwalkingActionArgs onArgs;
                onArgs.Target = target;
                onArgs.Sender = player;
                onArgs.Type = SDK::OrbwalkingType::OnAttack;
                onArgs.Process = true;
                SDK::Orbwalker::Instance().InvokeAction(onArgs);

                m_lastTarget = target;
                return;
            }
        }

        // MOVE PHASE
        DebugStage("OrbPlugin::Orbwalk::MovePhase");
        if (!CanMove()) return;
        if (m_blockMove) return;

        // LimitAttack: skip kiting if AS > 2.5
        auto* settings = m_menu ? m_menu->GetSubMenu("settings") : nullptr;
        bool limitAttack = settings ? settings->GetBoolValue("limitAttack", false) : false;
        if (limitAttack) {
            float delay = player.AttackDelay();
            if (delay < 0.3846f && m_autoAttackCounter % 3 != 0)
                return;
        }

        if (IsComboNoMove()) return;

        // TODO: NonKillableMinion detection removed — will be rewritten with farm logic

        // Movement settings
        float moveDelay = 0.05f;
        bool moveRandom = false;
        float holdDist = 50.0f;
        int maxMoveDist = 0;
        bool highOrb = false;

        if (settings) {
            moveDelay = (float)settings->GetSliderValue("moveDelay", 50) / 1000.0f;
            moveRandom = settings->GetBoolValue("moveRandom", false);
            holdDist = (float)settings->GetSliderValue("extraHold", 50);
            maxMoveDist = settings->GetSliderValue("maxMoveDistance", 0);
            highOrb = settings->GetBoolValue("highOrb", false);
        }

        if (highOrb) moveDelay = (std::min)(moveDelay, 0.02f);
        if (now < m_lastMoveTime + moveDelay) return;

        SDK::Vector3 mousePos = SDK::Game::CursorPos();
        SDK::Vector3 playerPos = player.Position();

        float distToCursor = playerPos.Distance2D(mousePos);
        if (distToCursor < (std::max)(30.0f, holdDist + player.BoundingRadius()))
            return;

        SDK::Vector3 finalPos = mousePos;

        // Max move distance
        if (maxMoveDist > 0 && distToCursor > (float)maxMoveDist) {
            SDK::Vector3 dir(mousePos.x - playerPos.x, 0, mousePos.z - playerPos.z);
            float dirLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            if (dirLen > 1.0f) {
                dir = SDK::Vector3(dir.x / dirLen, 0, dir.z / dirLen);
                finalPos = SDK::Vector3(playerPos.x + dir.x * (float)maxMoveDist,
                    mousePos.y, playerPos.z + dir.z * (float)maxMoveDist);
            }
        }

        // Randomize movement
        if (moveRandom && distToCursor > 100.0f) {
            SDK::Vector3 moveDir(finalPos.x - playerPos.x, 0, finalPos.z - playerPos.z);
            float moveDirLen = std::sqrt(moveDir.x * moveDir.x + moveDir.z * moveDir.z);
            if (moveDirLen > 1.0f) {
                float rndFactor = 0.6f + (float)(rand() % 40) / 100.0f;
                float rndDist = rndFactor * 400.0f;
                SDK::Vector3 dir(moveDir.x / moveDirLen, 0, moveDir.z / moveDirLen);
                finalPos = SDK::Vector3(playerPos.x + dir.x * rndDist,
                    mousePos.y, playerPos.z + dir.z * rndDist);
            }
        }

        // Angle check
        SDK::Vector3 newDir(finalPos.x - playerPos.x, 0, finalPos.z - playerPos.z);
        float newDirLen = std::sqrt(newDir.x * newDir.x + newDir.z * newDir.z);
        if (newDirLen > 1.0f && m_lastMoveDir.x != 0.0f) {
            newDir = SDK::Vector3(newDir.x / newDirLen, 0, newDir.z / newDirLen);
            float dot = newDir.x * m_lastMoveDir.x + newDir.z * m_lastMoveDir.z;
            if (dot > 0.996f && (now - m_lastMoveTime) < 0.15f) {
                return;
            }
        }

        // Movement event
        SDK::OrbwalkingActionArgs moveArgs;
        moveArgs.Position = finalPos;
        moveArgs.Sender = player;
        moveArgs.Type = SDK::OrbwalkingType::Movement;
        moveArgs.Process = true;
        SDK::Orbwalker::Instance().InvokeAction(moveArgs);

        if (!moveArgs.Process) return;
        finalPos = moveArgs.Position;

        DebugStage("OrbPlugin::Orbwalk::MoveTo");
        MoveTo(finalPos);
    }

    void Attack(SDK::AIBaseClient& target) {
        auto player = SDK::ObjectManager::Player();
        player.IssueOrder(SDK::GameObjectOrder::AttackUnit, target);
        float now = SDK::Game::Time();
        m_lastAttackCommandTime = now;
        m_missileLaunched = false;
        m_confirmedAttackStartTime = 0.0f; // reset, will be set by PollAttackState
        m_afterAttackFired = false;
        m_autoAttackCounter++;

        // BlockOrders: prevent move/attack for windup safety window
        float ping = SDK::Game::Ping() / 2000.0f; // half-ping in seconds
        m_blockOrdersUntil = now + 0.07f + ping;

        // Sync SDK state
        SDK::Orbwalker::Instance().LastAutoAttackTick = SDK::Game::TickCount();
        SDK::Orbwalker::Instance().MissileLaunched = false;
        SDK::Orbwalker::Instance().TotalAutoAttacks++;
    }

    void MoveTo(SDK::Vector3 pos) {
        auto player = SDK::ObjectManager::Player();
        SDK::Vector3 playerPos = player.Position();
        SDK::Vector3 dir(pos.x - playerPos.x, 0, pos.z - playerPos.z);
        float dirLen = std::sqrt(dir.x * dir.x + dir.z * dir.z);
        if (dirLen > 1.0f)
            m_lastMoveDir = SDK::Vector3(dir.x / dirLen, 0, dir.z / dirLen);

        player.IssueOrder(SDK::GameObjectOrder::MoveTo, pos);
        m_lastMoveTime = SDK::Game::Time();
    }

};

} // namespace Plugins
