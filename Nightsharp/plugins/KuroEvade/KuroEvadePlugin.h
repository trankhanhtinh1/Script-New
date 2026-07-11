#pragma once

// KuroEvade is intentionally separate from the original EzEvade plugin.
// Runtime logic lives in plugins/KuroEvade/Native as native C++.

#include "../IPlugin.h"
#include "../Core/KuroCombatCoordinator.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"

#include "Native/KuroEvadeNative.h"

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace Plugins {

class KuroEvadePlugin final : public IPlugin {
public:
    const char* GetName() const override { return "KuroEvade"; }
    const char* GetInternalId() const override { return "core.kuroevade"; }
    const char* GetAuthor() const override { return "Kuro"; }
    PluginCategory GetCategory() const override { return PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return true; }

    void OnLoad() override {
        s_instance = this;
        KuroCombatCoordination::Coordinator::Reset();
        CreateMenu();
        m_detector.SetSpellEnabledPredicate([this](const KuroEvade::Generated::SpellDataEntry& data) {
            return IsSpellDodgeEnabled(data);
        });
        m_detector.Clear();

        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &KuroEvadePlugin::OnRawProcessSpellImmediateStatic);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnDoCast, &KuroEvadePlugin::OnRawDoCastImmediateStatic);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &KuroEvadePlugin::OnRawProcessCastSpellImmediateStatic);
        SDK::Events::AddOnCreateObject(&KuroEvadePlugin::OnObjectCreateStatic);
        SDK::Events::AddOnDeleteObject(&KuroEvadePlugin::OnObjectDeleteStatic);
        SDK::Events::AddOnMissileCreate(&KuroEvadePlugin::OnMissileCreateStatic);
        SDK::Events::AddOnMissileDelete(&KuroEvadePlugin::OnMissileDeleteStatic);
        SDK::Game::AddOnWndProc(&KuroEvadePlugin::OnWndProcStatic);
        SDK::Game::OnUpdate += &KuroEvadePlugin::OnUpdateStatic;

        NightSharpDebug::Logf("[KuroEvade] loaded");
    }

    void OnUnload() override {
        SDK::Game::OnUpdate -= &KuroEvadePlugin::OnUpdateStatic;
        SDK::Game::RemoveOnWndProc(&KuroEvadePlugin::OnWndProcStatic);
        SDK::Events::RemoveOnMissileDelete(&KuroEvadePlugin::OnMissileDeleteStatic);
        SDK::Events::RemoveOnMissileCreate(&KuroEvadePlugin::OnMissileCreateStatic);
        SDK::Events::RemoveOnDeleteObject(&KuroEvadePlugin::OnObjectDeleteStatic);
        SDK::Events::RemoveOnCreateObject(&KuroEvadePlugin::OnObjectCreateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &KuroEvadePlugin::OnRawProcessCastSpellImmediateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnDoCast, &KuroEvadePlugin::OnRawDoCastImmediateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &KuroEvadePlugin::OnRawProcessSpellImmediateStatic);

        m_detector.SetSpellEnabledPredicate({});
        m_detector.Clear();
        m_spellVisuals.clear();
        m_evadeIntervening = false;
        DestroyMenu();
        KuroEvade::Evade::RestoreOrbwalkerMove();
        KuroCombatCoordination::Coordinator::Reset();
        if (s_instance == this) {
            s_instance = nullptr;
        }

        NightSharpDebug::Logf("[KuroEvade] unloaded");
    }

    void OnRender() override {
        if (!Enabled() || !DrawSpells() || !ImGui::GetCurrentContext()) {
            return;
        }

        KuroEvade::SpellDrawer::Draw(
            m_detector.Skillshots(), m_spellVisuals, DrawingStyleSnapshot());
        if (DrawEvadeRoute() && m_evadeIntervening && m_evade.HasMoveTarget()) {
            const auto player = SDK::ObjectManager::Player();
            if (player.IsValid()) {
                KuroEvade::SpellDrawer::DrawRoute(
                    player.ServerPosition().To2D(),
                    m_evade.MoveTarget(),
                    RouteColor(),
                    m_evade.IsWaitingForWindup());
            }
        }
        KuroEvade::RenderObjects::Render();
    }

    void OnMenu() override {
        if (!m_menu) {
            return;
        }

        m_menu->DrawImGui();
        ImGui::Separator();
        ImGui::Text("Tracked skillshots: %d",
                    static_cast<int>(m_detector.Skillshots().size()));
        ImGui::Text("Spell menu entries: %d", m_spellMenuEntries);
        ImGui::Text("Evade spell entries: %d", m_evadeSpellMenuEntries);
        ImGui::Text("Optimization: %s", OptimizationProfileName());
        ImGui::Text("Visual threats: %d", RelevantVisualCount());
        ImGui::Text("Orbwalker sync: %s",
                    KuroCombatCoordination::Coordinator::PhaseName(
                        KuroCombatCoordination::Coordinator::Phase(
                            SDK::Variables::TickCount())));
        ImGui::Text("Last dodge: %s", m_lastEvent);
    }

private:
    static inline KuroEvadePlugin* s_instance = nullptr;

    Menu* m_menu = nullptr;
    MenuBool* m_enabledMenu = nullptr;
    MenuKeyBind* m_dodgeKeyMenu = nullptr;
    MenuBool* m_dodgeDangerousMenu = nullptr;
    MenuBool* m_dodgeFowMenu = nullptr;
    MenuBool* m_dodgeCircularMenu = nullptr;
    MenuBool* m_dangerKeysEnabledMenu = nullptr;
    MenuKeyBind* m_dangerKeyMenu = nullptr;
    MenuKeyBind* m_dangerKey2Menu = nullptr;
    MenuBool* m_comboOnlyEnabledMenu = nullptr;
    MenuKeyBind* m_comboKeyMenu = nullptr;
    MenuBool* m_dontDodgeEnabledMenu = nullptr;
    MenuKeyBind* m_dontDodgeKeyMenu = nullptr;
    MenuBool* m_drawSpellsMenu = nullptr;
    MenuKeyBind* m_useEvadeSpellsMenu = nullptr;
    MenuBool* m_preferEvadeSpellsMenu = nullptr;
    MenuList* m_optimizationProfileMenu = nullptr;
    MenuList* m_evadeModeMenu = nullptr;
    MenuBool* m_extremeEvadeMenu = nullptr;
    MenuBool* m_kurokamiPosMenu = nullptr;
    MenuBool* m_higherPrecisionMenu = nullptr;
    MenuBool* m_calculateWindupMenu = nullptr;
    MenuBool* m_checkCollisionMenu = nullptr;
    MenuBool* m_clickRemoveMenu = nullptr;
    MenuBool* m_preventTowerMenu = nullptr;
    MenuBool* m_preventEnemyMenu = nullptr;
    MenuSlider* m_extraDelayMenu = nullptr;
    MenuSlider* m_extraDistMenu = nullptr;
    MenuSlider* m_extraSpellRadiusMenu = nullptr;
    MenuSlider* m_extraEvadeDistanceMenu = nullptr;
    MenuSlider* m_extraAvoidDistanceMenu = nullptr;
    MenuSlider* m_minComfortZoneMenu = nullptr;
    MenuSlider* m_reactionTimeMenu = nullptr;
    MenuSlider* m_spellDetectionTimeMenu = nullptr;
    MenuSlider* m_minHitTimeMenu = nullptr;
    MenuSlider* m_dodgeIntervalMenu = nullptr;
    MenuSlider* m_dodgeHpMenu = nullptr;
    MenuSlider* m_spellActivationMenu = nullptr;
    MenuSlider* m_fastActivationMenu = nullptr;
    MenuSlider* m_rejectMinDistanceMenu = nullptr;
    MenuColor* m_spellColorMenu = nullptr;
    MenuBool* m_fillSkillshotsMenu = nullptr;
    MenuBool* m_drawIrrelevantMenu = nullptr;
    MenuBool* m_drawEvadeRouteMenu = nullptr;
    MenuBool* m_drawThreatLabelsMenu = nullptr;
    MenuSlider* m_irrelevantOpacityMenu = nullptr;
    MenuSlider* m_threatOpacityMenu = nullptr;
    MenuColor* m_irrelevantColorMenu = nullptr;
    MenuColor* m_pathThreatColorMenu = nullptr;
    MenuColor* m_interventionColorMenu = nullptr;
    MenuColor* m_routeColorMenu = nullptr;
    MenuBool* m_clickOnlyOnceMenu = nullptr;
    Menu* m_spellsMenu = nullptr;
    Menu* m_evadeSpellsMenu = nullptr;

    struct SpellMenuOption {
        MenuBool* Dodge = nullptr;
        MenuBool* Draw = nullptr;
    };

    std::unordered_map<std::string, SpellMenuOption> m_spellOptions;
    int m_spellMenuEntries = 0;

    struct EvadeSpellMenuOption {
        MenuBool* Use = nullptr;
        MenuList* Danger = nullptr;
        MenuList* Mode = nullptr;
    };

    std::unordered_map<std::string, EvadeSpellMenuOption> m_evadeSpellOptions;
    int m_evadeSpellMenuEntries = 0;

    char m_lastEvent[96] = "none";
    int m_lastDodgeTick = 0;
    KuroEvade::SpellDetector m_detector;
    KuroEvade::Evade m_evade;
    KuroEvade::SpellDrawer::VisualMap m_spellVisuals;
    bool m_evadeIntervening = false;
    bool m_visualInterventionState = false;
    int m_lastVisualUpdateTick = 0;

    static inline uintptr_t s_lastImmediateProcessCastInfo = 0;
    static inline int s_lastImmediateProcessTick = 0;

    static void OnRawProcessSpellImmediateStatic(const SDK::Events::CoreHookArgs& raw) {
        if (!s_instance || raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        auto decoded = ::Core::Events::DecodeProcessSpell(raw);
        s_lastImmediateProcessCastInfo = decoded.CastInfo;
        s_lastImmediateProcessTick = SDK::Variables::TickCount();
        s_instance->m_evade.OnProcessSpell(decoded);
        s_instance->m_detector.OnProcessSpell(decoded);
    }

    static void OnRawDoCastImmediateStatic(const SDK::Events::CoreHookArgs& raw) {
        if (!s_instance || raw.Id != SDK::Events::Hooks::OnDoCast) return;
        auto decoded = ::Core::Events::DecodeDoCast(raw);
        if (decoded.CastInfo && decoded.CastInfo == s_lastImmediateProcessCastInfo &&
            SDK::Variables::TickCount() - s_lastImmediateProcessTick <= 50) {
            return;
        }
        s_instance->m_evade.OnProcessSpell(decoded);
        s_instance->m_detector.OnProcessSpell(decoded);
    }

    static void OnRawProcessCastSpellImmediateStatic(const SDK::Events::CoreHookArgs& raw) {
        if (!s_instance || raw.Id != SDK::Events::Hooks::ProcessCastSpell) return;
        auto decoded = ::Core::Events::DecodeProcessCastSpell(raw);
        s_instance->m_detector.OnProcessCastSpell(decoded);
    }

    static void OnMissileCreateStatic(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->m_detector.OnMissileCreate(args);
        }
    }

    static void OnObjectCreateStatic(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->m_detector.OnObjectCreate(args);
        }
    }

    static void OnObjectDeleteStatic(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->m_detector.OnObjectDelete(args);
        }
    }

    static void OnMissileDeleteStatic(const SDK::Events::ObjectEventArgs& args) {
        if (s_instance) {
            s_instance->m_detector.OnMissileDelete(args);
        }
    }

    static void OnUpdateStatic() {
        if (s_instance) {
            s_instance->Tick();
        }
    }

    static void OnWndProcStatic(SDK::Game::WndEventArgs& args) {
        if (!s_instance || args.Msg != WM_LBUTTONDOWN ||
            !s_instance->m_clickRemoveMenu ||
            !s_instance->m_clickRemoveMenu->Value) {
            return;
        }
        const int removed = s_instance->m_detector.RemoveAtPosition(
            SDK::Game::CursorPos().To2D(), 50.0f);
        if (removed > 0) {
            std::snprintf(s_instance->m_lastEvent,
                          sizeof(s_instance->m_lastEvent),
                          "removed %d spell%s",
                          removed,
                          removed == 1 ? "" : "s");
        }
    }

    bool Enabled() const { return !m_enabledMenu || m_enabledMenu->Value; }
    bool DrawSpells() const { return !m_drawSpellsMenu || m_drawSpellsMenu->Value; }
    bool DodgeKeyActive() const {
        const bool baseActive = !m_dodgeKeyMenu || m_dodgeKeyMenu->Active;
        const bool comboAllowed = !m_comboOnlyEnabledMenu ||
            !m_comboOnlyEnabledMenu->Value ||
            (m_comboKeyMenu && m_comboKeyMenu->Active);
        const bool blocked = m_dontDodgeEnabledMenu &&
            m_dontDodgeEnabledMenu->Value &&
            m_dontDodgeKeyMenu &&
            m_dontDodgeKeyMenu->Active;
        return baseActive && comboAllowed && !blocked;
    }
    bool DodgeDangerousOnly() const {
        const bool hotkey = m_dangerKeysEnabledMenu &&
            m_dangerKeysEnabledMenu->Value &&
            ((m_dangerKeyMenu && m_dangerKeyMenu->Active) ||
             (m_dangerKey2Menu && m_dangerKey2Menu->Active));
        return (m_dodgeDangerousMenu && m_dodgeDangerousMenu->Value) || hotkey;
    }
    bool DodgeFow() const { return !m_dodgeFowMenu || m_dodgeFowMenu->Value; }
    bool DodgeCircular() const { return !m_dodgeCircularMenu || m_dodgeCircularMenu->Value; }
    bool UseEvadeSpells() const { return !m_useEvadeSpellsMenu || m_useEvadeSpellsMenu->Active; }
    bool PreferEvadeSpells() const { return m_preferEvadeSpellsMenu && m_preferEvadeSpellsMenu->Value; }
    bool ExtremeEvade() const { return m_extremeEvadeMenu && m_extremeEvadeMenu->Value; }
    bool KurokamiPosition() const { return !m_kurokamiPosMenu || m_kurokamiPosMenu->Value; }
    bool HigherPrecision() const { return ExtremeEvade() || (m_higherPrecisionMenu && m_higherPrecisionMenu->Value); }
    bool CalculateWindupDelay() const { return !m_calculateWindupMenu || m_calculateWindupMenu->Value; }
    bool CheckSpellCollision() const { return m_checkCollisionMenu && m_checkCollisionMenu->Value; }
    bool PreventTower() const { return m_preventTowerMenu && m_preventTowerMenu->Value; }
    bool PreventEnemy() const { return !m_preventEnemyMenu || m_preventEnemyMenu->Value; }
    int EvadeMode() const { return m_evadeModeMenu ? m_evadeModeMenu->Index : 2; }
    float ExtraDelay() const { return m_extraDelayMenu ? static_cast<float>(m_extraDelayMenu->Value) : 30.0f; }
    float ExtraDist() const { return m_extraDistMenu ? static_cast<float>(m_extraDistMenu->Value) : 10.0f; }
    float ExtraSpellRadius() const { return m_extraSpellRadiusMenu ? static_cast<float>(m_extraSpellRadiusMenu->Value) : 0.0f; }
    float ExtraEvadeDistance() const { return m_extraEvadeDistanceMenu ? static_cast<float>(m_extraEvadeDistanceMenu->Value) : 100.0f; }
    float ExtraAvoidDistance() const { return m_extraAvoidDistanceMenu ? static_cast<float>(m_extraAvoidDistanceMenu->Value) : 50.0f; }
    float MinComfortZone() const { return m_minComfortZoneMenu ? static_cast<float>(m_minComfortZoneMenu->Value) : 550.0f; }
    int ReactionTime() const { return m_reactionTimeMenu ? m_reactionTimeMenu->Value : 0; }
    int SpellDetectionTime() const {
        return m_spellDetectionTimeMenu ? m_spellDetectionTimeMenu->Value : 0;
    }
    int MinHitTime() const { return m_minHitTimeMenu ? m_minHitTimeMenu->Value : 900; }
    int DodgeInterval() const { return m_dodgeIntervalMenu ? m_dodgeIntervalMenu->Value : 0; }
    float DodgeHp() const { return m_dodgeHpMenu ? static_cast<float>(m_dodgeHpMenu->Value) : 100.0f; }
    int SpellActivationTime() const { return m_spellActivationMenu ? m_spellActivationMenu->Value : 400; }
    float FastActivationTime() const { return m_fastActivationMenu ? static_cast<float>(m_fastActivationMenu->Value) : 65.0f; }
    float RejectMinDistance() const { return m_rejectMinDistanceMenu ? static_cast<float>(m_rejectMinDistanceMenu->Value) : 10.0f; }
    bool ClickOnlyOnce() const {
        return !m_clickOnlyOnceMenu || m_clickOnlyOnceMenu->Value;
    }
    std::uint32_t SpellColor() const {
        return m_spellColorMenu ? m_spellColorMenu->GetImU32() : IM_COL32(255, 80, 80, 220);
    }
    int OptimizationProfile() const {
        return m_optimizationProfileMenu ? m_optimizationProfileMenu->Index : 0;
    }
    const char* OptimizationProfileName() const {
        switch (OptimizationProfile()) {
        case 1: return "Fastest Dodge";
        case 2: return "Smooth Movement";
        case 3: return "Low FPS Cost";
        case 4: return "Custom";
        default: return "Balanced";
        }
    }
    bool FillSkillshots() const {
        return !m_fillSkillshotsMenu || m_fillSkillshotsMenu->Value;
    }
    bool DrawIrrelevant() const {
        return !m_drawIrrelevantMenu || m_drawIrrelevantMenu->Value;
    }
    bool DrawEvadeRoute() const {
        return !m_drawEvadeRouteMenu || m_drawEvadeRouteMenu->Value;
    }
    bool DrawThreatLabels() const {
        return m_drawThreatLabelsMenu && m_drawThreatLabelsMenu->Value;
    }
    int IrrelevantOpacity() const {
        return m_irrelevantOpacityMenu ? m_irrelevantOpacityMenu->Value : 10;
    }
    int ThreatOpacity() const {
        return m_threatOpacityMenu ? m_threatOpacityMenu->Value : 42;
    }
    ImU32 IrrelevantColor() const {
        return m_irrelevantColorMenu
            ? m_irrelevantColorMenu->GetImU32()
            : IM_COL32(145, 154, 164, 255);
    }
    ImU32 PathThreatColor() const {
        return m_pathThreatColorMenu
            ? m_pathThreatColorMenu->GetImU32()
            : IM_COL32(255, 190, 65, 255);
    }
    ImU32 InterventionColor() const {
        return m_interventionColorMenu
            ? m_interventionColorMenu->GetImU32()
            : IM_COL32(82, 220, 255, 255);
    }
    ImU32 RouteColor() const {
        return m_routeColorMenu
            ? m_routeColorMenu->GetImU32()
            : IM_COL32(100, 255, 150, 255);
    }

    KuroEvade::SpellDrawStyle DrawingStyleSnapshot() const {
        KuroEvade::SpellDrawStyle style;
        style.Fill = FillSkillshots();
        style.DrawIrrelevant = DrawIrrelevant();
        style.DrawLabels = DrawThreatLabels();
        style.IrrelevantOpacity = IrrelevantOpacity();
        style.ThreatOpacity = ThreatOpacity();
        style.IrrelevantColor = IrrelevantColor();
        style.PathColor = PathThreatColor();
        style.DirectColor = SpellColor();
        style.InterventionColor = InterventionColor();
        style.RouteColor = RouteColor();
        return style;
    }

    int RelevantVisualCount() const {
        return static_cast<int>(std::count_if(
            m_spellVisuals.begin(), m_spellVisuals.end(),
            [](const auto& entry) {
                return entry.second.State != KuroEvade::SpellVisualState::Irrelevant;
            }));
    }

    int VisualRefreshInterval() const {
        switch (OptimizationProfile()) {
        case 2: return 24;
        case 3: return 50;
        default: return 16;
        }
    }

    KuroEvade::EvadeSettings SettingsSnapshot() const {
        KuroEvade::EvadeSettings settings;
        settings.Enabled = Enabled();
        settings.DrawSpells = DrawSpells();
        settings.DodgeKeyActive = DodgeKeyActive();
        settings.DodgeDangerousOnly = DodgeDangerousOnly();
        settings.DodgeFow = DodgeFow();
        settings.DodgeCircular = DodgeCircular();
        settings.UseEvadeSpells = UseEvadeSpells();
        settings.PreferEvadeSpells = PreferEvadeSpells();
        settings.ExtremeEvade = ExtremeEvade();
        settings.KurokamiPosition = KurokamiPosition();
        settings.HigherPrecision = HigherPrecision();
        settings.CalculateWindupDelay = CalculateWindupDelay();
        settings.CheckSpellCollision = CheckSpellCollision();
        settings.ClickOnlyOnce = ClickOnlyOnce();
        settings.PreventTower = PreventTower();
        settings.PreventEnemy = PreventEnemy();
        settings.EvadeMode = EvadeMode();
        settings.ExtraDelay = ExtraDelay();
        settings.ExtraDist = ExtraDist();
        settings.ExtraSpellRadius = ExtraSpellRadius();
        settings.ExtraEvadeDistance = ExtraEvadeDistance();
        settings.ExtraAvoidDistance = ExtraAvoidDistance();
        settings.MinComfortZone = MinComfortZone();
        settings.ReactionTime = ReactionTime();
        settings.SpellDetectionTime = SpellDetectionTime();
        settings.MinHitTime = MinHitTime();
        settings.DodgeInterval = DodgeInterval();
        settings.DodgeHp = DodgeHp();
        settings.SpellActivationTime = SpellActivationTime();
        settings.FastActivationTime = FastActivationTime();
        settings.RejectMinDistance = RejectMinDistance();
        settings.SpellColor = SpellColor();

        switch (OptimizationProfile()) {
        case 1: // Fastest Dodge: react early and spend more candidates once.
            settings.CandidateBudget = 80;
            settings.EvadeMode = 1;
            settings.ReactionTime = 0;
            settings.SpellDetectionTime = 0;
            settings.MinHitTime = std::max(settings.MinHitTime, 1200);
            settings.FastActivationTime = std::max(settings.FastActivationTime, 100.0f);
            settings.KurokamiPosition = true;
            settings.ClickOnlyOnce = true;
            break;
        case 2: // Smooth Movement: stable scoring and fewer direction changes.
            settings.CandidateBudget = 48;
            settings.EvadeMode = 0;
            settings.HigherPrecision = false;
            settings.KurokamiPosition = true;
            settings.ClickOnlyOnce = true;
            break;
        case 3: // Low FPS Cost: analytical/gradient candidates, minimal scans.
            settings.CandidateBudget = 28;
            settings.ExtremeEvade = false;
            settings.HigherPrecision = false;
            settings.KurokamiPosition = false;
            settings.CheckSpellCollision = false;
            settings.ClickOnlyOnce = true;
            break;
        case 4: // Custom: use every manual option exactly as configured.
            settings.CandidateBudget = 0;
            break;
        default: // Balanced.
            settings.CandidateBudget = settings.HigherPrecision ? 150 : 50;
            break;
        }
        return settings;
    }

    void UpdateSpellVisuals(const KuroEvade::EvadeSettings& settings,
                            const std::vector<Vec3>& observedPath,
                            bool intervention) {
        if (!settings.DrawSpells) {
            m_spellVisuals.clear();
            return;
        }

        const int now = SDK::Variables::TickCount();
        const bool interventionChanged =
            intervention != m_visualInterventionState;
        if (!interventionChanged && !m_spellVisuals.empty() &&
            now - m_lastVisualUpdateTick < VisualRefreshInterval()) {
            return;
        }
        m_lastVisualUpdateTick = now;
        m_visualInterventionState = intervention;
        m_spellVisuals.clear();
        m_spellVisuals.reserve(m_detector.Skillshots().size());

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        KuroEvade::EvadeSettings visualSettings = settings;
        visualSettings.PreventEnemy = false;
        visualSettings.PreventTower = false;
        visualSettings.CurrentWindupDelay =
            settings.CalculateWindupDelay && SDK::Orbwalker::IsAutoAttacking()
                ? static_cast<float>(SDK::Orbwalker::AttackCastDelayRemaining())
                : 0.0f;
        KuroEvade::EvadeHelper helper(visualSettings);

        const Vec2 heroPos = player.ServerPosition().To2D();
        const float boundingRadius = player.BoundingRadius();
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float pathDelay = settings.ExtraDelay +
            static_cast<float>(SDK::Game::Ping()) +
            visualSettings.CurrentWindupDelay;
        const float directWindow = static_cast<float>(
            std::max(settings.MinHitTime, 1200));

        for (const auto& skillshot : m_detector.Skillshots()) {
            if (!skillshot || !ShouldDrawSpell(skillshot->SData)) {
                continue;
            }

            KuroEvade::SpellVisualInfo info;
            info.Danger = KuroEvade::EvadeHelper::DangerValue(*skillshot);
            info.HitTimeMs = KuroEvade::EvadeHelper::SpellHitTime(
                *skillshot, heroPos);

            bool directThreat = false;
            bool pathThreat = false;
            if (helper.ShouldConsiderSpell(*skillshot)) {
                directThreat =
                    info.HitTimeMs <= directWindow &&
                    KuroEvade::EvadeHelper::InSkillShot(
                        *skillshot, heroPos,
                        boundingRadius + settings.ExtraSpellRadius);
                pathThreat = helper.SkillshotAffectsPath(
                    *skillshot, observedPath, pathDelay,
                    heroPos, boundingRadius, moveSpeed);
            }

            if (intervention && (directThreat || pathThreat)) {
                info.State = KuroEvade::SpellVisualState::Intervention;
            } else if (directThreat) {
                info.State = KuroEvade::SpellVisualState::DirectThreat;
            } else if (pathThreat) {
                info.State = KuroEvade::SpellVisualState::PathThreat;
            }
            m_spellVisuals.emplace(skillshot.get(), info);
        }
    }

    void Tick() {
        const auto settings = SettingsSnapshot();
        m_detector.SetCollisionEnabled(settings.CheckSpellCollision);
        m_detector.SetFowEnabled(settings.DodgeFow);
        m_detector.Update();
        RemoveDisabledSkillshots();
        const auto player = SDK::ObjectManager::Player();
        const std::vector<Vec3> observedPath = player.IsValid()
            ? player.Path()
            : std::vector<Vec3>();
        m_evadeIntervening = m_evade.Tick(
            settings, m_detector.Skillshots(), m_lastDodgeTick,
            [this](const KuroEvade::EvadeSpellData& data) {
                return ResolveEvadeSpellConfig(data);
            },
            m_lastEvent, sizeof(m_lastEvent));
        const auto phase = !m_evadeIntervening
            ? KuroCombatCoordination::EvadePhase::Idle
            : m_evade.IsWaitingForWindup()
                ? KuroCombatCoordination::EvadePhase::WindupHold
                : m_evade.IsMovementBlocking()
                    ? KuroCombatCoordination::EvadePhase::PathRecovery
                    : KuroCombatCoordination::EvadePhase::Dodging;
        KuroCombatCoordination::Coordinator::Publish(
            phase, SDK::Variables::TickCount());
        UpdateSpellVisuals(settings, observedPath, m_evadeIntervening);
    }

    bool IsSpellDodgeEnabled(const KuroEvade::Generated::SpellDataEntry& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        if (it == m_spellOptions.end()) {
            return !data.DisabledByDefault;
        }
        return !it->second.Dodge || it->second.Dodge->Value;
    }

    bool IsSpellDodgeEnabled(const SDK::SpellDatabaseEntry& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it == m_spellOptions.end() || !it->second.Dodge || it->second.Dodge->Value;
    }

    bool ShouldDrawSpell(const SDK::SpellDatabaseEntry& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it == m_spellOptions.end() || !it->second.Draw || it->second.Draw->Value;
    }

    void RemoveDisabledSkillshots() {
        auto& skillshots = m_detector.Skillshots();
        skillshots.erase(
            std::remove_if(skillshots.begin(), skillshots.end(), [&](const auto& skillshot) {
                return skillshot && !IsSpellDodgeEnabled(skillshot->SData);
            }),
            skillshots.end());
    }

    void CreateMenu() {
        DestroyMenu();
        m_menu = new Menu(GetInternalId(), GetName(), true);

        auto* main = m_menu->AddSubMenu(new Menu("main", "Main"));
        m_enabledMenu = main->Add(new MenuBool("enabled", "Enable KuroEvade", true));
        m_dodgeKeyMenu = main->Add(new MenuKeyBind(
            "dodgeKey", "Dodge Skillshots", 'K', KeyBindType::Toggle, true));
        m_dodgeDangerousMenu = main->Add(new MenuBool("dodgeDangerous", "Dodge Only Dangerous", false));
        m_dodgeFowMenu = main->Add(new MenuBool("dodgeFow", "Dodge FOW Skillshots", true));
        m_dodgeCircularMenu = main->Add(new MenuBool("dodgeCircular", "Dodge Circular Skillshots", true));
        m_drawSpellsMenu = main->Add(new MenuBool("drawSpells", "Draw Skillshots", true));
        m_useEvadeSpellsMenu = main->Add(new MenuKeyBind(
            "activateEvadeSpells", "Use Evade Spells", 'K', KeyBindType::Toggle, true));
        m_preferEvadeSpellsMenu = main->Add(new MenuBool("preferEvadeSpells", "Prefer Evade Spells", false));
        m_optimizationProfileMenu = main->Add(new MenuList(
            "optimizationProfile", "Optimization Preset",
            { "Balanced", "Fastest Dodge", "Smooth Movement", "Low FPS Cost", "Custom" }, 0));
        m_evadeModeMenu = main->Add(new MenuList("evadeMode", "Position Scoring",
            { "Smooth", "Fastest", "GuessWho" }, 2));
        m_extremeEvadeMenu = main->Add(new MenuBool("extremeEvade", "Extreme Evade", false));
        m_kurokamiPosMenu = main->Add(new MenuBool("kurokamiPos", "Kurokami Evade Pos", true));

        auto* drawing = m_menu->AddSubMenu(new Menu("drawing", "Smart Drawing"));
        m_fillSkillshotsMenu = drawing->Add(new MenuBool(
            "fillSkillshots", "Filled Skillshots", true));
        m_drawIrrelevantMenu = drawing->Add(new MenuBool(
            "drawIrrelevant", "Show Irrelevant Skillshots", true));
        m_drawEvadeRouteMenu = drawing->Add(new MenuBool(
            "drawEvadeRoute", "Draw Active Evade Route", true));
        m_drawThreatLabelsMenu = drawing->Add(new MenuBool(
            "drawThreatLabels", "Draw Spell / Hit-Time Labels", false));
        m_irrelevantOpacityMenu = drawing->Add(new MenuSlider(
            "irrelevantOpacity", "Irrelevant Fill Opacity %", 8, 0, 35));
        m_threatOpacityMenu = drawing->Add(new MenuSlider(
            "threatOpacity", "Threat Fill Opacity %", 38, 5, 85));
        m_irrelevantColorMenu = drawing->Add(new MenuColor(
            "irrelevantColor", "Irrelevant / Gray", 0.57f, 0.60f, 0.64f, 1.0f));
        m_pathThreatColorMenu = drawing->Add(new MenuColor(
            "pathThreatColor", "Current Path Threat", 1.0f, 0.74f, 0.25f, 1.0f));
        m_spellColorMenu = drawing->Add(new MenuColor(
            "spellColor", "Direct Threat", 1.0f, 0.28f, 0.28f, 1.0f));
        m_interventionColorMenu = drawing->Add(new MenuColor(
            "interventionColor", "Evade Intervention", 0.32f, 0.86f, 1.0f, 1.0f));
        m_routeColorMenu = drawing->Add(new MenuColor(
            "routeColor", "Evade Route", 0.39f, 1.0f, 0.59f, 1.0f));

        auto* keys = m_menu->AddSubMenu(new Menu("keys", "Key Settings"));
        m_dangerKeysEnabledMenu = keys->Add(new MenuBool(
            "dangerKeysEnabled", "Enable Dangerous Only Keys", false));
        m_dangerKeyMenu = keys->Add(new MenuKeyBind(
            "dangerKey", "Dangerous Only Key", VK_SPACE, KeyBindType::Press, false));
        m_dangerKey2Menu = keys->Add(new MenuKeyBind(
            "dangerKey2", "Dangerous Only Key 2", 'V', KeyBindType::Press, false));
        m_comboOnlyEnabledMenu = keys->Add(new MenuBool(
            "comboOnlyEnabled", "Dodge Only On Combo Key", false));
        m_comboKeyMenu = keys->Add(new MenuKeyBind(
            "comboKey", "Combo Key", VK_SPACE, KeyBindType::Press, false));
        m_dontDodgeEnabledMenu = keys->Add(new MenuBool(
            "dontDodgeEnabled", "Enable Don't Dodge Key", false));
        m_dontDodgeKeyMenu = keys->Add(new MenuKeyBind(
            "dontDodgeKey", "Don't Dodge Key", 'Z', KeyBindType::Press, false));

        auto* misc = m_menu->AddSubMenu(new Menu("misc", "Misc Settings"));
        m_higherPrecisionMenu = misc->Add(new MenuBool("higherPrecision", "Enhanced Dodge Precision", false));
        m_calculateWindupMenu = misc->Add(new MenuBool("calculateWindup", "Calculate Windup Delay", true));
        m_checkCollisionMenu = misc->Add(new MenuBool("checkCollision", "Check Spell Collision", true));
        m_clickRemoveMenu = misc->Add(new MenuBool(
            "clickRemove", "Allow Left Click Removal", true));
        m_preventTowerMenu = misc->Add(new MenuBool("preventTower", "Prevent Dodging Under Tower", false));
        m_preventEnemyMenu = misc->Add(new MenuBool("preventEnemy", "Prevent Dodging Near Enemies", true));

        auto* buffers = m_menu->AddSubMenu(new Menu("buffers", "Extra Buffers"));
        m_extraDelayMenu = buffers->Add(new MenuSlider(
            "extraDelay", "Extra Ping Buffer (ms)", 0, 0, 200));
        m_extraDistMenu = buffers->Add(new MenuSlider(
            "extraDist", "Extra CPA Distance", 10, 0, 150));
        m_extraSpellRadiusMenu = buffers->Add(new MenuSlider(
            "extraSpellRadius", "Extra Spell Radius", 0, 0, 100));
        m_extraEvadeDistanceMenu = buffers->Add(new MenuSlider(
            "extraEvadeDistance", "Extra Evade Distance", 100, 0, 300));
        m_extraAvoidDistanceMenu = buffers->Add(new MenuSlider(
            "extraAvoidDistance", "Extra Avoid Distance", 50, 0, 300));
        m_minComfortZoneMenu = buffers->Add(new MenuSlider(
            "minComfortZone", "Min Distance to Champion", 550, 0, 1000));

        auto* fast = m_menu->AddSubMenu(new Menu("fast", "Fast Evade"));
        m_spellActivationMenu = fast->Add(new MenuSlider(
            "spellActivationTime", "Spell Activation Time", 400, 0, 1000));
        m_fastActivationMenu = fast->Add(new MenuSlider(
            "fastActivation", "FastEvade Activation Time", 0, 0, 500));
        m_rejectMinDistanceMenu = fast->Add(new MenuSlider(
            "rejectMinDistance", "Collision Distance Buffer", 10, 0, 100));

        auto* humanizer = m_menu->AddSubMenu(new Menu("humanizer", "Humanizer / Gates"));
        m_clickOnlyOnceMenu = humanizer->Add(new MenuBool(
            "clickOnlyOnce", "Click Only Once", true));
        m_spellDetectionTimeMenu = humanizer->Add(new MenuSlider(
            "spellDetectionTime", "Spell Detection Time (ms)", 0, 0, 1000));
        m_reactionTimeMenu = humanizer->Add(new MenuSlider(
            "reactionTime", "Reaction Time (ms)", 0, 0, 500));
        m_minHitTimeMenu = humanizer->Add(new MenuSlider(
            "minHitTime", "Max Hit Time to Dodge (ms)", 900, 100, 2000));
        m_dodgeIntervalMenu = humanizer->Add(new MenuSlider(
            "dodgeInterval", "Dodge Re-issue Interval (ms)", 0, 0, 500));
        m_dodgeHpMenu = humanizer->Add(new MenuSlider(
            "dodgeHp", "Only Dodge Below HP %", 100, 1, 100));

        BuildSpellMenu();
        BuildEvadeSpellMenu();
        m_menu->Attach();
    }

    void BuildSpellMenu() {
        m_spellOptions.clear();
        m_spellMenuEntries = 0;
        m_spellsMenu = m_menu->AddSubMenu(new Menu("spells", "Enemy Spells"));

        auto* globalMenu = m_spellsMenu->AddSubMenu(new Menu("all_champions", "All Champions"));
        for (const auto& data : KuroEvade::SpellDatabase::Spells()) {
            if (_stricmp(data.sdk.ChampionName.c_str(), "AllChampions") == 0) {
                AddSpellMenuEntry(globalMenu, data);
            }
        }

        std::unordered_set<std::string> addedChampions;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid()) {
                continue;
            }

            const std::string championName = enemy.CharacterName();
            if (championName.empty()) {
                continue;
            }

            const std::string championKey = KuroEvade::SpellMenuKey::Lower(championName);
            if (!addedChampions.insert(championKey).second) {
                continue;
            }

            auto* championMenu = m_spellsMenu->AddSubMenu(
                new Menu(KuroEvade::SpellMenuKey::Sanitize(championName).c_str(), championName.c_str()));

            for (const auto& data : KuroEvade::SpellDatabase::Spells()) {
                if (_stricmp(data.sdk.ChampionName.c_str(), championName.c_str()) == 0) {
                    AddSpellMenuEntry(championMenu, data);
                }
            }
        }
    }

    void AddSpellMenuEntry(Menu* parent, const KuroEvade::Generated::SpellDataEntry& data) {
        if (!parent || data.IsSpecialIgnore) {
            return;
        }

        const std::string key = KuroEvade::SpellMenuKey::Key(data);
        if (m_spellOptions.find(key) != m_spellOptions.end()) {
            return;
        }

        const std::string display = std::string(KuroEvade::SpellMenuKey::SlotName(data.sdk.Slot)) +
            " - " + KuroEvade::SpellMenuKey::DisplayName(data);
        const std::string menuId = "spell_" + KuroEvade::SpellMenuKey::Sanitize(key);
        auto* spellMenu = parent->AddSubMenu(new Menu(menuId.c_str(), display.c_str()));

        SpellMenuOption option;
        option.Dodge = spellMenu->Add(new MenuBool("dodge", "Dodge", !data.DisabledByDefault));
        option.Draw = spellMenu->Add(new MenuBool("draw", "Draw", true));
        m_spellOptions.emplace(key, option);
        ++m_spellMenuEntries;
    }

    KuroEvade::EvadeSpellConfig ResolveEvadeSpellConfig(const KuroEvade::EvadeSpellData& data) const {
        KuroEvade::EvadeSpellConfig config;
        config.Enabled = data.IsEnabled;
        config.DangerLevel = data.DangerLevel;
        config.Mode = data.IndexUseWhen;

        const auto it = m_evadeSpellOptions.find(KuroEvade::EvadeSpell::MenuKey(data));
        if (it == m_evadeSpellOptions.end()) {
            return config;
        }

        const EvadeSpellMenuOption& option = it->second;
        config.Enabled = !option.Use || option.Use->Value;
        config.DangerLevel = option.Danger ? option.Danger->Index + 1 : config.DangerLevel;
        config.Mode = option.Mode ? option.Mode->Index : config.Mode;
        return config;
    }

    void BuildEvadeSpellMenu() {
        m_evadeSpellOptions.clear();
        m_evadeSpellMenuEntries = 0;
        m_evadeSpellsMenu = m_menu->AddSubMenu(new Menu("evade_spells", "Evade Spells"));

        const auto player = SDK::ObjectManager::Player();
        if (!player.IsValid()) {
            return;
        }

        const std::string championName = player.CharacterName();
        auto* championMenu = m_evadeSpellsMenu->AddSubMenu(new Menu(
            KuroEvade::SpellMenuKey::Sanitize("self_" + championName).c_str(),
            championName.empty() ? "My Champion" : championName.c_str()));
        auto* globalMenu = m_evadeSpellsMenu->AddSubMenu(new Menu("all_champions_evade", "All Champions"));

        for (const auto* data : KuroEvade::EvadeSpellDatabase::ForChampion(championName.c_str(), false)) {
            AddEvadeSpellMenuEntry(championMenu, *data);
        }
        for (const auto& data : KuroEvade::EvadeSpellDatabase::Spells()) {
            if (_stricmp(data.ChampionName.c_str(), "AllChampions") == 0) {
                AddEvadeSpellMenuEntry(globalMenu, data);
            }
        }
    }

    void AddEvadeSpellMenuEntry(Menu* parent, const KuroEvade::EvadeSpellData& data) {
        if (!parent) {
            return;
        }

        const std::string key = KuroEvade::EvadeSpell::MenuKey(data);
        if (m_evadeSpellOptions.find(key) != m_evadeSpellOptions.end()) {
            return;
        }

        const std::string display = data.Name.empty() ? "Evade Spell" : data.Name;
        const std::string menuId = "evadespell_" + KuroEvade::SpellMenuKey::Sanitize(key);
        auto* spellMenu = parent->AddSubMenu(new Menu(menuId.c_str(), display.c_str()));

        EvadeSpellMenuOption option;
        option.Use = spellMenu->Add(new MenuBool("use", "Use Spell", data.IsEnabled));
        option.Danger = spellMenu->Add(new MenuList("danger", "Danger Level",
            { "Low", "Normal", "High", "Extreme" },
            std::clamp(data.DangerLevel - 1, 0, 3)));
        option.Mode = spellMenu->Add(new MenuList("mode", "Spell Mode",
            { "Undodgeable", "Activation Time", "Always" },
            std::clamp(data.IndexUseWhen, 0, 2)));
        m_evadeSpellOptions.emplace(key, option);
        ++m_evadeSpellMenuEntries;
    }

    void DestroyMenu() {
        if (!m_menu) {
            return;
        }

        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_enabledMenu = nullptr;
        m_dodgeKeyMenu = nullptr;
        m_dodgeDangerousMenu = nullptr;
        m_dodgeFowMenu = nullptr;
        m_dodgeCircularMenu = nullptr;
        m_dangerKeysEnabledMenu = nullptr;
        m_dangerKeyMenu = nullptr;
        m_dangerKey2Menu = nullptr;
        m_comboOnlyEnabledMenu = nullptr;
        m_comboKeyMenu = nullptr;
        m_dontDodgeEnabledMenu = nullptr;
        m_dontDodgeKeyMenu = nullptr;
        m_drawSpellsMenu = nullptr;
        m_useEvadeSpellsMenu = nullptr;
        m_preferEvadeSpellsMenu = nullptr;
        m_optimizationProfileMenu = nullptr;
        m_evadeModeMenu = nullptr;
        m_extremeEvadeMenu = nullptr;
        m_kurokamiPosMenu = nullptr;
        m_higherPrecisionMenu = nullptr;
        m_calculateWindupMenu = nullptr;
        m_checkCollisionMenu = nullptr;
        m_clickRemoveMenu = nullptr;
        m_preventTowerMenu = nullptr;
        m_preventEnemyMenu = nullptr;
        m_extraDelayMenu = nullptr;
        m_extraDistMenu = nullptr;
        m_extraSpellRadiusMenu = nullptr;
        m_extraEvadeDistanceMenu = nullptr;
        m_extraAvoidDistanceMenu = nullptr;
        m_minComfortZoneMenu = nullptr;
        m_reactionTimeMenu = nullptr;
        m_spellDetectionTimeMenu = nullptr;
        m_minHitTimeMenu = nullptr;
        m_dodgeIntervalMenu = nullptr;
        m_dodgeHpMenu = nullptr;
        m_spellActivationMenu = nullptr;
        m_fastActivationMenu = nullptr;
        m_rejectMinDistanceMenu = nullptr;
        m_spellColorMenu = nullptr;
        m_fillSkillshotsMenu = nullptr;
        m_drawIrrelevantMenu = nullptr;
        m_drawEvadeRouteMenu = nullptr;
        m_drawThreatLabelsMenu = nullptr;
        m_irrelevantOpacityMenu = nullptr;
        m_threatOpacityMenu = nullptr;
        m_irrelevantColorMenu = nullptr;
        m_pathThreatColorMenu = nullptr;
        m_interventionColorMenu = nullptr;
        m_routeColorMenu = nullptr;
        m_clickOnlyOnceMenu = nullptr;
        m_spellsMenu = nullptr;
        m_evadeSpellsMenu = nullptr;
        m_spellOptions.clear();
        m_spellMenuEntries = 0;
        m_evadeSpellOptions.clear();
        m_evadeSpellMenuEntries = 0;
        m_spellVisuals.clear();
        m_lastVisualUpdateTick = 0;
        m_visualInterventionState = false;
    }
};

} // namespace Plugins
