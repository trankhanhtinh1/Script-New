#pragma once

#include "../IPlugin.h"
#include "../../SDK/SDK.h"
#include "../../SDK/Data/EmbeddedAssets.h"
#include "../../SDK/UI/Icons.h"
#include "../../Core/CoreRuntime.h"
#include "../../DebugLog.h"
#include "ActivatorEngine.h"
#include "AwarenessIconAssets.h"
#include "AwarenessImGuiRenderer.h"
#include "SdkObservationBridge.h"
#include "PatchOverrides.h"
#include "AwarenessActivatorMenu.h"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <string_view>

namespace NightSharp::Companion {

class AwarenessActivatorPlugin final : public Plugins::IPlugin {
public:
    AwarenessActivatorPlugin() = default;

    const char* GetName() const override { return "Awareness + Activator"; }
    const char* GetInternalId() const override { return "core.awareness_activator"; }
    const char* GetAuthor() const override { return "NightSharp"; }
    Plugins::PluginCategory GetCategory() const override { return Plugins::PluginCategory::Core; }
    bool AutoLoadByDefault() const override { return false; }
    bool CanLoad() const override { return CoreRuntime::EnsureInitialized(); }

    void OnLoad() override {
        if (!CanLoad()) {
            loaded_ = false;
            return;
        }
        awareness_.Reset();
        layerGuard_.Reset();
        updateFrame_ = 0;
        renderFrame_ = 0;
        attackRange_ = 0.0f;
        localPosition_ = {};
        hasLocalPosition_ = false;
        lastAttackRangeAt_ = -1000.0f;
        lastActivatorEvalAt_ = -1000.0f;
        lastAudioPollAt_ = -1000.0f;
        lastCombatForecastAt_ = -1000.0f;
        renderForecast_ = {};
        hasRenderForecast_ = false;
        iconLoadAttempted_ = false;
        iconCache_ = {};
        iconCacheCount_ = 0;
        LoadPatchOverrides();
        ConfigureDiagnostics();
        bridge_.Attach(awareness_);
        loaded_ = bridge_.IsAttached();
        if (loaded_) {
            menu_.Attach(
                settings_, awareness_.Registry().PatchVersion(),
                &AwarenessActivatorPlugin::ExportDecisionLogCallback, this,
                &AwarenessActivatorPlugin::ApplyChampionPresetCallback, this);
        }
        NightSharpDebug::Logf("[AwarenessActivator] load attached=%d", loaded_ ? 1 : 0);
    }

    void OnUnload() override {
        menu_.Detach();
        bridge_.Detach();
        hasCandidate_ = false;
        renderFrame_ = 0;
        attackRange_ = 0.0f;
        localPosition_ = {};
        hasLocalPosition_ = false;
        lastAttackRangeAt_ = -1000.0f;
        lastActivatorEvalAt_ = -1000.0f;
        lastAudioPollAt_ = -1000.0f;
        lastCombatForecastAt_ = -1000.0f;
        renderForecast_ = {};
        hasRenderForecast_ = false;
        iconLoadAttempted_ = false;
        iconCache_ = {};
        iconCacheCount_ = 0;
        diagnostics_.Configure(false, false, true, 60u, 8.0f);
        bridge_.SetDiagnostics(nullptr);
        loaded_ = false;
        NightSharpDebug::Logf("[AwarenessActivator] unloaded");
    }

    void OnUpdate() override {
        NS_PROFILE("AwarenessActivator.OnUpdate");
        if (!loaded_ || !bridge_.IsAttached()) return;

        ConfigureDiagnostics();
        ++updateFrame_;
        diagnostics_.BeginFrame(false, updateFrame_);
        bridge_.Update();
        const float now = awareness_.Now();

        if (IsDue(now, lastAttackRangeAt_, 0.50f)) {
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::PluginAttackRange);
            RefreshAttackRange();
            scope.SetCounts(1, 0, 0, 1);
        }
        if (IsDue(now, lastAudioPollAt_, 0.10f)) {
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::PluginAudio);
            HandleAudioAlerts();
            scope.SetCounts(awareness_.Store().Alerts().Size(), 0, 0,
                            awareness_.Store().Alerts().Size());
        }
        if (settings_.drawCombatState &&
            IsDue(now, lastCombatForecastAt_, 0.10f)) {
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::PluginCombatForecast);
            RefreshRenderForecast();
            scope.SetCounts(awareness_.Store().ChampionCount(), 0, 0,
                            awareness_.Store().ChampionCount());
        }

        if (settings_.enabled &&
            IsDue(now, lastActivatorEvalAt_, 1.0f / 30.0f)) {
            hasCandidate_ = false;
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::ActivatorEvaluate);
            const ActionRequest* request =
                activator_.Evaluate(awareness_, settings_);
            if (activator_.HasSelfForecast()) {
                renderForecast_ = activator_.SelfForecast();
                hasRenderForecast_ = true;
            }
            const std::size_t objects =
                awareness_.Store().ChampionCount() +
                awareness_.Store().Wards().Size() +
                awareness_.Store().Objectives().Size() +
                awareness_.Store().Jungles().Size() +
                awareness_.Store().Threats().Size();
            scope.SetCounts(objects, request ? 1 : 0, 0, objects);
            if (request) {
                candidate_ = *request;
                hasCandidate_ = true;
                auto executeScope = diagnostics_.Begin(
                    AwarenessDiagnostics::Stage::PluginExecute);
                TryExecute(candidate_);
                executeScope.SetCounts(1, 1, 0, 1);
            }
        } else if (!settings_.enabled) {
            hasCandidate_ = false;
        }

        diagnostics_.EndFrame(false);
    }

    void OnRender() override {
        NS_PROFILE("AwarenessActivator.OnRender");
        if (!loaded_ || SDK::Drawing::IsAllDrawingHidden() ||
            !settings_.drawOverlay || settings_.audioOnly) {
            return;
        }

        const bool worldEnabled =
            settings_.drawWorldLayer && HasWorldDrawWork();
        const bool minimapEnabled =
            settings_.drawMinimapLayer && HasMinimapDrawWork();
        const bool enemyHudEnabled = settings_.drawEnemyHud;
        const bool screenHudEnabled = settings_.drawAlertCenter ||
            settings_.drawObjectives || settings_.drawInsights;
        if (!worldEnabled && !minimapEnabled &&
            !enemyHudEnabled && !screenHudEnabled) {
            return;
        }

        ConfigureDiagnostics();
        ++renderFrame_;
        diagnostics_.BeginFrame(true, renderFrame_);
        bool rendererReady = false;
        {
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::RenderBegin);
            rendererReady = renderer_.BeginFrame(
                settings_.performanceMode,
                worldEnabled || enemyHudEnabled);
            scope.SetCounts(1, rendererReady ? 1u : 0u,
                            rendererReady ? 0u : 1u, 1);
        }
        if (!rendererReady) {
            diagnostics_.EndFrame(true);
            return;
        }

        if (NeedsLiveChampionPositions(
                worldEnabled, minimapEnabled, enemyHudEnabled)) {
            auto scope = diagnostics_.Begin(
                AwarenessDiagnostics::Stage::RenderLivePositions);
            const std::size_t count = bridge_.RefreshRenderPositions();
            scope.SetCounts(count, 0, 0, count);
        }

        if (settings_.drawIcons) {
            EnsureAwarenessIcons();
        }
        RenderLayer(
            LayerWorld, worldEnabled,
            &AwarenessActivatorPlugin::DrawWorldLayer);
        RenderLayer(
            LayerMinimap, minimapEnabled,
            &AwarenessActivatorPlugin::DrawMinimapLayer);
        RenderLayer(
            LayerEnemyHud, enemyHudEnabled,
            &AwarenessActivatorPlugin::DrawEnemyHud);
        RenderLayer(
            LayerAlertCenter, screenHudEnabled,
            &AwarenessActivatorPlugin::DrawPanel);
        diagnostics_.EndFrame(true);
    }


    void OnMenu() override {
        if (!ImGui::CollapsingHeader("Awareness + Activator")) return;
        ImGui::Text("Patch registry: %s", awareness_.Registry().PatchVersion());
        int languageIndex = settings_.vietnamese ? 1 : 0;
        if (ImGui::Combo(
                "Language", &languageIndex,
                "English\0Vietnamese\0\0")) {
            settings_.vietnamese = languageIndex == 1;
            menu_.SyncMenuFromSettings();
        }

        ConfigureDiagnostics();
        ImGui::Checkbox("Enabled", &settings_.enabled);
        ImGui::Checkbox("Draw awareness overlay", &settings_.drawOverlay);

        if (ImGui::TreeNode("World drawing")) {
            ImGui::Checkbox("Enable world layer",
                            &settings_.drawWorldLayer);
            ImGui::Checkbox("Enemy champions",
                            &settings_.drawWorldChampions);
            ImGui::Checkbox("Enemy reachable areas",
                            &settings_.drawReachableAreas);
            ImGui::Checkbox("Threat geometry", &settings_.drawThreats);
            ImGui::Checkbox("Wards and vision", &settings_.drawWards);
            ImGui::Checkbox("Objectives", &settings_.drawObjectives);
            ImGui::Checkbox("Jungle timers", &settings_.drawJungle);
            ImGui::Checkbox("Combat state", &settings_.drawCombatState);
            ImGui::Checkbox(
                settings_.vietnamese ? "Thong tin nang cao" : "Advanced insights",
                &settings_.drawInsights);
            ImGui::Checkbox(
                settings_.vietnamese ? "Trang thai linh" : "Wave state",
                &settings_.drawWave);
            ImGui::Checkbox("Replay activity heatmap",
                            &settings_.drawActivityHeatmap);
            ImGui::Checkbox("Vision coverage heatmap",
                            &settings_.drawVisionHeatmap);
            ImGui::SliderFloat(
                "World draw distance", &settings_.worldDrawDistance,
                1800.0f, 9000.0f, "%.0f");
            ImGui::SliderFloat(
                "Reachable area radius cap",
                &settings_.reachableAreaMaxRadius,
                800.0f, 6000.0f, "%.0f");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Minimap drawing")) {
            ImGui::Checkbox("Enable minimap layer",
                            &settings_.drawMinimapLayer);
            ImGui::Checkbox("Enemy champions##minimap",
                            &settings_.drawMinimapChampions);
            ImGui::Checkbox("Observed path targets",
                            &settings_.drawPathTargets);
            ImGui::Checkbox("Wards##minimap",
                            &settings_.drawMinimapWards);
            ImGui::Checkbox("Objectives##minimap",
                            &settings_.drawMinimapObjectives);
            ImGui::Checkbox("Jungle camps##minimap",
                            &settings_.drawMinimapJungle);
            ImGui::Checkbox("Text labels##minimap",
                            &settings_.drawMinimapLabels);
            ImGui::TreePop();
        }
        if (ImGui::TreeNode("Professional HUD")) {
            ImGui::Checkbox("Draw icons", &settings_.drawIcons);
            ImGui::Checkbox(
                "Draw prioritized alert center",
                &settings_.drawAlertCenter);
            ImGui::Checkbox(
                "Draw enemy overhead cooldown HUD",
                &settings_.drawEnemyHud);
            ImGui::Combo(
                "HUD arrangement", &settings_.hudLayoutIndex,
                "Vertical\0Horizontal\0\0");
            ImGui::SliderFloat(
                "Icon scale", &settings_.iconScale,
                0.50f, 2.00f, "%.2fx");
            if (ImGui::Button("Reset smart HUD positions")) {
                settings_.alertPanelX = -1.0f;
                settings_.alertPanelY = -1.0f;
                settings_.objectivePanelX = -1.0f;
                settings_.objectivePanelY = -1.0f;
                settings_.insightPanelX = -1.0f;
                settings_.insightPanelY = -1.0f;
            }
            ImGui::TextDisabled(
                "Every screen HUD can always be dragged directly.");
            ImGui::TreePop();
        }

        if (ImGui::TreeNode(
                "Performance, accessibility and diagnostics")) {
            ImGui::Checkbox("Performance mode", &settings_.performanceMode);
            ImGui::Checkbox(
                "Profile Awareness FPS by stage",
                &settings_.diagnosticsEnabled);
            ImGui::Checkbox(
                "Log Awareness FPS to debug console",
                &settings_.diagnosticsConsoleLog);
            ImGui::Checkbox(
                "Include object counts and complexity",
                &settings_.diagnosticsVerbose);
            ImGui::SliderFloat(
                "Diagnostics report interval (frames)",
                &settings_.diagnosticsReportInterval,
                15.0f, 600.0f, "%.0f");
            ImGui::SliderFloat(
                "Slow render frame threshold (ms)",
                &settings_.diagnosticsSlowFrameMs,
                1.0f, 50.0f, "%.1f");
            ImGui::Checkbox(
                "Audio-only accessibility", &settings_.audioOnly);
            ImGui::Checkbox(
                "Streamer privacy mode", &settings_.streamerMode);
            if (diagnostics_.Active()) {
                ImGui::SeparatorText("Awareness profiler");
                const std::size_t stageCount = static_cast<std::size_t>(
                    AwarenessDiagnostics::Stage::Count);
                for (std::size_t index = 0; index < stageCount; ++index) {
                    const auto stage =
                        static_cast<AwarenessDiagnostics::Stage>(index);
                    const auto& metric = diagnostics_.Metric(stage);
                    if (metric.samples == 0) continue;
                    const double averageMs =
                        static_cast<double>(metric.totalUs) /
                        static_cast<double>(metric.samples) / 1000.0;
                    ImGui::Text(
                        "%s: n=%llu avg=%.3f ms max=%.3f ms",
                        AwarenessDiagnostics::StageName(stage),
                        static_cast<unsigned long long>(metric.samples),
                        averageMs,
                        static_cast<double>(metric.maxUs) / 1000.0);
                    if (diagnostics_.Verbose()) {
                        ImGui::Text(
                            "  objects=%llu drawn=%llu culled=%llu work=%llu",
                            static_cast<unsigned long long>(metric.objects),
                            static_cast<unsigned long long>(metric.drawn),
                            static_cast<unsigned long long>(metric.culled),
                            static_cast<unsigned long long>(metric.work));
                    }
                }
            }
            ImGui::TreePop();
        }
        ImGui::Checkbox("Summoner activator", &settings_.summonersEnabled);
        ImGui::Checkbox("Defensive items", &settings_.defensiveItemsEnabled);
        ImGui::Checkbox("Support items", &settings_.supportItemsEnabled);
        ImGui::Checkbox("Movement items", &settings_.movementItemsEnabled);
        ImGui::Checkbox("Offensive items", &settings_.offensiveItemsEnabled);
        ImGui::Checkbox("Vision items", &settings_.visionItemsEnabled);
        ImGui::Checkbox("Potion", &settings_.potionEnabled);
        ImGui::Checkbox("Allow practice automation", &settings_.allowPracticeAutomation);
        ImGui::Checkbox("Do not interrupt recall", &settings_.doNotInterruptRecall);
        ImGui::SliderInt("Confirmation virtual key", &settings_.confirmationVirtualKey, 1, 255);
        ImGui::SliderFloat("Defensive horizon", &settings_.defensiveHorizon, 0.15f, 3.0f, "%.2f s");
        ImGui::SliderFloat("Protection threshold", &settings_.protectionThreshold, 0.15f, 1.25f, "%.2f");
        ImGui::SliderFloat("Ally save threshold", &settings_.allySaveThreshold, 0.10f, 0.90f, "%.2f");
        ImGui::SliderFloat("Ignite safety margin", &settings_.offensiveSafetyMargin, 0.0f, 100.0f, "%.1f");
        ImGui::Checkbox("Reserve one Smite charge from Scuttle", &settings_.reserveSmiteCharge);
        ImGui::SliderFloat("Cleanse reaction delay", &settings_.cleanseReactionDelay,
                           0.0f, 0.20f, "%.2f s");

        ImGui::SeparatorText("Capability action modes");
        DrawCapabilityMode("Barrier", Capability::Barrier);
        DrawCapabilityMode("Cleanse", Capability::Cleanse);
        DrawCapabilityMode("Exhaust", Capability::Exhaust);
        DrawCapabilityMode("Flash", Capability::Flash);
        DrawCapabilityMode("Ghost", Capability::Ghost);
        DrawCapabilityMode("Heal", Capability::Heal);
        DrawCapabilityMode("Ignite", Capability::Ignite);
        DrawCapabilityMode("Smite", Capability::Smite);
        DrawCapabilityMode("Teleport", Capability::Teleport);
        DrawCapabilityMode("QSS", Capability::Qss);
        DrawCapabilityMode("Mercurial", Capability::Mercurial);
        DrawCapabilityMode("Mikael", Capability::Mikael);
        DrawCapabilityMode("Zhonya", Capability::Zhonya);
        DrawCapabilityMode("Seeker", Capability::Seeker);
        DrawCapabilityMode("Seraph", Capability::Seraph);
        DrawCapabilityMode("Locket", Capability::Locket);
        DrawCapabilityMode("Redemption", Capability::Redemption);
        DrawCapabilityMode("Shurelya", Capability::Shurelya);
        DrawCapabilityMode("Youmuu", Capability::Youmuu);
        DrawCapabilityMode("Rocketbelt", Capability::Rocketbelt);
        DrawCapabilityMode("Stridebreaker", Capability::Stridebreaker);
        DrawCapabilityMode("Gunblade", Capability::Gunblade);
        DrawCapabilityMode("Tiamat", Capability::Tiamat);
        DrawCapabilityMode("Ravenous Hydra", Capability::RavenousHydra);
        DrawCapabilityMode("Titanic Hydra", Capability::TitanicHydra);
        DrawCapabilityMode("Profane Hydra", Capability::ProfaneHydra);
        DrawCapabilityMode("Randuin", Capability::Randuin);
        DrawCapabilityMode("Actualizer", Capability::Actualizer);
        DrawCapabilityMode("Oracle Lens", Capability::Oracle);
        DrawCapabilityMode("Farsight", Capability::Farsight);
        DrawCapabilityMode("Ward", Capability::Ward);
        DrawCapabilityMode("Potion", Capability::Potion);
        DrawCapabilityMode("Knight's Vow", Capability::KnightsVow);
        if (ImGui::Button("Export decisions + telemetry")) {
            ExportDecisionLog();
        }
        ImGui::SameLine();
        if (ImGui::Button("Apply champion preset")) {
            ApplyChampionPreset();
        }
        ImGui::Text("events: %zu  alerts: %zu",
                    awareness_.Store().TeamfightTimeline().Size(),
                    awareness_.Store().Alerts().Size());
        if (ImGui::CollapsingHeader(
                "Decision log viewer",
                ImGuiTreeNodeFlags_DefaultOpen)) {
            const auto& entries = awareness_.Log().Entries();
            std::size_t shown = 0;
            for (std::size_t i = entries.Size();
                 i > 0 && shown < 8; --i, ++shown) {
                const DecisionLogEntry& entry =
                    entries.At(i - 1);
                ImGui::Text(
                    "[%6.2f] P%d %s: %s [%s]",
                    entry.at, entry.priority, entry.subject,
                    entry.outcome,
                    ConfidenceName(entry.confidence));
                ImGui::TextWrapped("  %s", entry.reason);
            }
            if (entries.Size() == 0) {
                ImGui::TextUnformatted("No decisions recorded.");
            }
        }
    }

    bool LoadSucceeded() const override { return loaded_; }

private:
    void ConfigureDiagnostics() noexcept {
        float interval = settings_.diagnosticsReportInterval;
        if (!std::isfinite(interval) || interval < 1.0f) {
            interval = 60.0f;
        }
        float slowFrameMs = settings_.diagnosticsSlowFrameMs;
        if (!std::isfinite(slowFrameMs) || slowFrameMs < 1.0f) {
            slowFrameMs = 8.0f;
        }
        diagnostics_.Configure(
            settings_.diagnosticsEnabled,
            settings_.diagnosticsConsoleLog,
            settings_.diagnosticsVerbose,
            static_cast<std::uint32_t>(interval),
            slowFrameMs);
        bridge_.SetDiagnostics(&diagnostics_);
    }
    enum PresentationLayer : std::uint32_t {
        LayerWorld = 1u << 0,
        LayerMinimap = 1u << 1,
        LayerEnemyHud = 1u << 2,
        LayerAlertCenter = 1u << 3,
    };

    using LayerDraw = void (AwarenessActivatorPlugin::*)();
    using WorldStageDraw = void (AwarenessActivatorPlugin::*)() const;

    struct RenderLimits final {
        std::size_t minimapWardMarkers = 128;
        std::size_t minimapWardLabels = 128;
        std::size_t minimapPathTargets = 32;
        std::size_t minimapJungleMarkers = 64;
        std::size_t minimapJungleLabels = 64;
        std::size_t worldWardMarkers = 128;
        std::size_t worldWardLabels = 128;
        std::size_t worldObjectiveMarkers = 16;
        std::size_t worldObjectiveLabels = 16;
        std::size_t worldJungleMarkers = 64;
        std::size_t worldJungleLabels = 64;
        std::size_t worldThreatMarkers = 128;
        std::size_t worldThreatLabels = 128;
        std::size_t reachableAreas = 32;
        std::size_t activityMarkers = 512;
        std::size_t visionMarkers = 128;
    };

    bool HasWorldDrawWork() const noexcept {
        return settings_.drawWorldChampions ||
               settings_.drawReachableAreas ||
               settings_.drawWards ||
               settings_.drawObjectives ||
               settings_.drawJungle ||
               settings_.drawThreats ||
               settings_.drawCombatState ||
               settings_.drawInsights ||
               settings_.drawActivityHeatmap ||
               settings_.drawVisionHeatmap;
    }

    bool HasMinimapDrawWork() const noexcept {
        return settings_.drawMinimapChampions ||
               settings_.drawPathTargets ||
               settings_.drawMinimapWards ||
               settings_.drawMinimapObjectives ||
               settings_.drawMinimapJungle;
    }

    bool NeedsLiveChampionPositions(bool worldEnabled,
                                    bool minimapEnabled,
                                    bool enemyHudEnabled) const noexcept {
        return enemyHudEnabled ||
               (worldEnabled &&
                (settings_.drawWorldChampions ||
                 settings_.drawReachableAreas ||
                 settings_.drawCombatState)) ||
               (minimapEnabled &&
                (settings_.drawMinimapChampions ||
                 settings_.drawPathTargets));
    }

    RenderLimits CurrentRenderLimits() const noexcept {
        if (!settings_.performanceMode) return {};
        RenderLimits limits{};
        limits.minimapWardMarkers = 24;
        limits.minimapWardLabels = 6;
        limits.minimapPathTargets = 5;
        limits.minimapJungleMarkers = 16;
        limits.minimapJungleLabels = 16;
        limits.worldWardMarkers = 12;
        limits.worldWardLabels = 4;
        limits.worldObjectiveMarkers = 6;
        limits.worldObjectiveLabels = 2;
        limits.worldJungleMarkers = 8;
        limits.worldJungleLabels = 3;
        limits.worldThreatMarkers = 16;
        limits.worldThreatLabels = 6;
        limits.reachableAreas = 2;
        limits.activityMarkers = 24;
        limits.visionMarkers = 8;
        return limits;
    }

    void RenderLayer(std::uint32_t layer,
                     bool enabled,
                     LayerDraw draw) noexcept {
        if (!enabled || !draw) return;
        const bool wasFaulted = layerGuard_.IsFaulted(layer);
        AwarenessDiagnostics::Stage stage =
            AwarenessDiagnostics::Stage::RenderPanel;
        switch (layer) {
        case LayerWorld:
            stage = AwarenessDiagnostics::Stage::RenderWorld;
            break;
        case LayerMinimap:
            stage = AwarenessDiagnostics::Stage::RenderMinimap;
            break;
        case LayerEnemyHud:
            stage = AwarenessDiagnostics::Stage::RenderEnemyHud;
            break;
        default:
            break;
        }
        auto scope = diagnostics_.Begin(stage);
        layerGuard_.Run(layer, enabled, [&]() {
            (this->*draw)();
        });
        scope.SetCounts(1u, 1u, 0u, 1u);
        if (!wasFaulted && layerGuard_.IsFaulted(layer)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] presentation layer faulted: %u",
                layer);
        }
    }

    static bool IsDue(float now, float& lastAt,
                      float interval) noexcept {
        if (!std::isfinite(now)) return false;
        if (lastAt < 0.0f || now < lastAt ||
            now - lastAt >= interval) {
            lastAt = now;
            return true;
        }
        return false;
    }

    void RefreshAttackRange() noexcept {
        attackRange_ = 0.0f;
        localPosition_ = {};
        hasLocalPosition_ = false;
        const auto player = SDK::GameObjects::Player();
        if (!player.IsValid()) return;
        attackRange_ = std::max(
            0.0f, player.AttackRange() +
                player.BoundingRadius());
        const auto position = player.Position();
        localPosition_ = {
            position.x, position.y, position.z
        };
        hasLocalPosition_ = localPosition_.IsValid() &&
                            !localPosition_.IsZero();
    }

    void RefreshRenderForecast() noexcept {
        if (settings_.enabled && activator_.HasSelfForecast() &&
            awareness_.Now() - lastActivatorEvalAt_ <= 0.20f) {
            renderForecast_ = activator_.SelfForecast();
            hasRenderForecast_ = true;
            return;
        }

        const ChampionState* player = nullptr;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (state.local) player = &state;
            });
        if (!player || player->dead || !player->visible ||
            player->health.value <= 0.0f) {
            renderForecast_ = {};
            hasRenderForecast_ = false;
            return;
        }
        renderForecast_ = CombatPredictionService::Forecast(
            *player, awareness_.Store(), awareness_.Now(),
            settings_.defensiveHorizon);
        hasRenderForecast_ = true;
    }

    static const char* ActionModeName(ActionMode mode) noexcept {
        switch (mode) {
        case ActionMode::Off: return "Off";
        case ActionMode::Suggest: return "Suggest";
        case ActionMode::Confirm: return "Confirm";
        case ActionMode::Auto: return "Auto";
        default: return "Unknown";
        }
    }

    static const char* RuntimeModeName(RuntimeMode mode) noexcept {
        switch (mode) {
        case RuntimeMode::Practice: return "Practice";
        case RuntimeMode::Research: return "Research";
        case RuntimeMode::Spectator: return "Spectator";
        case RuntimeMode::Replay: return "Replay";
        default: return "Companion";
        }
    }

    static SDK::Vector3 ToSdkPoint(const Point3& point) noexcept {
        return SDK::Vector3(point.x, point.y, point.z);
    }

    static bool IsValidPoint(const Point3& point) noexcept {
        return !point.IsZero() && std::isfinite(point.x) && std::isfinite(point.y) && std::isfinite(point.z);
    }
    struct IconCacheEntry final {
        char key[96] = {};
        ImTextureID texture = nullptr;
    };

    void EnsureAwarenessIcons() noexcept {
        if (iconLoadAttempted_) {
            return;
        }
        iconLoadAttempted_ = true;
        AwarenessIconAssets::Load();
        iconCache_ = {};
        iconCacheCount_ = 0;
    }

    bool WorldToMinimap(const SDK::Vector3& world,
                        SDK::Vector2& out) const noexcept {
        return renderer_.WorldToMinimap(world, out);
    }
    bool WorldToScreen(const SDK::Vector3& world,
                       SDK::Vector2& out) const noexcept {
        return renderer_.WorldToScreen(world, out);
    }

    void DrawLine(const SDK::Vector2& start,
                  const SDK::Vector2& end,
                  std::uint32_t color,
                  float thickness = 1.5f) const noexcept {
        renderer_.DrawLine(start, end, color, thickness);
    }

    void DrawLine(const SDK::Vector3& start,
                  const SDK::Vector3& end,
                  std::uint32_t color,
                  float thickness = 1.5f) const noexcept {
        renderer_.DrawLine(start, end, color, thickness);
    }

    void DrawCircle(const SDK::Vector2& position,
                    float radius,
                    float thickness,
                    std::uint32_t color,
                    int segments = 32) const noexcept {
        renderer_.DrawCircle(position, radius, thickness, color, segments);
    }

    void DrawCircle(const SDK::Vector3& center,
                    float radius,
                    std::uint32_t color,
                    float thickness = 1.5f,
                    int segments = 32) const noexcept {
        renderer_.DrawCircle(center, radius, color, thickness, segments);
    }

    void DrawText(float x,
                  float y,
                  std::uint32_t color,
                  const char* text) const noexcept {
        renderer_.DrawText(x, y, color, text);
    }

    void DrawText(const SDK::Vector2& position,
                  const char* text,
                  std::uint32_t color = 0xFFFFFFFFu,
                  bool centered = false) const noexcept {
        renderer_.DrawText(position, text, color, centered);
    }

    void DrawText(const SDK::Vector3& world,
                  const char* text,
                  std::uint32_t color = 0xFFFFFFFFu,
                  bool centered = false) const noexcept {
        renderer_.DrawText(world, text, color, centered);
    }
    void DrawTextSmall(const SDK::Vector2& position,
                       const char* text,
                       std::uint32_t color = 0xFFFFFFFFu,
                       bool centered = false,
                       float fontSize = 11.0f) const noexcept {
        renderer_.DrawTextSmall(
            position, text, color, centered, fontSize);
    }

    void DrawRectFilled(const SDK::Vector2& min,
                        const SDK::Vector2& max,
                        std::uint32_t color,
                        float rounding = 0.0f) const noexcept {
        renderer_.DrawRectFilled(min, max, color, rounding);
    }

    bool DrawIcon(const SDK::Vector2& center,
                  ImTextureID texture,
                  float size,
                  std::uint32_t tint = 0xFFFFFFFFu) const noexcept {
        const float scaledSize = size * std::clamp(
            settings_.iconScale, 0.50f, 2.00f);
        return settings_.drawIcons && IsRealIcon(texture) &&
               renderer_.DrawIcon(center, texture, scaledSize, tint);
    }

    bool DrawIcon(const Point3& world,
                  ImTextureID texture,
                  float size,
                  std::uint32_t tint = 0xFFFFFFFFu) const noexcept {
        const float scaledSize = size * std::clamp(
            settings_.iconScale, 0.50f, 2.00f);
        return settings_.drawIcons && IsRealIcon(texture) &&
               renderer_.DrawIcon(
                   ToSdkPoint(world), texture, scaledSize, tint);
    }

    static bool IsRealIcon(ImTextureID texture) noexcept {
        return texture != nullptr &&
               texture != SDK::UI::Icons::GetPlaceholder();
    }

    static std::size_t NormalizeIconKey(
        std::string_view value,
        char* out,
        std::size_t outSize,
        bool compact) noexcept {
        if (!out || outSize == 0) {
            return 0;
        }
        std::size_t count = 0;
        for (const unsigned char character : value) {
            const bool allowed = std::isalnum(character) ||
                                 character == '_' ||
                                 (!compact && character == '.');
            if (!allowed) {
                continue;
            }
            if (count + 1 >= outSize) {
                break;
            }
            out[count++] = static_cast<char>(
                std::tolower(character));
        }
        out[count] = '\0';
        return count;
    }

    void TryLoadEmbeddedIcon(const char* key) const noexcept {
        if (!key || !key[0]) {
            return;
        }
        std::size_t count = 0;
        const auto* assets =
            SDK::Data::EmbeddedAssets::ImageAssets(count);
        for (std::size_t i = 0; i < count; ++i) {
            const auto& asset = assets[i];
            if (!asset.Key || std::strcmp(asset.Key, key) != 0 ||
                !asset.Bytes || asset.Size == 0) {
                continue;
            }
            (void)SDK::UI::Icons::LoadIconFromBytes(
                key, asset.Bytes, static_cast<int>(asset.Size));
            return;
        }
    }

    ImTextureID ResolveIcon(
        std::string_view value, bool compact) const noexcept {
        char normalized[96] = {};
        const std::size_t length = NormalizeIconKey(
            value, normalized, sizeof(normalized), compact);
        if (length == 0) {
            return nullptr;
        }

        for (std::size_t i = 0; i < iconCacheCount_; ++i) {
            if (std::strcmp(iconCache_[i].key, normalized) == 0) {
                return iconCache_[i].texture;
            }
        }

        const std::string lookup(normalized, length);
        ImTextureID texture = SDK::UI::Icons::GetIcon(lookup);
        if (!IsRealIcon(texture)) {
            TryLoadEmbeddedIcon(normalized);
            texture = SDK::UI::Icons::GetIcon(lookup);
        }

        if (iconCacheCount_ < iconCache_.size()) {
            IconCacheEntry& entry = iconCache_[iconCacheCount_++];
            std::char_traits<char>::copy(
                entry.key, normalized, length);
            entry.key[length] = '\0';
            entry.texture = texture;
        }
        return texture;
    }

    ImTextureID ResolveIconVariants(
        std::string_view value) const noexcept {
        ImTextureID texture = ResolveIcon(value, false);
        if (IsRealIcon(texture)) {
            return texture;
        }
        return ResolveIcon(value, true);
    }

    ImTextureID IconForChampion(
        const ChampionState& state) const noexcept {
        ImTextureID texture = ResolveIconVariants(state.activeFormId);
        if (IsRealIcon(texture)) {
            return texture;
        }
        texture = ResolveIconVariants(state.baseChampionId);
        if (IsRealIcon(texture)) {
            return texture;
        }
        return ResolveIconVariants(state.championId);
    }

    ImTextureID IconForSpell(
        const ChampionState& state,
        const ObservedSpell& spell) const noexcept {
        ImTextureID texture = ResolveIconVariants(spell.name);
        if (IsRealIcon(texture) || spell.slot < 0 ||
            spell.slot >= 6) {
            return texture;
        }

        const char suffixes[] = { 'q', 'w', 'e', 'r', 'd', 'f' };
        char champion[64] = {};
        if (NormalizeIconKey(
                state.activeFormId, champion, sizeof(champion), true) == 0) {
            NormalizeIconKey(
                state.baseChampionId, champion,
                sizeof(champion), true);
        }
        if (champion[0] == '\0') {
            NormalizeIconKey(
                state.championId, champion,
                sizeof(champion), true);
        }
        if (champion[0] == '\0') {
            return texture;
        }

        char candidate[96] = {};
        std::snprintf(
            candidate, sizeof(candidate), "%s_%c",
            champion, suffixes[spell.slot]);
        texture = ResolveIconVariants(candidate);
        if (IsRealIcon(texture)) {
            return texture;
        }
        std::snprintf(
            candidate, sizeof(candidate), "%s%c",
            champion, suffixes[spell.slot]);
        return ResolveIconVariants(candidate);
    }

    ImTextureID IconForCapability(
        Capability capability) const noexcept {
        const char* key = nullptr;
        switch (capability) {
        case Capability::Barrier: key = "summonerbarrier"; break;
        case Capability::Cleanse: key = "summonerboost"; break;
        case Capability::Exhaust: key = "summonerexhaust"; break;
        case Capability::Flash: key = "summonerflash"; break;
        case Capability::Ghost: key = "summonerhaste"; break;
        case Capability::Heal: key = "summonerheal"; break;
        case Capability::Ignite: key = "summonerdot"; break;
        case Capability::Smite: key = "summonersmite"; break;
        case Capability::Teleport: key = "summonerteleport"; break;
        default: key = "awareness_alert"; break;
        }
        return ResolveIconVariants(key);
    }

    ImTextureID IconForWard(bool enemy) const noexcept {
        return ResolveIcon(enemy
                               ? "awareness_ward_enemy"
                               : "awareness_ward_ally",
                           false);
    }

    ImTextureID IconForObjective(
        ObjectiveKind kind) const noexcept {
        switch (kind) {
        case ObjectiveKind::ElementalDragon:
        case ObjectiveKind::DragonSoul:
            return ResolveIcon("awareness_dragon", false);
        case ObjectiveKind::ElderDragon:
            return ResolveIcon("awareness_elder_dragon", false);
        case ObjectiveKind::RiftHerald:
            return ResolveIcon("awareness_riftherald", false);
        case ObjectiveKind::VoidGrubs:
            return ResolveIcon("awareness_grub", false);
        case ObjectiveKind::Baron:
            return ResolveIcon("awareness_baron", false);
        default:
            return ResolveIcon("awareness_alert", false);
        }
    }

    ImTextureID IconForCamp(
        std::string_view campId) const noexcept {
        if (TextContainsInsensitive(campId, "blue") ||
            TextContainsInsensitive(campId, "sentinel")) {
            return ResolveIcon("awareness_blue", false);
        }
        if (TextContainsInsensitive(campId, "red") ||
            TextContainsInsensitive(campId, "bramble")) {
            return ResolveIcon("awareness_red", false);
        }
        return ResolveIcon("awareness_camp", false);
    }

    bool ShouldDrawWorld(const Point3& point,
                         float radius = 0.0f) const noexcept {
        if (!settings_.performanceMode || !hasLocalPosition_) {
            return true;
        }
        const float extra = std::clamp(radius, 0.0f, 900.0f);
        const float distance =
            std::max(500.0f, settings_.worldDrawDistance) + extra;
        return point.DistanceSquared(localPosition_) <=
               distance * distance;
    }

    float ReachableDrawRadius(float radius) const noexcept {
        const float maximum = settings_.performanceMode
            ? std::clamp(settings_.reachableAreaMaxRadius,
                         800.0f, 6000.0f)
            : 6000.0f;
        return std::clamp(radius, 120.0f, maximum);
    }

    static std::uint32_t Fingerprint(const ActionRequest& request) noexcept {
        std::uint32_t value = 2166136261u;
        const auto mix = [&value](std::uint32_t part) {
            value ^= part;
            value *= 16777619u;
        };
        mix(static_cast<std::uint32_t>(request.capability));
        mix(static_cast<std::uint32_t>(request.itemId));
        mix(static_cast<std::uint32_t>(request.spellSlot + 2));
        mix(static_cast<std::uint32_t>(request.itemSlot + 2));
        mix(request.targetId);
        mix(static_cast<std::uint32_t>(std::max(0.0f, request.position.x) * 0.25f));
        mix(static_cast<std::uint32_t>(std::max(0.0f, request.position.z) * 0.25f));
        return value;
    }

    void DrawCapabilityMode(const char* label, Capability capability) {
        int mode = static_cast<int>(settings_.ModeFor(capability));
        char id[96] = {};
        std::snprintf(id, sizeof(id), "%s##capability_%u", label, static_cast<unsigned>(capability));
        static const char* modes[] = { "Off", "Suggest", "Confirm", "Auto" };
        if (ImGui::Combo(id, &mode, modes, 4)) {
            settings_.SetMode(capability, static_cast<ActionMode>(std::clamp(mode, 0, 3)));
        }
    }

    bool InputSafe() const noexcept {
        if (!SDK::Game::IsFocused() || !SDK::Game::ShouldProcessInput()) return false;
        if (settings_.doNotInterruptRecall) {
            bool recalling = false;
            awareness_.Store().ForEachChampion([&](const ChampionState& state) {
                if (state.local) recalling = state.recalling;
            });
            if (recalling) return false;
        }
        return true;
    }

    bool ConfirmationReady(const ActionRequest& request) const noexcept {
        if (request.mode == ActionMode::Suggest || request.mode == ActionMode::Off) return false;
        if (request.mode == ActionMode::Auto &&
            awareness_.Mode() == RuntimeMode::Practice && settings_.allowPracticeAutomation) {
            return true;
        }
        if (request.mode == ActionMode::Auto) return false;
        return (GetAsyncKeyState(settings_.confirmationVirtualKey) & 0x8000) != 0;
    }

    bool LiveActionValid(
        const SDK::AIHeroClient& player,
        const SDK::AIBaseClient& target,
        const ActionRequest& request) const noexcept {
        float range = 0.0f;
        if (request.itemId != SDK::ItemId::Unknown) {
            const ItemDefinition* definition =
                awareness_.Registry().FindItem(
                    request.itemId);
            if (definition) range = definition->range;
        } else {
            const SummonerDefinition* definition =
                awareness_.Registry().FindAvailableSummoner(
                    request.capability);
            if (definition) range = definition->range;
        }

        if (target.IsValid()) {
            if (request.capability != Capability::Teleport &&
                (target.IsDead() || !target.IsTargetable())) {
                return false;
            }
            switch (request.capability) {
            case Capability::Ignite:
            case Capability::Exhaust:
            case Capability::Gunblade:
                if (!target.IsEnemy()) return false;
                break;
            case Capability::Mikael:
            case Capability::KnightsVow:
            case Capability::Teleport:
                if (!target.IsAlly()) return false;
                break;
            default:
                break;
            }
            if (range > 0.0f &&
                player.Position().Distance2D(
                    target.Position()) >
                    range + target.BoundingRadius() + 35.0f) {
                return false;
            }
        }

        if (IsValidPoint(request.position) &&
            range > 0.0f &&
            request.capability != Capability::Rocketbelt &&
            player.Position().Distance2D(
                ToSdkPoint(request.position)) >
                range + 35.0f) {
            return false;
        }
        return true;
    }

    static SDK::AIBaseClient FindUnitByNetworkId(std::uint32_t networkId) {
        if (networkId == 0) return {};
        const auto objects = SDK::GameObjects::AllGameObjects();
        for (const auto& object : objects) {
            if (object.CachedNetworkId() == networkId) {
                return SDK::AIBaseClient(object.Handle());
            }
        }
        return {};
    }

    bool CastAction(const ActionRequest& request) {
        SDK::AIHeroClient player = SDK::GameObjects::Player();
        if (!player.IsValid()) return false;

        SDK::AIBaseClient target;
        if (request.targetId != 0) {
            target = FindUnitByNetworkId(request.targetId);
            if (!target.IsValid()) return false;
        }
        if (!LiveActionValid(
                player, target, request)) {
            return false;
        }

        if (request.itemId != SDK::ItemId::Unknown) {
            if (request.itemSlot < 0 ||
                request.itemSlot > ::CoreItem::kTrinketSlot) {
                return false;
            }
            const ::CoreItem::ItemSlot liveSlot =
                ::CoreItem::ReadSlot(
                    player.Address(), request.itemSlot);
            if (!liveSlot.hasItem ||
                ::CoreItem::NormalizeItemId(liveSlot.id) !=
                    SDK::ItemIdValue(request.itemId) ||
                !SDK::Items::CanUseItem(
                    player, request.itemId)) {
                return false;
            }
            if (target.IsValid()) return SDK::Items::UseItem(player, request.itemId, target);
            if (IsValidPoint(request.position)) return SDK::Items::UseItem(player, request.itemId, ToSdkPoint(request.position));
            return SDK::Items::UseItem(player, request.itemId);
        }

        if (request.spellSlot < 0) return false;
        if (!ActivatorEngine::IsSummonerSpellSlot(
                request.spellSlot)) return false;
        const SDK::SpellSlot slot = static_cast<SDK::SpellSlot>(request.spellSlot);
        const auto spellbook = player.Spellbook();
        if (!spellbook.IsValid() || spellbook.CanUseSpell(slot) != CoreSpellBook::State_Ready) return false;
        if (target.IsValid()) return spellbook.CastSpell(slot, target);
        if (IsValidPoint(request.position)) return spellbook.CastSpell(slot, ToSdkPoint(request.position));
        return spellbook.CastSpell(slot);
    }

    void TryExecute(const ActionRequest& request) {
        if (!ConfirmationReady(request) || !InputSafe()) return;
        const float now = awareness_.Now();
        const std::uint32_t fingerprint = Fingerprint(request);
        if (fingerprint == lastAttemptFingerprint_ && now - lastAttemptAt_ < 0.35f) return;
        lastAttemptFingerprint_ = fingerprint;
        lastAttemptAt_ = now;

        const bool success = CastAction(request);
        awareness_.Log().Add(request.createdAt, CapabilityName(request.capability),
                             success ? "executed" : "cast rejected",
                             success ? "SDK cast accepted" : "target, range, or spell state rejected cast",
                             static_cast<int>(request.priority), request.confidence);
    }

    void DrawWorldStage(AwarenessDiagnostics::Stage stage,
                        bool enabled,
                        WorldStageDraw draw,
                        std::size_t objects) const {
        if (!enabled || !draw) return;
        auto scope = diagnostics_.Begin(stage);
        (this->*draw)();
        scope.SetCounts(objects, 0, 0, objects);
    }

    void DrawWorldLayer() {
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldHeatmaps,
            settings_.drawActivityHeatmap || settings_.drawVisionHeatmap,
            &AwarenessActivatorPlugin::DrawHeatmaps,
            awareness_.Insights().activity.Size() +
                awareness_.Store().Wards().Size());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldChampions,
            settings_.drawWorldChampions || settings_.drawReachableAreas,
            &AwarenessActivatorPlugin::DrawChampions,
            awareness_.Store().ChampionCount());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldWards,
            settings_.drawWards,
            &AwarenessActivatorPlugin::DrawWards,
            awareness_.Store().Wards().Size());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldObjectives,
            settings_.drawObjectives,
            &AwarenessActivatorPlugin::DrawObjectives,
            awareness_.Store().Objectives().Size());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldJungle,
            settings_.drawJungle,
            &AwarenessActivatorPlugin::DrawJungle,
            awareness_.Store().Jungles().Size());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldThreats,
            settings_.drawThreats,
            &AwarenessActivatorPlugin::DrawThreats,
            awareness_.Store().Threats().Size());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldCombat,
            settings_.drawCombatState,
            &AwarenessActivatorPlugin::DrawCombatState,
            awareness_.Store().ChampionCount());
        DrawWorldStage(
            AwarenessDiagnostics::Stage::RenderWorldInsights,
            settings_.drawInsights,
            &AwarenessActivatorPlugin::DrawInsights,
            awareness_.Insights().objectiveSetups.Size() + 1u);
    }

    void DrawMinimapLayer() {
        const RenderLimits limits = CurrentRenderLimits();

        if (settings_.drawMinimapChampions) {
            awareness_.Store().ForEachChampion(
                [&](const ChampionState& state) {
                    if (!state.enemy || state.local || state.dead ||
                        state.clone) {
                        return;
                    }
                    const Point3 point = RenderChampionPosition(state);
                    if (!IsValidPoint(point)) return;
                    Evidence evidence = state.health.evidence;
                    if (!state.visible) {
                        evidence.provenance = Provenance::LastSeen;
                        evidence.observedAt = state.lastSeenAt;
                    }
                    if (!VisibilityGuard::CanExposePosition(
                            evidence, awareness_.Mode(),
                            state.visible)) {
                        return;
                    }
                    SDK::Vector2 minimap{};
                    if (!WorldToMinimap(
                            ToSdkPoint(point), minimap)) {
                        return;
                    }
                    const EvidenceVisualStyle style =
                        AwarenessPresentationPolicy::StyleFor(
                            evidence);
                    DrawCircle(
                        minimap, 6.0f, style.thickness,
                        style.color, 18);
                    if (settings_.drawIcons) {
                        DrawIcon(
                            minimap, IconForChampion(state), 12.0f);
                    }
                });
        }

        DrawPathTargetsOnMinimap(limits);
        DrawJungleOnMinimap(limits);

        if (settings_.drawMinimapWards) {
            const auto& wards = awareness_.Store().Wards();
            std::size_t wardMarkers = 0;
            std::size_t wardLabels = 0;
            // Newest observations first. Old ring entries are the least useful
            // and were the largest source of minimap draw-call growth.
            for (std::size_t cursor = wards.Size();
                 cursor > 0 && wardMarkers < limits.minimapWardMarkers;
                 --cursor) {
                const WardState& ward = wards.At(cursor - 1);
                if (ward.destroyed ||
                    !IsValidPoint(ward.position) ||
                    !ward.evidence.IsKnown() ||
                    !VisibilityGuard::CanExposePosition(
                        ward.evidence, awareness_.Mode(), ward.visible)) {
                    continue;
                }
                SDK::Vector2 minimap{};
                if (!WorldToMinimap(
                        ToSdkPoint(ward.position), minimap)) {
                    continue;
                }
                const EvidenceVisualStyle style =
                    AwarenessPresentationPolicy::StyleFor(
                        ward.evidence);
                DrawCircle(
                    minimap, 3.5f, style.thickness,
                    style.color, 12);
                if (settings_.drawIcons) {
                    DrawIcon(minimap, IconForWard(ward.enemy), 9.0f);
                }
                ++wardMarkers;
                const bool importantLabel = ward.enemy || ward.faelight;
                if (settings_.drawMinimapLabels &&
                    wardLabels < limits.minimapWardLabels &&
                    (!settings_.performanceMode || importantLabel)) {
                    char label[128] = {};
                    std::snprintf(
                        label, sizeof(label), "%c ward%s [%s/%s]",
                        style.marker, ward.visible ? "" : " last seen",
                        ConfidenceName(ward.evidence.confidence),
                        ProvenanceName(ward.evidence.provenance));
                    DrawText(
                        minimap.x + 5.0f, minimap.y - 5.0f,
                        style.color, label);
                    ++wardLabels;
                }
            }
        }

        if (settings_.drawMinimapObjectives) {
            const auto& objectives =
                awareness_.Store().Objectives();
            for (std::size_t i = 0;
                 i < objectives.Size(); ++i) {
                const ObjectiveState& objective =
                    objectives.At(i);
                if (!IsValidPoint(objective.position) ||
                    !objective.evidence.IsKnown()) {
                    continue;
                }
                const ExposureDecision exposure =
                    VisibilityGuard::CanExpose(
                        objective.evidence, awareness_.Mode(),
                        objective.visible, true);
                if (!exposure.allowed) continue;
                SDK::Vector2 minimap{};
                if (!WorldToMinimap(
                        ToSdkPoint(objective.position), minimap)) {
                    continue;
                }
                const EvidenceVisualStyle style =
                    AwarenessPresentationPolicy::StyleFor(
                        objective.evidence);
                DrawCircle(
                    minimap, 5.0f, style.thickness,
                    style.color, 16);
                if (settings_.drawIcons) {
                    DrawIcon(
                        minimap, IconForObjective(objective.kind), 11.0f);
                }
                // Objective count is small, so labels remain available. In
                // performance mode hide stale non-visible labels.
                if (settings_.drawMinimapLabels &&
                    (!settings_.performanceMode || objective.visible ||
                    objective.status == ObjectiveStatus::SpawningSoon ||
                    objective.status == ObjectiveStatus::Respawning ||
                    objective.status == ObjectiveStatus::InCombatVisible)) {
                    char label[128] = {};
                    std::snprintf(
                        label, sizeof(label), "%c %s%s [%s/%s]",
                        style.marker, ObjectiveName(objective.kind),
                        objective.visible ? "" : " last seen",
                        ConfidenceName(objective.evidence.confidence),
                        ProvenanceName(objective.evidence.provenance));
                    DrawText(
                        minimap.x + 6.0f, minimap.y - 6.0f,
                        style.color, label);
                }
            }
        }
    }

    void DrawPathTargetsOnMinimap(
        const RenderLimits& limits) const {
        if (!settings_.drawPathTargets) return;
        const float now = awareness_.Now();
        std::size_t drawn = 0;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (drawn >= limits.minimapPathTargets ||
                    !state.enemy || state.local || state.dead ||
                    state.clone || !state.pathEvidence.IsKnown() ||
                    state.pathEvidence.IsExpired(now) ||
                    !IsValidPoint(state.pathTargetPosition)) {
                    return;
                }
                const Point3 source = RenderChampionPosition(state);
                if (!IsValidPoint(source) ||
                    source.Distance(state.pathTargetPosition) < 60.0f ||
                    !VisibilityGuard::CanExposePosition(
                        state.pathEvidence, awareness_.Mode(),
                        state.visible)) {
                    return;
                }

                SDK::Vector2 sourceMap{};
                SDK::Vector2 targetMap{};
                if (!WorldToMinimap(
                        ToSdkPoint(source), sourceMap) ||
                    !WorldToMinimap(
                        ToSdkPoint(state.pathTargetPosition), targetMap)) {
                    return;
                }

                const EvidenceVisualStyle style =
                    AwarenessPresentationPolicy::StyleFor(
                        state.pathEvidence);
                DrawMinimapDottedSegment(
                    sourceMap, targetMap, style.color);
                DrawCircle(
                    targetMap, 7.0f, 1.5f,
                    style.color, 18);
                DrawCircle(
                    targetMap, 2.2f, 1.0f,
                    style.color, 10);
                if (settings_.drawIcons) {
                    DrawIcon(
                        targetMap, IconForChampion(state), 12.0f);
                }

                if (settings_.drawMinimapLabels) {
                    const float eta = std::max(
                        0.0f, state.pathExpectedArrivalAt - now);
                    char label[96] = {};
                    if (eta > 0.05f) {
                        std::snprintf(
                            label, sizeof(label), "%s target %.1fs",
                            settings_.streamerMode
                                ? "enemy"
                                : (state.name[0] ? state.name : "enemy"),
                            eta);
                    } else {
                        std::snprintf(
                            label, sizeof(label), "%s target",
                            settings_.streamerMode
                                ? "enemy"
                                : (state.name[0] ? state.name : "enemy"));
                    }
                    DrawText(
                        targetMap.x + 8.0f, targetMap.y - 7.0f,
                        style.color, label);
                }
                ++drawn;
            });
    }

    void DrawJungleOnMinimap(
        const RenderLimits& limits) const {
        if (!settings_.drawMinimapJungle) return;
        const float now = awareness_.Now();
        const auto& camps = awareness_.Store().Jungles();
        std::size_t markers = 0;
        std::size_t labels = 0;
        for (std::size_t cursor = camps.Size();
             cursor > 0 && markers < limits.minimapJungleMarkers;
             --cursor) {
            const JungleCampState& camp = camps.At(cursor - 1);
            if (!IsValidPoint(camp.position) ||
                !camp.evidence.IsKnown() ||
                !VisibilityGuard::CanExposePosition(
                    camp.evidence, awareness_.Mode(), camp.visible)) {
                continue;
            }

            SDK::Vector2 minimap{};
            if (!WorldToMinimap(
                    ToSdkPoint(camp.position), minimap)) {
                continue;
            }
            const EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(
                    camp.evidence);
            DrawCircle(
                minimap, 4.5f, style.thickness,
                style.color, 14);
            if (settings_.drawIcons) {
                DrawIcon(minimap, IconForCamp(camp.campId), 10.0f);
            }
            ++markers;

            if (!settings_.drawMinimapLabels ||
                labels >= limits.minimapJungleLabels) continue;
            char status[24] = {};
            FormatJungleStatus(camp, now, status, sizeof(status));
            char label[96] = {};
            std::snprintf(
                label, sizeof(label), "%s %s",
                JungleCampDisplayName(camp.campId), status);
            DrawText(
                minimap.x + 6.0f, minimap.y - 6.0f,
                style.color, label);
            ++labels;
        }
    }

    void DrawMinimapDottedSegment(
        const SDK::Vector2& start,
        const SDK::Vector2& end,
        std::uint32_t color) const {
        constexpr int kDots = 6;
        for (int i = 1; i < kDots; ++i) {
            const float t = static_cast<float>(i) /
                static_cast<float>(kDots);
            SDK::Vector2 point{};
            point.x = start.x + (end.x - start.x) * t;
            point.y = start.y + (end.y - start.y) * t;
            DrawCircle(
                point, 1.25f, 1.0f, color, 8);
        }
    }

    static void FormatJungleStatus(
        const JungleCampState& camp, float now,
        char* out, std::size_t outSize) noexcept {
        if (!out || outSize == 0) return;
        if (!camp.alive && camp.respawnAt > now) {
            const int total = std::max(
                0, static_cast<int>(std::ceil(camp.respawnAt - now)));
            const int minutes = total / 60;
            const int seconds = total % 60;
            std::snprintf(
                out, outSize, camp.observedDeath
                    ? "%d:%02d" : "~%d:%02d",
                minutes, seconds);
            return;
        }
        if (camp.alive) {
            std::snprintf(
                out, outSize,
                camp.confidence == Confidence::Confirmed
                    ? "UP" : "UP?");
            return;
        }
        std::snprintf(out, outSize, "?");
    }

    static const char* JungleCampDisplayName(
        std::string_view id) noexcept {
        if (TextContainsInsensitive(id, "blue") ||
            TextContainsInsensitive(id, "sentinel")) {
            return "Blue";
        }
        if (TextContainsInsensitive(id, "red") ||
            TextContainsInsensitive(id, "bramble")) {
            return "Red";
        }
        if (TextContainsInsensitive(id, "gromp")) return "Gromp";
        if (TextContainsInsensitive(id, "krug")) return "Krugs";
        if (TextContainsInsensitive(id, "raptor") ||
            TextContainsInsensitive(id, "razorbeak")) {
            return "Raptors";
        }
        if (TextContainsInsensitive(id, "wolf") ||
            TextContainsInsensitive(id, "murkwolf")) {
            return "Wolves";
        }
        return "Camp";
    }

    Point3 RenderChampionPosition(const ChampionState& state) const noexcept {
        if (!state.visible) return state.lastSeenPosition;
        SdkObservationBridge::RenderPosition live{};
        if (bridge_.ReadRenderPosition(state.networkId, live)) {
            return live.visible && !live.dead && IsValidPoint(live.position)
                ? live.position : Point3{};
        }
        return state.position;
    }

    void DrawChampions() const {
        const auto& store = awareness_.Store();
        const RenderLimits limits = CurrentRenderLimits();
        std::size_t reachableDrawn = 0;
        store.ForEachChampion([&](const ChampionState& state) {
            if (state.local || !state.enemy || state.dead) return;
            Evidence evidence = state.health.evidence;
            if (!state.visible) {
                evidence.provenance = Provenance::LastSeen;
                evidence.observedAt = state.lastSeenAt;
            }
            EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(evidence);
            if (state.clone) {
                style = { 0xFFFFAA44u, 2.0f, 'C' };
            }
            const Point3 renderPosition = RenderChampionPosition(state);
            if (settings_.drawWorldChampions && state.visible &&
                IsValidPoint(renderPosition) &&
                ShouldDrawWorld(renderPosition, 125.0f)) {
                DrawCircle(
                    ToSdkPoint(renderPosition), 125.0f,
                    style.color, style.thickness);
                if (settings_.drawIcons) {
                    DrawIcon(
                        renderPosition, IconForChampion(state), 30.0f);
                }
            } else if (settings_.drawReachableAreas &&
                       reachableDrawn < limits.reachableAreas &&
                       evidence.provenance ==
                           Provenance::LastSeen &&
                       IsValidPoint(state.lastSeenPosition) &&
                       ShouldDrawWorld(
                           state.lastSeenPosition,
                           ReachableDrawRadius(state.reachableRadius)) &&
                       VisibilityGuard::CanExposePosition(
                           evidence, awareness_.Mode(), false)) {
                DrawCircle(
                    ToSdkPoint(state.lastSeenPosition),
                    ReachableDrawRadius(state.reachableRadius),
                    style.color, style.thickness);
                if (settings_.drawIcons) {
                    DrawIcon(
                        state.lastSeenPosition,
                        IconForChampion(state), 24.0f, 0xAAFFFFFFu);
                }
                ++reachableDrawn;
            }
        });
    }

    void DrawEnemyHud() {
        if (!settings_.drawEnemyHud) return;

        // One compact row: Q W E R | D F. There is no champion portrait,
        // title, health bar, large panel, or separate cooldown text row.
        // The user scale is applied exactly once to the whole strip.
        const float scale = EffectiveIconScale();
        const float cellSize = 20.0f * scale;
        const float iconSize = 17.0f * scale;
        const float gap = std::max(2.0f, 2.0f * scale);
        const float groupGap = std::max(5.0f, 5.0f * scale);
        constexpr char kSlotLetters[] = { 'Q', 'W', 'E', 'R', 'D', 'F' };
        const float totalWidth = cellSize * 6.0f + gap * 4.0f + groupGap;
        std::size_t shown = 0;

        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (shown >= 8 || state.local || !state.enemy ||
                    state.clone || state.dead || !state.visible) {
                    return;
                }
                const Point3 renderPosition = RenderChampionPosition(state);
                if (!IsValidPoint(renderPosition) ||
                    !ShouldDrawWorld(renderPosition, 125.0f)) {
                    return;
                }

                SDK::Vector2 screen{};
                if (!WorldToScreen(ToSdkPoint(renderPosition), screen)) {
                    return;
                }

                const ObservedSpell* slots[6] = {};
                for (std::size_t i = 0; i < state.spellCount; ++i) {
                    const ObservedSpell& spell = state.spells[i];
                    if (spell.slot >= 0 && spell.slot < 6) {
                        slots[spell.slot] = &spell;
                    }
                }

                float centerX = screen.x + settings_.enemyHudOffsetX;
                float centerY = screen.y + settings_.enemyHudOffsetY;
                const float halfCell = cellSize * 0.5f;
                if (centerY - halfCell < 4.0f &&
                    settings_.enemyHudOffsetY < 0.0f) {
                    centerY = screen.y +
                        std::max(28.0f, -settings_.enemyHudOffsetY);
                }
                const float left = centerX - totalWidth * 0.5f;

                float cursor = left;
                for (int slot = 0; slot < 6; ++slot) {
                    if (slot == 4) cursor += groupGap;

                    const ObservedSpell* spell = slots[slot];
                    const bool known = spell && spell->evidence.IsKnown();
                    const bool ready = known && spell->ready;
                    float remaining = 0.0f;
                    if (known && !ready) {
                        remaining = std::max(
                            spell->cooldownRemaining,
                            spell->cooldownKind ==
                                    CooldownKind::RangeEstimated
                                ? spell->cooldownMax
                                : 0.0f);
                    }

                    const SDK::Vector2 cellMin(
                        cursor, centerY - halfCell);
                    const SDK::Vector2 cellMax(
                        cursor + cellSize, centerY + halfCell);
                    const std::uint32_t borderColor = ready
                        ? (slot == 3 ? 0xFFD6A84Bu : 0xFF62D69Au)
                        : 0xC03A4652u;
                    DrawRectFilled(cellMin, cellMax, borderColor, 3.0f * scale);
                    DrawRectFilled(
                        SDK::Vector2(cellMin.x + 1.0f, cellMin.y + 1.0f),
                        SDK::Vector2(cellMax.x - 1.0f, cellMax.y - 1.0f),
                        ready ? 0xC0182630u : 0xE00A1017u,
                        std::max(1.0f, 2.0f * scale));

                    const SDK::Vector2 center(
                        cursor + halfCell, centerY);
                    bool hasIcon = false;
                    if (settings_.drawIcons && spell) {
                        const ImTextureID texture = IconForSpell(state, *spell);
                        if (IsRealIcon(texture)) {
                            hasIcon = renderer_.DrawIcon(
                                center, texture, iconSize,
                                ready ? 0xFFFFFFFFu : 0x78FFFFFFu);
                        }
                    }

                    if (!ready) {
                        DrawRectFilled(
                            SDK::Vector2(cellMin.x + 1.0f, cellMin.y + 1.0f),
                            SDK::Vector2(cellMax.x - 1.0f, cellMax.y - 1.0f),
                            0x65000000u,
                            std::max(1.0f, 2.0f * scale));

                        char cooldown[8] = {};
                        if (remaining > 0.01f) {
                            const int seconds = std::clamp(
                                static_cast<int>(std::ceil(remaining)),
                                1, 999);
                            std::snprintf(
                                cooldown, sizeof(cooldown), "%d", seconds);
                        } else {
                            std::snprintf(cooldown, sizeof(cooldown), "?");
                        }
                        DrawTextSmall(
                            center, cooldown, 0xFFFFFFFFu, true,
                            std::clamp(9.0f * scale, 8.0f, 13.0f));
                    } else if (!hasIcon) {
                        char letter[2] = { kSlotLetters[slot], '\0' };
                        DrawTextSmall(
                            center, letter, 0xFFFFFFFFu, true,
                            std::clamp(10.0f * scale, 9.0f, 14.0f));
                    }

                    cursor += cellSize;
                    if (slot != 3 && slot != 5) cursor += gap;
                }
                ++shown;
            });
    }

    void DrawWards() const {
        if (!settings_.drawWards) return;
        const RenderLimits limits = CurrentRenderLimits();
        const auto& wards = awareness_.Store().Wards();
        std::size_t markers = 0;
        std::size_t labels = 0;
        for (std::size_t cursor = wards.Size();
             cursor > 0 && markers < limits.worldWardMarkers;
             --cursor) {
            const WardState& ward = wards.At(cursor - 1);
            if (ward.destroyed || !IsValidPoint(ward.position) ||
                !ShouldDrawWorld(ward.position, ward.radius)) continue;
            const ExposureDecision exposure =
                VisibilityGuard::CanExpose(
                    ward.evidence, awareness_.Mode(),
                    ward.visible, true);
            if (!exposure.allowed) continue;
            const std::uint32_t color = ward.enemy
                ? 0xFFFF7799u
                : (ward.visible ? 0xFF66FF99u : 0x9988AA99u);
            const float radius = std::max(35.0f, ward.radius);
            DrawCircle(
                ToSdkPoint(ward.position), radius, color, 1.5f);
            if (settings_.drawIcons) {
                DrawIcon(
                    ward.position, IconForWard(ward.enemy), 18.0f);
            }
            ++markers;
            const float now = awareness_.Now();
            const float bonusRemaining = ward.bonusVisionUntil > 0.0f
                ? std::max(0.0f, ward.bonusVisionUntil - now) : 0.0f;
            if (ward.visible && ward.faelight &&
                ward.bonusVisionObserved && bonusRemaining > 0.0f &&
                (!settings_.performanceMode || ward.enemy)) {
                DrawCircle(
                    ToSdkPoint(ward.position), radius,
                    0x7799DDFFu, 2.0f);
            }
            const bool importantLabel = ward.enemy || ward.faelight;
            if (labels >= limits.worldWardLabels ||
                (settings_.performanceMode && !importantLabel)) {
                continue;
            }
            char bonus[48] = {};
            if (ward.visible && ward.faelight && bonusRemaining > 0.0f) {
                std::snprintf(
                    bonus, sizeof(bonus), " +25%% vision/%.0fs",
                    bonusRemaining);
            }
            char label[160] = {};
            const float remaining = ward.expiresAt > 0.0f
                ? std::max(0.0f, ward.expiresAt - now) : 0.0f;
            std::snprintf(
                label, sizeof(label), "%s%s %.0fs [%s/%s]%s",
                ward.enemy ? "enemy " : "",
                ward.visible ? "ward" : "ward (last seen)",
                remaining, ConfidenceName(ward.evidence.confidence),
                ProvenanceName(ward.evidence.provenance), bonus);
            DrawText(
                ToSdkPoint(ward.position), label, color, true);
            ++labels;
        }
    }

    void DrawObjectives() const {
        if (!settings_.drawObjectives) return;
        const RenderLimits limits = CurrentRenderLimits();
        std::size_t worldMarkers = 0;
        std::size_t worldLabels = 0;
        const auto& objectives = awareness_.Store().Objectives();
        for (std::size_t i = 0; i < objectives.Size(); ++i) {
            const ObjectiveState& objective = objectives.At(i);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                objective.evidence, awareness_.Mode(),
                objective.visible, true);
            if (!exposure.allowed) continue;

            const float now = awareness_.Now();
            char stateText[96] = {};
            if ((objective.status == ObjectiveStatus::NotSpawned ||
                 objective.status == ObjectiveStatus::SpawningSoon) &&
                objective.spawnAt > now) {
                std::snprintf(stateText, sizeof(stateText), "spawns %.0fs",
                              objective.spawnAt - now);
            } else if ((objective.status == ObjectiveStatus::Dead ||
                        objective.status == ObjectiveStatus::Respawning) &&
                       objective.respawnAt > now) {
                std::snprintf(stateText, sizeof(stateText), "respawns %.0fs",
                              objective.respawnAt - now);
            } else if (objective.status == ObjectiveStatus::AliveVisible) {
                const float healthPercent = objective.maxHealth > 0.0f
                    ? 100.0f * objective.health / objective.maxHealth : 0.0f;
                std::snprintf(stateText, sizeof(stateText), "alive %.0f%%",
                              healthPercent);
            } else if (objective.status == ObjectiveStatus::InCombatVisible) {
                std::snprintf(stateText, sizeof(stateText), "in combat");
            } else if (objective.status == ObjectiveStatus::Disabled) {
                std::snprintf(stateText, sizeof(stateText), "disabled");
            } else {
                std::snprintf(stateText, sizeof(stateText), "alive?");
            }

            const EvidenceVisualStyle style =
                AwarenessPresentationPolicy::StyleFor(
                    objective.evidence);
            char label[192] = {};
            std::snprintf(
                label, sizeof(label), "%s%s: %s [%s/%s]",
                objective.visible ? "" : "last seen ",
                ObjectiveName(objective.kind), stateText,
                ConfidenceName(objective.evidence.confidence),
                ProvenanceName(objective.evidence.provenance));
            const std::uint32_t statusColor =
                objective.status == ObjectiveStatus::Dead ||
                objective.status == ObjectiveStatus::Respawning
                    ? 0xFF888888u : 0xFFFFCC55u;
            const std::uint32_t color = objective.visible
                ? statusColor : style.color;
            if (worldMarkers < limits.worldObjectiveMarkers &&
                IsValidPoint(objective.position) &&
                ShouldDrawWorld(objective.position, 220.0f)) {
                DrawCircle(
                    ToSdkPoint(objective.position), 220.0f,
                    color, objective.visible ? 2.0f : style.thickness);
                if (settings_.drawIcons) {
                    DrawIcon(
                        objective.position,
                        IconForObjective(objective.kind), 32.0f);
                }
                ++worldMarkers;
                if (worldLabels < limits.worldObjectiveLabels &&
                    (!settings_.performanceMode || objective.visible ||
                     objective.status == ObjectiveStatus::InCombatVisible)) {
                    DrawText(
                        ToSdkPoint(objective.position), label, color, true);
                    ++worldLabels;
                }
            }
        }
    }

    void DrawJungle() const {
        if (!settings_.drawJungle) return;
        const RenderLimits limits = CurrentRenderLimits();
        const auto& camps = awareness_.Store().Jungles();
        std::size_t markers = 0;
        std::size_t labels = 0;
        for (std::size_t cursor = camps.Size();
             cursor > 0 && markers < limits.worldJungleMarkers;
             --cursor) {
            const JungleCampState& camp = camps.At(cursor - 1);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                camp.evidence, awareness_.Mode(), camp.visible, true);
            if (!exposure.allowed || !IsValidPoint(camp.position) ||
                !ShouldDrawWorld(camp.position, 135.0f)) continue;

            const bool estimated =
                camp.evidence.provenance == Provenance::Estimated;
            const std::uint32_t color = camp.visible
                ? 0xFF55DD88u
                : (estimated ? 0xCCFFBB55u : 0xAA88AA88u);
            DrawCircle(
                ToSdkPoint(camp.position), 135.0f, color, 1.5f);
            if (settings_.drawIcons) {
                DrawIcon(
                    camp.position, IconForCamp(camp.campId), 24.0f);
            }
            ++markers;
            if (labels >= limits.worldJungleLabels ||
                (settings_.performanceMode && !camp.visible && !camp.alive)) {
                continue;
            }
            char stateText[96] = {};
            if (camp.alive) {
                std::snprintf(stateText, sizeof(stateText), "alive");
            } else if (camp.respawnAt > awareness_.Now()) {
                std::snprintf(
                    stateText, sizeof(stateText), "respawn %.0fs",
                    camp.respawnAt - awareness_.Now());
            } else if (estimated) {
                std::snprintf(stateText, sizeof(stateText), "possibly down");
            } else {
                std::snprintf(stateText, sizeof(stateText), "down");
            }
            char label[160] = {};
            std::snprintf(label, sizeof(label), "%s: %s [%s/%s]",
                          camp.campId[0] ? camp.campId : "jungle",
                          stateText,
                          ConfidenceName(camp.evidence.confidence),
                          ProvenanceName(camp.evidence.provenance));
            DrawText(
                ToSdkPoint(camp.position), label, color, true);
            ++labels;
        }
    }


    void DrawThreats() const {
        if (!settings_.drawThreats) return;
        const RenderLimits limits = CurrentRenderLimits();
        const auto& threats = awareness_.Store().Threats();
        std::size_t markers = 0;
        std::size_t labels = 0;
        for (std::size_t cursor = threats.Size();
             cursor > 0 && markers < limits.worldThreatMarkers;
             --cursor) {
            const ThreatState& threat = threats.At(cursor - 1);
            const ExposureDecision exposure = VisibilityGuard::CanExpose(
                threat.evidence, awareness_.Mode(), true, true);
            if (!threat.visible || !exposure.allowed) continue;
            const Point3 center =
                IsValidPoint(threat.end) ? threat.end : threat.start;
            if (!IsValidPoint(center) ||
                !ShouldDrawWorld(center, threat.radius)) continue;
            const std::uint32_t color =
                threat.control != CrowdControl::None
                    ? 0xFFFF5577u : 0xFFFFAA44u;
            switch (threat.geometry) {
            case ThreatGeometry::Line:
            case ThreatGeometry::Cone:
                if (IsValidPoint(threat.start) &&
                    IsValidPoint(threat.end)) {
                    DrawLine(
                        ToSdkPoint(threat.start), ToSdkPoint(threat.end),
                        color,
                        std::max(1.5f, threat.radius * 0.03f));
                }
                if (threat.geometry == ThreatGeometry::Cone) {
                    DrawCircle(
                        ToSdkPoint(center),
                        std::max(35.0f, threat.radius), color, 1.0f);
                }
                break;
            case ThreatGeometry::Circle:
            case ThreatGeometry::Ring:
            case ThreatGeometry::Point:
                DrawCircle(
                    ToSdkPoint(center),
                    std::max(35.0f, threat.radius), color,
                    threat.geometry == ThreatGeometry::Ring ? 2.0f : 1.0f);
                break;
            }
            ++markers;
            const bool importantLabel =
                threat.control != CrowdControl::None ||
                threat.targetId != 0 || threat.damage > 0.0f;
            if (labels >= limits.worldThreatLabels ||
                (settings_.performanceMode && !importantLabel)) {
                continue;
            }
            char label[160] = {};
            std::snprintf(
                label, sizeof(label), "%.1fs %.0f dmg %s [%s/%s]",
                std::max(0.0f, threat.impactAt - awareness_.Now()),
                threat.damage, CrowdControlName(threat.control),
                ConfidenceName(threat.evidence.confidence),
                ProvenanceName(threat.evidence.provenance));
            DrawText(
                ToSdkPoint(center), label, color, true);
            ++labels;
        }
    }

    void DrawCombatState() const {
        if (!settings_.drawCombatState) return;
        const ChampionState* player = nullptr;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& state) {
                if (state.local) player = &state;
            });
        if (!player || player->dead) return;
        const Point3 renderPosition = RenderChampionPosition(*player);
        if (!IsValidPoint(renderPosition)) return;

        const ThreatForecast emptyForecast{};
        const ThreatForecast* forecast =
            hasRenderForecast_ ? &renderForecast_ : &emptyForecast;
        int controlCount = 0;
        float longestControl = 0.0f;
        for (std::size_t i = 0; i < player->buffCount; ++i) {
            const ObservedBuff& buff = player->buffs[i];
            const BuffDefinition* definition =
                awareness_.Registry().ResolveBuff(
                    buff.idHash, buff.name);
            if (!definition ||
                definition->control == CrowdControl::None) {
                continue;
            }
            ++controlCount;
            longestControl = std::max(
                longestControl,
                std::max(0.0f, buff.endTime - awareness_.Now()));
        }
        char label[192] = {};
        std::snprintf(
            label, sizeof(label),
            "combat: %.0f incoming / %d threats%s | CC %d %.1fs%s",
            forecast->incomingDamage, forecast->threatCount,
            forecast->hardCc ? " hard-CC" : "",
            controlCount, longestControl,
            player->channeling ? " | channeling" : "");
        DrawText(
            ToSdkPoint(renderPosition), label,
            forecast->incomingDamage >= player->health.value
                ? 0xFFFF4466u : 0xFF66DDFFu, true);
        if (attackRange_ > 0.0f) {
            DrawCircle(
                ToSdkPoint(renderPosition), attackRange_,
                0x4455DDFFu, 1.0f);
        }
        if (!player->lastDirection.IsZero()) {
            const Point3 endpoint =
                renderPosition + player->lastDirection * 300.0f;
            DrawLine(
                ToSdkPoint(renderPosition), ToSdkPoint(endpoint),
                0x6655FFCCu, 1.5f);
        }
    }

    void DrawHeatmaps() const {
        if (settings_.drawActivityHeatmap &&
            (awareness_.Mode() == RuntimeMode::Replay ||
             awareness_.Mode() == RuntimeMode::Spectator)) {
            const RenderLimits limits = CurrentRenderLimits();
            const auto& samples = awareness_.Insights().activity;
            const std::size_t start = samples.Size() > limits.activityMarkers
                ? samples.Size() - limits.activityMarkers : 0;
            for (std::size_t i = start; i < samples.Size(); ++i) {
                const ActivitySample& sample = samples.At(i);
                const float age =
                    std::max(0.0f, awareness_.Now() - sample.at);
                if (age > 600.0f ||
                    !IsValidPoint(sample.position) ||
                    !sample.evidence.IsKnown() ||
                    !VisibilityGuard::CanExposePosition(
                        sample.evidence, awareness_.Mode(),
                        sample.evidence.provenance ==
                            Provenance::VisibleNow) ||
                    !ShouldDrawWorld(sample.position, 220.0f)) {
                    continue;
                }
                const std::uint32_t color =
                    sample.kind == ActivityKind::Death
                        ? 0x99FF3355u
                        : (sample.kind == ActivityKind::Damage
                               ? 0x66FFAA33u
                               : (sample.team == 100
                                      ? 0x3344AAFFu : 0x33FF5566u));
                const float radius =
                    sample.kind == ActivityKind::Death ? 220.0f : 110.0f;
                DrawCircle(
                    ToSdkPoint(sample.position), radius, color, 1.0f);
            }
        }
        if (settings_.drawVisionHeatmap) {
            const RenderLimits limits = CurrentRenderLimits();
            const auto& wards = awareness_.Store().Wards();
            std::size_t markers = 0;
            for (std::size_t cursor = wards.Size();
                 cursor > 0 && markers < limits.visionMarkers;
                 --cursor) {
                const WardState& ward = wards.At(cursor - 1);
                if (ward.destroyed || !ward.ally ||
                    !IsValidPoint(ward.position) ||
                    !ward.evidence.IsKnown() ||
                    !VisibilityGuard::CanExposePosition(
                        ward.evidence, awareness_.Mode(),
                        ward.visible) ||
                    !ShouldDrawWorld(ward.position, ward.radius)) {
                    continue;
                }
                DrawCircle(
                    ToSdkPoint(ward.position),
                    std::max(100.0f, ward.radius),
                    ward.faelight ? 0x4455FFAAu : 0x3344CC88u,
                    ward.faelight ? 2.0f : 1.0f);
                ++markers;
            }
        }
    }

    void DrawInsights() const {
        if (!settings_.drawInsights || !settings_.drawWave) return;
        const WaveState& wave = awareness_.Store().Wave();
        if (!wave.evidence.IsKnown() || !IsValidPoint(wave.center) ||
            !ShouldDrawWorld(wave.center, 150.0f)) {
            return;
        }
        const AwarenessLocale locale = settings_.vietnamese
            ? AwarenessLocale::Vietnamese
            : AwarenessLocale::English;
        char line[192] = {};
        std::snprintf(
            line, sizeof(line), "%s: %s %d-%d [%.0f/%s]",
            AwarenessLocalization::Text("wave", locale),
            wave.classification, wave.allyMinions,
            wave.enemyMinions, wave.laneBias,
            ConfidenceName(wave.evidence.confidence));
        DrawText(
            ToSdkPoint(wave.center), line,
            0xCC99DDAAu, true);
    }

    struct HudCardEntry final {
        ImTextureID icon = nullptr;
        std::uint32_t accent = 0xFF66CCFFu;
        char title[80] = {};
        char detail[192] = {};
        char badge[40] = {};
    };

    enum class HudDefaultAnchor : std::uint8_t {
        TopLeft = 0,
        TopRight,
        BottomLeft,
        BottomCenter,
    };

    static void TruncateHudText(const char* source,
                                char* out,
                                std::size_t outSize,
                                std::size_t maxCharacters) noexcept {
        if (!out || outSize == 0) return;
        out[0] = '\0';
        if (!source || !source[0] || maxCharacters == 0) return;
        const std::size_t length = std::strlen(source);
        const std::size_t capacity = outSize - 1;
        const std::size_t limit = std::min(maxCharacters, capacity);
        if (length <= limit) {
            std::char_traits<char>::copy(out, source, length);
            out[length] = '\0';
            return;
        }
        if (limit <= 3) {
            const std::size_t count = std::min(limit, length);
            std::char_traits<char>::copy(out, source, count);
            out[count] = '\0';
            return;
        }
        const std::size_t prefix = limit - 3;
        std::char_traits<char>::copy(out, source, prefix);
        out[prefix] = '.';
        out[prefix + 1] = '.';
        out[prefix + 2] = '.';
        out[prefix + 3] = '\0';
    }

    static void SetHudEntry(HudCardEntry& entry,
                            ImTextureID icon,
                            std::uint32_t accent,
                            const char* title,
                            const char* detail,
                            const char* badge) noexcept {
        entry = {};
        entry.icon = icon;
        entry.accent = accent;
        CopyText(entry.title, title ? title : "");
        CopyText(entry.detail, detail ? detail : "");
        CopyText(entry.badge, badge ? badge : "");
    }

    static const char* ObjectiveDisplayName(ObjectiveKind kind,
                                            bool vietnamese) noexcept {
        if (!vietnamese) return ObjectiveName(kind);
        switch (kind) {
        case ObjectiveKind::ElementalDragon: return "Rồng Nguyên Tố";
        case ObjectiveKind::DragonSoul: return "Linh Hồn Rồng";
        case ObjectiveKind::ElderDragon: return "Rồng Ngàn Tuổi";
        case ObjectiveKind::RiftHerald: return "Sứ Giả Khe Nứt";
        case ObjectiveKind::VoidGrubs: return "Sâu Hư Không";
        case ObjectiveKind::Baron: return "Baron Nashor";
        case ObjectiveKind::Scuttle: return "Cua Kỳ Cục";
        default: return "Mục tiêu lớn";
        }
    }

    float EffectiveIconScale() const noexcept {
        return std::clamp(settings_.iconScale, 0.50f, 2.00f);
    }

    void DrawScreenHud(const char* windowId,
                       const char* title,
                       const char* subtitle,
                       ImTextureID headerIcon,
                       std::uint32_t headerAccent,
                       const HudCardEntry* entries,
                       std::size_t entryCount,
                       float& storedX,
                       float& storedY,
                       HudDefaultAnchor defaultAnchor) {
        if (!windowId || !title || !entries || entryCount == 0 ||
            !ImGui::GetCurrentContext()) {
            return;
        }
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        if (!viewport || viewport->Size.x <= 0.0f ||
            viewport->Size.y <= 0.0f) {
            return;
        }

        const bool horizontal = settings_.hudLayoutIndex == 1;
        const float scale = EffectiveIconScale();
        const float margin = 10.0f;
        const float headerHeight = 38.0f;
        const float gap = 7.0f;
        const float verticalWidth = std::max(360.0f, 338.0f + 22.0f * scale);
        const float verticalRowHeight = std::max(54.0f, 40.0f * scale + 14.0f);
        const float tileWidth = std::max(178.0f, 158.0f + 20.0f * scale);
        const float tileHeight = std::max(78.0f, 55.0f + 23.0f * scale);

        std::size_t columns = 1;
        std::size_t rows = entryCount;
        float panelWidth = verticalWidth;
        float panelHeight = headerHeight + margin +
            verticalRowHeight * static_cast<float>(entryCount) +
            gap * static_cast<float>(entryCount > 0 ? entryCount - 1 : 0) +
            margin;
        if (horizontal) {
            const float available = std::max(200.0f, viewport->Size.x - 48.0f);
            const std::size_t maximumColumns = std::max<std::size_t>(
                1, static_cast<std::size_t>(
                    std::floor((available + gap) / (tileWidth + gap))));
            columns = std::min(entryCount, maximumColumns);
            rows = (entryCount + columns - 1) / columns;
            panelWidth = margin * 2.0f +
                tileWidth * static_cast<float>(columns) +
                gap * static_cast<float>(columns > 0 ? columns - 1 : 0);
            panelHeight = headerHeight + margin +
                tileHeight * static_cast<float>(rows) +
                gap * static_cast<float>(rows > 0 ? rows - 1 : 0) +
                margin;
        }

        const float viewLeft = viewport->Pos.x;
        const float viewTop = viewport->Pos.y;
        const float viewRight = viewLeft + viewport->Size.x;
        const float viewBottom = viewTop + viewport->Size.y;
        if (!std::isfinite(storedX) || !std::isfinite(storedY) ||
            storedX < viewLeft || storedY < viewTop) {
            switch (defaultAnchor) {
            case HudDefaultAnchor::TopRight:
                storedX = viewRight - panelWidth - 24.0f;
                storedY = viewTop + 72.0f;
                break;
            case HudDefaultAnchor::BottomLeft:
                storedX = viewLeft + 24.0f;
                storedY = viewBottom - panelHeight - 96.0f;
                break;
            case HudDefaultAnchor::BottomCenter:
                storedX = viewLeft +
                    (viewport->Size.x - panelWidth) * 0.5f;
                storedY = viewBottom - panelHeight - 72.0f;
                break;
            default:
                storedX = viewLeft + 24.0f;
                storedY = viewTop + 120.0f;
                break;
            }
        }
        storedX = std::clamp(
            storedX, viewLeft + 6.0f,
            std::max(viewLeft + 6.0f, viewRight - panelWidth - 6.0f));
        storedY = std::clamp(
            storedY, viewTop + 6.0f,
            std::max(viewTop + 6.0f, viewBottom - panelHeight - 6.0f));

        ImGui::SetNextWindowPos(ImVec2(storedX, storedY), ImGuiCond_Always);
        ImGui::SetNextWindowSize(
            ImVec2(panelWidth, panelHeight), ImGuiCond_Always);
        const ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoTitleBar |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoScrollbar |
            ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoBackground |
            ImGuiWindowFlags_NoNav;
        ImVec2 panelPosition(storedX, storedY);
        bool hovered = false;
        bool active = false;
        if (ImGui::Begin(windowId, nullptr, flags)) {
            panelPosition = ImGui::GetWindowPos();
            ImGui::SetCursorPos(ImVec2(0.0f, 0.0f));
            ImGui::InvisibleButton(
                "##HudDragSurface", ImVec2(panelWidth, panelHeight));
            hovered = ImGui::IsItemHovered();
            active = ImGui::IsItemActive();
            if (active && ImGui::IsMouseDragging(0, 0.0f)) {
                const ImVec2 delta = ImGui::GetIO().MouseDelta;
                storedX += delta.x;
                storedY += delta.y;
                storedX = std::clamp(
                    storedX, viewLeft + 6.0f,
                    std::max(viewLeft + 6.0f,
                             viewRight - panelWidth - 6.0f));
                storedY = std::clamp(
                    storedY, viewTop + 6.0f,
                    std::max(viewTop + 6.0f,
                             viewBottom - panelHeight - 6.0f));
            }
            if (hovered || active) {
                ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
            }
        }
        ImGui::End();

        const SDK::Vector2 panelMin(panelPosition.x, panelPosition.y);
        const SDK::Vector2 panelMax(
            panelPosition.x + panelWidth,
            panelPosition.y + panelHeight);
        DrawRectFilled(
            SDK::Vector2(panelMin.x + 4.0f, panelMin.y + 5.0f),
            SDK::Vector2(panelMax.x + 4.0f, panelMax.y + 5.0f),
            0x66000000u, 10.0f);
        DrawRectFilled(panelMin, panelMax, 0xED0C141Fu, 9.0f);
        DrawRectFilled(
            panelMin,
            SDK::Vector2(panelMax.x, panelMin.y + headerHeight),
            0xF21A2735u, 9.0f);
        DrawRectFilled(
            panelMin,
            SDK::Vector2(panelMin.x + 4.0f, panelMax.y),
            headerAccent, 9.0f);

        const float headerBaseIcon = 20.0f;
        const float headerIconWidth = headerBaseIcon * scale;
        const SDK::Vector2 headerCenter(
            panelMin.x + margin + headerIconWidth * 0.5f,
            panelMin.y + headerHeight * 0.5f);
        const bool hasHeaderIcon = DrawIcon(
            headerCenter, headerIcon, headerBaseIcon);
        const float headerTextX = panelMin.x + margin +
            (hasHeaderIcon ? headerIconWidth + 8.0f : 0.0f);
        DrawTextSmall(
            SDK::Vector2(headerTextX, panelMin.y + 9.0f),
            title, 0xFFF2F7FFu, false, 14.0f);
        if (subtitle && subtitle[0]) {
            DrawTextSmall(
                SDK::Vector2(headerTextX, panelMin.y + 24.0f),
                subtitle, 0xFF8EA4B8u, false, 9.0f);
        }
        DrawTextSmall(
            SDK::Vector2(panelMax.x - 12.0f, panelMin.y + 13.0f),
            settings_.vietnamese ? "KÉO" : "DRAG",
            hovered || active ? 0xFFFFFFFFu : 0xFF6F8294u,
            true, 9.0f);

        const float contentX = panelMin.x + margin;
        const float contentY = panelMin.y + headerHeight + margin;
        for (std::size_t index = 0; index < entryCount; ++index) {
            float cardX = contentX;
            float cardY = contentY;
            float cardWidth = verticalWidth - margin * 2.0f;
            float cardHeight = verticalRowHeight;
            if (horizontal) {
                const std::size_t column = index % columns;
                const std::size_t row = index / columns;
                cardX += static_cast<float>(column) * (tileWidth + gap);
                cardY += static_cast<float>(row) * (tileHeight + gap);
                cardWidth = tileWidth;
                cardHeight = tileHeight;
            } else {
                cardY += static_cast<float>(index) *
                    (verticalRowHeight + gap);
            }

            const HudCardEntry& entry = entries[index];
            const SDK::Vector2 cardMin(cardX, cardY);
            const SDK::Vector2 cardMax(
                cardX + cardWidth, cardY + cardHeight);
            DrawRectFilled(cardMin, cardMax, 0xE6162230u, 6.0f);
            DrawRectFilled(
                cardMin,
                SDK::Vector2(cardMin.x + 3.0f, cardMax.y),
                entry.accent, 6.0f);

            const float baseIcon = horizontal ? 27.0f : 29.0f;
            const float scaledIcon = baseIcon * scale;
            const SDK::Vector2 iconCenter(
                cardMin.x + 11.0f + scaledIcon * 0.5f,
                cardMin.y + cardHeight * 0.5f);
            const bool hasIcon = DrawIcon(
                iconCenter, entry.icon, baseIcon);
            if (!hasIcon) {
                DrawRectFilled(
                    SDK::Vector2(
                        iconCenter.x - scaledIcon * 0.5f,
                        iconCenter.y - scaledIcon * 0.5f),
                    SDK::Vector2(
                        iconCenter.x + scaledIcon * 0.5f,
                        iconCenter.y + scaledIcon * 0.5f),
                    0xFF26384Au, 5.0f);
                DrawTextSmall(
                    iconCenter, "!", entry.accent, true, 13.0f);
            }

            const float textX = cardMin.x + 18.0f + scaledIcon;
            const std::size_t titleLimit = horizontal ? 19u : 42u;
            const std::size_t detailLimit = horizontal ? 26u : 58u;
            char titleText[96] = {};
            char detailText[128] = {};
            TruncateHudText(
                entry.title, titleText, sizeof(titleText), titleLimit);
            TruncateHudText(
                entry.detail, detailText, sizeof(detailText), detailLimit);
            DrawTextSmall(
                SDK::Vector2(textX, cardMin.y + 10.0f),
                titleText, 0xFFF0F5FAu, false, 12.0f);
            DrawTextSmall(
                SDK::Vector2(textX, cardMin.y + 27.0f),
                detailText, 0xFF9EB0C0u, false, 9.0f);

            if (entry.badge[0]) {
                const float badgeWidth = std::clamp(
                    14.0f + static_cast<float>(std::strlen(entry.badge)) * 5.5f,
                    34.0f, horizontal ? 76.0f : 98.0f);
                const SDK::Vector2 badgeMin(
                    cardMax.x - badgeWidth - 8.0f,
                    cardMax.y - 19.0f);
                const SDK::Vector2 badgeMax(
                    cardMax.x - 8.0f,
                    cardMax.y - 6.0f);
                DrawRectFilled(badgeMin, badgeMax, 0xCC243445u, 5.0f);
                DrawTextSmall(
                    SDK::Vector2(
                        (badgeMin.x + badgeMax.x) * 0.5f,
                        badgeMin.y + 2.0f),
                    entry.badge, entry.accent, true, 8.0f);
            }
        }
    }

    void HandleAudioAlerts() {
        if (!settings_.audioOnly) return;
        const auto& alerts = awareness_.Store().Alerts();
        const AlertState* newest = nullptr;
        for (std::size_t i = 0; i < alerts.Size(); ++i) {
            const AlertState& alert = alerts.At(i);
            if (alert.priority < 45 || alert.expiresAt <= awareness_.Now()) {
                continue;
            }
            if (!newest || alert.at > newest->at) newest = &alert;
        }
        if (!newest ||
            (newest->at < lastAudioAlertAt_) ||
            (newest->at == lastAudioAlertAt_ &&
             newest->id == lastAudioAlertId_)) {
            return;
        }
        MessageBeep(newest->priority >= 70
                        ? MB_ICONHAND : MB_ICONEXCLAMATION);
        lastAudioAlertAt_ = newest->at;
        lastAudioAlertId_ = newest->id;
    }

    void DrawPanel() {
        const bool vi = settings_.vietnamese;
        const ImTextureID alertIcon =
            ResolveIcon("awareness_alert", false);

        if (settings_.drawAlertCenter) {
            std::array<HudCardEntry, 8> entries{};
            std::size_t count = 0;
            if (hasCandidate_ && count < entries.size()) {
                char title[96] = {};
                char detail[192] = {};
                char badge[40] = {};
                std::snprintf(
                    title, sizeof(title), "%s: %s",
                    vi ? "Đề xuất" : "Candidate",
                    CapabilityName(candidate_.capability));
                std::snprintf(
                    detail, sizeof(detail), "%s - %s",
                    ActionModeName(candidate_.mode), candidate_.reason);
                std::snprintf(
                    badge, sizeof(badge), "%s %d%%",
                    ConfidenceName(candidate_.confidence),
                    static_cast<int>(candidate_.confidence));
                SetHudEntry(
                    entries[count++],
                    IconForCapability(candidate_.capability),
                    0xFF55D9F4u, title, detail, badge);
            }

            const auto& alerts = awareness_.Store().Alerts();
            std::array<const AlertState*, 64> prioritized{};
            const std::size_t alertCount =
                AwarenessPresentationPolicy::PrioritizeAlerts(
                    alerts, awareness_.Now(), prioritized);
            const std::size_t shown = std::min<std::size_t>(
                5, std::min(alertCount, entries.size() - count));
            for (std::size_t i = 0; i < shown; ++i) {
                const AlertState& alert = *prioritized[i];
                const Evidence evidence{
                    Provenance::ObservedEvent,
                    alert.confidence, alert.at,
                    alert.expiresAt, alert.id
                };
                const EvidenceVisualStyle style =
                    AwarenessPresentationPolicy::StyleFor(evidence);
                char badge[40] = {};
                std::snprintf(
                    badge, sizeof(badge), "%s",
                    ConfidenceName(alert.confidence));
                SetHudEntry(
                    entries[count++], alertIcon,
                    alert.priority >= 70
                        ? 0xFFFF5263u : style.color,
                    alert.title,
                    settings_.streamerMode ? "" : alert.detail,
                    badge);
            }
            if (count == 0) {
                SetHudEntry(
                    entries[count++], alertIcon, 0xFF58C98Bu,
                    vi ? "Không có cảnh báo" : "No active alerts",
                    vi ? "Tình huống hiện tại đang ổn định"
                       : "The current situation is stable",
                    vi ? "AN TOÀN" : "CLEAR");
            }
            char subtitle[96] = {};
            std::snprintf(
                subtitle, sizeof(subtitle), "%s · %.1fs",
                RuntimeModeName(awareness_.Mode()), awareness_.Now());
            DrawScreenHud(
                "##AwarenessAlertHud",
                vi ? "Trung tâm cảnh báo" : "Alert Center",
                subtitle, alertIcon, 0xFF55D9F4u,
                entries.data(), count,
                settings_.alertPanelX, settings_.alertPanelY,
                HudDefaultAnchor::TopRight);
        }

        if (settings_.drawObjectives) {
            std::array<HudCardEntry, 12> entries{};
            std::size_t count = 0;
            const auto& objectives = awareness_.Store().Objectives();
            for (std::size_t i = 0;
                 i < objectives.Size() && count < entries.size(); ++i) {
                const ObjectiveState& objective = objectives.At(i);
                const ExposureDecision exposure = VisibilityGuard::CanExpose(
                    objective.evidence, awareness_.Mode(),
                    objective.visible, false);
                if (!exposure.allowed) continue;

                const float now = awareness_.Now();
                char detail[128] = {};
                if ((objective.status == ObjectiveStatus::NotSpawned ||
                     objective.status == ObjectiveStatus::SpawningSoon) &&
                    objective.spawnAt > now) {
                    std::snprintf(
                        detail, sizeof(detail),
                        vi ? "Xuất hiện sau %.0f giây"
                           : "Spawns in %.0f seconds",
                        objective.spawnAt - now);
                } else if ((objective.status == ObjectiveStatus::Dead ||
                            objective.status == ObjectiveStatus::Respawning) &&
                           objective.respawnAt > now) {
                    std::snprintf(
                        detail, sizeof(detail),
                        vi ? "Hồi sinh sau %.0f giây"
                           : "Respawns in %.0f seconds",
                        objective.respawnAt - now);
                } else if (objective.status == ObjectiveStatus::AliveVisible ||
                           objective.status == ObjectiveStatus::InCombatVisible) {
                    const float healthPercent = objective.maxHealth > 0.0f
                        ? 100.0f * objective.health / objective.maxHealth
                        : 0.0f;
                    std::snprintf(
                        detail, sizeof(detail),
                        objective.status == ObjectiveStatus::InCombatVisible
                            ? (vi ? "Đang giao tranh · %.0f%% máu"
                                  : "In combat · %.0f%% health")
                            : (vi ? "Đang sống · %.0f%% máu"
                                  : "Alive · %.0f%% health"),
                        healthPercent);
                } else {
                    CopyText(
                        detail,
                        vi ? "Trạng thái đang được ước tính"
                           : "State is currently estimated");
                }
                char badge[40] = {};
                std::snprintf(
                    badge, sizeof(badge), "%s",
                    ConfidenceName(objective.evidence.confidence));
                const std::uint32_t accent =
                    objective.status == ObjectiveStatus::Dead ||
                    objective.status == ObjectiveStatus::Respawning
                        ? 0xFF8290A0u
                        : (objective.status == ObjectiveStatus::InCombatVisible
                               ? 0xFFFF6D57u : 0xFFFFC857u);
                SetHudEntry(
                    entries[count++], IconForObjective(objective.kind),
                    accent,
                    ObjectiveDisplayName(objective.kind, vi),
                    detail, badge);
            }
            if (count == 0) {
                SetHudEntry(
                    entries[count++], alertIcon, 0xFF8290A0u,
                    vi ? "Chưa có dữ liệu mục tiêu"
                       : "No objective data",
                    vi ? "Đang chờ quan sát hợp lệ"
                       : "Waiting for a valid observation",
                    vi ? "CHỜ" : "WAIT");
            }
            DrawScreenHud(
                "##AwarenessObjectiveHud",
                vi ? "Theo dõi mục tiêu lớn" : "Objective Tracker",
                vi ? "Thời gian và trạng thái quan sát"
                   : "Observed timers and states",
                IconForObjective(ObjectiveKind::Baron),
                0xFFFFC857u,
                entries.data(), count,
                settings_.objectivePanelX, settings_.objectivePanelY,
                HudDefaultAnchor::TopLeft);
        }

        if (settings_.drawInsights) {
            std::array<HudCardEntry, 12> entries{};
            std::size_t count = 0;
            const WaveState& wave = awareness_.Store().Wave();
            if (settings_.drawWave && wave.evidence.IsKnown() &&
                count < entries.size()) {
                char detail[160] = {};
                char badge[40] = {};
                std::snprintf(
                    detail, sizeof(detail), "%s · %d-%d · lệch %.0f",
                    wave.classification, wave.allyMinions,
                    wave.enemyMinions, wave.laneBias);
                if (!vi) {
                    std::snprintf(
                        detail, sizeof(detail), "%s · %d-%d · bias %.0f",
                        wave.classification, wave.allyMinions,
                        wave.enemyMinions, wave.laneBias);
                }
                std::snprintf(
                    badge, sizeof(badge), "%s",
                    ConfidenceName(wave.evidence.confidence));
                SetHudEntry(
                    entries[count++], alertIcon, 0xFF77D6A6u,
                    vi ? "Trạng thái đợt lính" : "Wave state",
                    detail, badge);
            }

            const RecallAdvice& recall = awareness_.Insights().recall;
            if (recall.evidence.IsKnown() && count < entries.size()) {
                char badge[40] = {};
                std::snprintf(badge, sizeof(badge), "%d/100", recall.score);
                SetHudEntry(
                    entries[count++], alertIcon,
                    recall.recommended ? 0xFF55D98Au : 0xFFD8B85Au,
                    vi ? "Thời điểm Biến Về" : "Recall timing",
                    recall.reason, badge);
            }

            const WardEfficiencySummary& wards =
                awareness_.Insights().wardEfficiency;
            if (wards.evidence.IsKnown() && count < entries.size()) {
                char detail[128] = {};
                char badge[40] = {};
                std::snprintf(
                    detail, sizeof(detail),
                    vi ? "%d mắt hoạt động · %d gần mục tiêu"
                       : "%d active wards · %d near objectives",
                    wards.active, wards.objectiveCoverage);
                std::snprintf(badge, sizeof(badge), "%d/100", wards.score);
                SetHudEntry(
                    entries[count++], IconForWard(false), 0xFF59CFA2u,
                    vi ? "Hiệu quả tầm nhìn" : "Vision efficiency",
                    detail, badge);
            }

            const auto& setups = awareness_.Insights().objectiveSetups;
            for (std::size_t i = 0;
                 i < setups.Size() && i < 3 && count < entries.size(); ++i) {
                const ObjectiveSetupAssessment& setup = setups.At(i);
                char detail[160] = {};
                char badge[40] = {};
                std::snprintf(
                    detail, sizeof(detail),
                    vi ? "Đồng minh %d · Địch %d · Tầm nhìn %d"
                       : "Allies %d · Enemies %d · Vision %d",
                    setup.nearbyAllies, setup.nearbyEnemies,
                    setup.alliedVision);
                std::snprintf(badge, sizeof(badge), "%d/100", setup.score);
                SetHudEntry(
                    entries[count++], IconForObjective(setup.kind),
                    setup.score >= 70 ? 0xFF55D98Au
                                      : (setup.score >= 45
                                             ? 0xFFFFC857u
                                             : 0xFFFF6D57u),
                    ObjectiveDisplayName(setup.kind, vi),
                    detail, badge);
            }

            awareness_.Store().ForEachChampion(
                [&](const ChampionState& ally) {
                    if (count >= entries.size() ||
                        (!ally.ally && !ally.local) || ally.dead) {
                        return;
                    }
                    const ObservedSpell* ultimate = nullptr;
                    for (std::size_t i = 0; i < ally.spellCount; ++i) {
                        if (ally.spells[i].slot == 3) {
                            ultimate = &ally.spells[i];
                            break;
                        }
                    }
                    if (!ultimate || !ultimate->evidence.IsKnown()) return;
                    char cooldown[32] = {};
                    FormatCooldown(
                        ultimate, ally.visible || ally.ally,
                        cooldown, sizeof(cooldown));
                    const char* name = settings_.streamerMode
                        ? (vi ? "Đồng minh" : "Ally")
                        : (ally.name[0] ? ally.name
                                        : (vi ? "Đồng minh" : "Ally"));
                    char title[96] = {};
                    std::snprintf(
                        title, sizeof(title),
                        vi ? "Chiêu cuối của %s" : "%s ultimate", name);
                    SetHudEntry(
                        entries[count++], IconForSpell(ally, *ultimate),
                        0xFF72AFFFu, title,
                        vi ? "Trạng thái hồi chiêu đã quan sát"
                           : "Observed cooldown state",
                        cooldown);
                });

            const DeathRecapSummary& recap =
                awareness_.Insights().deathRecap;
            if (recap.evidence.IsKnown() && count < entries.size()) {
                char detail[160] = {};
                std::snprintf(
                    detail, sizeof(detail),
                    vi ? "%.0f sát thương · %d nguồn · %.1f giây"
                       : "%.0f damage · %d sources · %.1f seconds",
                    recap.totalDamage, recap.sourceCount,
                    std::max(0.0f, recap.deathAt - recap.firstDamageAt));
                SetHudEntry(
                    entries[count++], alertIcon, 0xFFFF6F83u,
                    vi ? "Tóm tắt lần hạ gục" : "Death recap",
                    detail, ConfidenceName(recap.evidence.confidence));
            }

            if (count == 0) {
                SetHudEntry(
                    entries[count++], alertIcon, 0xFF8290A0u,
                    vi ? "Chưa có phân tích chiến thuật"
                       : "No tactical insights",
                    vi ? "Đang chờ đủ dữ liệu quan sát"
                       : "Waiting for enough observed data",
                    vi ? "CHỜ" : "WAIT");
            }
            DrawScreenHud(
                "##AwarenessInsightHud",
                vi ? "Phân tích chiến thuật" : "Tactical Insights",
                vi ? "Ưu tiên thông tin có thể hành động"
                   : "Actionable observed information",
                alertIcon, 0xFF77D6A6u,
                entries.data(), count,
                settings_.insightPanelX, settings_.insightPanelY,
                HudDefaultAnchor::BottomCenter);
        }
    }

    static void FormatCooldown(const ObservedSpell* spell, bool visible,
                               char* out, std::size_t outSize) noexcept {
        if (!out || outSize == 0) return;
        if (!spell || !spell->evidence.IsKnown()) {
            std::snprintf(out, outSize, "?");
            return;
        }
        if (!visible && spell->cooldownKind == CooldownKind::Unknown) {
            std::snprintf(out, outSize, "?[%c]",
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        if (spell->ready && visible) {
            std::snprintf(out, outSize, "up");
            return;
        }
        if (spell->cooldownKind == CooldownKind::RangeEstimated &&
            spell->cooldownMax - spell->cooldownMin > 0.5f) {
            std::snprintf(out, outSize, "~%.0f-%.0f[%c]",
                          spell->cooldownMin, spell->cooldownMax,
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        if (spell->cooldownRemaining > 0.01f) {
            std::snprintf(out, outSize, "%s%.0f[%c]",
                          visible ? "" : "~", spell->cooldownRemaining,
                          ConfidenceName(spell->evidence.confidence)[0]);
            return;
        }
        std::snprintf(out, outSize, visible ? "up" : "?[%c]",
                      ConfidenceName(spell->evidence.confidence)[0]);
    }

    static const char* CrowdControlName(CrowdControl control) noexcept {
        switch (control) {
        case CrowdControl::Stun: return "stun";
        case CrowdControl::Root: return "root";
        case CrowdControl::Charm: return "charm";
        case CrowdControl::Fear: return "fear";
        case CrowdControl::Taunt: return "taunt";
        case CrowdControl::Blind: return "blind";
        case CrowdControl::Silence: return "silence";
        case CrowdControl::Polymorph: return "polymorph";
        case CrowdControl::Slow: return "slow";
        case CrowdControl::Sleep: return "sleep";
        case CrowdControl::Suppression: return "suppression";
        case CrowdControl::Airborne: return "airborne";
        case CrowdControl::Stasis: return "stasis";
        case CrowdControl::Grounded: return "grounded";
        default: return "no-CC";
        }
    }

    static const char* ObjectiveName(ObjectiveKind kind) noexcept {
        switch (kind) {
        case ObjectiveKind::ElementalDragon: return "dragon";
        case ObjectiveKind::DragonSoul: return "dragon soul";
        case ObjectiveKind::ElderDragon: return "elder";
        case ObjectiveKind::RiftHerald: return "herald";
        case ObjectiveKind::VoidGrubs: return "grubs";
        case ObjectiveKind::Baron: return "baron";
        case ObjectiveKind::Scuttle: return "scuttle";
        default: return "objective";
        }
    }


    static void ExportDecisionLogCallback(void* context) {
        auto* plugin = static_cast<AwarenessActivatorPlugin*>(context);
        if (plugin) plugin->ExportDecisionLog();
    }

    static void ApplyChampionPresetCallback(void* context) {
        auto* plugin = static_cast<AwarenessActivatorPlugin*>(context);
        if (plugin) plugin->ApplyChampionPreset();
    }

    void ApplyChampionPreset() {
        const ChampionState* player = nullptr;
        awareness_.Store().ForEachChampion(
            [&](const ChampionState& champion) {
                if (champion.local) player = &champion;
            });
        if (!player) return;
        const AwarenessPresetProfile profile =
            AwarenessPresetService::ForChampion(player->championId);
        settings_.drawEnemyHud = profile.enemyHud;
        settings_.drawCombatState = profile.combat;
        settings_.drawWards = profile.wards;
        settings_.drawMinimapWards = profile.wards;
        settings_.drawJungle = profile.jungle;
        settings_.drawMinimapJungle = profile.jungle;
        settings_.drawObjectives = profile.objectives;
        settings_.drawMinimapObjectives = profile.objectives;
        settings_.drawActivityHeatmap = profile.heatmaps;
        settings_.defensiveHorizon = profile.defensiveHorizon;
        menu_.SyncMenuFromSettings();
    }

    void LoadPatchOverrides() {
        char path[MAX_PATH] = {};
        const HMODULE module = SDK::Variables::Detail::CurrentModule();
        const DWORD length = module
            ? GetModuleFileNameA(module, path, static_cast<DWORD>(sizeof(path)))
            : 0;
        if (length == 0 || length >= sizeof(path)) return;
        char* slash = std::max(strrchr(path, '\\'), strrchr(path, '/'));
        if (!slash) return;
        strcpy_s(slash + 1,
                 sizeof(path) - static_cast<std::size_t>(slash + 1 - path),
                 "NightSharp.AwarenessActivator.csv");
        const std::size_t applied =
            PatchOverrideLoader::LoadCsv(awareness_.Registry(), path);
        if (applied > 0) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] loaded %zu patch override rows from %s",
                applied, path);
        }
    }

    void ExportDecisionLog() const {
        char directory[MAX_PATH] = {};
        if (!GetTempPathA(
                static_cast<DWORD>(sizeof(directory)), directory)) {
            return;
        }
        char decisions[MAX_PATH] = {};
        char telemetry[MAX_PATH] = {};
        strcpy_s(decisions, directory);
        strcpy_s(telemetry, directory);
        strcat_s(
            decisions, sizeof(decisions),
            "NightSharp_awareness_decisions.jsonl");
        strcat_s(
            telemetry, sizeof(telemetry),
            "NightSharp_awareness_telemetry.json");
        if (awareness_.Log().ExportJsonl(decisions)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] decision log exported: %s",
                decisions);
        }
        if (LocalTelemetryExporter::Export(
                telemetry, awareness_.Store(), awareness_.Insights(),
                awareness_.Log(), settings_.streamerMode)) {
            NightSharpDebug::Logf(
                "[AwarenessActivator] local telemetry exported: %s",
                telemetry);
        }
    }

    AwarenessEngine awareness_{};
    mutable AwarenessDiagnostics diagnostics_{};
    SdkObservationBridge bridge_{};
    ActivatorEngine activator_{};
    ActivatorSettings settings_{};
    AwarenessActivatorMenu menu_{};
    AwarenessImGuiRenderer renderer_{};
    mutable std::array<IconCacheEntry, 128> iconCache_{};
    mutable std::size_t iconCacheCount_ = 0;
    bool iconLoadAttempted_ = false;
    PresentationLayerGuard layerGuard_{};
    ActionRequest candidate_{};
    bool loaded_ = false;
    bool hasCandidate_ = false;
    std::uint64_t updateFrame_ = 0;
    std::uint64_t renderFrame_ = 0;
    float attackRange_ = 0.0f;
    Point3 localPosition_{};
    bool hasLocalPosition_ = false;
    float lastAttackRangeAt_ = -1000.0f;
    float lastActivatorEvalAt_ = -1000.0f;
    float lastAudioPollAt_ = -1000.0f;
    float lastCombatForecastAt_ = -1000.0f;
    ThreatForecast renderForecast_{};
    bool hasRenderForecast_ = false;
    std::uint32_t lastAttemptFingerprint_ = 0;
    float lastAttemptAt_ = -100.0f;
    std::uint32_t lastAudioAlertId_ = 0;
    float lastAudioAlertAt_ = -100.0f;
};

} // namespace NightSharp::Companion
