#pragma once

#include "../../DebugLog.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>

namespace NightSharp::Companion {

class AwarenessDiagnostics final {
public:
    enum class Stage : std::uint8_t {
        UpdateFrame = 0,
        BridgeContext,
        BridgeHeroes,
        BridgeLocalHero,
        BridgeWards,
        BridgeWave,
        BridgeObjectives,
        BridgeJungle,
        BridgeRegistry,
        BridgeInsights,
        ActivatorEvaluate,
        PluginAttackRange,
        PluginAudio,
        PluginCombatForecast,
        PluginExecute,
        RenderFrame,
        RenderBegin,
        RenderLivePositions,
        RenderWorld,
        RenderWorldHeatmaps,
        RenderWorldChampions,
        RenderWorldWards,
        RenderWorldObjectives,
        RenderWorldJungle,
        RenderWorldThreats,
        RenderWorldCombat,
        RenderWorldInsights,
        RenderMinimap,
        RenderEnemyHud,
        RenderPanel,
        Count
    };

    struct StageMetric final {
        std::uint64_t samples = 0;
        std::uint64_t totalUs = 0;
        std::uint64_t maxUs = 0;
        std::uint64_t objects = 0;
        std::uint64_t drawn = 0;
        std::uint64_t culled = 0;
        std::uint64_t calls = 0;
        std::uint64_t work = 0;
    };

    class Scope final {
    public:
        Scope() noexcept = default;

        Scope(AwarenessDiagnostics* owner, Stage stage) noexcept
            : owner_(owner), stage_(stage),
              startUs_(owner ? owner->NowUs() : 0),
              active_(owner && owner->Active()) {}

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        Scope(Scope&& other) noexcept
            : owner_(other.owner_), stage_(other.stage_),
              startUs_(other.startUs_), objects_(other.objects_),
              drawn_(other.drawn_), culled_(other.culled_),
              work_(other.work_), active_(other.active_) {
            other.owner_ = nullptr;
            other.active_ = false;
        }

        Scope& operator=(Scope&& other) noexcept {
            if (this != &other) {
                Finish();
                owner_ = other.owner_;
                stage_ = other.stage_;
                startUs_ = other.startUs_;
                objects_ = other.objects_;
                drawn_ = other.drawn_;
                culled_ = other.culled_;
                work_ = other.work_;
                active_ = other.active_;
                other.owner_ = nullptr;
                other.active_ = false;
            }
            return *this;
        }

        ~Scope() { Finish(); }

        void SetCounts(std::uint64_t objects,
                       std::uint64_t drawn = 0,
                       std::uint64_t culled = 0,
                       std::uint64_t work = 0) noexcept {
            objects_ = objects;
            drawn_ = drawn;
            culled_ = culled;
            work_ = work == 0 ? objects : work;
        }

        void AddCounts(std::uint64_t objects,
                       std::uint64_t drawn = 0,
                       std::uint64_t culled = 0,
                       std::uint64_t work = 0) noexcept {
            objects_ += objects;
            drawn_ += drawn;
            culled_ += culled;
            work_ += work == 0 ? objects : work;
        }

        void Finish() noexcept {
            if (!active_ || !owner_) return;
            active_ = false;
            const std::uint64_t nowUs = owner_->NowUs();
            owner_->Record(
                stage_, nowUs >= startUs_ ? nowUs - startUs_ : 0,
                objects_, drawn_, culled_, work_);
        }

    private:
        AwarenessDiagnostics* owner_ = nullptr;
        Stage stage_ = Stage::Count;
        std::uint64_t startUs_ = 0;
        std::uint64_t objects_ = 0;
        std::uint64_t drawn_ = 0;
        std::uint64_t culled_ = 0;
        std::uint64_t work_ = 0;
        bool active_ = false;
    };

    AwarenessDiagnostics() = default;

    void Configure(bool enabled,
                   bool consoleLog,
                   bool verbose,
                   std::uint32_t reportEveryFrames,
                   float slowFrameMs) noexcept {
        const bool nextActive = enabled || consoleLog;
        const std::uint32_t nextInterval =
            (std::max)(1u, (std::min)(reportEveryFrames, 3600u));
        const std::uint64_t nextSlowUs =
            static_cast<std::uint64_t>(
                (std::clamp)(slowFrameMs, 1.0f, 1000.0f) * 1000.0f);
        if (nextActive != active_ || consoleLog != consoleLog_ ||
            verbose != verbose_ || nextInterval != reportEveryFrames_ ||
            nextSlowUs != slowFrameUs_) {
            ResetWindow();
        }
        active_ = nextActive;
        consoleLog_ = consoleLog;
        verbose_ = verbose;
        reportEveryFrames_ = nextInterval;
        slowFrameUs_ = nextSlowUs;
    }

    bool Active() const noexcept { return active_; }
    bool ConsoleLogEnabled() const noexcept { return consoleLog_; }
    bool Verbose() const noexcept { return verbose_; }

    void BeginFrame(bool render, std::uint64_t frameIndex) noexcept {
        if (!Active()) return;
        if (render) {
            renderOpen_ = true;
            renderStartUs_ = NowUs();
            renderFrameIndex_ = frameIndex;
        } else {
            updateOpen_ = true;
            updateStartUs_ = NowUs();
            updateFrameIndex_ = frameIndex;
        }
    }

    void EndFrame(bool render) noexcept {
        if (!Active()) return;
        const std::uint64_t nowUs = NowUs();
        if (render && renderOpen_) {
            renderOpen_ = false;
            const std::uint64_t elapsed =
                nowUs >= renderStartUs_ ? nowUs - renderStartUs_ : 0;
            Record(Stage::RenderFrame, elapsed, 0, 0, 0, 0);
            ++renderFrames_;
            renderTotalUs_ += elapsed;
            renderMaxUs_ = (std::max)(renderMaxUs_, elapsed);
            lastRenderUs_ = elapsed;
            lastRenderFrame_ = renderFrameIndex_;
            if (consoleLog_ && ShouldReport(elapsed)) {
                EmitReport();
                ResetWindow();
            }
        } else if (!render && updateOpen_) {
            updateOpen_ = false;
            const std::uint64_t elapsed =
                nowUs >= updateStartUs_ ? nowUs - updateStartUs_ : 0;
            Record(Stage::UpdateFrame, elapsed, 0, 0, 0, 0);
            ++updateFrames_;
            updateTotalUs_ += elapsed;
            updateMaxUs_ = (std::max)(updateMaxUs_, elapsed);
        }
    }

    Scope Begin(Stage stage) noexcept {
        return Scope(Active() ? this : nullptr, stage);
    }

    const StageMetric& Metric(Stage stage) const noexcept {
        return metrics_[Index(stage)];
    }

    std::uint64_t LastRenderUs() const noexcept { return lastRenderUs_; }
    std::uint64_t LastReportFrame() const noexcept { return lastReportFrame_; }

    static const char* StageName(Stage stage) noexcept {
        return StageInfoFor(stage).name;
    }

    static const char* Complexity(Stage stage) noexcept {
        return StageInfoFor(stage).complexity;
    }

private:
    struct StageInfo final {
        const char* name;
        const char* complexity;
    };
    static constexpr std::size_t Index(Stage stage) noexcept {
        return static_cast<std::size_t>(stage);
    }
    static constexpr std::size_t kStageCount =
        static_cast<std::size_t>(Stage::Count);

    static const StageInfo& StageInfoFor(Stage stage) noexcept {
        static constexpr std::array<StageInfo, kStageCount> infos = {{
            { "Update.Frame", "O(1)" },
            { "Bridge.Context", "O(1)" },
            { "Bridge.Heroes", "O(H)" },
            { "Bridge.LocalHero", "O(1)" },
            { "Bridge.Wards", "O(W*S_w)" },
            { "Bridge.Wave", "O(M_a+M_e)" },
            { "Bridge.Objectives", "O(J*O_s)" },
            { "Bridge.Jungle", "O(J*S_j)" },
            { "Bridge.Registry", "O(I)" },
            { "Bridge.Insights", "O(A+C)" },
            { "Activator.Evaluate", "O(C+W+O+T)" },
            { "Plugin.AttackRange", "O(1)" },
            { "Plugin.Audio", "O(1)" },
            { "Plugin.CombatForecast", "O(C)" },
            { "Plugin.Execute", "O(1)" },
            { "Render.Frame", "O(1)" },
            { "Render.Begin", "O(1)" },
            { "Render.LivePositions", "O(H)" },
            { "Render.World", "O(C+W+O+J+T)" },
            { "Render.World.Heatmaps", "O(A+V)" },
            { "Render.World.Champions", "O(E)" },
            { "Render.World.Wards", "O(W)" },
            { "Render.World.Objectives", "O(O)" },
            { "Render.World.Jungle", "O(J)" },
            { "Render.World.Threats", "O(T)" },
            { "Render.World.Combat", "O(C_t)" },
            { "Render.World.Insights", "O(A)" },
            { "Render.Minimap", "O(E+W+O+J+P)" },
            { "Render.EnemyHud", "O(E*6)" },
            { "Render.Panel", "O(A+C)" }
        }};
        const std::size_t index = Index(stage);
        return index < infos.size() ? infos[index] : infos[0];
    }

    static std::uint64_t NowUs() noexcept {
        using Clock = std::chrono::steady_clock;
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                Clock::now().time_since_epoch()).count());
    }

    bool ShouldReport(std::uint64_t renderUs) const noexcept {
        return renderUs >= slowFrameUs_ ||
               (reportEveryFrames_ > 0 &&
                (renderFrameIndex_ % reportEveryFrames_) == 0);
    }

    void Record(Stage stage,
                std::uint64_t elapsedUs,
                std::uint64_t objects,
                std::uint64_t drawn,
                std::uint64_t culled,
                std::uint64_t work) noexcept {
        const std::size_t index = Index(stage);
        if (index >= metrics_.size()) return;
        StageMetric& metric = metrics_[index];
        ++metric.samples;
        metric.totalUs += elapsedUs;
        metric.maxUs = (std::max)(metric.maxUs, elapsedUs);
        metric.objects += objects;
        metric.drawn += drawn;
        metric.culled += culled;
        ++metric.calls;
        metric.work += work;
    }

    void EmitReport() noexcept {
        const double avgRenderMs = renderFrames_ > 0
            ? static_cast<double>(renderTotalUs_) /
              static_cast<double>(renderFrames_) / 1000.0
            : 0.0;
        const double avgUpdateMs = updateFrames_ > 0
            ? static_cast<double>(updateTotalUs_) /
              static_cast<double>(updateFrames_) / 1000.0
            : 0.0;
        const double fps = avgRenderMs > 0.0
            ? 1000.0 / avgRenderMs : 0.0;

        NightSharpDebug::Logf(
            "[<b-cyan>AwarenessFPS</b-cyan>][<b-yellow>Summary</b-yellow>] "
            "frame=%llu windowRender=%llu windowUpdate=%llu "
            "avgRenderMs=%.3f maxRenderMs=%.3f avgUpdateMs=%.3f "
            "fps=%.1f slowThresholdMs=%.3f",
            static_cast<unsigned long long>(renderFrameIndex_),
            static_cast<unsigned long long>(renderFrames_),
            static_cast<unsigned long long>(updateFrames_),
            avgRenderMs,
            static_cast<double>(renderMaxUs_) / 1000.0,
            avgUpdateMs,
            fps,
            static_cast<double>(slowFrameUs_) / 1000.0);

        for (std::size_t i = 0; i < metrics_.size(); ++i) {
            const Stage stage = static_cast<Stage>(i);
            const StageMetric& metric = metrics_[i];
            if (metric.samples == 0) continue;
            const double avgUs = static_cast<double>(metric.totalUs) /
                                 static_cast<double>(metric.samples);
            NightSharpDebug::Logf(
                "[<b-cyan>AwarenessFPS</b-cyan>][<cyan>Stage</cyan>] "
                "name=<yellow>%s</yellow> samples=%llu avgUs=%.1f "
                "maxUs=%llu objects=%llu drawn=%llu culled=%llu "
                "calls=%llu work=%llu complexity=<magenta>%s</magenta>",
                StageName(stage),
                static_cast<unsigned long long>(metric.samples),
                avgUs,
                static_cast<unsigned long long>(metric.maxUs),
                static_cast<unsigned long long>(metric.objects),
                static_cast<unsigned long long>(metric.drawn),
                static_cast<unsigned long long>(metric.culled),
                static_cast<unsigned long long>(metric.calls),
                static_cast<unsigned long long>(metric.work),
                Complexity(stage));
        }
        lastReportFrame_ = renderFrameIndex_;
    }

    void ResetWindow() noexcept {
        for (StageMetric& metric : metrics_) metric = {};
        renderFrames_ = 0;
        updateFrames_ = 0;
        renderTotalUs_ = 0;
        updateTotalUs_ = 0;
        renderMaxUs_ = 0;
        updateMaxUs_ = 0;
    }

    std::array<StageMetric, kStageCount> metrics_{};
    bool active_ = false;
    bool consoleLog_ = false;
    bool verbose_ = false;
    std::uint32_t reportEveryFrames_ = 60;
    std::uint64_t slowFrameUs_ = 8000;
    bool renderOpen_ = false;
    bool updateOpen_ = false;
    std::uint64_t renderStartUs_ = 0;
    std::uint64_t updateStartUs_ = 0;
    std::uint64_t renderFrameIndex_ = 0;
    std::uint64_t updateFrameIndex_ = 0;
    std::uint64_t renderFrames_ = 0;
    std::uint64_t updateFrames_ = 0;
    std::uint64_t renderTotalUs_ = 0;
    std::uint64_t updateTotalUs_ = 0;
    std::uint64_t renderMaxUs_ = 0;
    std::uint64_t updateMaxUs_ = 0;
    std::uint64_t lastRenderUs_ = 0;
    std::uint64_t lastRenderFrame_ = 0;
    std::uint64_t lastReportFrame_ = 0;
};

} // namespace NightSharp::Companion
