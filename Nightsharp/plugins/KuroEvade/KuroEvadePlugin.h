#pragma once

// Standalone NightSharp port of the supplied Evade project.
// Runtime logic lives in plugins/KuroEvade/Native as native C++.

#include "../IPlugin.h"
#include "../../Core/KuroCombatCoordinator.h"
#include "../../Core/CoreEvadeState.h"
#include "../../DebugLog.h"
#include "../../SDK/SDK.h"
#include "../../SDK/UI/IMenu/Menu.h"
#include "../../menu/ConfigStore.h"

#include "Native/KuroEvadeNative.h"

#include <Windows.h>
#include <algorithm>
#include <cfloat>
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

    std::string GetHeroCharacterName(const SDK::AIHeroClient& hero) const {
        if (!hero.IsValid()) return "";
        return hero.CharacterName();
    }

    void OnLoad() override {
        s_instance = this;
        KuroCombatCoordination::Coordinator::Reset();
        CoreEvadeState::SetSpellBlockPredicate(
            &KuroEvadePlugin::ShouldBlockCastStatic);
        CreateMenu();
        m_detector.Clear();

        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &KuroEvadePlugin::OnRawProcessSpellImmediateStatic);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::OnDoCast, &KuroEvadePlugin::OnRawDoCastImmediateStatic);
        SDK::Events::AddOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &KuroEvadePlugin::OnRawProcessCastSpellImmediateStatic);
        SDK::Events::AddOnCreateObject(&KuroEvadePlugin::OnObjectCreateStatic);
        SDK::Events::AddOnDeleteObject(&KuroEvadePlugin::OnObjectDeleteStatic);
        SDK::Events::AddOnMissileCreate(&KuroEvadePlugin::OnMissileCreateStatic);
        SDK::Events::AddOnMissileDelete(&KuroEvadePlugin::OnMissileDeleteStatic);
        SDK::Game::AddOnWndProc(&KuroEvadePlugin::OnWndProcStatic);
        SDK::Orbwalker::OnBeforeMove += &KuroEvadePlugin::OnBeforeMoveStatic;
        SDK::Game::OnUpdate += &KuroEvadePlugin::OnUpdateStatic;

        NightSharpDebug::Logf("[KuroEvade] loaded");
    }

    void OnUnload() override {
        SDK::Game::OnUpdate -= &KuroEvadePlugin::OnUpdateStatic;
        SDK::Orbwalker::OnBeforeMove -= &KuroEvadePlugin::OnBeforeMoveStatic;
        SDK::Game::RemoveOnWndProc(&KuroEvadePlugin::OnWndProcStatic);
        SDK::Events::RemoveOnMissileDelete(&KuroEvadePlugin::OnMissileDeleteStatic);
        SDK::Events::RemoveOnMissileCreate(&KuroEvadePlugin::OnMissileCreateStatic);
        SDK::Events::RemoveOnDeleteObject(&KuroEvadePlugin::OnObjectDeleteStatic);
        SDK::Events::RemoveOnCreateObject(&KuroEvadePlugin::OnObjectCreateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::ProcessCastSpell, &KuroEvadePlugin::OnRawProcessCastSpellImmediateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnDoCast, &KuroEvadePlugin::OnRawDoCastImmediateStatic);
        SDK::Events::RemoveOnCoreHook(SDK::Events::Hooks::OnProcessSpell, &KuroEvadePlugin::OnRawProcessSpellImmediateStatic);

        m_evade.Shutdown();
        m_detector.Clear();
        m_benchmark.Stop();
        m_spellVisuals.clear();
        m_evadeIntervening = false;
        m_loadedChampions.clear();
        DestroyMenu();
        KuroEvade::SourceEvadeEngine::RestoreOrbwalkerMove();
        KuroCombatCoordination::Coordinator::Reset();
        CoreEvadeState::SetStrictEvadeActive(false);
        CoreEvadeState::SetSpellBlockPredicate(nullptr);
        if (s_instance == this) {
            s_instance = nullptr;
        }

        NightSharpDebug::Logf("[KuroEvade] unloaded");
    }

    void OnRender() override {
        if (!DrawSpells() || !ImGui::GetCurrentContext()) {
            return;
        }

        KuroEvade::SpellDrawer::Draw(
            m_detector.Skillshots(), m_spellVisuals, DrawingStyleSnapshot());
        if (Enabled() && DrawEvadeRoute() && m_evadeIntervening && m_evade.HasMoveTarget()) {
            const auto player = GameObjects::Player();
            if (player.IsValid()) {
                KuroEvade::SpellDrawer::DrawRoute(
                    player.Position().To2D(),
                    m_evade.MoveTarget(),
                    RouteColor(),
                    m_evade.IsWaitingForWindup());
            }
        }

        if ((m_showEvadeStatusMenu && m_showEvadeStatusMenu->Value) ||
            (m_drawWarningMessageMenu && m_drawWarningMessageMenu->Value &&
             m_evadeIntervening)) {
            ImDrawList* draw = ImGui::GetForegroundDrawList();
            const ImVec2 base(24.0f, 120.0f);
            if (m_showEvadeStatusMenu && m_showEvadeStatusMenu->Value) {
                draw->AddText(base, Enabled() ? IM_COL32(90, 255, 120, 255)
                                              : IM_COL32(255, 90, 90, 255),
                              Enabled() ? "Evade: On" : "Evade: Off");
            }
            if (m_drawWarningMessageMenu && m_drawWarningMessageMenu->Value &&
                m_evadeIntervening) {
                draw->AddText(ImVec2(base.x, base.y + 22.0f),
                              IM_COL32(255, 210, 80, 255),
                              "Right click is blocked while evading");
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
        ImGui::Text("Recommended config: %s", RecommendedConfigName());
        ImGui::Text("Visual threats: %d", RelevantVisualCount());
        ImGui::Text("Orbwalker sync: %s",
                    KuroCombatCoordination::Coordinator::PhaseName(
                        KuroCombatCoordination::Coordinator::Phase(
                            SDK::Variables::TickCount())));
        if (m_evade.IsHoldingPosition()) {
            ImGui::Text("Safe hold: movement locked, orbwalker attack enabled");
        } else if (m_evade.IsWaitingForWindup()) {
            ImGui::Text("Windup hold: %d ms (route verified)",
                        m_evade.WindupRemainingMs());
        }
        const auto& livePlan = m_evade.LastPlan();
        if (livePlan.GeneratedCandidateCount > 0) {
            ImGui::Text("Planner: %d generated | %d gradient steps",
                        livePlan.GeneratedCandidateCount,
                        livePlan.GradientSteps);
            if (livePlan.Found) {
                ImGui::Text("Clearance: spell %.1f | wall %.1f",
                            livePlan.Best.Clearance,
                            livePlan.Best.WallClearance);
                if (livePlan.Best.OuterRingExits > 0 ||
                    livePlan.Best.InnerRingShelters > 0) {
                    ImGui::Text("Ring route: %s",
                        livePlan.Best.OuterRingExits > 0
                            ? "outer exit preferred"
                            : "inner pocket fallback");
                }
            }
            if (livePlan.HasCandidate) {
                ImGui::Text("Planned remaining threats: %d%s",
                            livePlan.Best.PathThreatCount,
                            livePlan.Found ? " (full evade)" : " (partial)");
            }
        }
        ImGui::Text("Last dodge: %s", m_lastEvent);
        if (m_benchmarkResult.Iterations > 0) {
            ImGui::Separator();
            ImGui::Text("Benchmark: %d iterations | %d plans",
                        m_benchmarkResult.Iterations,
                        m_benchmarkResult.PlansFound);
            ImGui::Text("Planner us: avg %.2f | min %.2f | max %.2f",
                        m_benchmarkResult.AverageMicroseconds,
                        m_benchmarkResult.MinimumMicroseconds,
                        m_benchmarkResult.MaximumMicroseconds);
            ImGui::Text("Last candidates: %d ranked / %d generated",
                        m_benchmarkResult.LastCandidateCount,
                        m_benchmarkResult.LastGeneratedCandidateCount);
            ImGui::Text("Coverage: %d -> %d threats | improved %d/%d",
                        m_benchmarkResult.LastBaselineThreats,
                        m_benchmarkResult.LastRemainingThreats,
                        m_benchmarkResult.CoverageImprovingPlans,
                        m_benchmarkResult.Iterations);
            ImGui::Text("Gradient steps: avg %.2f | last %d | wall %.1f",
                        m_benchmarkResult.AverageGradientSteps,
                        m_benchmarkResult.LastGradientSteps,
                        m_benchmarkResult.LastWallClearance);
            if (m_benchmarkResult.LastOuterRingExits > 0 ||
                m_benchmarkResult.LastInnerRingShelters > 0) {
                ImGui::Text("Ring benchmark: outer %d | inner fallback %d",
                            m_benchmarkResult.LastOuterRingExits,
                            m_benchmarkResult.LastInnerRingShelters);
            }
            ImGui::Text("Database: %d skillshots + %d evade spells | invalid %d",
                        m_benchmarkResult.SkillshotDatabaseEntries,
                        m_benchmarkResult.EvadeSpellDatabaseEntries,
                        m_benchmarkResult.InvalidDatabaseEntries);
            ImGui::Text("Collision DB: %d profiles | %d multi-hit | %d continuation | %d wall",
                        m_benchmarkResult.CollisionProfileEntries,
                        m_benchmarkResult.MultiHitCollisionEntries,
                        m_benchmarkResult.ContinuationCollisionEntries,
                        m_benchmarkResult.ProjectileWallEntries);
            ImGui::Text("Secondary areas: %d explosions | %d bounce | %d special",
                        m_benchmarkResult.EndExplosionEntries,
                        m_benchmarkResult.BouncingExplosionEntries,
                        m_benchmarkResult.SpecialGeometryEntries);
            ImGui::Text("Collision regression: %d/%d passed",
                        m_benchmarkResult.CollisionRegressionPassed,
                        m_benchmarkResult.CollisionRegressionChecks);
            ImGui::Text("Dynamic caster regression: %d/%d passed",
                        m_benchmarkResult.DynamicCasterRegressionPassed,
                        m_benchmarkResult.DynamicCasterRegressionChecks);
        }

    }

private:
    static inline KuroEvadePlugin* s_instance = nullptr;

    Menu* m_menu = nullptr;
    MenuBool* m_focusOnEvadeMenu = nullptr;
    MenuBool* m_onlyEvadeWhileCanMenu = nullptr;
    MenuBool* m_enhanceDetectMenu = nullptr;
    MenuList* m_recommendedConfigMenu = nullptr;
    MenuSlider* m_skillshotsExtraRadiusMenu = nullptr;
    MenuSlider* m_skillshotsExtraRangeMenu = nullptr;
    MenuSlider* m_extraEvadeDistanceMenu = nullptr;
    // Config.cs keeps these source controls instantiated but commented out of
    // the visible menu. Presets still update their backing values.
    int m_gridSize = 10;
    int m_pathFindingDistance = 300;
    int m_pathFindingDistance2 = 100;
    int m_diagonalEvadePointsCount = 7;
    int m_diagonalEvadePointsStep = 20;
    int m_crossingTimeOffset = 250;
    int m_evadingFirstTimeOffset = 250;
    int m_evadingSecondTimeOffset = 80;
    int m_evadingRouteChangeTimeOffset = 250;
    int m_evadePointChangeInterval = 250;

    MenuBool* m_smoothEvadeSpellMenu = nullptr;
    MenuBool* m_improveMoveMenu = nullptr;
    MenuBool* m_useCurrentPathMenu = nullptr;
    MenuBool* m_lowEvadeSmoothMenu = nullptr;

    MenuBool* m_spellBlockerQMenu = nullptr;
    MenuBool* m_spellBlockerWMenu = nullptr;
    MenuBool* m_spellBlockerEMenu = nullptr;
    MenuBool* m_spellBlockerRMenu = nullptr;

    MenuBool* m_minionCollisionMenu = nullptr;
    MenuBool* m_heroCollisionMenu = nullptr;
    MenuBool* m_yasuoCollisionMenu = nullptr;
    MenuBool* m_enableCollisionMenu = nullptr;

    MenuColor* m_enabledColorMenu = nullptr;
    MenuColor* m_disabledColorMenu = nullptr;
    MenuColor* m_missileColorMenu = nullptr;
    MenuSlider* m_borderMenu = nullptr;
    MenuBool* m_enableDrawingsMenu = nullptr;
    MenuBool* m_drawWarningMessageMenu = nullptr;

    MenuList* m_blockSpellsMenu = nullptr;
    MenuSlider* m_allowAaLevelMenu = nullptr;
    MenuBool* m_disableFowMenu = nullptr;
    MenuBool* m_showEvadeStatusMenu = nullptr;
    MenuBool* m_disableOlafRMenu = nullptr;

    MenuKeyBind* m_enabledMenu = nullptr;
    MenuKeyBind* m_onlyDangerousMenu = nullptr;
    MenuBool* m_devModeMenu = nullptr;
    MenuBool* m_devSameTeamMenu = nullptr;
    MenuKeyBind* m_benchmarkLineKeyMenu = nullptr;
    MenuKeyBind* m_benchmarkCircleKeyMenu = nullptr;
    MenuKeyBind* m_benchmarkRunKeyMenu = nullptr;
    Menu* m_spellsMenu = nullptr;
    Menu* m_evadeSpellsMenu = nullptr;
    Menu* m_allyShieldMenu = nullptr;
    KuroEvade::Benchmarking::Benchmark m_benchmark;
    KuroEvade::Benchmarking::BenchmarkResult m_benchmarkResult;

    struct SpellMenuOption {
        MenuSlider* Danger = nullptr;
        MenuBool* Dangerous = nullptr;
        MenuBool* Draw = nullptr;
        MenuBool* Enabled = nullptr;
    };

    std::unordered_map<std::string, SpellMenuOption> m_spellOptions;
    int m_spellMenuEntries = 0;

    struct EvadeSpellMenuOption {
        MenuSlider* Danger = nullptr;
        MenuBool* WardJump = nullptr;
        MenuBool* Enabled = nullptr;
    };

    std::unordered_map<std::string, EvadeSpellMenuOption> m_evadeSpellOptions;
    int m_evadeSpellMenuEntries = 0;
    std::unordered_map<std::uint32_t, MenuBool*> m_allyShieldOptions;

    char m_lastEvent[96] = "none";
    int m_lastDodgeTick = 0;
    KuroEvade::SourceSkillshotDetector m_detector;
    KuroEvade::SourceEvadeEngine m_evade;
    KuroEvade::SpellDrawer::VisualMap m_spellVisuals;
    bool m_evadeIntervening = false;
    std::unordered_set<std::string> m_loadedChampions;
    bool m_visualInterventionState = false;
    bool m_currentDangerousThreat = false;
    int m_currentDangerLevel = 0;
    int m_lastVisualUpdateTick = 0;

    static inline uintptr_t s_lastImmediateProcessCastInfo = 0;
    static inline int s_lastImmediateProcessTick = 0;

    static bool ShouldBlockCastStatic(int rawSlot) {
        if (!s_instance || !s_instance->Enabled() ||
            !s_instance->m_evadeIntervening) {
            return false;
        }
        const int blockMode = s_instance->m_blockSpellsMenu
            ? s_instance->m_blockSpellsMenu->Index
            : 1;
        if (blockMode == 0 ||
            (blockMode == 1 && !s_instance->m_currentDangerousThreat)) {
            return false;
        }
        switch (static_cast<SDK::SpellSlot>(rawSlot)) {
        case SDK::SpellSlot::Q:
            return s_instance->m_spellBlockerQMenu && s_instance->m_spellBlockerQMenu->Value;
        case SDK::SpellSlot::W:
            return s_instance->m_spellBlockerWMenu && s_instance->m_spellBlockerWMenu->Value;
        case SDK::SpellSlot::E:
            return s_instance->m_spellBlockerEMenu && s_instance->m_spellBlockerEMenu->Value;
        case SDK::SpellSlot::R:
            return s_instance->m_spellBlockerRMenu && s_instance->m_spellBlockerRMenu->Value;
        default:
            return false;
        }
    }

    static void OnRawProcessSpellImmediateStatic(const SDK::Events::CoreHookArgs& raw) {
        if (!s_instance || raw.Id != SDK::Events::Hooks::OnProcessSpell) return;
        auto decoded = ::Core::Events::DecodeProcessSpell(raw);
        s_lastImmediateProcessCastInfo = decoded.CastInfo;
        s_lastImmediateProcessTick = SDK::Variables::TickCount();
        s_instance->m_detector.OnProcessSpell(decoded);
    }

    static void OnRawDoCastImmediateStatic(const SDK::Events::CoreHookArgs& raw) {
        if (!s_instance || raw.Id != SDK::Events::Hooks::OnDoCast) return;
        auto decoded = ::Core::Events::DecodeDoCast(raw);
        if (decoded.CastInfo && decoded.CastInfo == s_lastImmediateProcessCastInfo &&
            SDK::Variables::TickCount() - s_lastImmediateProcessTick <= 50) {
            return;
        }
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

    static void OnBeforeMoveStatic(SDK::OrbwalkingActionArgs& args) {
        if (!s_instance || !args.Process) {
            return;
        }
        const auto settings = s_instance->SettingsSnapshot();
        if (s_instance->m_evade.ShouldBlockMove(
                args.Position.To2D(), settings,
                s_instance->m_detector.Skillshots())) {
            args.Process = false;
        }
    }

    static void OnWndProcStatic(SDK::Game::WndEventArgs& args) {
        if (!s_instance) {
            return;
        }

        if (args.Msg == WM_RBUTTONDOWN || args.Msg == WM_RBUTTONUP || args.Msg == WM_RBUTTONDBLCLK) {
            if (s_instance->Enabled() && s_instance->m_evadeIntervening) {
                args.Process = false;
                return;
            }

            if (args.Msg == WM_RBUTTONDOWN || args.Msg == WM_RBUTTONDBLCLK) {
                const auto player = GameObjects::Player();
                if (player.IsValid() && !player.IsDead()) {
                    const auto settings = s_instance->SettingsSnapshot();
                    if (settings.Enabled) {
                        const Vec2 to = SDK::Game::CursorPos().To2D();
                        if (s_instance->m_evade.ShouldBlockMove(
                                to, settings,
                                s_instance->m_detector.Skillshots())) {
                            args.Process = false;
                            return;
                        }
                    }
                }
            }
        }

        if (s_instance->DevMode() && args.Msg == WM_KEYDOWN &&
            (args.LParam & (1u << 30)) == 0) {
            if (s_instance->m_benchmarkLineKeyMenu &&
                args.WParam == s_instance->m_benchmarkLineKeyMenu->Key) {
                s_instance->SpawnBenchmarkSkillshot(false);
                return;
            }
            if (s_instance->m_benchmarkCircleKeyMenu &&
                args.WParam == s_instance->m_benchmarkCircleKeyMenu->Key) {
                s_instance->SpawnBenchmarkSkillshot(true);
                return;
            }
            if (s_instance->m_benchmarkRunKeyMenu &&
                args.WParam == s_instance->m_benchmarkRunKeyMenu->Key) {
                s_instance->RunPlannerBenchmark();
                return;
            }
        }

        if (s_instance->DevMode() && args.Msg == WM_LBUTTONDOWN) {
            s_instance->m_benchmark.CaptureStart(
                SDK::Game::CursorPos().To2D());
        } else if (s_instance->DevMode() && args.Msg == WM_LBUTTONUP) {
            s_instance->m_benchmark.CaptureEnd(
                SDK::Game::CursorPos().To2D());
        }

    }

    bool Enabled() const { return m_enabledMenu && m_enabledMenu->Active; }
    bool DrawSpells() const { return m_enableDrawingsMenu && m_enableDrawingsMenu->Value; }
    bool DevMode() const { return m_devModeMenu && m_devModeMenu->Value; }
    bool SameTeam() const { return m_devSameTeamMenu && m_devSameTeamMenu->Value; }
    bool DrawEvadeRoute() const { return DrawSpells(); }
    ImU32 EnabledColor() const { return m_enabledColorMenu ? m_enabledColorMenu->GetImU32() : IM_COL32_WHITE; }
    ImU32 DisabledColor() const { return m_disabledColorMenu ? m_disabledColorMenu->GetImU32() : IM_COL32(255, 0, 0, 255); }
    ImU32 MissileColor() const { return m_missileColorMenu ? m_missileColorMenu->GetImU32() : IM_COL32(50, 205, 50, 255); }
    ImU32 RouteColor() const { return MissileColor(); }

    const char* RecommendedConfigName() const {
        switch (m_recommendedConfigMenu ? m_recommendedConfigMenu->Index : 0) {
        case 1: return "Configs 2";
        case 2: return "Configs 3";
        default: return "Configs 1";
        }
    }

    KuroEvade::SpellDrawStyle DrawingStyleSnapshot() const {
        KuroEvade::SpellDrawStyle style;
        style.Fill = true;
        style.DrawIrrelevant = true;
        style.DrawLabels = false;
        style.IrrelevantOpacity = 15;
        style.ThreatOpacity = 70;
        style.IrrelevantColor = DisabledColor();
        style.PathColor = EnabledColor();
        style.DirectColor = EnabledColor();
        style.InterventionColor = EnabledColor();
        style.RouteColor = RouteColor();
        style.BorderWidth = m_borderMenu ? m_borderMenu->Value : 2;
        style.MissileColor = MissileColor();
        return style;
    }

    int RelevantVisualCount() const {
        return static_cast<int>(std::count_if(
            m_spellVisuals.begin(), m_spellVisuals.end(),
            [](const auto& entry) {
                return entry.second.State != KuroEvade::SpellVisualState::Irrelevant;
            }));
    }

    int VisualRefreshInterval() const { return 16; }

    KuroEvade::EvadeSettings SettingsSnapshot() const {
        KuroEvade::EvadeSettings settings;
        settings.Enabled = Enabled();
        settings.OnlyDangerous = m_onlyDangerousMenu && m_onlyDangerousMenu->Active;
        settings.FocusOnEvade = !m_focusOnEvadeMenu || m_focusOnEvadeMenu->Value;
        settings.OnlyEvadeWhileCan = m_onlyEvadeWhileCanMenu && m_onlyEvadeWhileCanMenu->Value;
        settings.EnhanceDetect = !m_enhanceDetectMenu || m_enhanceDetectMenu->Value;
        settings.SmoothEvadeSpell = !m_smoothEvadeSpellMenu || m_smoothEvadeSpellMenu->Value;
        settings.ImproveMove = !m_improveMoveMenu || m_improveMoveMenu->Value;
        settings.LowEvadeSmooth = m_lowEvadeSmoothMenu && m_lowEvadeSmoothMenu->Value;
        settings.UseCurrentPath = !m_useCurrentPathMenu || m_useCurrentPathMenu->Value;
        settings.TestOnAllies = SameTeam();
        settings.EnableCollision = m_enableCollisionMenu && m_enableCollisionMenu->Value;
        settings.MinionCollision = m_minionCollisionMenu && m_minionCollisionMenu->Value;
        settings.HeroCollision = m_heroCollisionMenu && m_heroCollisionMenu->Value;
        settings.YasuoCollision = !m_yasuoCollisionMenu || m_yasuoCollisionMenu->Value;
        settings.EnableDrawings = DrawSpells();
        settings.DrawWarningMessage = !m_drawWarningMessageMenu || m_drawWarningMessageMenu->Value;
        settings.ShowEvadeStatus = m_showEvadeStatusMenu && m_showEvadeStatusMenu->Value;
        settings.DisableFow = m_disableFowMenu && m_disableFowMenu->Value;
        settings.DisableEvadeForOlafR = !m_disableOlafRMenu || m_disableOlafRMenu->Value;
        settings.BlockSpells = m_blockSpellsMenu ? m_blockSpellsMenu->Index : 1;
        settings.AllowAutoAttackDangerLevel = m_allowAaLevelMenu ? m_allowAaLevelMenu->Value : 4;
        settings.BorderWidth = m_borderMenu ? m_borderMenu->Value : 2;
        settings.SkillShotsExtraRadius = m_skillshotsExtraRadiusMenu ? m_skillshotsExtraRadiusMenu->Value : 0;
        settings.SkillShotsExtraRange = m_skillshotsExtraRangeMenu ? m_skillshotsExtraRangeMenu->Value : 0;
        settings.GridSize = m_gridSize;
        settings.ExtraEvadeDistance = m_extraEvadeDistanceMenu ? m_extraEvadeDistanceMenu->Value : 25;
        settings.PathFindingDistance = m_pathFindingDistance;
        settings.PathFindingDistance2 = m_pathFindingDistance2;
        settings.DiagonalEvadePointsCount = m_diagonalEvadePointsCount;
        settings.DiagonalEvadePointsStep = m_diagonalEvadePointsStep;
        settings.CrossingTimeOffset = m_crossingTimeOffset;
        settings.EvadingFirstTimeOffset = m_evadingFirstTimeOffset;
        settings.EvadingSecondTimeOffset = m_evadingSecondTimeOffset;
        settings.EvadingRouteChangeTimeOffset = m_evadingRouteChangeTimeOffset;
        settings.EvadePointChangeInterval = m_evadePointChangeInterval;
        settings.EnabledColor = EnabledColor();
        settings.DisabledColor = DisabledColor();
        settings.MissileColor = MissileColor();
        return settings;
    }

    void UpdateSpellVisuals(const KuroEvade::EvadeSettings& settings,
                            const std::vector<Vec2>& observedPath,
                            bool intervention) {
        if (!settings.EnableDrawings) {
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

        const auto player = GameObjects::Player();
        if (!player.IsValid() || player.IsDead()) {
            return;
        }

        KuroEvade::EvadeSettings visualSettings = settings;

        const Vec2 heroPos = player.Position().To2D();
        const float boundingRadius = player.BoundingRadius();
        const float moveSpeed = std::max(50.0f, player.MoveSpeed());
        const float directWindow = 1200.0f;

        for (const auto& skillshot : m_detector.Skillshots()) {
            if (!skillshot || !ShouldDrawSpell(skillshot->SpellData())) {
                continue;
            }

            KuroEvade::SpellVisualInfo info;
            info.Danger = KuroEvade::SourceEvader::DangerValue(*skillshot);
            info.HitTimeMs = skillshot->HitTime(heroPos, visualSettings);

            bool directThreat = false;
            bool pathThreat = false;
            if (settings.Enabled && KuroEvade::SourceEvader::ShouldConsider(
                    skillshot, visualSettings)) {
                directThreat =
                    info.HitTimeMs <= directWindow &&
                    skillshot->ContainsStatic(
                        heroPos, boundingRadius, visualSettings);
                if (observedPath.size() >= 2) {
                    pathThreat = !skillshot->IsSafePath(
                        observedPath, 250, moveSpeed,
                        0,
                        boundingRadius, visualSettings).IsSafe;
                }
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
        bool needRebuild = false;

        const auto player = GameObjects::Player();
        if (player.IsValid()) {
            const std::string selfName = GetHeroCharacterName(player);
            if (!selfName.empty()) {
                const std::string selfKey = KuroEvade::SpellMenuKey::Lower(selfName);
                if (m_loadedChampions.find("self_" + selfKey) == m_loadedChampions.end()) {
                    m_loadedChampions.insert("self_" + selfKey);
                    needRebuild = true;
                }
            }
        }

        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid()) {
                continue;
            }
            const std::string enemyName = GetHeroCharacterName(enemy);
            if (!enemyName.empty()) {
                const std::string enemyKey = KuroEvade::SpellMenuKey::Lower(enemyName);
                if (m_loadedChampions.find("enemy_" + enemyKey) == m_loadedChampions.end()) {
                    m_loadedChampions.insert("enemy_" + enemyKey);
                    needRebuild = true;
                }
            }
        }

        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsMe()) {
                continue;
            }
            const std::string allyName = GetHeroCharacterName(ally);
            if (!allyName.empty()) {
                const std::string allyKey =
                    KuroEvade::SpellMenuKey::Lower(allyName);
                if (m_loadedChampions.find("ally_" + allyKey) ==
                    m_loadedChampions.end()) {
                    m_loadedChampions.insert("ally_" + allyKey);
                    needRebuild = true;
                }
            }
        }

        if (needRebuild) {
            RebuildSpellsMenu();
            RebuildEvadeSpellsMenu();
            RebuildAllyShieldMenu();
            ::ConfigStore::ApplyLoaded(m_menu);
        }

        const auto settings = SettingsSnapshot();
        m_detector.SetCollisionEnabled(settings.EnableCollision);
        m_detector.SetCollisionTypes(settings.MinionCollision,
                                     settings.HeroCollision,
                                     settings.YasuoCollision);
        m_detector.SetFowEnabled(!settings.DisableFow);
        m_detector.SetEnhancedDetection(settings.EnhanceDetect);
        m_detector.SetDevSameTeam(settings.TestOnAllies);
        if (DevMode()) {
            m_benchmark.Update(m_detector);
        } else {
            m_benchmark.Stop();
        }
        m_detector.Update();
        ApplySpellMenuValues();
        m_currentDangerousThreat = false;
        m_currentDangerLevel = 0;
        if (player.IsValid()) {
            const Vec2 position = player.Position().To2D();
            for (const auto& skillshot : m_detector.Skillshots()) {
                if (!skillshot ||
                    !KuroEvade::SourceEvader::ShouldConsider(skillshot, settings) ||
                    !skillshot->ContainsStatic(
                        position, player.BoundingRadius(), settings)) {
                    continue;
                }
                m_currentDangerLevel = std::max(
                    m_currentDangerLevel, skillshot->Data.DangerValue);
                m_currentDangerousThreat = m_currentDangerousThreat ||
                    skillshot->Data.IsDangerous;
            }
        }
        std::vector<Vec2> observedPath;
        if (player.IsValid()) {
            observedPath.push_back(player.Position().To2D());
            for (const Vec3& point : player.Path()) {
                const Vec2 value = point.To2D();
                if (!value.IsZero() &&
                    observedPath.back().DistanceSqr(value) > 4.0f) {
                    observedPath.push_back(value);
                }
            }
            if (observedPath.size() == 1) {
                const Vec2 cursor = SDK::Game::CursorPos().To2D();
                if (!cursor.IsZero()) {
                    observedPath.push_back(cursor);
                }
            }
        }
        m_evadeIntervening = m_evade.Tick(
            settings, m_detector.Skillshots(), m_lastDodgeTick,
            [this](const KuroEvade::Database::EvadeSpellData& data) {
                return ResolveEvadeSpellConfig(data);
            },
            [this](const SDK::AIBaseClient& ally) {
                const auto it = m_allyShieldOptions.find(ally.NetworkId());
                return it == m_allyShieldOptions.end() ||
                    !it->second || it->second->Value;
            },
            m_lastEvent, sizeof(m_lastEvent));
        const auto phase = !m_evadeIntervening
            ? KuroCombatCoordination::EvadePhase::Idle
            : m_evade.IsHoldingPosition()
                ? KuroCombatCoordination::EvadePhase::SafePositionHold
                : m_evade.IsWaitingForWindup()
                ? KuroCombatCoordination::EvadePhase::WindupHold
                : m_evade.IsMovementBlocking()
                    ? KuroCombatCoordination::EvadePhase::PathRecovery
                    : KuroCombatCoordination::EvadePhase::Dodging;
        KuroCombatCoordination::Coordinator::Publish(
            phase, SDK::Variables::TickCount());
        const bool lucianPassive = player.IsValid() &&
            player.HasBuff("LucianPassiveBuff");
        const bool blockAttacks = m_evadeIntervening && !lucianPassive &&
            m_currentDangerLevel > settings.AllowAutoAttackDangerLevel;
        CoreEvadeState::SetEvadeInterventionState(
            m_evadeIntervening, blockAttacks);
        UpdateSpellVisuals(settings, observedPath, m_evadeIntervening);
    }

    bool IsSpellDodgeEnabled(const KuroEvade::Database::SpellData& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        if (it == m_spellOptions.end()) {
            return !data.DisabledByDefault;
        }
        return !it->second.Enabled || it->second.Enabled->Value;
    }

    bool IsSpellDodgeEnabled(const SDK::SpellDatabaseEntry& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it == m_spellOptions.end() || !it->second.Enabled || it->second.Enabled->Value;
    }

    bool ShouldDrawSpell(const SDK::SpellDatabaseEntry& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it == m_spellOptions.end() || !it->second.Draw || it->second.Draw->Value;
    }

    int SpellDangerLevel(const KuroEvade::Database::SpellData& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it != m_spellOptions.end() && it->second.Danger
            ? it->second.Danger->Value
            : std::clamp(data.DangerValue, 1, 5);
    }

    bool IsSpellDangerous(const KuroEvade::Database::SpellData& data) const {
        const auto it = m_spellOptions.find(KuroEvade::SpellMenuKey::Key(data));
        return it != m_spellOptions.end() && it->second.Dangerous
            ? it->second.Dangerous->Value
            : data.IsDangerous || data.DangerValue >= 3;
    }

    void ApplySpellMenuValues() {
        for (const auto& skillshot : m_detector.Skillshots()) {
            if (!skillshot || !skillshot->Native) continue;
            skillshot->Data.DangerValue = SpellDangerLevel(skillshot->Data);
            skillshot->Data.IsDangerous = IsSpellDangerous(skillshot->Data);
            // Detection and drawing remain alive for a disabled evade entry;
            // only the dodge decision ignores it, matching Skillshot.Evade().
            skillshot->ForceDisabled = !IsSpellDodgeEnabled(skillshot->Data);
            skillshot->Data.Runtime.DangerValue = skillshot->Data.DangerValue;
            skillshot->Native->SData.DangerValue = skillshot->Data.DangerValue;
        }
    }

    void SpawnBenchmarkSkillshot(bool circle) {
        const bool spawned = circle
            ? m_benchmark.StartCircle(m_detector)
            : m_benchmark.StartLine(m_detector);
        std::snprintf(m_lastEvent, sizeof(m_lastEvent),
                      spawned ? "benchmark %s spawned" : "benchmark spawn failed",
                      circle ? "circle" : "line");
    }

    void RunPlannerBenchmark() {
        const auto player = GameObjects::Player();
        m_benchmarkResult = m_benchmark.Run(
            player, m_detector.Skillshots(), SettingsSnapshot(), 100);
        std::snprintf(m_lastEvent, sizeof(m_lastEvent),
                      "benchmark avg %.2fus (%d/%d plans)",
                      m_benchmarkResult.AverageMicroseconds,
                      m_benchmarkResult.PlansFound,
                      m_benchmarkResult.Iterations);
        NightSharpDebug::Logf(
            "[KuroEvade][Benchmark] iterations=%d plans=%d candidates=%d avg=%.2fus min=%.2fus max=%.2fus db=%d+%d invalid=%d collision=%d/%d dynamic=%d/%d profiles=%d multi=%d continuation=%d wall=%d bounce=%d explosions=%d",
            m_benchmarkResult.Iterations,
            m_benchmarkResult.PlansFound,
            m_benchmarkResult.LastCandidateCount,
            m_benchmarkResult.AverageMicroseconds,
            m_benchmarkResult.MinimumMicroseconds,
            m_benchmarkResult.MaximumMicroseconds,
            m_benchmarkResult.SkillshotDatabaseEntries,
            m_benchmarkResult.EvadeSpellDatabaseEntries,
            m_benchmarkResult.InvalidDatabaseEntries,
            m_benchmarkResult.CollisionRegressionPassed,
            m_benchmarkResult.CollisionRegressionChecks,
            m_benchmarkResult.DynamicCasterRegressionPassed,
            m_benchmarkResult.DynamicCasterRegressionChecks,
            m_benchmarkResult.CollisionProfileEntries,
            m_benchmarkResult.MultiHitCollisionEntries,
            m_benchmarkResult.ContinuationCollisionEntries,
            m_benchmarkResult.ProjectileWallEntries,
            m_benchmarkResult.BouncingExplosionEntries,
            m_benchmarkResult.EndExplosionEntries);
    }

    static void OnRecommendedConfigChangedStatic(MenuItem* sender, void* userData) {
        auto* self = static_cast<KuroEvadePlugin*>(userData);
        auto* list = sender ? sender->As<MenuList>() : nullptr;
        if (self && list) {
            self->ApplyRecommendedConfig(list->Index);
        }
    }

    static void OnEnabledChangedStatic(MenuItem* sender, void* userData) {
        auto* self = static_cast<KuroEvadePlugin*>(userData);
        auto* key = sender ? sender->As<MenuKeyBind>() : nullptr;
        if (self && key && !key->Active) {
            self->ReleaseEvadeIntervention();
        }
    }

    void ReleaseEvadeIntervention() {
        m_evade.Shutdown();
        m_evadeIntervening = false;
        m_currentDangerousThreat = false;
        m_currentDangerLevel = 0;
        KuroCombatCoordination::Coordinator::Publish(
            KuroCombatCoordination::EvadePhase::Idle,
            SDK::Variables::TickCount());
        CoreEvadeState::SetEvadeInterventionState(false, false);
    }

    void ApplyRecommendedConfig(int index) {
        const auto values = KuroEvade::EvadeConfig::Recommended(index);
        if (m_skillshotsExtraRadiusMenu) m_skillshotsExtraRadiusMenu->Set(values.SkillShotsExtraRadius);
        if (m_skillshotsExtraRangeMenu) m_skillshotsExtraRangeMenu->Set(values.SkillShotsExtraRange);
        m_gridSize = values.GridSize;
        if (m_extraEvadeDistanceMenu) m_extraEvadeDistanceMenu->Set(values.ExtraEvadeDistance);
        m_pathFindingDistance = values.PathFindingDistance;
        m_pathFindingDistance2 = values.PathFindingDistance2;
        m_diagonalEvadePointsCount = values.DiagonalEvadePointsCount;
        m_diagonalEvadePointsStep = values.DiagonalEvadePointsStep;
        m_crossingTimeOffset = values.CrossingTimeOffset;
        m_evadingFirstTimeOffset = values.EvadingFirstTimeOffset;
        m_evadingSecondTimeOffset = values.EvadingSecondTimeOffset;
        m_evadingRouteChangeTimeOffset = values.EvadingRouteChangeTimeOffset;
        m_evadePointChangeInterval = values.EvadePointChangeInterval;
    }

    void CreateMenu() {
        DestroyMenu();
        m_gridSize = 10;
        m_pathFindingDistance = 300;
        m_pathFindingDistance2 = 100;
        m_diagonalEvadePointsCount = 7;
        m_diagonalEvadePointsStep = 20;
        m_crossingTimeOffset = 250;
        m_evadingFirstTimeOffset = 250;
        m_evadingSecondTimeOffset = 80;
        m_evadingRouteChangeTimeOffset = 250;
        m_evadePointChangeInterval = 250;
        m_menu = new Menu(GetInternalId(), GetName(), true);

        m_focusOnEvadeMenu = m_menu->Add(new MenuBool("FocusOnEvade", "Focus on Evade", true));
        m_onlyEvadeWhileCanMenu = m_menu->Add(new MenuBool("OnlyEvadeWhileCan", "Only Evade if can", false));
        m_enhanceDetectMenu = m_menu->Add(new MenuBool("EnchanceDetect", "EnchanceDetect", true));

        auto* configs = m_menu->AddSubMenu(new Menu("Configs Evade", "Configs"));
        m_recommendedConfigMenu = configs->Add(new MenuList(
            "Recommand", "Recommand Configs", { "Configs 1", "Configs 2", "Configs 3" }, 0));
        m_recommendedConfigMenu->ValueChanged = &KuroEvadePlugin::OnRecommendedConfigChangedStatic;
        m_recommendedConfigMenu->ValueChangedUd = this;
        m_skillshotsExtraRadiusMenu = configs->Add(new MenuSlider(
            "SkillShotsExtraRadius", "SkillShots Extra Radius", 0, 0, 30));
        m_skillshotsExtraRangeMenu = configs->Add(new MenuSlider(
            "SkillShotsExtraRange", "SkillShots Extra Range", 0, 0, 50));
        m_extraEvadeDistanceMenu = configs->Add(new MenuSlider(
            "ExtraEvadeDistance", "Extra Evade Distance", 25, 0, 100));

        // Keep the same root ordering as Config.cs. The remaining preset
        // values are intentionally hidden there and live in backing fields.
        BuildEvadeSpellMenu();
        BuildSpellMenu();
        BuildAllyShieldMenu();

        auto* blocker = m_menu->AddSubMenu(new Menu("Spell Blocker", "Spell Blocker"));
        m_spellBlockerQMenu = blocker->Add(new MenuBool("spellBlockerQ", "Q",
            KuroEvade::Database::SpellBlocker::ShouldBlockForPlayer(static_cast<int>(SDK::SpellSlot::Q))));
        m_spellBlockerWMenu = blocker->Add(new MenuBool("spellBlockerW", "W",
            KuroEvade::Database::SpellBlocker::ShouldBlockForPlayer(static_cast<int>(SDK::SpellSlot::W))));
        m_spellBlockerEMenu = blocker->Add(new MenuBool("spellBlockerE", "E",
            KuroEvade::Database::SpellBlocker::ShouldBlockForPlayer(static_cast<int>(SDK::SpellSlot::E))));
        m_spellBlockerRMenu = blocker->Add(new MenuBool("spellBlockerR", "R",
            KuroEvade::Database::SpellBlocker::ShouldBlockForPlayer(static_cast<int>(SDK::SpellSlot::R))));

        auto* collision = m_menu->AddSubMenu(new Menu("Collision", "Collision"));
        m_minionCollisionMenu = collision->Add(new MenuBool("MinionCollision", "Minion collision", true));
        m_heroCollisionMenu = collision->Add(new MenuBool("HeroCollision", "Hero collision", true));
        m_yasuoCollisionMenu = collision->Add(new MenuBool(
            "YasuoCollision", "Projectile barriers (Yasuo/Samira/Mel)", true));
        m_enableCollisionMenu = collision->Add(new MenuBool("EnableCollision", "Enabled", true));

        auto* drawings = m_menu->AddSubMenu(new Menu("Drawings", "Drawings"));
        m_enabledColorMenu = drawings->Add(new MenuColor("EnabledColor", "Enabled spell color", 1.0f, 1.0f, 1.0f, 1.0f));
        m_disabledColorMenu = drawings->Add(new MenuColor("DisabledColor", "Disabled spell color", 1.0f, 0.0f, 0.0f, 1.0f));
        m_missileColorMenu = drawings->Add(new MenuColor("MissileColor", "Missile color", 0.20f, 0.80f, 0.20f, 1.0f));
        m_borderMenu = drawings->Add(new MenuSlider("Border", "Border Width", 2, 1, 10));
        m_enableDrawingsMenu = drawings->Add(new MenuBool("EnableDrawings", "Enabled", true));
        m_drawWarningMessageMenu = drawings->Add(new MenuBool(
            "DrawWarningMsg", "Draw Right Click warning msg", true));

        auto* misc = m_menu->AddSubMenu(new Menu("Misc", "Misc"));
        m_blockSpellsMenu = misc->Add(new MenuList("BlockSpells", "Block spells while evading",
            { "No", "Only dangerous", "Always" }, 1));
        m_allowAaLevelMenu = misc->Add(new MenuSlider(
            "AllowAaLevel", "Allow auto-attacks danger level", 4, 1, 5));
        m_disableFowMenu = misc->Add(new MenuBool(
            "DisableFow", "Disable fog of war dodging", false));
        m_showEvadeStatusMenu = misc->Add(new MenuBool(
            "ShowEvadeStatus", "Show Evade Status", false));
        const auto player = GameObjects::Player();
        if (player.IsValid() && _stricmp(GetHeroCharacterName(player).c_str(), "Olaf") == 0) {
            m_disableOlafRMenu = misc->Add(new MenuBool(
                "DisableEvadeForOlafR", "Automatic disable Evade when Olaf's ulti is active!", true));
        }

        auto* benchmark = m_menu->AddSubMenu(new Menu("Benchmarking", "Evade Bench"));
        m_devModeMenu = benchmark->Add(new MenuBool("devMode", "Enable Benchmark", false));
        m_devSameTeamMenu = benchmark->Add(new MenuBool("TestOnAllies", "Test On Allies", false));
        m_benchmarkLineKeyMenu = benchmark->Add(new MenuKeyBind(
            "KeyStartL", "Spawn Line (drag LMB first)", 'Z', KeyBindType::Press, false));
        m_benchmarkCircleKeyMenu = benchmark->Add(new MenuKeyBind(
            "KeyStartC", "Spawn Circle (drag LMB first)", 'X', KeyBindType::Press, false));
        m_benchmarkRunKeyMenu = benchmark->Add(new MenuKeyBind(
            "RunPlanner", "Run Planner Benchmark", 'B', KeyBindType::Press, false));

        m_enabledMenu = m_menu->Add(new MenuKeyBind(
            "Enabled", "Enabled", 'K', KeyBindType::Toggle, false));
        m_enabledMenu->ValueChanged = &KuroEvadePlugin::OnEnabledChangedStatic;
        m_enabledMenu->ValueChangedUd = this;
        m_enabledMenu->AddPermashow("Evade");
        m_onlyDangerousMenu = m_menu->Add(new MenuKeyBind(
            "OnlyDangerous", "Dodge only dangerous", 0,
            KeyBindType::Press, false));
        m_onlyDangerousMenu->AddPermashow();
        m_menu->Attach();
    }

    void BuildSpellMenu() {
        m_spellOptions.clear();
        m_spellMenuEntries = 0;
        m_spellsMenu = m_menu->AddSubMenu(new Menu("Skillshots", "Skill Shots"));

        PopulateSpellMenu(m_spellsMenu);
    }

    void PopulateSpellMenu(Menu* parent) {
        if (!parent) {
            return;
        }

        for (const auto& data : KuroEvade::Database::SpellDatabase::Spells()) {
            if (_stricmp(data.Runtime.ChampionName.c_str(), "AllChampions") == 0) {
                AddSpellMenuEntry(parent, data);
            }
        }

        std::unordered_set<std::string> enemyChampions;
        for (const auto& enemy : SDK::GameObjects::EnemyHeroes()) {
            if (!enemy.IsValid()) {
                continue;
            }
            const std::string championName = GetHeroCharacterName(enemy);
            if (!championName.empty()) {
                enemyChampions.insert(
                    KuroEvade::SpellMenuKey::Lower(championName));
            }
        }
        for (const std::string& loaded : m_loadedChampions) {
            if (loaded.rfind("enemy_", 0) == 0) {
                enemyChampions.insert(loaded.substr(6));
            }
        }

        for (const auto& data : KuroEvade::Database::SpellDatabase::Spells()) {
            const std::string champion = KuroEvade::SpellMenuKey::Lower(
                data.Runtime.ChampionName);
            if (enemyChampions.find(champion) != enemyChampions.end()) {
                AddSpellMenuEntry(parent, data);
            }
        }

        // Dynamic hostile casters use abilities whose source champion is on
        // the player's team: Sylas steals their R, Viego possesses their Q/W/E,
        // and Mel recreates reflected projectiles as missiles owned by herself.
        // Add those source entries to the existing per-spell menu instead of
        // silently falling back to unconfigurable defaults.
        const bool enemySylas = enemyChampions.contains("sylas");
        const bool enemyViego = enemyChampions.contains("viego");
        const bool enemyMel = enemyChampions.contains("mel");
        if (!enemySylas && !enemyViego && !enemyMel) {
            return;
        }

        std::unordered_set<std::string> allyChampions;
        const auto player = GameObjects::Player();
        if (player.IsValid()) {
            const std::string name = GetHeroCharacterName(player);
            if (!name.empty()) {
                allyChampions.insert(KuroEvade::SpellMenuKey::Lower(name));
            }
        }
        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid()) {
                continue;
            }
            const std::string name = GetHeroCharacterName(ally);
            if (!name.empty()) {
                allyChampions.insert(KuroEvade::SpellMenuKey::Lower(name));
            }
        }
        for (const std::string& loaded : m_loadedChampions) {
            if (loaded.rfind("ally_", 0) == 0) {
                allyChampions.insert(loaded.substr(5));
            } else if (loaded.rfind("self_", 0) == 0) {
                allyChampions.insert(loaded.substr(5));
            }
        }

        for (const auto& data : KuroEvade::Database::SpellDatabase::Spells()) {
            const std::string champion = KuroEvade::SpellMenuKey::Lower(
                data.Runtime.ChampionName);
            if (allyChampions.find(champion) == allyChampions.end()) {
                continue;
            }
            const bool stolenUltimate = enemySylas &&
                data.Runtime.Slot == SDK::SpellSlot::R;
            const bool possessedBasic = enemyViego &&
                data.Runtime.Slot >= SDK::SpellSlot::Q &&
                data.Runtime.Slot <= SDK::SpellSlot::E;
            const bool reflectedProjectile = enemyMel &&
                (!data.MissileSpellName.empty() ||
                 !data.ExtraMissileNames.empty());
            if (stolenUltimate || possessedBasic || reflectedProjectile) {
                AddSpellMenuEntry(parent, data);
            }
        }
    }

    void AddSpellMenuEntry(Menu* parent, const KuroEvade::Database::SpellData& data) {
        if (!parent || data.DontProcess) {
            return;
        }

        const std::string key = KuroEvade::SpellMenuKey::Key(data);
        if (m_spellOptions.find(key) != m_spellOptions.end()) {
            return;
        }

        const std::string display = KuroEvade::SpellMenuKey::DisplayName(data) +
            " [" + KuroEvade::SpellMenuKey::SlotName(data.Runtime.Slot) + "]";
        const std::string menuId = "spell_" + KuroEvade::SpellMenuKey::Sanitize(key);
        auto* spellMenu = parent->AddSubMenu(new Menu(menuId.c_str(), display.c_str()));

        SpellMenuOption option;
        option.Danger = spellMenu->Add(new MenuSlider(
            "DangerLevel", "Danger level", std::clamp(data.DangerValue, 1, 5), 1, 5));
        option.Dangerous = spellMenu->Add(new MenuBool(
            "IsDangerous", "Is Dangerous", data.IsDangerous || data.DangerValue >= 3));
        option.Draw = spellMenu->Add(new MenuBool("Draw", "Draw", true));
        option.Enabled = spellMenu->Add(new MenuBool(
            "Enabled", "Enabled", !data.DisabledByDefault));
        m_spellOptions.emplace(key, option);
        ++m_spellMenuEntries;
    }

    KuroEvade::EvadeSpellConfig ResolveEvadeSpellConfig(const KuroEvade::Database::EvadeSpellData& data) const {
        KuroEvade::EvadeSpellConfig config;
        config.Enabled = data.IsEnabled;
        config.DangerLevel = data.DangerLevel;

        const auto it = m_evadeSpellOptions.find(KuroEvade::SourceEvadeSpell::MenuKey(data));
        if (it == m_evadeSpellOptions.end()) {
            return config;
        }

        const EvadeSpellMenuOption& option = it->second;
        config.Enabled = !option.Enabled || option.Enabled->Value;
        config.DangerLevel = option.Danger ? option.Danger->Value : config.DangerLevel;
        config.WardJump = !option.WardJump || option.WardJump->Value;
        return config;
    }

    void BuildEvadeSpellMenu() {
        m_evadeSpellOptions.clear();
        m_evadeSpellMenuEntries = 0;
        m_evadeSpellsMenu = m_menu->AddSubMenu(new Menu("Evade spells", "Evade Spells"));
        m_smoothEvadeSpellMenu = m_evadeSpellsMenu->Add(new MenuBool(
            "SmoothEvadeSpell", "Use 'Smooth Evade Spell'", true));

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return;
        }

        const std::string championName = GetHeroCharacterName(player);
        bool first = true;
        for (const auto* data :
             KuroEvade::Database::EvadeSpellDatabase::ForChampion(
                 championName.c_str(), true)) {
            AddEvadeSpellMenuEntry(m_evadeSpellsMenu, *data, first);
            first = false;
        }
    }

    void AddEvadeSpellMenuEntry(
            Menu* parent,
            const KuroEvade::Database::EvadeSpellData& data,
            bool addMovementControls = false) {
        if (!parent) {
            return;
        }

        const std::string key = KuroEvade::SourceEvadeSpell::MenuKey(data);
        if (m_evadeSpellOptions.find(key) != m_evadeSpellOptions.end()) {
            return;
        }

        const std::string display = data.Name.empty() ? "Evade Spell" : data.Name;
        const std::string menuId = "evadespell_" + KuroEvade::SpellMenuKey::Sanitize(key);
        auto* spellMenu = parent->AddSubMenu(new Menu(menuId.c_str(), display.c_str()));

        EvadeSpellMenuOption option;
        if (addMovementControls) {
            m_improveMoveMenu = spellMenu->Add(new MenuBool(
                "ImproveMove", "Improve Move", true));
            m_useCurrentPathMenu = spellMenu->Add(new MenuBool(
                "UseCurrentPath", "Use Current Path", true));
            m_lowEvadeSmoothMenu = spellMenu->Add(new MenuBool(
                "LowEvadeSmooth", "Delay Walking for Humanity", false));
        }
        option.Danger = spellMenu->Add(new MenuSlider(
            "DangerLevel", "Danger level", std::clamp(data.DangerLevel, 1, 5), 1, 5));
        if (std::find(data.ValidTargets.begin(), data.ValidTargets.end(),
                      KuroEvade::SpellTargets::Targetables) != data.ValidTargets.end()) {
            option.WardJump = spellMenu->Add(new MenuBool(
                "WardJump", "Ward Jump", true));
        }
        option.Enabled = spellMenu->Add(new MenuBool(
            "Enabled", "Enabled", data.IsEnabled));
        m_evadeSpellOptions.emplace(key, option);
        ++m_evadeSpellMenuEntries;
    }

    void BuildAllyShieldMenu() {
        m_allyShieldOptions.clear();
        m_allyShieldMenu = m_menu->AddSubMenu(new Menu("Ally shielding", "Shield"));
        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsMe()) {
                continue;
            }
            const std::string name = GetHeroCharacterName(ally);
            const std::string id = "shield" + KuroEvade::SpellMenuKey::Sanitize(name) +
                std::to_string(ally.NetworkId());
            m_allyShieldOptions[ally.NetworkId()] = m_allyShieldMenu->Add(
                new MenuBool(id.c_str(), ("Shield " + name).c_str(), true));
        }
    }

    void RebuildSpellsMenu() {
        if (!m_spellsMenu) return;
        for (int i = 0; i < m_spellsMenu->Components.size(); ++i) {
            delete m_spellsMenu->Components[i];
        }
        m_spellsMenu->Components.clear();

        m_spellOptions.clear();
        m_spellMenuEntries = 0;

        PopulateSpellMenu(m_spellsMenu);
    }

    void RebuildEvadeSpellsMenu() {
        if (!m_evadeSpellsMenu) return;
        for (int i = 0; i < m_evadeSpellsMenu->Components.size(); ++i) {
            delete m_evadeSpellsMenu->Components[i];
        }
        m_evadeSpellsMenu->Components.clear();

        m_evadeSpellOptions.clear();
        m_evadeSpellMenuEntries = 0;
        m_improveMoveMenu = nullptr;
        m_useCurrentPathMenu = nullptr;
        m_lowEvadeSmoothMenu = nullptr;

        m_smoothEvadeSpellMenu = m_evadeSpellsMenu->Add(new MenuBool(
            "SmoothEvadeSpell", "Use 'Smooth Evade Spell'", true));

        const auto player = GameObjects::Player();
        if (!player.IsValid()) {
            return;
        }

        const std::string championName = GetHeroCharacterName(player);
        bool first = true;
        for (const auto* data :
             KuroEvade::Database::EvadeSpellDatabase::ForChampion(
                 championName.c_str(), true)) {
            AddEvadeSpellMenuEntry(m_evadeSpellsMenu, *data, first);
            first = false;
        }
    }

    void RebuildAllyShieldMenu() {
        if (!m_allyShieldMenu) return;
        for (int i = 0; i < m_allyShieldMenu->Components.size(); ++i) {
            delete m_allyShieldMenu->Components[i];
        }
        m_allyShieldMenu->Components.clear();
        m_allyShieldOptions.clear();
        for (const auto& ally : SDK::GameObjects::AllyHeroes()) {
            if (!ally.IsValid() || ally.IsMe()) continue;
            const std::string name = GetHeroCharacterName(ally);
            const std::string id = "shield" + KuroEvade::SpellMenuKey::Sanitize(name) +
                std::to_string(ally.NetworkId());
            m_allyShieldOptions[ally.NetworkId()] = m_allyShieldMenu->Add(
                new MenuBool(id.c_str(), ("Shield " + name).c_str(), true));
        }
    }

    void DestroyMenu() {
        if (!m_menu) {
            return;
        }

        MenuManager::Instance().Remove(m_menu);
        delete m_menu;
        m_menu = nullptr;
        m_focusOnEvadeMenu = nullptr;
        m_onlyEvadeWhileCanMenu = nullptr;
        m_enhanceDetectMenu = nullptr;
        m_recommendedConfigMenu = nullptr;
        m_skillshotsExtraRadiusMenu = nullptr;
        m_skillshotsExtraRangeMenu = nullptr;
        m_extraEvadeDistanceMenu = nullptr;
        m_smoothEvadeSpellMenu = nullptr;
        m_improveMoveMenu = nullptr;
        m_useCurrentPathMenu = nullptr;
        m_lowEvadeSmoothMenu = nullptr;
        m_spellBlockerQMenu = nullptr;
        m_spellBlockerWMenu = nullptr;
        m_spellBlockerEMenu = nullptr;
        m_spellBlockerRMenu = nullptr;
        m_minionCollisionMenu = nullptr;
        m_heroCollisionMenu = nullptr;
        m_yasuoCollisionMenu = nullptr;
        m_enableCollisionMenu = nullptr;
        m_enabledColorMenu = nullptr;
        m_disabledColorMenu = nullptr;
        m_missileColorMenu = nullptr;
        m_borderMenu = nullptr;
        m_enableDrawingsMenu = nullptr;
        m_drawWarningMessageMenu = nullptr;
        m_blockSpellsMenu = nullptr;
        m_allowAaLevelMenu = nullptr;
        m_disableFowMenu = nullptr;
        m_showEvadeStatusMenu = nullptr;
        m_disableOlafRMenu = nullptr;
        m_enabledMenu = nullptr;
        m_onlyDangerousMenu = nullptr;
        m_devModeMenu = nullptr;
        m_devSameTeamMenu = nullptr;
        m_benchmarkLineKeyMenu = nullptr;
        m_benchmarkCircleKeyMenu = nullptr;
        m_benchmarkRunKeyMenu = nullptr;
        m_spellsMenu = nullptr;
        m_evadeSpellsMenu = nullptr;
        m_allyShieldMenu = nullptr;
        m_spellOptions.clear();
        m_spellMenuEntries = 0;
        m_evadeSpellOptions.clear();
        m_evadeSpellMenuEntries = 0;
        m_allyShieldOptions.clear();
        m_spellVisuals.clear();
        m_lastVisualUpdateTick = 0;
        m_visualInterventionState = false;
        m_currentDangerousThreat = false;
        m_currentDangerLevel = 0;
    }
};

} // namespace Plugins
