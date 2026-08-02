#pragma once

#include "imgui/imgui.h"

#include "SectionProfiler.h"
#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

namespace NightSharpPerf {

struct PhaseSample {
    const char* Name = "";
    double Ms = 0.0;
};

struct PluginSample {
    const char* Name = "";
    const char* InternalId = "";
    double UpdateMs = 0.0;
    double RenderMs = 0.0;
    double MenuMs = 0.0;
    unsigned UpdateHits = 0;
    unsigned RenderHits = 0;
    unsigned MenuHits = 0;
};

struct EventSample {
    const char* Name = "";
    double LastMs = 0.0;
    double MaxMs = 0.0;
    double TotalMs = 0.0;
    unsigned Hits = 0;
    DWORD LastTick = 0;
};

// Off by default. Enabled is the master switch gating all frame/plugin/event
// timing collection; re-enable from the menu (Debug & Profiler > Profiler) or
// with F10 for the overlay.
inline bool Enabled = false;
inline bool OverlayVisible = false;
inline bool LogEnabled = false;
inline double SlowFrameMs = 33.0;
inline double SlowPhaseMs = 6.0;
inline double SlowPluginMs = 2.0;
inline double SlowEventMs = 2.0;

inline LARGE_INTEGER Frequency = {};
inline LARGE_INTEGER FrameStart = {};
inline DWORD FrameTick = 0;
inline DWORD LastLogTick = 0;
inline DWORD LastEventLogTick = 0;
inline DWORD LastToggleTick = 0;
inline double LastFrameMs = 0.0;
inline double MaxFrameMs = 0.0;
inline double LastUpdateMs = 0.0;
inline double LastRenderMs = 0.0;
inline double LastMenuMs = 0.0;
inline double LastPresentMs = 0.0;
inline double LastSleepMs = 0.0;
inline double LastCoreTickMs = 0.0;
inline int SlowFrameCount = 0;
inline int FrameCount = 0;
inline PhaseSample PhaseSamples[32] = {};
inline int PhaseCount = 0;
inline PluginSample PluginSamples[128] = {};
inline int PluginCount = 0;
inline EventSample EventSamples[64] = {};
inline int EventCount = 0;

inline void EnsureFrequency() {
    if (Frequency.QuadPart == 0) {
        QueryPerformanceFrequency(&Frequency);
    }
}

inline LARGE_INTEGER Now() {
    EnsureFrequency();
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    return now;
}

inline double MsBetween(const LARGE_INTEGER& start, const LARGE_INTEGER& end) {
    EnsureFrequency();
    return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
           static_cast<double>(Frequency.QuadPart);
}

inline double MsSince(const LARGE_INTEGER& start) {
    return MsBetween(start, Now());
}

inline void ResetFrame() {
    PhaseCount = 0;
    PluginCount = 0;
    LastUpdateMs = 0.0;
    LastRenderMs = 0.0;
    LastMenuMs = 0.0;
    LastPresentMs = 0.0;
    LastSleepMs = 0.0;
    LastCoreTickMs = 0.0;
}

inline void BeginFrame() {
    SectionsActive = Enabled;
    SectionLogEnabled = LogEnabled;
    if (!Enabled) {
        return;
    }
    DumpSections();
    EnsureFrequency();
    QueryPerformanceCounter(&FrameStart);
    FrameTick = GetTickCount();
    ++FrameCount;
    ResetFrame();
}

inline void AddPhase(const char* name, double ms) {
    if (!Enabled || !name) {
        return;
    }
    if (PhaseCount < static_cast<int>(std::size(PhaseSamples))) {
        PhaseSamples[PhaseCount++] = { name, ms };
    }

    if (std::strcmp(name, "CoreRuntime::TickRead") == 0) {
        LastCoreTickMs = ms;
    } else if (std::strcmp(name, "PluginManager::OnUpdate") == 0) {
        LastUpdateMs = ms;
    } else if (std::strcmp(name, "PluginManager::OnRender") == 0) {
        LastRenderMs = ms;
    } else if (std::strcmp(name, "NightSharpMenu::Render") == 0) {
        LastMenuMs = ms;
    } else if (std::strcmp(name, "Present") == 0) {
        LastPresentMs = ms;
    } else if (std::strcmp(name, "Sleep") == 0) {
        LastSleepMs = ms;
    }
}

inline PluginSample* FindOrAddPlugin(const char* internalId, const char* name) {
    if (!Enabled) {
        return nullptr;
    }
    internalId = internalId ? internalId : "";
    name = name ? name : internalId;
    for (int i = 0; i < PluginCount; ++i) {
        if (PluginSamples[i].InternalId &&
            std::strcmp(PluginSamples[i].InternalId, internalId) == 0) {
            return &PluginSamples[i];
        }
    }
    if (PluginCount >= static_cast<int>(std::size(PluginSamples))) {
        return nullptr;
    }
    auto& sample = PluginSamples[PluginCount++];
    sample = {};
    sample.Name = name;
    sample.InternalId = internalId;
    return &sample;
}

inline void AddPluginTiming(const char* stage,
                            const char* internalId,
                            const char* name,
                            double ms) {
    auto* sample = FindOrAddPlugin(internalId, name);
    if (!sample || !stage) {
        return;
    }
    if (std::strcmp(stage, "update") == 0) {
        sample->UpdateMs += ms;
        sample->UpdateHits += 1;
    } else if (std::strcmp(stage, "render") == 0) {
        sample->RenderMs += ms;
        sample->RenderHits += 1;
    } else if (std::strcmp(stage, "menu") == 0) {
        sample->MenuMs += ms;
        sample->MenuHits += 1;
    }
}

inline void AppendLog(const char* text) {
    if (!text || !LogEnabled) {
        return;
    }
    HANDLE file = CreateFileA(
        "C:\\Users\\Public\\nightsharp_fps_drop_debug.txt",
        FILE_APPEND_DATA,
        FILE_SHARE_READ,
        nullptr,
        OPEN_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD written = 0;
    WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
    CloseHandle(file);
}

inline EventSample* FindOrAddEvent(const char* name) {
    if (!Enabled || !name) {
        return nullptr;
    }
    for (int i = 0; i < EventCount; ++i) {
        if (EventSamples[i].Name &&
            std::strcmp(EventSamples[i].Name, name) == 0) {
            return &EventSamples[i];
        }
    }
    if (EventCount >= static_cast<int>(std::size(EventSamples))) {
        return nullptr;
    }
    auto& sample = EventSamples[EventCount++];
    sample = {};
    sample.Name = name;
    return &sample;
}

inline void AddEventTiming(const char* name, double ms, int handlerCount) {
    auto* sample = FindOrAddEvent(name);
    if (!sample) {
        return;
    }

    sample->LastMs = ms;
    sample->MaxMs = std::max(sample->MaxMs, ms);
    sample->TotalMs += ms;
    sample->Hits += 1;
    sample->LastTick = GetTickCount();

    const DWORD now = sample->LastTick;
    if (LogEnabled && ms >= SlowEventMs && now - LastEventLogTick >= 500) {
        LastEventLogTick = now;
        char line[512] = {};
        std::snprintf(
            line,
            sizeof(line),
            "[PerfDrop] slow-event tick=%lu event=%s handlers=%d ms=%.2f max=%.2f hits=%u\r\n",
            static_cast<unsigned long>(now),
            name,
            handlerCount,
            ms,
            sample->MaxMs,
            sample->Hits);
        AppendLog(line);
    }
}

inline void AddEventHandlerTiming(const char* eventName,
                                  int handlerIndex,
                                  const void* handler,
                                  double ms) {
    if (!Enabled || !LogEnabled || ms < SlowEventMs) {
        return;
    }

    const DWORD now = GetTickCount();
    if (now - LastEventLogTick < 250) {
        return;
    }
    LastEventLogTick = now;

    char line[512] = {};
    std::snprintf(
        line,
        sizeof(line),
        "[PerfDrop] slow-handler tick=%lu event=%s index=%d handler=0x%p ms=%.2f\r\n",
        static_cast<unsigned long>(now),
        eventName ? eventName : "",
        handlerIndex,
        handler,
        ms);
    AppendLog(line);
}

inline const PluginSample* SlowestPlugin() {
    const PluginSample* best = nullptr;
    double bestMs = 0.0;
    for (int i = 0; i < PluginCount; ++i) {
        const auto& sample = PluginSamples[i];
        const double total = sample.UpdateMs + sample.RenderMs + sample.MenuMs;
        if (total > bestMs) {
            bestMs = total;
            best = &sample;
        }
    }
    return best;
}

inline void LogSlowFrame() {
    const DWORD now = GetTickCount();
    if (!LogEnabled || now - LastLogTick < 750) {
        return;
    }
    LastLogTick = now;

    char line[4096] = {};
    const auto* slowest = SlowestPlugin();
    std::snprintf(
        line,
        sizeof(line),
        "[PerfDrop] tick=%lu frame=%.2fms core=%.2f update=%.2f render=%.2f menu=%.2f present=%.2f sleep=%.2f slowestPlugin='%s' update=%.2f render=%.2f menu=%.2f\r\n",
        static_cast<unsigned long>(now),
        LastFrameMs,
        LastCoreTickMs,
        LastUpdateMs,
        LastRenderMs,
        LastMenuMs,
        LastPresentMs,
        LastSleepMs,
        slowest ? slowest->Name : "",
        slowest ? slowest->UpdateMs : 0.0,
        slowest ? slowest->RenderMs : 0.0,
        slowest ? slowest->MenuMs : 0.0);
    AppendLog(line);

    for (int i = 0; i < PhaseCount; ++i) {
        if (PhaseSamples[i].Ms < SlowPhaseMs) {
            continue;
        }
        std::snprintf(
            line,
            sizeof(line),
            "  phase %s %.2fms\r\n",
            PhaseSamples[i].Name ? PhaseSamples[i].Name : "",
            PhaseSamples[i].Ms);
        AppendLog(line);
    }

    for (int i = 0; i < PluginCount; ++i) {
        const auto& sample = PluginSamples[i];
        const double total = sample.UpdateMs + sample.RenderMs + sample.MenuMs;
        if (total < SlowPluginMs) {
            continue;
        }
        std::snprintf(
            line,
            sizeof(line),
            "  plugin name='%s' id='%s' total=%.2fms update=%.2fms(%u) render=%.2fms(%u) menu=%.2fms(%u)\r\n",
            sample.Name ? sample.Name : "",
            sample.InternalId ? sample.InternalId : "",
            total,
            sample.UpdateMs,
            sample.UpdateHits,
            sample.RenderMs,
            sample.RenderHits,
            sample.MenuMs,
            sample.MenuHits);
        AppendLog(line);
    }

    for (int i = 0; i < EventCount; ++i) {
        const auto& sample = EventSamples[i];
        if (GetTickCount() - sample.LastTick > 1500 || sample.LastMs < SlowEventMs) {
            continue;
        }
        std::snprintf(
            line,
            sizeof(line),
            "  event name='%s' last=%.2fms max=%.2fms hits=%u\r\n",
            sample.Name ? sample.Name : "",
            sample.LastMs,
            sample.MaxMs,
            sample.Hits);
        AppendLog(line);
    }
}

// Heartbeat: dumps the full phase + slow-event breakdown UNCONDITIONALLY every
// ~1s (not gated on SlowFrameMs), so IDLE frames — which never cross the slow
// threshold — are still captured. This is the evidence needed to see where the
// per-frame time goes before any combo/logic runs (overlay-thread phases +
// game-thread GameUpdate event ms).
inline DWORD LastHeartbeatTick = 0;

inline void LogHeartbeat() {
    if (!LogEnabled) {
        return;
    }
    const DWORD now = GetTickCount();
    if (now - LastHeartbeatTick < 1000) {
        return;
    }
    LastHeartbeatTick = now;

    char line[1024] = {};
    std::snprintf(
        line,
        sizeof(line),
        "[Heartbeat] tick=%lu frame=%.2fms fps=%.0f core=%.2f memhacks=%.2f update=%.2f render=%.2f draw=%.2f menu=%.2f present=%.2f sleep=%.2f\r\n",
        static_cast<unsigned long>(now),
        LastFrameMs,
        LastFrameMs > 0.0 ? 1000.0 / LastFrameMs : 0.0,
        LastCoreTickMs,
        // CoreMemoryHacks::Tick + DispatchDraw are captured as generic phases;
        // pull them from the phase array below rather than dedicated fields.
        0.0,
        LastUpdateMs,
        LastRenderMs,
        0.0,
        LastMenuMs,
        LastPresentMs,
        LastSleepMs);
    AppendLog(line);

    // All phases this frame (so CoreMemoryHacks::Tick / DispatchDraw / DispatchEndScene show up).
    for (int i = 0; i < PhaseCount; ++i) {
        std::snprintf(
            line, sizeof(line), "  phase %s %.2fms\r\n",
            PhaseSamples[i].Name ? PhaseSamples[i].Name : "", PhaseSamples[i].Ms);
        AppendLog(line);
    }

    // Recent game-thread events (GameUpdate etc.) — the handler cost that runs on
    // the game thread and caps in-game FPS at idle.
    const DWORD tnow = GetTickCount();
    for (int i = 0; i < EventCount; ++i) {
        const auto& s = EventSamples[i];
        if (tnow - s.LastTick > 1500) {
            continue;
        }
        std::snprintf(
            line, sizeof(line), "  event %s last=%.2fms max=%.2fms hits=%u\r\n",
            s.Name ? s.Name : "", s.LastMs, s.MaxMs, s.Hits);
        AppendLog(line);
    }
}

inline void EndFrame() {
    if (!Enabled) {
        return;
    }
    LastFrameMs = MsSince(FrameStart);
    MaxFrameMs = std::max(MaxFrameMs, LastFrameMs);
    if (LastFrameMs >= SlowFrameMs) {
        ++SlowFrameCount;
        LogSlowFrame();
    }
    LogHeartbeat();
}

// Renders the collected timing stats with ImGui. Assumes an ImGui window is
// already active — used both by the floating RenderOverlay and by the in-menu
// profiler panel (NightSharpMenu Debug Info section).
inline void DrawStatsBody() {
    ImGui::Text(
        "frame %.2f ms  max %.2f  slow %d/%d",
        LastFrameMs,
        MaxFrameMs,
        SlowFrameCount,
        FrameCount);
    ImGui::Text(
        "core %.2f  update %.2f  render %.2f  menu %.2f  present %.2f  sleep %.2f",
        LastCoreTickMs,
        LastUpdateMs,
        LastRenderMs,
        LastMenuMs,
        LastPresentMs,
        LastSleepMs);

    const auto* slowest = SlowestPlugin();
    if (slowest) {
        ImGui::Separator();
        ImGui::Text(
            "slowest plugin: %s  U %.2f / R %.2f / M %.2f",
            slowest->Name ? slowest->Name : "",
            slowest->UpdateMs,
            slowest->RenderMs,
            slowest->MenuMs);
    }

    ImGui::Separator();
    ImGui::Text("Recent slow events");
    const DWORD now = GetTickCount();
    int shownEvents = 0;
    for (int i = 0; i < EventCount && shownEvents < 8; ++i) {
        const auto& sample = EventSamples[i];
        if (now - sample.LastTick > 2500 || sample.LastMs < 0.05) {
            continue;
        }
        ImGui::Text(
            "%s: last %.2f ms max %.2f hits %u",
            sample.Name ? sample.Name : "",
            sample.LastMs,
            sample.MaxMs,
            sample.Hits);
        ++shownEvents;
    }

    ImGui::Separator();
    for (int i = 0; i < PluginCount; ++i) {
        const auto& sample = PluginSamples[i];
        const double total = sample.UpdateMs + sample.RenderMs + sample.MenuMs;
        if (total < 0.05) {
            continue;
        }
        ImGui::Text(
            "%s: %.2f ms (U %.2f R %.2f M %.2f)",
            sample.Name ? sample.Name : "",
            total,
            sample.UpdateMs,
            sample.RenderMs,
            sample.MenuMs);
    }
}

inline void RenderOverlay() {
    if (!Enabled || !OverlayVisible || !ImGui::GetCurrentContext()) {
        return;
    }

    ImGui::SetNextWindowBgAlpha(0.62f);
    ImGui::SetNextWindowPos(ImVec2(18.0f, 310.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin(
            "NightSharp FPS Drop Debug",
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize |
                ImGuiWindowFlags_NoCollapse |
                ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::End();
        return;
    }

    ImGui::Text("F10 overlay | log %s (menu)", LogEnabled ? "ON" : "OFF");
    DrawStatsBody();

    ImGui::End();
}

class ScopedTimer {
public:
    explicit ScopedTimer(const char* phase)
        : phase_(phase),
          start_(Enabled ? Now() : LARGE_INTEGER{}) {}

    ~ScopedTimer() {
        if (Enabled) {
            AddPhase(phase_, MsSince(start_));
        }
    }

private:
    const char* phase_ = "";
    LARGE_INTEGER start_ = {};
};

} // namespace NightSharpPerf

// WndProc-based hotkey handler (declared here after NightSharpPerf types to avoid circular include with Events.h)
#include "SDK/Core/Game.h"

namespace NightSharpPerf {
inline void ToggleHotkeysWndProc(SDK::Game::WndEventArgs& args) {
    if (args.Msg == WM_KEYDOWN) {
        const DWORD now = GetTickCount();
        if (args.WParam == VK_F10 && now - LastToggleTick >= 150) {
            if (!Enabled) {
                Enabled = true;
                OverlayVisible = true;
            } else {
                OverlayVisible = !OverlayVisible;
            }
            LastToggleTick = now;
        }
        // F11 log toggle removed — driven by the menu checkbox
        // "Ghi log ra file (.txt)" (NightSharpPerf::LogEnabled) instead.
    }
}

inline bool ToggleHotkeysWndProcInstalled = false;

inline void ToggleHotkeys() {
    if (!ToggleHotkeysWndProcInstalled) {
        SDK::Game::AddOnWndProc(&ToggleHotkeysWndProc);
        ToggleHotkeysWndProcInstalled = true;
    }
}
} // namespace NightSharpPerf
