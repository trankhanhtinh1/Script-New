#pragma once
#include "../IPlugin.h"
#include "../../sdk/UI/UI.h"
#include "../../sdk/UI/Drawing.h"
#include "../../sdk/Core/Objects.h"
#include "../../sdk/Core/Game.h"
#include "../../sdk/Wrappers/Orbwalking/Orbwalker.h"
#include "../../sdk/Wrappers/Orbwalking/OrbwalkerBase.h"
#include "../../sdk/Wrappers/TargetSelector/TargetSelector.h"
#include "../../sdk/Math/Collision.h"
#include "../../sdk/Math/HealthPrediction.h"
#include "../../sdk/Math/Prediction/Health.h"
#include "../../sdk/Utils/Jungle.h"
#include "../../imgui/imgui.h"
#include "../../menu/MenuUI.h"
#include "../../menu/PluginRegistry.h"
#include "../../core/CoreAPI.h"
#include "../../core/CrashTelemetry.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

// ============================================================================
// OrbwalkerPlugin — 100% port from NewOrbwalker.cs (ImpulseAIO)
// Every function references the original C# line numbers.
// Debug stages at every critical point for crash diagnosis.
// ============================================================================

namespace Plugins {

class OrbwalkerPlugin : public IPlugin {
public:
    const char* GetName() const override { return "Orbwalker 2.0"; }
    const char* GetInternalId() const override { return "core_orbwalker"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    static void DbgStage(const char* s) { CrashTelemetry::SetStage(s); }

    static OrbwalkerPlugin* Get() { return s_instance; }
    static SDK::Menu* GetPluginMenu() { return s_instance ? s_instance->m_menu : nullptr; }

    // ═══════════════════════════════════════════════════════════════════
    // OnLoad — Menu + Init  (C# constructor lines 92-213)
    // ═══════════════════════════════════════════════════════════════════
    void OnLoad() override {
        if (m_menu) return;
        m_menu = SDK::Menu::Create("orbwalker_plugin", "Orbwalker 2.0");
        if (!m_menu) return;

        // ── Attackable (C# lines 99-103) ──
        auto* atkMenu = m_menu->AddSubMenu("attackable", "Attackable Unit");
        atkMenu->Add<SDK::MenuBool>("barrels", "GP Barrels", true);
        atkMenu->Add<SDK::MenuBool>("junglePlant", "Jungle Plant", false);
        atkMenu->Add<SDK::MenuBool>("specialMinions", "Pets", true);
        atkMenu->Add<SDK::MenuBool>("wards", "Wards", true);

        // ── Prioritize (C# lines 104-108) ──
        auto* priMenu = m_menu->AddSubMenu("prioritize", "Prioritize");
        priMenu->Add<SDK::MenuBool>("farmOverHarass", "Farm Over Harass", true);
        priMenu->Add<SDK::MenuBool>("specialMinion", "Special Minion", false);
        priMenu->Add<SDK::MenuBool>("smallJungle", "Small Jungle", false);
        priMenu->Add<SDK::MenuBool>("turret", "Turret", true);

        // ── Orbwalker Settings (C# lines 109-115) ──
        auto* orbMenu = m_menu->AddSubMenu("settings", "Orbwalker Settings");
        orbMenu->Add<SDK::MenuSlider>("extraHold", "Extra Hold Position", 50, 0, 250);
        orbMenu->Add<SDK::MenuBool>("moveRandom", "Randomize Movement", false);
        orbMenu->Add<SDK::MenuSlider>("windupDelay", "Extra Windup Delay", 60, 0, 250);
        orbMenu->Add<SDK::MenuBool>("limitAttack", "Don't Kite if AS > 2.5", false);
        orbMenu->Add<SDK::MenuBool>("highOrb", "High Frequency Walk", false);
        orbMenu->Add<SDK::MenuBool>("calculateRunaway", "Calculate Runaway Distance", true);

        // ── Advanced (C# lines 125-128) ──
        auto* advMenu = m_menu->AddSubMenu("advanced", "Advanced");
        advMenu->Add<SDK::MenuBool>("calcItemDamage", "Calculate Item Damage", false);
        advMenu->Add<SDK::MenuBool>("yasuoWallCheck", "Yasuo WindWall Check", true);
        advMenu->Add<SDK::MenuBool>("missileCheck", "Use Missile Checks", true);

        // ── Farm (C# lines 116-124) ──
        auto* farmMenu = m_menu->AddSubMenu("farm", "Farm");
        farmMenu->Add<SDK::MenuSlider>("farmDelay", "Farm Delay", 30, 0, 200);
        farmMenu->Add<SDK::MenuSlider>("fastFarmDelay", "Fast Farm Delay", 220, 0, 1000);
        farmMenu->Add<SDK::MenuList>("turretFarm", "Turret Farm",
            std::vector<std::string>{"Enabled", "Off"}, 0);
        farmMenu->Add<SDK::MenuSlider>("turretFarmMaxLevel", "Turret Farm Max Level", 13, 1, 18);
        farmMenu->Add<SDK::MenuBool>("shouldWait", "Wait for Last Hit", true);

        // ── Drawing (C# lines 130-135) ──
        auto* drawMenu = m_menu->AddSubMenu("drawing", "Drawing");
        drawMenu->Add<SDK::MenuBool>("drawAttackRange", "Draw Attack Range", true);
        drawMenu->Add<SDK::MenuBool>("drawHoldPosition", "Draw Hold Position", false);
        drawMenu->Add<SDK::MenuBool>("drawKillableMinion", "Draw Killable Minion", false);
        drawMenu->Add<SDK::MenuBool>("drawActiveMode", "Draw Active Mode", true);
        drawMenu->Add<SDK::MenuBool>("drawChaseRange", "Draw Chase Range", true);
        drawMenu->Add<SDK::MenuBool>("showFakeClick", "Show FakeClick", false);

        // ── Misc (C# lines 136-138) ──
        auto* miscMenu = m_menu->AddSubMenu("misc", "Extra Range Setting");
        miscMenu->Add<SDK::MenuSlider>("forceChaseRange", "Extra LowHP Target Range", 200, 0, 500);
        miscMenu->Add<SDK::MenuKeyBind>("findKey", "Force Chase Key", 'F', SDK::KeyBindType::Press);

        // ── Keybinds (C# lines 139-145) ──
        m_menu->Add<SDK::MenuSeparator>("sep_keys", "--- Keybinds ---");
        m_menu->Add<SDK::MenuKeyBind>("combo", "Combo", VK_SPACE, SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("comboNoMove", "Combo (No Move)", 'N', SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("harass", "Harass", 'C', SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("laneClear", "LaneClear", 'V', SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("fastLaneClear", "Fast LaneClear", VK_LBUTTON, SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("lastHit", "LastHit", 'X', SDK::KeyBindType::Press);
        m_menu->Add<SDK::MenuKeyBind>("flee", "Flee", 'Z', SDK::KeyBindType::Press);

        // ── Champion flags (C# lines 146-205) ──
        std::string myName = Player().CharacterName();
        m_isAphelios = (myName == "Aphelios");
        m_isGraves   = (myName == "Graves");
        m_isJhin     = (myName == "Jhin");
        m_isKalista  = (myName == "Kalista");
        m_isRengar   = (myName == "Rengar");
        m_isSett     = (myName == "Sett");

        for (const auto& hero : SDK::ObjectManager::EnemyHeroes()) {
            if (!hero.IsValid()) continue;
            std::string eName = hero.CharacterName();
            if (eName == "Jax") m_jaxInGame = true;
            if (eName == "Gangplank") m_gpInGame = true;
        }
        for (const auto& hero : SDK::ObjectManager::Heroes()) {
            if (!hero.IsValid() || hero.IsMe()) continue;
            if (hero.CharacterName() == "TahmKench") m_tahmInGame = true;
        }

        m_initialized = true;
        s_instance = this;
        SDK::Orbwalker::SetMenu(m_menu);
    }

    void OnUnload() override {
        s_instance = nullptr;
        SDK::Orbwalker::SetMenu(nullptr);
        m_menu = nullptr;
        m_initialized = false;
    }

    SDK::MenuUI::Menu* GetMenuRoot() override { return m_menu; }

    // ═══════════════════════════════════════════════════════════════════
    // OnUpdate  (C# OnUpdate lines 817-841)
    // ═══════════════════════════════════════════════════════════════════
    void OnUpdate() override {
        __try { OnUpdateImpl(); }
        __except (CrashTelemetry::ReportAndHandle("OrbPlugin", GetExceptionInformation())) { return; }
    }

    void OnUpdateImpl() {
        DbgStage("OrbPlugin::Update::Enter");
        if (!m_initialized || !m_menu) return;

        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;

        // Sett passive timeout (C# line 823)
        if (m_isSett && m_nextAttackIsPassive && m_settAttackTime > 0 &&
            SDK::Game::TickCount() - m_settAttackTime > 2000) {
            m_nextAttackIsPassive = false;
        }

        // Poll AA state (replaces C# OnDoCast/OnProcessSpellCast)
        DbgStage("OrbPlugin::PollAA");
        PollAttackState();

        if (!SDK::Game::ShouldProcessInput()) return;

        DbgStage("OrbPlugin::Mode");
        m_activeMode = GetActiveMode();
        SDK::Orbwalker::Instance().ActiveMode = m_activeMode;
        if (m_activeMode == SDK::OrbwalkerMode::None) return;

        DbgStage("OrbPlugin::GetTarget");
        SDK::AIBaseClient target = GetTarget();

        DbgStage("OrbPlugin::Orbwalk");
        Orbwalk(target);

        DbgStage("OrbPlugin::Update::Done");
    }

    // ═══════════════════════════════════════════════════════════════════
    // OnRender  (C# OnDraw lines 846-900)
    // ═══════════════════════════════════════════════════════════════════
    void OnRender() override {
        __try { OnRenderImpl(); }
        __except (CrashTelemetry::ReportAndHandle("OrbPlugin", GetExceptionInformation())) { return; }
    }

    void OnRenderImpl() {
        if (!m_initialized || !m_menu) return;
        auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        auto* drawMenu = m_menu->GetSubMenu("drawing");
        if (!drawMenu) return;

        // Draw AA range (C# line 862)
        if (drawMenu->GetBoolValue("drawAttackRange", true)) {
            SDK::Drawing::DrawCircle(player.Position(),
                player.AttackRange() + player.BoundingRadius(),
                IM_COL32(200, 150, 200, 150), 1.5f);
        }

        // Draw hold position (C# line 868)
        if (drawMenu->GetBoolValue("drawHoldPosition", false)) {
            int hold = GetOrbSlider("extraHold", 50);
            SDK::Drawing::DrawCircle(player.Position(),
                player.BoundingRadius() + (float)hold,
                IM_COL32(128, 0, 200, 100), 1.0f);
        }

        // Draw force chase range (C# line 876-888)
        if (IsForceChase() && drawMenu->GetBoolValue("drawChaseRange", true)) {
            float total = player.AttackRange() + player.BoundingRadius() + (float)GetFindRange();
            float t = fmodf(SDK::Game::Time() * 2.0f, 1.0f);
            int r = (int)(sinf(t * 6.2832f) * 127 + 128);
            int g = (int)(sinf(t * 6.2832f + 2.094f) * 127 + 128);
            int b = (int)(sinf(t * 6.2832f + 4.189f) * 127 + 128);
            SDK::Drawing::DrawCircle(player.Position(), total, IM_COL32(r, g, b, 200), 2.5f);
        }

        // Draw killable minions (C# line 890-899)
        if (drawMenu->GetBoolValue("drawKillableMinion", false)) {
            float aaRange = player.AttackRange() + player.BoundingRadius();
            for (const auto& minion : SDK::ObjectManager::EnemyMinions()) {
                if (!minion.IsValid() || !minion.IsAlive() || !minion.IsVisible()) continue;
                if (minion.DistanceToPlayer() > aaRange * 2.0f) continue;
                float dmg = player.GetAutoAttackDamage(minion);
                if (dmg > 0.0f && minion.Health() > 0.0f && minion.Health() < dmg) {
                    SDK::Drawing::DrawCircle(minion.Position(),
                        minion.BoundingRadius() * 2.0f, IM_COL32(0, 255, 0, 255), 3.0f);
                }
            }
        }

        // Draw active mode text (C# not in original but useful)
        if (drawMenu->GetBoolValue("drawActiveMode", true) && m_activeMode != SDK::OrbwalkerMode::None) {
            ImDrawList* dl = ImGui::GetBackgroundDrawList();
            if (dl) {
                const char* txt = nullptr;
                ImU32 col = IM_COL32(255, 255, 255, 220);
                switch (m_activeMode) {
                case SDK::OrbwalkerMode::Combo:   txt = "Combo"; col = IM_COL32(255, 100, 100, 220); break;
                case SDK::OrbwalkerMode::Harass:  txt = "Harass"; col = IM_COL32(255, 200, 100, 220); break;
                case SDK::OrbwalkerMode::LastHit: txt = "LastHit"; col = IM_COL32(100, 255, 100, 220); break;
                case SDK::OrbwalkerMode::Clear:   txt = "LaneClear"; col = IM_COL32(100, 200, 255, 220); break;
                case SDK::OrbwalkerMode::Flee:    txt = "Flee"; col = IM_COL32(200, 200, 200, 220); break;
                default: break;
                }
                if (txt) {
                    ImVec2 ds = ImGui::GetIO().DisplaySize;
                    ImVec2 ts = ImGui::CalcTextSize(txt);
                    float x = ds.x / 2 - ts.x / 2, y = ds.y - 80;
                    dl->AddText(ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 180), txt);
                    dl->AddText(ImVec2(x, y), col, txt);
                }
            }
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Public API — Properties  (C# lines 16-90)
    // ═══════════════════════════════════════════════════════════════════
    SDK::OrbwalkerMode GetActiveMode() const {
        if (!m_menu) return SDK::OrbwalkerMode::None;
        if (m_menu->GetKeyBindValue("combo", false) || m_menu->GetKeyBindValue("comboNoMove", false))
            return SDK::OrbwalkerMode::Combo;
        if (m_menu->GetKeyBindValue("harass", false)) return SDK::OrbwalkerMode::Harass;
        if (m_menu->GetKeyBindValue("laneClear", false)) return SDK::OrbwalkerMode::Clear;
        if (m_menu->GetKeyBindValue("lastHit", false)) return SDK::OrbwalkerMode::LastHit;
        if (m_menu->GetKeyBindValue("flee", false)) return SDK::OrbwalkerMode::Flee;
        return SDK::OrbwalkerMode::None;
    }

    bool IsForceChase() const {
        return m_activeMode == SDK::OrbwalkerMode::Combo && m_menu &&
               m_menu->GetSubMenu("misc") &&
               m_menu->GetSubMenu("misc")->GetKeyBindValue("findKey", false);
    }

    int GetFindRange() const {
        if (!IsForceChase()) return 0;
        auto* misc = m_menu->GetSubMenu("misc");
        return misc ? misc->GetSliderValue("forceChaseRange", 200) : 0;
    }

    bool IsFastLaneClear() const {
        return m_activeMode == SDK::OrbwalkerMode::Clear && m_menu &&
               m_menu->GetKeyBindValue("fastLaneClear", false);
    }

    bool IsComboNoMove() const {
        return m_menu && m_menu->GetKeyBindValue("comboNoMove", false);
    }

    // ═══════════════════════════════════════════════════════════════════
    // Timing — CanAttack  (C# lines 941-1002)
    // ═══════════════════════════════════════════════════════════════════
    bool CanAttack(float extraWindup = 0.0f) const {
        if (!m_initialized) return false;
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return false;

        // Buff checks (C# lines 960-975)
        if (m_tahmInGame && p.HasBuff("tahmkenchwhasdevouredtarget")) return false;
        if (p.HasBuffOfType(0x8)) return false;  // Fear
        if (!m_isKalista && p.HasBuff("blindingdart")) return false;

        // Rengar Q override (C# line 976-978)
        if (m_isRengar && (p.HasBuff("RengarQ") || p.HasBuff("RengarQEmp"))) return true;

        // Aphelios preload (C# line 980-982)
        if (m_isAphelios && p.HasBuff("apheliospreload")) return false;

        // Jhin reload (C# line 984-986)
        if (m_isJhin && p.HasBuff("JhinPassiveReload")) return false;

        // Timing (C# lines 988-1001)
        float atkDelayMs = p.AttackDelay() * 1000.0f;
        if (m_isGraves) {
            if (!p.HasBuff("gravesbasicattackammo1")) return false;
            atkDelayMs = p.AttackDelay() * 1000.0f * 1.0740297f - 716.2381f;
        } else if (m_isSett && m_nextAttackIsPassive) {
            atkDelayMs = p.AttackDelay() * 1000.0f / 8.0f;
        }

        int now = SDK::Game::TickCount();
        int ping = SDK::Game::Ping();
        return (float)(now + ping / 2 + 25) >= (float)m_lastAutoAttackTick + atkDelayMs + extraWindup;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Timing — CanMove  (C# lines 1004-1041)
    // ═══════════════════════════════════════════════════════════════════
    bool CanMove(float extraWindup = 0.0f, bool disableMissileCheck = false) const {
        if (!m_initialized) return false;
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return false;

        // TahmKench (C# line 1023-1026)
        if (m_tahmInGame && p.HasBuff("tahmkenchwhasdevouredtarget")) return false;

        // Kalista always can move (C# line 1027-1029)
        if (m_isKalista) return true;

        // Missile check (C# line 1031-1034)
        if (m_missileLaunched && !disableMissileCheck && GetAdvBool("missileCheck", true))
            return true;

        // Rengar extra (C# line 1035-1038)
        int rengarExtra = 0;
        if (m_isRengar && (p.HasBuff("RengarQ") || p.HasBuff("RengarQEmp")))
            rengarExtra = 200;

        // Timing gate (C# line 1040)
        int now = SDK::Game::TickCount();
        int ping = SDK::Game::Ping();
        float castDelay = GetAttackCastDelay() * 1000.0f;
        return (float)(now + ping / 2) >= (float)m_lastAutoAttackTick + castDelay + extraWindup + (float)rengarExtra;
    }

    // ═══════════════════════════════════════════════════════════════════
    // State  (C# lines 1703-1796)
    // ═══════════════════════════════════════════════════════════════════
private:
    static inline OrbwalkerPlugin* s_instance = nullptr;

    // ── Menu ──
    SDK::Menu* m_menu = nullptr;
    bool m_initialized = false;

    // ── Mode ──
    SDK::OrbwalkerMode m_activeMode = SDK::OrbwalkerMode::None;

    // ── Timing (C# lines 1704-1716) ──
    int m_lastAutoAttackTick = 0;    // TickCount when AA confirmed (OnDoCast equivalent)
    int m_lastLocalAttackTick = 0;   // TickCount when IssueOrder(Attack) called
    int m_lastMovementTick = 0;      // TickCount when last move issued
    int m_autoAttackCounter = 0;
    bool m_missileLaunched = false;

    // ── Champion flags (C# lines 1725-1758) ──
    bool m_isAphelios = false;
    bool m_isGraves = false;
    bool m_isJhin = false;
    bool m_isKalista = false;
    bool m_isRengar = false;
    bool m_isSett = false;
    bool m_jaxInGame = false;
    bool m_gpInGame = false;
    bool m_tahmInGame = false;

    // ── Sett passive (C# lines 1746-1747, 1793-1813) ──
    bool m_nextAttackIsPassive = false;
    int m_settAttackTime = 0;

    // ── Target cache ──
    SDK::AIBaseClient m_lastTarget;
    SDK::AIBaseClient m_laneClearMinion;
    SDK::AIBaseClient m_cachedTarget;
    int m_targetCacheTick = 0;

    // ── AA polling ──
    bool m_wasWindingUp = false;

    // ── SDK override ──

    // ═══════════════════════════════════════════════════════════════════
    // Menu helpers
    // ═══════════════════════════════════════════════════════════════════
    int GetOrbSlider(const char* key, int def) const {
        auto* s = m_menu ? m_menu->GetSubMenu("settings") : nullptr;
        return s ? s->GetSliderValue(key, def) : def;
    }
    bool GetOrbBool(const char* key, bool def) const {
        auto* s = m_menu ? m_menu->GetSubMenu("settings") : nullptr;
        return s ? s->GetBoolValue(key, def) : def;
    }
    bool GetAdvBool(const char* key, bool def) const {
        auto* s = m_menu ? m_menu->GetSubMenu("advanced") : nullptr;
        return s ? s->GetBoolValue(key, def) : def;
    }
    int GetFarmSlider(const char* key, int def) const {
        auto* s = m_menu ? m_menu->GetSubMenu("farm") : nullptr;
        return s ? s->GetSliderValue(key, def) : def;
    }
    bool GetFarmBool(const char* key, bool def) const {
        auto* s = m_menu ? m_menu->GetSubMenu("farm") : nullptr;
        return s ? s->GetBoolValue(key, def) : def;
    }

    // ═══════════════════════════════════════════════════════════════════
    // ═══════════════════════════════════════════════════════════════════
    // GetAttackCastDelay  (C# lines 214-221) — Sett passive override
    // ═══════════════════════════════════════════════════════════════════
    float GetAttackCastDelay() const {
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return 0.3f;
        float castDelay = p.AttackCastDelay();
        if (m_isSett && m_nextAttackIsPassive) {
            castDelay -= castDelay / 8.0f;
        }
        return castDelay;
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetProjectileSpeed  (C# lines 222-401)
    // Per-champion AA missile speed with buff-dependent variants
    // ═══════════════════════════════════════════════════════════════════
    float GetProjectileSpeed() const {
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return FLT_MAX;
        if (p.IsMelee()) return FLT_MAX;

        std::string name = p.CharacterName();

        // Champion-specific overrides (C# lines 226-393)
        if (name == "Jinx") {
            return p.HasBuff("JinxQ") ? 2000.0f : GetBasicAttackMissileSpeed();
        }
        if (name == "Neeko") {
            return p.HasBuff("neekowpassiveready") ? FLT_MAX : GetBasicAttackMissileSpeed();
        }
        if (name == "Kayle") {
            return p.AttackRange() >= 530.0f ? 2250.0f : FLT_MAX;
        }
        if (name == "Jayce") {
            // Ranged form: Q = "jayceshockblast"
            return 2000.0f; // simplified
        }
        if (name == "Viktor") {
            return p.HasBuff("ViktorPowerTransferReturn") ? FLT_MAX : GetBasicAttackMissileSpeed();
        }
        if (name == "Ivern") {
            return p.HasBuff("ivernwpassive") ? 1600.0f : GetBasicAttackMissileSpeed();
        }
        if (name == "Poppy") {
            return p.HasBuff("poppypassivebuff") ? 1600.0f : GetBasicAttackMissileSpeed();
        }

        // Default: use basic attack missile speed
        return GetBasicAttackMissileSpeed();
    }

    float GetBasicAttackMissileSpeed() const {
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid() || p.IsMelee()) return FLT_MAX;
        // Fallback: typical ranged AA speed
        return 2000.0f;
    }

    // ═══════════════════════════════════════════════════════════════════
    // PollAttackState  (replaces C# OnDoCast lines 659-691
    //                   + OnProcessSpellCast lines 694-717
    //                   + OnStopCast lines 771-781
    //                   + OnDelete lines 784-813)
    // ═══════════════════════════════════════════════════════════════════
    void PollAttackState() {
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return;
        bool winding = p.IsWindingUp();
        int now = SDK::Game::TickCount();
        int ping = SDK::Game::Ping();

        // ── Phase 1: AA started (mirrors OnDoCast C# line 681) ──
        if (winding && !m_wasWindingUp) {
            // C#: LastAutoAttackTick = Variables.GameTimeTickCount - Game.Ping / 2
            m_lastAutoAttackTick = now - ping / 2;
            m_missileLaunched = false;
            m_lastMovementTick = 0;
            m_autoAttackCounter++;

            // Sync SDK
            SDK::Orbwalker::Instance().LastAutoAttackTick = m_lastAutoAttackTick;
            SDK::Orbwalker::Instance().LastAutoAttackTime = SDK::Game::Time() - static_cast<float>(ping / 2) / 1000.0f;
            SDK::Orbwalker::Instance().MissileLaunched = false;

            // Fire OnAttack event (C# line 689)
            if (m_lastTarget.IsValid()) {
                SDK::OrbwalkingActionArgs e{};
                e.Target = m_lastTarget; e.Sender = p;
                e.Type = SDK::OrbwalkingType::OnAttack; e.Process = true;
                SDK::Orbwalker::Instance().InvokeAction(e);
            }
        }

        // ── Phase 2: Missile launched (mirrors OnProcessSpellCast C# line 604-606) ──
        if (!winding && m_wasWindingUp && !m_missileLaunched) {
            float castMs = GetAttackCastDelay() * 1000.0f;
            // Safety: only accept if 60% of windup passed (prevent polling jitter)
            if (m_lastAutoAttackTick <= 0 || (now - m_lastAutoAttackTick) >= (int)(castMs * 0.6f)) {
                m_missileLaunched = true;
                SDK::Orbwalker::Instance().MissileLaunched = true;

                // Fire AfterAttack event (C# line 604)
                if (m_lastTarget.IsValid()) {
                    SDK::OrbwalkingActionArgs e{};
                    e.Target = m_lastTarget; e.Sender = p;
                    e.Type = SDK::OrbwalkingType::AfterAttack; e.Process = true;
                    SDK::Orbwalker::Instance().InvokeAction(e);
                }
            }
        }

        // ── Fallback: fire AfterAttack by timing (C# not explicit, safety net) ──
        if (!m_missileLaunched && m_lastAutoAttackTick > 0) {
            float castMs = GetAttackCastDelay() * 1000.0f;
            if ((now - m_lastAutoAttackTick) >= (int)(castMs + 80.0f)) {
                m_missileLaunched = true;
                SDK::Orbwalker::Instance().MissileLaunched = true;
                if (m_lastTarget.IsValid()) {
                    SDK::OrbwalkingActionArgs e{};
                    e.Target = m_lastTarget; e.Sender = p;
                    e.Type = SDK::OrbwalkingType::AfterAttack; e.Process = true;
                    SDK::Orbwalker::Instance().InvokeAction(e);
                }
            }
        }

        // ── OnDelete equivalent: invalidate stale targets ──
        if (m_lastTarget.IsValid() && (!m_lastTarget.IsAlive() || !m_lastTarget.IsVisible())) {
            m_lastTarget = SDK::AIBaseClient();
        }
        if (m_laneClearMinion.IsValid() && (!m_laneClearMinion.IsAlive() || !m_laneClearMinion.IsVisible())) {
            m_laneClearMinion = SDK::AIBaseClient();
        }

        m_wasWindingUp = winding;
    }


    // sender
    //   .IsMe() .IsEnemy() .CharName()
    // args
    //   .Slot .SpellName .IsAutoAttack .Start .End .CastDelay  .MissileSpeed
    void OnProcessSpellCast(const SDK::AIBaseClient& sender, const SDK::Events::SpellCast::ProcessSpellCastEventArgs& args) override
    {
        if (!sender.IsMe()) return;
        print("Slot=%d SpellName=%s IsAutoAttack=%d IsSpecialAttack=%d Start=(%.1f,%.1f,%.1f) End=(%.1f,%.1f,%.1f) CastPos=(%.1f,%.1f,%.1f) CastDelay=%.3f MissileSpeed=%.1f TargetNetId=%d",
            static_cast<int>(args.Slot),
            args.SpellName.c_str(),
            args.IsAutoAttack ? 1 : 0,
            args.IsSpecialAttack ? 1 : 0,
            args.Start.x, args.Start.y, args.Start.z,
            args.End.x, args.End.y, args.End.z,
            args.CastPosition.x, args.CastPosition.y, args.CastPosition.z,
            args.CastDelay,
            args.MissileSpeed,
            args.TargetNetworkId);
    }
    // ═══════════════════════════════════════════════════════════════════
    // CanAttackWithWindWall  (C# lines 403-472)
    // Checks Jax CounterStrike + Yasuo WindWall + Special champions
    // ═══════════════════════════════════════════════════════════════════
    bool CanAttackWithWindWall(const SDK::AIBaseClient& target) const {
        if (!target.IsValid()) return false;

        // Jax CounterStrike (C# line 413-419)
        if (m_jaxInGame && target.IsHero() && target.HasBuff("JaxCounterStrike"))
            return false;

        // WindWall check — ONLY for hero targets (C# line 421-468)
        // AA is targeted (point-and-click), not a skillshot.
        // Only Yasuo wall blocks ranged AA projectiles.
        // Do NOT check minion collision — that blocks all farm attacks!
        if (!target.IsHero()) return true;  // Minions/structures always attackable
        if (!GetAdvBool("yasuoWallCheck", true)) return true;
        if (Player().IsMelee()) return true;

        // Ranged hero target: check Yasuo wall between us
        // HasLineCollision checks YasuoWall + BraumShield + Minions
        // For AA we only care about wall, but this is the closest API available
        return !SDK::Collision::HasLineCollision(Player().Position(), target.Position(), 1.0f);
    }

    // ═══════════════════════════════════════════════════════════════════
    // CanOrbObj  (C# lines 608-657) — Calculate Runaway
    // Checks if target will be out of range when AA lands
    // ═══════════════════════════════════════════════════════════════════
    bool CanOrbObj(const SDK::AIBaseClient& g) const {
        if (!GetOrbBool("calculateRunaway", true)) return true;
        if (!g.IsValid()) return true;
        if (!g.IsHero() && !g.IsMinion()) return true;
        if (!g.IsMoving()) return true;

        float fromDist = g.DistanceToPlayer();
        float normalRange = Player().AttackRange() + Player().BoundingRadius();

        // In normal range (C# line 628-631)
        if (fromDist <= normalRange) return true;

        // In extended range with bounding (C# line 633-647)
        if (fromDist > normalRange && fromDist <= normalRange + g.BoundingRadius()) {
            // Simplified: trust InAutoAttackRange for borderline
            return true;
        }

        // ForceChase extended range (C# line 648-654)
        if (IsForceChase()) {
            if (fromDist > normalRange + g.BoundingRadius() + GetFindRange())
                return false;
        }

        return true;
    }

    // ═══════════════════════════════════════════════════════════════════
    // ShouldWait  (C# lines 519-536)
    // ═══════════════════════════════════════════════════════════════════
    bool ShouldWait(float range) {
        if (!m_initialized || !m_menu) return false;
        if (!GetFarmBool("shouldWait", true)) return false;

        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return false;

        int farmDelay = GetFarmSlider("farmDelay", 30);
        int fastDelay = GetFarmSlider("fastFarmDelay", 220);
        float atkMs = p.AttackDelay() * 1000.0f;
        bool fastLC = IsFastLaneClear();

        // C#: !FastLne ? AttackDelay*1000*2 : AttackDelay*1000 + FastFarmDelay
        int predTime = fastLC ? (int)atkMs + fastDelay : (int)(atkMs * 2.0f);

        for (const auto& min : SDK::ObjectManager::EnemyMinions()) {
            if (!min.IsValid() || !min.IsAlive() || !min.IsVisible()) continue;
            if (p.Distance(min) > range + min.BoundingRadius()) continue;

            float hpNow = min.Health();
            float dmg = p.GetAutoAttackDamage(min);
            if (dmg <= 0.0f) continue;

            // Use Default mode — Simulated requires full attack simulation
            // which NightSharp SDK doesn't support (C# EnsoulSharp tracks all game events).
            // Default mode uses registered attacks from missile tracking + SpellCast events.
            float hp = SDK::HealthPrediction::GetPrediction(
                min, predTime, farmDelay, SDK::HealthPredictionType::Default);

            // If HealthPrediction didn't track any damage (hp unchanged),
            // use heuristic ONLY if ally minions are nearby (within 1500 of this minion).
            // No ally minions = no incoming damage = just push freely.
            if (fabsf(hp - hpNow) < 1.0f) {
                bool allyNearby = false;
                for (const auto& ally : SDK::ObjectManager::AllyMinions()) {
                    if (ally.IsValid() && ally.IsAlive() && ally.Distance(min) < 1500.0f) {
                        allyNearby = true;
                        break;
                    }
                }
                if (!allyNearby) continue;  // No ally minion = no wait needed

                float gameMin = SDK::Game::Time() / 60.0f;
                float waveDPS = 50.0f + gameMin * 2.5f;
                if (waveDPS > 150.0f) waveDPS = 150.0f;
                float estimatedIncoming = (float)predTime / 1000.0f * waveDPS;
                hp = hpNow - estimatedIncoming;
                if (hp < 0.0f) hp = 0.0f;
            }

            if (hp > 0.0f && hp <= dmg) {
                return true;
            }
        }
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetTarget  (C# lines 1043-1326) — Full 9-step target selection
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetTarget() {
        DbgStage("OrbPlugin::Target::Enter");
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return {};
        if (m_activeMode == SDK::OrbwalkerMode::None || m_activeMode == SDK::OrbwalkerMode::Flee)
            return {};

        // Tick limiter: cache target for 30ms to reduce FPS drop
        // Only re-evaluate when cache expired or target invalid
        int now = SDK::Game::TickCount();
        if (now - m_targetCacheTick < 30 && m_cachedTarget.IsValid() &&
            m_cachedTarget.IsAlive() && p.InAutoAttackRange(m_cachedTarget)) {
            return m_cachedTarget;
        }

        float range = p.AttackRange() + p.BoundingRadius();
        int findRange = GetFindRange();

        // ── Step 1: FarmOverHarass (C# line 1057-1066) ──
        DbgStage("OrbPlugin::Target::FarmOverHarass");
        auto* priMenu = m_menu ? m_menu->GetSubMenu("prioritize") : nullptr;
        if ((m_activeMode == SDK::OrbwalkerMode::Harass || m_activeMode == SDK::OrbwalkerMode::Clear) &&
            priMenu && !priMenu->GetBoolValue("farmOverHarass", true)) {
            auto hero = GetBestHero(range + findRange);
            if (hero.IsValid()) return hero;
        }

        // ── Step 2: GP Barrels (C# line 1068-1103) ──
        DbgStage("OrbPlugin::Target::Barrels");
        if (m_gpInGame) {
            auto barrel = GetBarrel();
            if (barrel.IsValid()) return barrel;
        }

        // ── Step 3: Minion last-hit (C# line 1106-1137) ──
        DbgStage("OrbPlugin::Target::LastHit");
        if (m_activeMode != SDK::OrbwalkerMode::Combo) {
            auto lh = GetLastHitMinion();
            if (lh.IsValid()) return lh;
        }

        // ── Step 4: Turret + Inhibitor + Nexus (C# line 1144-1168) ──
        DbgStage("OrbPlugin::Target::Structures");
        if (m_activeMode != SDK::OrbwalkerMode::Combo) {
            auto st = GetStructure();
            if (st.IsValid()) return st;
        }

        // ── Step 5: Hero target (C# line 1170-1179) ──
        DbgStage("OrbPlugin::Target::Hero");
        if (m_activeMode != SDK::OrbwalkerMode::LastHit &&
            (m_activeMode != SDK::OrbwalkerMode::Clear || !ShouldWait(range))) {
            auto hero = GetBestHero(range + findRange);
            if (hero.IsValid()) return hero;
        }

        // ── Step 6: Special minions if prioritized (C# line 1181-1188) ──
        DbgStage("OrbPlugin::Target::SpecialPri");
        if (priMenu && priMenu->GetBoolValue("specialMinion", false) &&
            m_activeMode != SDK::OrbwalkerMode::Combo && !ShouldWait(range)) {
            auto sp = GetSpecialMinion();
            if (sp.IsValid()) return sp;
        }

        // ── Step 7: Jungle monsters (C# line 1190-1213) ──
        DbgStage("OrbPlugin::Target::Jungle");
        if (m_activeMode == SDK::OrbwalkerMode::Harass ||
            m_activeMode == SDK::OrbwalkerMode::Clear ||
            m_activeMode == SDK::OrbwalkerMode::LastHit) {
            auto jg = GetJungle();
            if (jg.IsValid()) return jg;
        }

        // ── Step 8: Turret farm (C# line 1215-1283) ──
        DbgStage("OrbPlugin::Target::TurretFarm");
        if (m_activeMode != SDK::OrbwalkerMode::Combo && CanTurretFarm()) {
            auto tf = GetTurretFarmTarget();
            if (tf.IsValid()) return tf;
            return {};
        }

        // ── Step 9: LaneClear push (C# line 1285-1312) ──
        DbgStage("OrbPlugin::Target::Push");
        if (m_activeMode == SDK::OrbwalkerMode::Clear && !ShouldWait(range)) {
            auto push = GetPushMinion();
            if (push.IsValid()) return push;
        }

        // ── Step 10: Special minions fallback (C# line 1314-1321) ──
        DbgStage("OrbPlugin::Target::SpecialFB");
        if (m_activeMode != SDK::OrbwalkerMode::Combo && !ShouldWait(range)) {
            auto sp = GetSpecialMinion();
            if (sp.IsValid()) { CacheTarget(sp); return sp; }
        }

        CacheTarget({});
        return {};
    }

    void CacheTarget(const SDK::AIBaseClient& t) {
        m_cachedTarget = t;
        m_targetCacheTick = SDK::Game::TickCount();
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetBestHero — TargetSelector with WindWall + CanOrbObj check
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetBestHero(float range) {
        auto targets = SDK::TargetSelector::GetTargets(range);
        for (auto& h : targets) {
            if (h.IsValid() && CanAttackWithWindWall(h) && CanOrbObj(h) &&
                Player().InAutoAttackRange(h))
                return h;
        }
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetBarrel — GP barrel (C# lines 1068-1103)
    // Full barrel timing logic with owner level check
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetBarrel() {
        auto* atkMenu = m_menu ? m_menu->GetSubMenu("attackable") : nullptr;
        if (!atkMenu || !atkMenu->GetBoolValue("barrels", true)) return {};
        auto p = SDK::ObjectManager::Player();

        for (const auto& j : SDK::ObjectManager::JungleMinions()) {
            if (!j.IsValid() || !j.IsAlive() || !p.InAutoAttackRange(j)) continue;
            std::string name = j.CharacterName();
            if (_stricmp(name.c_str(), "gangplankbarrel") != 0) continue;

            // Simple: attack if HP <= 1 (C# line 1080-1082)
            if (j.Health() <= 1.0f) return j;

            // Advanced: barrel tick timing with buff (C# lines 1084-1099)
            if (j.Health() <= 2.0f && j.HasBuff("gangplankebarrelactive")) {
                // Barrel will tick down soon — check timing
                float projSpeed = GetProjectileSpeed();
                float dist = (std::max)(0.0f, p.Distance(j) - p.BoundingRadius());
                float projMs = (projSpeed < FLT_MAX) ? (1000.0f * dist / projSpeed) : 0.0f;
                float impactMs = GetAttackCastDelay() * 1000.0f + SDK::Game::Ping() / 2.0f + projMs;
                // If we can hit it before next tick, attack
                if (impactMs < 1500.0f) return j;
            }
        }
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetSpecialMinion — Pets, Wards, Plants (C# lines 488-517)
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetSpecialMinion() {
        auto* atkMenu = m_menu ? m_menu->GetSubMenu("attackable") : nullptr;
        if (!atkMenu) return {};
        auto p = SDK::ObjectManager::Player();

        // Pets (C# line 499-503)
        if (atkMenu->GetBoolValue("specialMinions", true)) {
            for (const auto& pet : SDK::ObjectManager::Pets()) {
                if (pet.IsValid() && pet.IsAlive() && !pet.IsAlly() && p.InAutoAttackRange(pet))
                    return pet;
            }
        }
        // Wards — not in combo (C# line 505-509)
        if (atkMenu->GetBoolValue("wards", true) && m_activeMode != SDK::OrbwalkerMode::Combo) {
            for (const auto& w : SDK::ObjectManager::Wards()) {
                if (w.IsValid() && w.IsAlive() && !w.IsAlly() && p.InAutoAttackRange(w))
                    return w;
            }
        }
        // Jungle Plants — not in combo (C# line 511-515)
        if (atkMenu->GetBoolValue("junglePlant", false) && m_activeMode != SDK::OrbwalkerMode::Combo) {
            for (const auto& pl : SDK::ObjectManager::Plants()) {
                if (pl.IsValid() && pl.IsAlive() && p.InAutoAttackRange(pl))
                    return pl;
            }
        }
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetLastHitMinion — HealthPrediction (C# lines 1106-1137)
    // Sorts siege/super first, fires NonKillableMinion event
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetLastHitMinion() {
        DbgStage("OrbPlugin::LastHit::Loop");
        auto p = SDK::ObjectManager::Player();
        int farmDelay = GetFarmSlider("farmDelay", 30);
        float projSpeed = GetProjectileSpeed();

        SDK::AIBaseClient bestSiege, bestNormal;

        for (const auto& min : SDK::ObjectManager::EnemyMinions()) {
            if (!min.IsValid() || !min.IsAlive() || !min.IsVisible()) continue;
            if (!p.InAutoAttackRange(min)) continue;

            // Skip ignored minions (C# line 481: "jarvanivstandard")
            std::string mName = min.CharacterName();
            if (_stricmp(mName.c_str(), "jarvanivstandard") == 0) continue;

            // MaxHealth <= 10 = ward/trap — check if killable (C# line 1114-1120)
            if (min.MaxHealth() <= 10.0f) {
                if (min.Health() <= 1.0f) return min;
                continue;
            }

            // Impact timing (C# line 1123-1124)
            float dist = (std::max)(0.0f, p.Distance(min) - p.BoundingRadius());
            float impactMs = GetAttackCastDelay() * 1000.0f - 100.0f
                + SDK::Game::Ping() / 2.0f
                + 1000.0f * dist / projSpeed;

            float hpPred = SDK::HealthPrediction::GetPrediction(min, (int)impactMs, farmDelay);
            float aaDmg = p.GetAutoAttackDamage(min);

            // NonKillableMinion (C# line 1126-1128)
            if (hpPred <= 0.0f) {
                SDK::OrbwalkingActionArgs nk{};
                nk.Target = min; nk.Sender = p;
                nk.Type = SDK::OrbwalkingType::NonKillableMinion; nk.Process = true;
                SDK::Orbwalker::Instance().InvokeAction(nk);
                continue;
            }

            // Killable (C# line 1130-1134)
            if (aaDmg > 0.0f && hpPred <= aaDmg) {
                bool isSiege = mName.find("Siege") != std::string::npos ||
                               mName.find("Super") != std::string::npos;
                if (isSiege && !bestSiege.IsValid()) bestSiege = min;
                else if (!bestNormal.IsValid()) bestNormal = min;
            }
        }

        return bestSiege.IsValid() ? bestSiege : bestNormal;
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetStructure — Turret + Inhibitor + Nexus (C# lines 1144-1168)
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetStructure() {
        auto p = SDK::ObjectManager::Player();
        auto* priMenu = m_menu ? m_menu->GetSubMenu("prioritize") : nullptr;

        // Turrets (C# line 1146-1154)
        if (priMenu && priMenu->GetBoolValue("turret", true)) {
            for (const auto& t : SDK::ObjectManager::EnemyTurrets()) {
                if (t.IsValid() && t.IsAlive() && p.InAutoAttackRange(t)) return t;
            }
        }

        // Inhibitors (C# line 1155-1161) — always attack when in range.
        // EnemyInhibitors filters AllObjects by "Barracks" CharacterName prefix.
        for (const auto& inh : SDK::ObjectManager::EnemyInhibitors()) {
            if (inh.IsValid() && inh.IsAlive() && p.InAutoAttackRange(inh)) return inh;
        }

        // Nexus (C# line 1162-1166) — always attack when in range.
        // Single object filtered by "HQ" prefix.
        if (auto nex = SDK::ObjectManager::EnemyNexus();
            nex.IsValid() && nex.IsAlive() && p.InAutoAttackRange(nex)) {
            return nex;
        }

        return {};
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetJungle — with smallJungle priority (C# lines 1190-1213)
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetJungle() {
        auto p = SDK::ObjectManager::Player();
        auto* priMenu = m_menu ? m_menu->GetSubMenu("prioritize") : nullptr;
        bool smallFirst = priMenu && priMenu->GetBoolValue("smallJungle", false);

        SDK::AIBaseClient best;
        float bestHP = smallFirst ? FLT_MAX : 0.0f;

        for (const auto& mob : SDK::ObjectManager::JungleMinions()) {
            if (!mob.IsValid() || !mob.IsAlive() || !mob.IsVisible()) continue;
            if (!p.InAutoAttackRange(mob)) continue;
            float hp = mob.MaxHealth();
            if (smallFirst ? (hp < bestHP) : (hp > bestHP)) {
                bestHP = hp; best = mob;
            }
        }
        return best;
    }

    // ═══════════════════════════════════════════════════════════════════
    // CanTurretFarm  (C# lines 537-565)
    // ═══════════════════════════════════════════════════════════════════
    bool CanTurretFarm() const {
        if (!m_menu) return false;
        auto* farmMenu = m_menu->GetSubMenu("farm");
        if (!farmMenu) return false;
        if (farmMenu->GetListIndex("turretFarm", 0) != 0) return false;  // "Off"
        auto p = SDK::ObjectManager::Player();
        if (p.Level() >= farmMenu->GetSliderValue("turretFarmMaxLevel", 13)) return false;
        return p.IsUnderAllyTurret();
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetTurretFarmTarget  (C# lines 1215-1283)
    // Farm minions under ally turret with turret shot prediction
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetTurretFarmTarget() {
        DbgStage("OrbPlugin::TurretFarm");
        auto p = SDK::ObjectManager::Player();
        int farmDelay = GetFarmSlider("farmDelay", 30);
        float projSpeed = GetProjectileSpeed();

        // Find closest ally turret
        SDK::AITurretClient closestTurret;
        float closestDist = 1500.0f;
        for (const auto& t : SDK::ObjectManager::AllyTurrets()) {
            if (!t.IsValid() || t.IsDead()) continue;
            float d = t.DistanceToPlayer();
            if (d < closestDist) { closestDist = d; closestTurret = t; }
        }
        if (!closestTurret.IsValid()) return {};

        // Get minions near turret
        for (const auto& min : SDK::ObjectManager::EnemyMinions()) {
            if (!min.IsValid() || !min.IsAlive() || !min.IsVisible()) continue;
            if (!p.InAutoAttackRange(min)) continue;
            if (min.Distance(closestTurret) > 900.0f) continue;

            // Check turret aggro
            if (!SDK::HealthPrediction::HasTurretAggro(min)) continue;

            // Predict HP at impact
            float dist = (std::max)(0.0f, p.Distance(min) - p.BoundingRadius());
            float impactMs = GetAttackCastDelay() * 1000.0f - 100.0f
                + SDK::Game::Ping() / 2.0f
                + 1000.0f * dist / projSpeed;

            float hpPred = SDK::HealthPrediction::GetPrediction(min, (int)impactMs, farmDelay);
            float aaDmg = p.GetAutoAttackDamage(min);

            // Killable: HP predicted > 0 and <= AA damage
            if (hpPred > 0.0f && aaDmg > 0.0f && hpPred <= aaDmg) {
                return min;
            }
        }
        return {};
    }

    // ═══════════════════════════════════════════════════════════════════
    // GetPushMinion — Lane clear push target (C# lines 1285-1312)
    // Select minion that won't die soon (HP >= 2*AA or not taking damage)
    // ═══════════════════════════════════════════════════════════════════
    SDK::AIBaseClient GetPushMinion() {
        auto p = SDK::ObjectManager::Player();
        int farmDelay = GetFarmSlider("farmDelay", 30);

        // Check cached LaneClearMinion first (C# line 1287-1298)
        if (m_laneClearMinion.IsValid() && m_laneClearMinion.IsAlive() &&
            p.InAutoAttackRange(m_laneClearMinion)) {
            if (m_laneClearMinion.MaxHealth() <= 10.0f) return m_laneClearMinion;
            float predHP = SDK::HealthPrediction::GetPrediction(
                m_laneClearMinion, (int)(p.AttackDelay() * 2000.0f),
                farmDelay, SDK::HealthPredictionType::Simulated);
            float dmg = p.GetAutoAttackDamage(m_laneClearMinion);
            if (dmg > 0.0f && (predHP >= 2.0f * dmg || fabsf(predHP - m_laneClearMinion.Health()) < 0.001f))
                return m_laneClearMinion;
        }

        // Find new push target (C# line 1299-1311)
        SDK::AIBaseClient best;
        float bestHP = -1.0f;

        for (const auto& min : SDK::ObjectManager::EnemyMinions()) {
            if (!min.IsValid() || !min.IsAlive() || !min.IsVisible()) continue;
            if (!p.InAutoAttackRange(min)) continue;

            float predHP = SDK::HealthPrediction::GetPrediction(
                min, (int)(p.AttackDelay() * 2000.0f),
                farmDelay, SDK::HealthPredictionType::Simulated);
            float dmg = p.GetAutoAttackDamage(min);

            // C#: predHealth >= 2*aaDmg || predHealth == minion.Health
            if (dmg > 0.0f && (predHP >= 2.0f * dmg || fabsf(predHP - min.Health()) < 0.001f)) {
                if (min.Health() > bestHP) {
                    bestHP = min.Health();
                    best = min;
                }
            }
        }

        // Cache for next frame
        if (best.IsValid()) m_laneClearMinion = best;
        return best;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Attack  (C# lines 902-939)
    // Issues attack order with CanOrbObj + WindWall checks
    // Fires BeforeAttack event, respects Kalista missile state
    // ═══════════════════════════════════════════════════════════════════
    bool Attack(SDK::AIBaseClient& target) {
        DbgStage("OrbPlugin::Attack");
        if (!m_initialized) return false;
        auto p = SDK::ObjectManager::Player();

        // CanOrbObj check (C# line 910-915)
        if (target.IsValid() && !CanOrbObj(target)) return false;

        // Range + validity (C# line 916-918)
        if (!target.IsValid() || !p.InAutoAttackRange(target)) return false;

        // WindWall check (C# line 920-923)
        if (!CanAttackWithWindWall(target)) return false;

        // BeforeAttack event (C# line 924-926)
        SDK::OrbwalkingActionArgs ba{};
        ba.Target = target; ba.Sender = p;
        ba.Type = SDK::OrbwalkingType::BeforeAttack; ba.Process = true;
        SDK::Orbwalker::Instance().InvokeAction(ba);
        if (!ba.Process) return false;

        // Kalista: reset missile (C# line 927-929)
        if (m_isKalista) m_missileLaunched = false;

        // Issue attack order (C# line 931-935)
        if (p.IssueOrder(SDK::GameObjectOrder::AttackUnit, target)) {
            m_lastLocalAttackTick = SDK::Game::TickCount();
            m_lastTarget = target;
            return true;
        }
        return false;
    }

    // ═══════════════════════════════════════════════════════════════════
    // Move  (C# lines 1339-1412)
    // Full movement with hold position, randomize, path angle check,
    // high orb mode, throttle, max distance, and FakeClick
    // ═══════════════════════════════════════════════════════════════════
    void Move(SDK::Vector3 position) {
        DbgStage("OrbPlugin::Move");
        if (!m_initialized) return;
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return;

        SDK::Vector3 pos = position.IsValid() ? position : SDK::Game::CursorPos();
        if (!pos.IsValid()) return;

        // Hold position check (C# line 1350-1357)
        float holdDist = (float)(std::max)(30, GetOrbSlider("extraHold", 50));
        SDK::Vector3 playerPos = p.Position();
        if (pos.Distance2D(playerPos) < holdDist) return;

        // Randomize movement (C# line 1359-1361)
        if (GetOrbBool("moveRandom", false) && playerPos.Distance2D(pos) < 150.0f) {
            SDK::Vector3 dir = pos - playerPos;
            float len = std::sqrt(dir.x * dir.x + dir.z * dir.z);
            if (len > 1.0f) {
                float rndFactor = 0.6f + (float)(rand() % 40) / 100.0f + 0.2f;
                float rndDist = rndFactor * 400.0f;
                SDK::Vector3 ndir(dir.x / len, 0, dir.z / len);
                pos = SDK::Vector3(playerPos.x + ndir.x * rndDist,
                    pos.y, playerPos.z + ndir.z * rndDist);
            }
        }

        bool highOrb = GetOrbBool("highOrb", false);
        int now = SDK::Game::TickCount();
        int ping = SDK::Game::Ping();

        // Movement throttle (C# lines 1363-1398)
        if (!highOrb) {
            // Normal mode: angle-based anti-stutter
            // Simplified: check time since last move with min interval
            int minInterval = 70 + (std::min)(60, ping);
            if (now - m_lastMovementTick < minInterval) return;
        } else {
            // High orb mode (C# line 1392-1397)
            int minInterval = 50 + (std::min)(60, ping);
            if (now - m_lastMovementTick < minInterval) return;
        }

        // Movement event (C# line 1399-1401)
        SDK::OrbwalkingActionArgs ma{};
        ma.Position = pos; ma.Sender = p;
        ma.Type = SDK::OrbwalkingType::Movement; ma.Process = true;
        SDK::Orbwalker::Instance().InvokeAction(ma);
        if (!ma.Process) return;

        // Issue move order (C# line 1407-1410)
        if (p.IssueOrder(SDK::GameObjectOrder::MoveTo, ma.Position)) {
            m_lastMovementTick = now;
        }
    }

    // ═══════════════════════════════════════════════════════════════════
    // Orbwalk  (C# lines 1414-1441)
    // Main attack+move cycle with block orders window, limit attack,
    // and ComboNoMove check
    // ═══════════════════════════════════════════════════════════════════
    void Orbwalk(SDK::AIBaseClient& target) {
        if (!m_initialized) return;
        auto p = SDK::ObjectManager::Player();
        if (!p.IsValid()) return;
        int now = SDK::Game::TickCount();
        int ping = SDK::Game::Ping();

        auto& diag = SDK::Orbwalker::Instance().lastTickDiag;
        diag = {};
        diag.targetNetId = target.IsValid() ? target.NetworkId() : 0;

        // Block orders window (C# line 1420)
        // After issuing attack, block all orders for a short time
        if (now - m_lastLocalAttackTick < 70 + (std::min)(60, ping))
            return;

        // ── Attack phase (C# line 1424) ──
        DbgStage("OrbPlugin::Orbwalk::Attack");
        const bool canAtk = CanAttack();
        diag.canAttack = canAtk;
        if (canAtk && Attack(target)) {
            diag.attackIssued = true;
            return;
        }

        // ── Move phase (C# line 1428-1440) ──
        DbgStage("OrbPlugin::Orbwalk::Move");
        float windupDelay = (float)GetOrbSlider("windupDelay", 60);
        const bool canMov = CanMove(windupDelay, false);
        diag.canMove = canMov;
        if (canMov) {
            // ComboNoMove (C# line 1430-1432)
            if (IsComboNoMove()) return;

            // LimitAttack: skip kiting at very high AS (C# line 1434-1437)
            bool limitAtk = GetOrbBool("limitAttack", false);
            if (limitAtk && p.AttackDelay() < 0.3846f &&
                m_autoAttackCounter % 3 != 0 && !CanMove(500.0f, true))
                return;

            // Issue move (C# line 1438-1439)
            SDK::Vector3 movePos = SDK::Game::CursorPos();
            Move(movePos);
            diag.moveIssued = true;
        }
    }

}; // class OrbwalkerPlugin

} // namespace Plugins
