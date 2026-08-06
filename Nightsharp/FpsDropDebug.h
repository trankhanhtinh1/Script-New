#pragma once

#include "imgui/imgui.h"

#include "DebugLog.h"
#include "SectionProfiler.h"
#include <Windows.h>
#include <DbgHelp.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <iterator>

#pragma comment(lib, "dbghelp.lib")

namespace NightSharpPerf {

inline void ResolveSymbolOrAddress(const void* ptr, char* outBuf, size_t bufSize) {
    if (!ptr || !outBuf || bufSize == 0) return;
    outBuf[0] = '\0';

    HANDLE process = GetCurrentProcess();
    static bool symInitialized = false;
    if (!symInitialized) {
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        SymInitialize(process, nullptr, TRUE);
        symInitialized = true;
    }

    const DWORD64 addr = reinterpret_cast<DWORD64>(ptr);
    char symbolStorage[sizeof(SYMBOL_INFO) + MAX_SYM_NAME] = {};
    auto* symbol = reinterpret_cast<SYMBOL_INFO*>(symbolStorage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
    symbol->MaxNameLen = MAX_SYM_NAME;

    DWORD64 displacement = 0;
    if (SymFromAddr(process, addr, &displacement, symbol) && symbol->Name && symbol->Name[0]) {
        _snprintf_s(outBuf, bufSize, _TRUNCATE, "%s+0x%llX", symbol->Name, static_cast<unsigned long long>(displacement));
    } else {
        NightSharpDebug::DescribeAddress(const_cast<void*>(ptr), outBuf, bufSize);
    }
}

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
    double MaxUpdateMs = 0.0;
    double MaxRenderMs = 0.0;
    double MaxTotalMs = 0.0;
    double TotalUpdateMs = 0.0;
    unsigned UpdateHits = 0;
    unsigned RenderHits = 0;
    unsigned MenuHits = 0;
    unsigned TotalHits = 0;
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
inline bool Enabled = true;
inline bool OverlayVisible = true;
inline bool LogEnabled = true;
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

struct SlowestHandlerInfo {
    const char* EventName = "";
    const void* HandlerPtr = nullptr;
    double MaxMs = 0.0;
};
inline SlowestHandlerInfo SlowestHandler = {};

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
    for (int i = 0; i < PluginCount; ++i) {
        PluginSamples[i].UpdateMs = 0.0;
        PluginSamples[i].RenderMs = 0.0;
        PluginSamples[i].MenuMs = 0.0;
    }
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
        sample->TotalUpdateMs += ms;
        if (ms > sample->MaxUpdateMs) {
            sample->MaxUpdateMs = ms;
        }
    } else if (std::strcmp(stage, "render") == 0) {
        sample->RenderMs += ms;
        sample->RenderHits += 1;
        if (ms > sample->MaxRenderMs) {
            sample->MaxRenderMs = ms;
        }
    } else if (std::strcmp(stage, "menu") == 0) {
        sample->MenuMs += ms;
        sample->MenuHits += 1;
    }

    const double currentTotal = sample->UpdateMs + sample->RenderMs + sample->MenuMs;
    if (currentTotal > sample->MaxTotalMs) {
        sample->MaxTotalMs = currentTotal;
    }
    sample->TotalHits += 1;
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
    if (!Enabled) {
        return;
    }
    if (ms > SlowestHandler.MaxMs) {
        SlowestHandler.EventName = eventName ? eventName : "";
        SlowestHandler.HandlerPtr = handler;
        SlowestHandler.MaxMs = ms;
    }

    if (!LogEnabled || ms < SlowEventMs) {
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

inline const EventSample* SlowestEventPeak() {
    const EventSample* best = nullptr;
    double maxMs = 0.0;
    for (int i = 0; i < EventCount; ++i) {
        if (EventSamples[i].MaxMs > maxMs) {
            maxMs = EventSamples[i].MaxMs;
            best = &EventSamples[i];
        }
    }
    return best;
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
inline const PluginSample* SlowestPeakPlugin() {
    const PluginSample* best = nullptr;
    double maxMs = 0.0;
    for (int i = 0; i < PluginCount; ++i) {
        if (PluginSamples[i].MaxUpdateMs > maxMs) {
            maxMs = PluginSamples[i].MaxUpdateMs;
            best = &PluginSamples[i];
        }
    }
    return best;
}

inline void ResetAllStats() {
    FrameCount = 0;
    SlowFrameCount = 0;
    MaxFrameMs = 0.0;
    SlowestHandler = {};
    EventCount = 0;
    for (int i = 0; i < static_cast<int>(std::size(EventSamples)); ++i) {
        EventSamples[i] = {};
    }
    SectionCount = 0;
    for (int i = 0; i < kMaxSections; ++i) {
        SectionStats[i] = {};
    }
    for (int i = 0; i < PluginCount; ++i) {
        PluginSamples[i].UpdateMs = 0.0;
        PluginSamples[i].RenderMs = 0.0;
        PluginSamples[i].MenuMs = 0.0;
        PluginSamples[i].MaxUpdateMs = 0.0;
        PluginSamples[i].MaxRenderMs = 0.0;
        PluginSamples[i].MaxTotalMs = 0.0;
        PluginSamples[i].TotalUpdateMs = 0.0;
        PluginSamples[i].UpdateHits = 0;
        PluginSamples[i].RenderHits = 0;
        PluginSamples[i].MenuHits = 0;
        PluginSamples[i].TotalHits = 0;
    }
}

// Renders the collected timing stats with ImGui. Assumes an ImGui window is
// already active — used both by the floating RenderOverlay and by the in-menu
// profiler panel (NightSharpMenu Debug Info section).
inline void DrawStatsBody() {
    if (ImGui::Button("Clear / Reset Stats")) {
        ResetAllStats();
    }
    ImGui::SameLine();
    ImGui::Text(" (FPS: %.0f | Frame %.2f ms | Max %.2f ms)",
        ImGui::GetIO().Framerate, LastFrameMs, MaxFrameMs);

    ImGui::Text("Core: %.2f ms | Update: %.2f ms | Render: %.2f ms | Menu: %.2f ms",
        LastCoreTickMs, LastUpdateMs, LastRenderMs, LastMenuMs);

    const auto* slowestEv = SlowestEventPeak();
    if (slowestEv && slowestEv->MaxMs > 0.01) {
        ImGui::TextColored(
            ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
            "PEAK Event Spike: %s -> MAX: %.2f ms (Hits: %u, Last: %.2f ms)",
            slowestEv->Name ? slowestEv->Name : "None",
            slowestEv->MaxMs,
            slowestEv->Hits,
            slowestEv->LastMs);
    } else {
        ImGui::TextDisabled("PEAK Event Spike: None recorded yet");
    }

    if (SlowestHandler.MaxMs > 0.01) {
        char handlerSym[256] = {};
        ResolveSymbolOrAddress(SlowestHandler.HandlerPtr, handlerSym, sizeof(handlerSym));
        ImGui::TextColored(
            ImVec4(1.0f, 0.5f, 0.2f, 1.0f),
            "Slowest Handler Callback: %s [%s] -> MAX: %.2f ms",
            SlowestHandler.EventName ? SlowestHandler.EventName : "None",
            handlerSym[0] ? handlerSym : "Unknown",
            SlowestHandler.MaxMs);
    } else {
        ImGui::TextDisabled("Slowest Handler Callback: None recorded yet");
    }

    const auto* peakSlowest = SlowestPeakPlugin();
    if (peakSlowest && peakSlowest->MaxUpdateMs > 0.01) {
        const double avgUpdate = peakSlowest->UpdateHits > 0
            ? (peakSlowest->TotalUpdateMs / peakSlowest->UpdateHits)
            : 0.0;
        ImGui::Text(
            "Plugin Direct OnUpdate Loop Max: %s -> MAX: %.2f ms (Avg: %.2f ms)",
            (peakSlowest->Name && peakSlowest->Name[0]) ? peakSlowest->Name : peakSlowest->InternalId,
            peakSlowest->MaxUpdateMs,
            avgUpdate);
    } else {
        ImGui::TextDisabled("Plugin Direct OnUpdate Loop Max: N/A");
    }

    // Section 1: Native & Hotpath Code Probes (SectionStats)
    if (SectionCount > 0) {
        ImGui::Separator();
        ImGui::Text("Native & Hotpath Event Probes");
        if (ImGui::BeginTable("SectionStatsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Probe Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Total ms", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            int indices[kMaxSections];
            int n = std::min(SectionCount, kMaxSections);
            for (int i = 0; i < n; ++i) indices[i] = i;
            std::sort(indices, indices + n, [](int a, int b) {
                return std::strcmp(SectionStats[a].Name ? SectionStats[a].Name : "",
                                   SectionStats[b].Name ? SectionStats[b].Name : "") < 0;
            });

            for (int i = 0; i < n; ++i) {
                const auto& stat = SectionStats[indices[i]];
                if (!stat.Name || stat.Hits == 0) continue;
                const double avgMs = stat.Hits > 0 ? (stat.TotalMs / stat.Hits) : 0.0;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(stat.Name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", stat.Hits);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", avgMs);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", stat.MaxMs);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.2f", stat.TotalMs);
            }
            ImGui::EndTable();
        }
    }

    // Section 2: Handler Event Timings (EventSamples)
    if (EventCount > 0) {
        ImGui::Separator();
        ImGui::Text("Event Handler Timings");
        if (ImGui::BeginTable("EventStatsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Event Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Hits", ImGuiTableColumnFlags_WidthFixed, 60.0f);
            ImGui::TableSetupColumn("Last ms", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableHeadersRow();

            int indices[64];
            int n = std::min(EventCount, static_cast<int>(std::size(EventSamples)));
            for (int i = 0; i < n; ++i) indices[i] = i;
            std::sort(indices, indices + n, [](int a, int b) {
                return std::strcmp(EventSamples[a].Name ? EventSamples[a].Name : "",
                                   EventSamples[b].Name ? EventSamples[b].Name : "") < 0;
            });

            for (int i = 0; i < n; ++i) {
                const auto& sample = EventSamples[indices[i]];
                if (!sample.Name || sample.Hits == 0) continue;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(sample.Name);
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%u", sample.Hits);
                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%.3f", sample.LastMs);
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.3f", sample.MaxMs);
            }
            ImGui::EndTable();
        }
    }

    // Section 3: Plugin Timings & Peak Spikes
    if (PluginCount > 0) {
        ImGui::Separator();
        ImGui::Text("Plugin Timings & Peak Spikes");
        if (ImGui::BeginTable("PluginStatsTable", 6, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
            ImGui::TableSetupColumn("Plugin Name", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Current ms", ImGuiTableColumnFlags_WidthFixed, 65.0f);
            ImGui::TableSetupColumn("Peak Update", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Avg Update", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableSetupColumn("Peak Render", ImGuiTableColumnFlags_WidthFixed, 75.0f);
            ImGui::TableSetupColumn("Peak Total", ImGuiTableColumnFlags_WidthFixed, 70.0f);
            ImGui::TableHeadersRow();

            int pIndices[128];
            int pCount = std::min(PluginCount, 128);
            for (int i = 0; i < pCount; ++i) pIndices[i] = i;
            std::sort(pIndices, pIndices + pCount, [](int a, int b) {
                return std::strcmp(PluginSamples[a].Name ? PluginSamples[a].Name : "",
                                   PluginSamples[b].Name ? PluginSamples[b].Name : "") < 0;
            });

            for (int i = 0; i < pCount; ++i) {
                const auto& sample = PluginSamples[pIndices[i]];
                const double currentTotal = sample.UpdateMs + sample.RenderMs + sample.MenuMs;
                const double avgUpdate = sample.UpdateHits > 0 ? (sample.TotalUpdateMs / sample.UpdateHits) : 0.0;

                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextUnformatted(sample.Name ? sample.Name : "");
                ImGui::TableSetColumnIndex(1);
                ImGui::Text("%.2f", currentTotal);
                ImGui::TableSetColumnIndex(2);
                if (sample.MaxUpdateMs > 3.0) {
                    ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f), "%.2f ms", sample.MaxUpdateMs);
                } else {
                    ImGui::Text("%.2f ms", sample.MaxUpdateMs);
                }
                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%.2f ms", avgUpdate);
                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%.2f ms", sample.MaxRenderMs);
                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%.2f ms", sample.MaxTotalMs);
            }
            ImGui::EndTable();
        }
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
