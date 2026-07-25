#pragma once

// ============================================================================
// ZDEvade.h — ZDEvade: skillshot detection + drawing only (evade logic stripped)
//
// Architecture: Detection -> SpellDetector. Drawing -> OnRender.
// Evade decision/action removed — rebuild from scratch.
// ============================================================================

#include "../IPlugin.h"
#include "../PluginRegistry.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include "ZDEvadeActivationPolicy.h"
#include "Debug/CandidateDebug.h"
#include "Debug/SelfSkillDebug.h"
#include "Detection/ThreatDetector.h"
#include "Evade/EvadeController.h"
#include "Database/SpellData.h"
#include "Database/SpellDatabase.h"
#include "Visual/TargetVisualDispatch.h"
#include "Visual/ThreatRenderer.h"

#include <Windows.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace Plugins {

class ZDEvadePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "ZDEvade"; }
    const char* GetInternalId() const override { return "core.zdevade"; }
    const char* GetAuthor() const override { return "ZD"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override {
        return ZDEvade::CanActivateZDEvade(ReadOtherEvadeState());
    }

    void OnLoad() override {
        s_instance = this;

        ZDEvade::ThreatDetector::Initialize();
        CreateMenu();
        RefreshOtherEvadeSuspension();

        const auto player = SDK::ObjectManager::Player();
        m_selfSkillDebug.Configure(
            player.IsValid()
                ? static_cast<std::uint32_t>(player.NetworkId())
                : 0u,
            player.IsValid()
                ? player.CharacterName().c_str()
                : "");
        SDK::Events::AddOnGameUpdate(&ZDEvadePlugin::OnGameUpdateStatic);
        SDK::Events::AddOnProcessSpell(&ZDEvadePlugin::OnProcessSpellStatic);
        SDK::Events::AddOnMissileCreate(&ZDEvadePlugin::OnMissileCreateStatic);
        SDK::Events::AddOnMissileDelete(&ZDEvadePlugin::OnMissileDeleteStatic);
        SDK::Game::AddOnWndProc(&ZDEvadePlugin::OnWndProcStatic);
        SDK::Orbwalker::OnBeforeMove += &ZDEvadePlugin::OnBeforeMoveStatic;
    }

    void OnUnload() override {
        SDK::Events::RemoveOnMissileDelete(&ZDEvadePlugin::OnMissileDeleteStatic);
        SDK::Events::RemoveOnMissileCreate(&ZDEvadePlugin::OnMissileCreateStatic);
        SDK::Events::RemoveOnProcessSpell(&ZDEvadePlugin::OnProcessSpellStatic);
        SDK::Events::RemoveOnGameUpdate(&ZDEvadePlugin::OnGameUpdateStatic);
        SDK::Orbwalker::OnBeforeMove -= &ZDEvadePlugin::OnBeforeMoveStatic;
        SDK::Game::RemoveOnWndProc(&ZDEvadePlugin::OnWndProcStatic);
        if (!RefreshOtherEvadeSuspension()) {
            m_controller.Reset();
        }
        m_suspended = false;
        m_suspendReason = ZDEvade::OtherEvadeReason::None;
        ZDEvade::ThreatDetector::Shutdown();

        DestroyMenu();

        if (s_instance == this) {
            s_instance = nullptr;
        }
    }

    void OnRender() override {
        if (!ImGui::GetCurrentContext()) return;

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;
        const float planeY = player.Position().y;
        const int now = SDK::Variables::TickCount();
        if (Enabled()) {
            const auto threats = ZDEvade::ThreatDetector::Snapshot();
            const RenderState renderState = GetRenderState();

            if (DrawSpells()) {
                for (const auto& threat : threats) {
                    ZDEvade::ThreatRenderer::Draw(threat, now, planeY);
                }
            }

            if (DrawCandidates()) {
                const int count = std::min(140, static_cast<int>(renderState.candidates.size()));
                for (int index = 0; index < count; ++index) {
                    const auto& candidate = renderState.candidates[static_cast<std::size_t>(index)];
                    const std::uint32_t color = candidate.strictSafe
                        ? 0xFF22DD55
                        : candidate.walkable ? 0xFFFFAA22 : 0xFF777777;
                    SDK::Drawing::DrawCircle(Vec3::From2D(candidate.position, planeY), 18.0f, color, 1.5f, 24);
                }
            }

            if (renderState.locked.valid) {
                const ZDEvade::LockedTargetVisualDispatch targetVisual =
                    ZDEvade::GetLockedTargetVisualDispatch(
                        renderState.locked.strictSafe,
                        player.BoundingRadius());
                // The locked marker is the hero's target footprint, not skill padding.
                SDK::Drawing::DrawCircle(
                    Vec3::From2D(renderState.locked.position, planeY),
                    targetVisual.footprintRadius,
                    targetVisual.color,
                    3.0f);
                SDK::Drawing::DrawLine(
                    player.ServerPosition(),
                    Vec3::From2D(renderState.locked.position, planeY),
                    targetVisual.color,
                    2.0f);
            }
        }

        const ZDEvade::SelfSkillDebugVisibility debugVisibility =
            SelfDebugVisibility();
        if (debugVisibility.masterEnabled) {
            for (const auto& snapshot : m_selfSkillDebug.Snapshot()) {
                if (!ZDEvade::ShouldDrawSelfSkillPhase(
                        debugVisibility,
                        snapshot.phase)) {
                    continue;
                }
                ZDEvade::SelfSkillDebug::Draw(
                    snapshot,
                    now,
                    planeY,
                    DrawOwnLabels(),
                    DrawOwnEndpoints());
            }
        }
    }

    void OnMenu() override {
        if (!m_menu) return;
        m_menu->DrawImGui();
        if (!DebugMode()) return;

        const ZDEvade::SelfSkillDebug::Diagnostics debug =
            m_selfSkillDebug.ReadDiagnostics();
        const RenderState renderState = GetRenderState();
        ImGui::Separator();
        ImGui::TextColored(
            ImVec4(0.0f, 0.9f, 1.0f, 1.0f),
            "SELF SKILLS ARE DRAW ONLY - NEVER FED TO EVADE");
        ImGui::Text(
            "Local champion: %s  DB entries: %d",
            debug.champion[0] ? debug.champion : "?",
            static_cast<int>(debug.databaseEntries));
        ImGui::Text(
            "Self debug: pending=%d live=%d terminal=%d",
            static_cast<int>(debug.pending),
            static_cast<int>(debug.live),
            static_cast<int>(debug.terminal));
        if (ShowDebugCounters()) {
            ImGui::Text(
                "Process: seen=%llu matched=%llu rejected=%llu unmatched=%llu",
                static_cast<unsigned long long>(debug.counters.processSeen),
                static_cast<unsigned long long>(debug.counters.processMatched),
                static_cast<unsigned long long>(debug.counters.processRejected),
                static_cast<unsigned long long>(debug.counters.processUnmatched));
            ImGui::Text(
                "Missile create: matched=%llu orphan=%llu duplicate=%llu rejected=%llu unmatched=%llu",
                static_cast<unsigned long long>(debug.counters.missileCreateMatched),
                static_cast<unsigned long long>(debug.counters.missileCreateOrphan),
                static_cast<unsigned long long>(debug.counters.missileCreateDuplicate),
                static_cast<unsigned long long>(debug.counters.missileCreateRejected),
                static_cast<unsigned long long>(debug.counters.missileCreateUnmatched));
            ImGui::Text(
                "Missile delete: matched=%llu unmatched=%llu",
                static_cast<unsigned long long>(debug.counters.missileDeleteMatched),
                static_cast<unsigned long long>(debug.counters.missileDeleteUnmatched));
            ImGui::Text(
                "Timeouts=%llu  capacity drops=%llu",
                static_cast<unsigned long long>(debug.counters.timeouts),
                static_cast<unsigned long long>(debug.counters.capacityDrops));
            ImGui::Text(
                "Last process matched='%s' unmatched='%s'",
                debug.lastMatchedProcess[0] ? debug.lastMatchedProcess : "-",
                debug.lastUnmatchedProcess[0] ? debug.lastUnmatchedProcess : "-");
            ImGui::Text(
                "Last missile matched='%s' unmatched='%s'",
                debug.lastMatchedMissile[0] ? debug.lastMatchedMissile : "-",
                debug.lastUnmatchedMissile[0] ? debug.lastUnmatchedMissile : "-");
        }
        ImGui::Text("Tracked threats: %d", static_cast<int>(ZDEvade::ThreatDetector::Snapshot().size()));
        ImGui::Text("Detector serial: %d", ZDEvade::ThreatDetector::ChangeSerial());
        ImGui::Text("Detector dropped: %d", ZDEvade::ThreatDetector::DroppedRawEvents());
        ImGui::Text("Unsupported Arc dropped: %d",
                    ZDEvade::ThreatDetector::UnsupportedArcDropped());
        ImGui::Text("Active Arc DB: %d",
                    ZDEvade::SpellDatabase::SupportedArcSpellCount());
        if (m_suspended) {
            ImGui::Text(
                "State: Suspended (%s)",
                ZDEvade::OtherEvadeReasonName(m_suspendReason));
        } else {
            ImGui::Text("State: %s", ZDEvade::ControllerStateName(renderState.state));
        }
        if (renderState.locked.valid) {
            ImGui::Text("Target: %s %s", ZDEvade::CandidateSourceName(renderState.locked.source),
                        renderState.locked.strictSafe ? "strict" : "fallback");
            ImGui::Text("Exit: %.1f  Travel: %.1f  Margin: %.1fms",
                        renderState.locked.exitDistance,
                        renderState.locked.travelDistance,
                        renderState.locked.timeMarginMs);
            ImGui::Text("Clearance: %.1f  Exposure: %.1fms",
                        renderState.locked.minimumClearance,
                        renderState.locked.dangerExposureMs);
        }
    }

private:
    struct RenderState {
        ZDEvade::EvadeControllerState state = ZDEvade::EvadeControllerState::Idle;
        ZDEvade::CandidateEvaluation locked;
        std::vector<ZDEvade::CandidateEvaluation> candidates;
    };

    struct SpellMenuBinding {
        MenuBool* enabled = nullptr;
        MenuSlider* danger = nullptr;
        MenuSlider* health = nullptr;
    };

    struct EvadeSpellMenuBinding {
        MenuBool* enabled = nullptr;
        MenuSlider* danger = nullptr;
        MenuBool* wardJump = nullptr;
    };

    static inline ZDEvadePlugin* s_instance = nullptr;

    Menu* m_menu = nullptr;
    Menu* m_spellsMenu = nullptr;
    Menu* m_evadeSpellOptionsMenu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuBool* m_walkEnabledMenu = nullptr;
    MenuBool* m_evadeSpellsMenu = nullptr;
    MenuBool* m_fallbackMenu = nullptr;
    MenuBool* m_drawSpellsMenu = nullptr;
    MenuBool* m_drawCandidatesMenu = nullptr;
    MenuBool* m_debugModeMenu = nullptr;
    MenuBool* m_drawOwnPendingMenu = nullptr;
    MenuBool* m_drawOwnLiveMenu = nullptr;
    MenuBool* m_drawOwnLabelsMenu = nullptr;
    MenuBool* m_drawOwnEndpointsMenu = nullptr;
    MenuBool* m_showDebugCountersMenu = nullptr;
    MenuSlider* m_terminalHoldMenu = nullptr;
    MenuSlider* m_minDangerMenu = nullptr;
    MenuSlider* m_evadeSpellDangerMenu = nullptr;
    MenuSlider* m_evadeSpellMarginMenu = nullptr;
    MenuSlider* m_endpointBufferMenu = nullptr;
    MenuSlider* m_pathBufferMenu = nullptr;
    MenuSlider* m_releaseBufferMenu = nullptr;
    MenuSlider* m_inputDelayMenu = nullptr;
    MenuSlider* m_minMarginMenu = nullptr;
    MenuSlider* m_preferredClearanceMenu = nullptr;
    MenuSlider* m_searchRadiusMenu = nullptr;
    MenuSlider* m_moveIntervalMenu = nullptr;
    MenuSlider* m_moveRefreshMenu = nullptr;
    MenuSlider* m_replanIntervalMenu = nullptr;

    ZDEvade::EvadeController m_controller;
    ZDEvade::SelfSkillDebug m_selfSkillDebug;
    std::unordered_map<std::string, SpellMenuBinding> m_spellBindings;
    std::unordered_map<std::string, EvadeSpellMenuBinding> m_evadeSpellBindings;
    ZDEvade::ThreatRuleMap m_threatRules;
    EvadeSpellRuleMap m_evadeSpellRules;
    mutable SRWLOCK m_renderStateLock = SRWLOCK_INIT;
    RenderState m_renderState;
    bool m_suspended = false;
    ZDEvade::OtherEvadeReason m_suspendReason =
        ZDEvade::OtherEvadeReason::None;

    bool Enabled() const { return !m_enabledMenu || m_enabledMenu->Value; }
    bool DrawSpells() const { return !m_drawSpellsMenu || m_drawSpellsMenu->Value; }
    bool DrawCandidates() const { return m_drawCandidatesMenu && m_drawCandidatesMenu->Value; }
    bool DebugMode() const { return m_debugModeMenu && m_debugModeMenu->Value; }
    bool DrawOwnLabels() const { return !m_drawOwnLabelsMenu || m_drawOwnLabelsMenu->Value; }
    bool DrawOwnEndpoints() const { return !m_drawOwnEndpointsMenu || m_drawOwnEndpointsMenu->Value; }
    bool ShowDebugCounters() const {
        return !m_showDebugCountersMenu || m_showDebugCountersMenu->Value;
    }
    int TerminalHoldMs() const {
        return m_terminalHoldMenu ? m_terminalHoldMenu->Value : 250;
    }
    ZDEvade::SelfSkillDebugVisibility SelfDebugVisibility() const {
        return {
            DebugMode(),
            !m_drawOwnPendingMenu || m_drawOwnPendingMenu->Value,
            !m_drawOwnLiveMenu || m_drawOwnLiveMenu->Value,
        };
    }

    static void OnGameUpdateStatic(const SDK::Events::GameUpdateEventArgs&) {
        if (s_instance) s_instance->Tick();
    }

    static void OnProcessSpellStatic(
            const SDK::Events::ProcessSpellEventArgs& args) {
        if (!s_instance || !s_instance->DebugMode()) return;
        s_instance->m_selfSkillDebug.OnProcessSpell(
            args,
            SDK::Variables::TickCount());
    }

    static void OnMissileCreateStatic(
            const SDK::Events::ObjectEventArgs& args) {
        if (!s_instance || !s_instance->DebugMode()) return;
        s_instance->m_selfSkillDebug.OnMissileCreate(
            args,
            SDK::Variables::TickCount());
    }

    static void OnMissileDeleteStatic(
            const SDK::Events::ObjectEventArgs& args) {
        if (!s_instance || !s_instance->DebugMode()) return;
        s_instance->m_selfSkillDebug.OnMissileDelete(
            args,
            SDK::Variables::TickCount(),
            s_instance->TerminalHoldMs());
    }

    static void OnBeforeMoveStatic(SDK::OrbwalkingActionArgs& args) {
        if (!s_instance || s_instance->RefreshOtherEvadeSuspension() ||
            !args.Process) {
            return;
        }
        if (s_instance->m_controller.HandleMoveRequest(
                args.Position.To2D(),
                ZDEvade::MoveIntentSource::Orbwalker,
                s_instance->BuildConfig())) args.Process = false;
    }

    static void OnWndProcStatic(SDK::Game::WndEventArgs& args) {
        if (!s_instance || s_instance->RefreshOtherEvadeSuspension() ||
            !args.Process ||
            (args.Msg != WM_RBUTTONDOWN &&
             args.Msg != WM_RBUTTONDBLCLK) ||
            !s_instance->Enabled()) {
            return;
        }
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) return;
        if (s_instance->m_controller.HandleMoveRequest(
                SDK::Game::CursorPos().To2D(),
                ZDEvade::MoveIntentSource::Manual,
                s_instance->BuildConfig())) {
            args.Process = false;
        }
    }

    ZDEvade::EvadeRuntimeConfig BuildConfig() const {
        ZDEvade::EvadeRuntimeConfig config;
        config.enabled = Enabled();
        config.walkingEnabled = !m_walkEnabledMenu || m_walkEnabledMenu->Value;
        config.evadeSpellsEnabled = !m_evadeSpellsMenu || m_evadeSpellsMenu->Value;
        config.leastDangerFallback = !m_fallbackMenu || m_fallbackMenu->Value;
        config.minimumDanger = m_minDangerMenu ? m_minDangerMenu->Value : 1;
        config.evadeSpellMinimumDanger = m_evadeSpellDangerMenu ? m_evadeSpellDangerMenu->Value : 3;
        config.evadeSpellMarginThresholdMs = static_cast<float>(
            m_evadeSpellMarginMenu ? m_evadeSpellMarginMenu->Value : 45);
        config.threatRules = &m_threatRules;
        config.evadeSpellRules = &m_evadeSpellRules;
        config.moveIntervalMs = m_moveIntervalMenu ? m_moveIntervalMenu->Value : 75;
        config.moveRefreshMs = m_moveRefreshMenu ? m_moveRefreshMenu->Value : 260;
        config.replanIntervalMs = m_replanIntervalMenu ? m_replanIntervalMenu->Value : 70;
        config.fallbackReplanIntervalMs = std::max(25, config.replanIntervalMs / 2);
        config.planner.endpointBuffer = static_cast<float>(
            m_endpointBufferMenu
                ? m_endpointBufferMenu->Value
                : ZDEvade::kEndpointMarginMenuDefault);
        config.planner.pathBuffer = static_cast<float>(m_pathBufferMenu ? m_pathBufferMenu->Value : 8);
        config.planner.releaseBuffer = static_cast<float>(m_releaseBufferMenu ? m_releaseBufferMenu->Value : 48);
        config.planner.inputDelayMs = static_cast<float>(m_inputDelayMenu ? m_inputDelayMenu->Value : 55) +
            static_cast<float>(std::max(0, SDK::Game::Ping())) * 0.5f;
        config.planner.minimumTimeMarginMs = static_cast<float>(m_minMarginMenu ? m_minMarginMenu->Value : 25);
        config.planner.preferredClearance = static_cast<float>(
            m_preferredClearanceMenu ? m_preferredClearanceMenu->Value : 18);
        config.planner.maxSearchRadius = static_cast<float>(m_searchRadiusMenu ? m_searchRadiusMenu->Value : 760);
        return config;
    }

    void Tick() {
        if (DebugMode())
            m_selfSkillDebug.OnGameUpdate(
                SDK::Variables::TickCount());
        if (RefreshOtherEvadeSuspension(true)) return;

        RefreshThreatRules();
        RefreshEvadeSpellRules();
        m_controller.Update(BuildConfig());
        RenderState next;
        next.state = m_controller.State();
        next.locked = m_controller.Locked();
        if (DrawCandidates()) next.candidates = m_controller.LastPlan().candidates;
        AcquireSRWLockExclusive(&m_renderStateLock);
        m_renderState = std::move(next);
        ReleaseSRWLockExclusive(&m_renderStateLock);
    }

    ZDEvade::OtherEvadeState ReadOtherEvadeState() const {
        return {
            PluginRegistry::IsLoaded("core.kuroevade", false),
            PluginRegistry::IsLoaded("core.ezevade", false),
        };
    }

    bool RefreshOtherEvadeSuspension(bool allowRelease = false) {
        const ZDEvade::OtherEvadeDecision decision =
            ZDEvade::DecideOtherEvadeState(
                ReadOtherEvadeState(),
                m_suspended,
                allowRelease);
        if (decision.suspended &&
            decision.reason != ZDEvade::OtherEvadeReason::None) {
            m_suspendReason = decision.reason;
        }
        if (decision.suspendNow) {
            m_controller.ResetForExternalOwner();
            AcquireSRWLockExclusive(&m_renderStateLock);
            m_renderState = {};
            ReleaseSRWLockExclusive(&m_renderStateLock);
        } else if (decision.releaseNow) {
            m_suspendReason = ZDEvade::OtherEvadeReason::None;
        }
        m_suspended = decision.suspended;
        return m_suspended;
    }

    RenderState GetRenderState() const {
        AcquireSRWLockShared(&m_renderStateLock);
        RenderState result = m_renderState;
        ReleaseSRWLockShared(&m_renderStateLock);
        return result;
    }

    void CreateMenu() {
        DestroyMenu();
        m_menu = new Menu(GetInternalId(), GetName(), true);

        auto* main = m_menu->AddSubMenu(new Menu("main", "Main"));
        m_enabledMenu = main->Add(new MenuBool("enabled", "Enable ZDEvade", true));
        m_walkEnabledMenu = main->Add(new MenuBool("walking", "Walking Evade", true));
        m_evadeSpellsMenu = main->Add(new MenuBool("evadeSpells", "Direct Evade Spells", true));
        m_fallbackMenu = main->Add(new MenuBool("leastDangerFallback", "Least Danger Fallback", true));
        m_minDangerMenu = main->Add(new MenuSlider("minimumDanger", "Minimum Danger", 1, 1, 4));
        m_evadeSpellDangerMenu = main->Add(new MenuSlider("evadeSpellDanger", "Evade Spell Minimum Danger", 3, 1, 4));
        m_evadeSpellMarginMenu = main->Add(new MenuSlider("evadeSpellMargin", "Evade Spell Margin Threshold", 45, 0, 250));

        m_spellsMenu = m_menu->AddSubMenu(new Menu("spells", "Enemy Spells"));
        CreateSpellMenus();
        m_evadeSpellOptionsMenu = m_menu->AddSubMenu(new Menu("evadeSpellOptions", "Evade Spells"));
        CreateEvadeSpellMenus();

        auto* safety = m_menu->AddSubMenu(new Menu("safety", "Safety and Timing"));
        m_endpointBufferMenu = safety->Add(
            new MenuSlider(
                ZDEvade::kEndpointMarginMenuPersistenceId,
                "Model Edge Exit Margin",
                ZDEvade::kEndpointMarginMenuDefault,
                ZDEvade::kEndpointMarginMenuMinimum,
                ZDEvade::kEndpointMarginMenuMaximum));
        m_pathBufferMenu = safety->Add(new MenuSlider("pathBuffer", "Path Buffer", 8, 0, 100));
        m_releaseBufferMenu = safety->Add(new MenuSlider(
            "releaseBuffer", "Release Control Margin", 48, 0, 140));
        m_inputDelayMenu = safety->Add(new MenuSlider("inputDelay", "Extra Input Delay", 55, 0, 200));
        m_minMarginMenu = safety->Add(new MenuSlider("minimumMargin", "Minimum Time Margin", 25, 0, 250));
        m_preferredClearanceMenu = safety->Add(new MenuSlider(
            "preferredClearance", "Preferred Clearance", 18, 0, 100));
        m_searchRadiusMenu = safety->Add(new MenuSlider("searchRadius", "Maximum Search Radius", 760, 300, 1200));

        auto* control = m_menu->AddSubMenu(new Menu("control", "Control"));
        m_moveIntervalMenu = control->Add(new MenuSlider("moveInterval", "Move Interval", 75, 25, 250));
        m_moveRefreshMenu = control->Add(new MenuSlider("moveRefresh", "Move Refresh", 260, 100, 600));
        m_replanIntervalMenu = control->Add(new MenuSlider("replanInterval", "Replan Interval", 70, 20, 250));

        auto* draw = m_menu->AddSubMenu(new Menu("draw", "Draw"));
        m_drawSpellsMenu = draw->Add(new MenuBool("drawSpells", "Draw Skillshots", true));
        m_drawCandidatesMenu = draw->Add(new MenuBool("drawCandidates", "Draw Candidates", false));

        auto* debug = m_menu->AddSubMenu(new Menu("debug", "Debug"));
        m_debugModeMenu = debug->Add(
            new MenuBool("debugMode", "Debug Mode", false));
        m_drawOwnPendingMenu = debug->Add(new MenuBool(
            "drawOwnPending",
            "Draw Own Pending Casts",
            true));
        m_drawOwnLiveMenu = debug->Add(new MenuBool(
            "drawOwnLive",
            "Draw Own Live Missiles",
            true));
        m_drawOwnLabelsMenu = debug->Add(new MenuBool(
            "drawOwnLabels",
            "Draw Own Labels",
            true));
        m_drawOwnEndpointsMenu = debug->Add(new MenuBool(
            "drawOwnEndpoints",
            "Draw Own Endpoints",
            true));
        m_showDebugCountersMenu = debug->Add(new MenuBool(
            "showDebugCounters",
            "Show Debug Counters",
            true));
        m_terminalHoldMenu = debug->Add(new MenuSlider(
            "terminalHold",
            "Terminal Hold (ms)",
            250,
            0,
            1000));
        debug->Add(new MenuButton(
            "clearDebugState",
            "Clear Debug State",
            "Clear",
            [this]() { m_selfSkillDebug.Clear(); }));

        m_menu->Attach();
    }

    void CreateSpellMenus() {
        if (!m_spellsMenu) return;
        std::unordered_set<std::string> enemyNames;
        for (const auto& enemy : SDK::ObjectManager::EnemyHeroes()) {
            if (!enemy.IsValid()) continue;
            enemyNames.insert(enemy.CharacterName());
        }
        int index = 0;
        for (const auto& spell : ZDEvade::SpellDatabase::Spells) {
            if (spell.spellName.empty()) continue;
            if (spell.charName != "AllChampions" && enemyNames.find(spell.charName) == enemyNames.end()) continue;
            if (m_spellBindings.find(spell.spellName) != m_spellBindings.end()) continue;
            const std::string id = "spell_" + std::to_string(index++);
            const std::string label = spell.charName + " - " +
                (spell.name.empty() ? spell.spellName : spell.name);
            auto* menu = m_spellsMenu->AddSubMenu(new Menu(id.c_str(), label.c_str()));
            SpellMenuBinding binding;
            binding.enabled = menu->Add(new MenuBool("enabled", "Dodge", !spell.defaultOff));
            binding.danger = menu->Add(new MenuSlider(
                "danger", "Danger", std::clamp(spell.dangerlevel, 1, 4), 1, 4));
            binding.health = menu->Add(new MenuSlider(
                "health", "Dodge Only Below HP", spell.dangerlevel == 1 ? 90 : 100, 0, 100));
            m_spellBindings.emplace(spell.spellName, binding);
        }
        RefreshThreatRules();
    }

    void CreateEvadeSpellMenus() {
        if (!m_evadeSpellOptionsMenu) return;
        EvadeSpellDatabase::Initialize();
        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) return;
        const std::string champion = player.CharacterName();
        int index = 0;
        for (const auto& spell : EvadeSpellDatabase::Spells) {
            const bool playerSpell = _stricmp(spell.charName.c_str(), champion.c_str()) == 0;
            const bool globalSpell = _stricmp(spell.charName.c_str(), "AllChampions") == 0 ||
                spell.isSummonerSpell || spell.isItem;
            if (!playerSpell && !globalSpell) continue;
            const std::string key = ZDEvade::EvadeSpellEngine::RuleKey(spell);
            if (m_evadeSpellBindings.find(key) != m_evadeSpellBindings.end()) continue;
            const std::string id = "evade_spell_" + std::to_string(index++);
            const std::string label = spell.name.empty() ? spell.spellName : spell.name;
            auto* menu = m_evadeSpellOptionsMenu->AddSubMenu(new Menu(id.c_str(), label.c_str()));
            EvadeSpellMenuBinding binding;
            binding.enabled = menu->Add(new MenuBool("enabled", "Use", true));
            binding.danger = menu->Add(new MenuSlider(
                "danger", "Minimum Danger", std::clamp(spell.dangerlevel, 1, 4), 1, 4));
            if (spell.castType == EvadeCastType::Target) {
                binding.wardJump = menu->Add(new MenuBool("wardJump", "Ward Jump", true));
            }
            m_evadeSpellBindings.emplace(key, binding);
        }
        RefreshEvadeSpellRules();
    }

    void RefreshEvadeSpellRules() {
        for (const auto& entry : m_evadeSpellBindings) {
            EvadeSpellRule& rule = m_evadeSpellRules[entry.first];
            rule.enabled = !entry.second.enabled || entry.second.enabled->Value;
            rule.danger = entry.second.danger ? entry.second.danger->Value : 1;
            rule.wardJump = !entry.second.wardJump || entry.second.wardJump->Value;
        }
    }

    void RefreshThreatRules() {
        for (const auto& entry : m_spellBindings) {
            ZDEvade::ThreatRule& rule = m_threatRules[entry.first];
            rule.enabled = !entry.second.enabled || entry.second.enabled->Value;
            rule.danger = entry.second.danger ? entry.second.danger->Value : 1;
            rule.dodgeHealthPercent = static_cast<float>(
                entry.second.health ? entry.second.health->Value : 100);
        }
    }

    void DestroyMenu() {
        if (!m_menu) return;
        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_spellsMenu = nullptr;
        m_evadeSpellOptionsMenu = nullptr;
        m_spellBindings.clear();
        m_evadeSpellBindings.clear();
        m_threatRules.clear();
        m_evadeSpellRules.clear();
        m_enabledMenu = nullptr;
        m_walkEnabledMenu = nullptr;
        m_evadeSpellsMenu = nullptr;
        m_fallbackMenu = nullptr;
        m_drawSpellsMenu = nullptr;
        m_drawCandidatesMenu = nullptr;
        m_debugModeMenu = nullptr;
        m_drawOwnPendingMenu = nullptr;
        m_drawOwnLiveMenu = nullptr;
        m_drawOwnLabelsMenu = nullptr;
        m_drawOwnEndpointsMenu = nullptr;
        m_showDebugCountersMenu = nullptr;
        m_terminalHoldMenu = nullptr;
        m_minDangerMenu = nullptr;
        m_evadeSpellDangerMenu = nullptr;
        m_evadeSpellMarginMenu = nullptr;
        m_endpointBufferMenu = nullptr;
        m_pathBufferMenu = nullptr;
        m_releaseBufferMenu = nullptr;
        m_inputDelayMenu = nullptr;
        m_minMarginMenu = nullptr;
        m_preferredClearanceMenu = nullptr;
        m_searchRadiusMenu = nullptr;
        m_moveIntervalMenu = nullptr;
        m_moveRefreshMenu = nullptr;
        m_replanIntervalMenu = nullptr;
        AcquireSRWLockExclusive(&m_renderStateLock);
        m_renderState = {};
        ReleaseSRWLockExclusive(&m_renderStateLock);
    }
};

} // namespace Plugins
