#pragma once

// ============================================================================
// SectionProfiler — self-contained aggregated timing for hot-path regions.
// ----------------------------------------------------------------------------
// Zero SDK dependencies (only Windows.h + cstdio) so it can be safely
// included from low-level core headers (CoreControl.h, CoreObjects.h, etc.)
// without risking circular includes. The richer FpsDropDebug.h wraps these
// same globals and adds ImGui overlay / WndProc hotkey wiring.
//
// Hot-path usage is a single QueryPerformanceCounter pair + a counter add —
// ~40-80ns of overhead per probe, negligible next to the millisecond-scale
// regions being measured. Aggregates (hits/total/max/min) are dumped to a log
// file periodically by DumpSections().
//
// Usage:
//   NS_PROFILE("orb.GetTarget");          // RAII scoped probe
//   NS_PROFILE("cc.IssueMove");
//
// Sections only record while SectionsActive == true, so the default (off)
// state adds zero cost on non-orbwalker frames.
// ============================================================================

#include <Windows.h>
#include <cstdio>
#include <cstring>

namespace NightSharpPerf {

inline constexpr int kMaxSections = 64;

struct SectionStat {
    const char* Name = "";
    unsigned Hits = 0;
    double TotalMs = 0.0;
    double MaxMs = 0.0;
    double MinMs = 1e9;
};

inline SectionStat SectionStats[kMaxSections] = {};
inline int SectionCount = 0;
inline DWORD LastSectionDumpTick = 0;
inline DWORD SectionDumpIntervalMs = 1000;   // how often to write summary to log
inline double SectionDumpMinMs = 0.005;      // skip regions below this avg ms
inline bool SectionsActive = false;          // master switch (toggled on orbwalker hold)
inline bool SectionLogEnabled = true;        // mirrors FpsDropDebug::LogEnabled

inline LARGE_INTEGER SectionFrequency = {};

inline void SectionEnsureFrequency() {
    if (SectionFrequency.QuadPart == 0) {
        QueryPerformanceFrequency(&SectionFrequency);
    }
}

inline LARGE_INTEGER SectionNow() {
    SectionEnsureFrequency();
    LARGE_INTEGER now = {};
    QueryPerformanceCounter(&now);
    return now;
}

inline double SectionMsBetween(const LARGE_INTEGER& start, const LARGE_INTEGER& end) {
    SectionEnsureFrequency();
    return static_cast<double>(end.QuadPart - start.QuadPart) * 1000.0 /
           static_cast<double>(SectionFrequency.QuadPart);
}

inline double SectionMsSince(const LARGE_INTEGER& start) {
    return SectionMsBetween(start, SectionNow());
}

inline SectionStat* FindOrAddSection(const char* name) {
    if (!name) {
        return nullptr;
    }
    for (int i = 0; i < SectionCount; ++i) {
        if (SectionStats[i].Name &&
            std::strcmp(SectionStats[i].Name, name) == 0) {
            return &SectionStats[i];
        }
    }
    if (SectionCount >= kMaxSections) {
        return nullptr;
    }
    auto& stat = SectionStats[SectionCount++];
    stat = {};
    stat.Name = name;
    return &stat;
}

inline void RecordSection(const char* name, double ms) {
    if (!SectionsActive || !name) {
        return;
    }
    auto* stat = FindOrAddSection(name);
    if (!stat) {
        return;
    }
    stat->Hits += 1;
    stat->TotalMs += ms;
    if (ms > stat->MaxMs) stat->MaxMs = ms;
    if (ms < stat->MinMs) stat->MinMs = ms;
}

// Writes a sorted-by-total-ms summary to a fixed log path. Only writes when
// SectionsActive so idle frames produce no log spam.
inline void SectionAppendLog(const char* path, const char* text) {
    if (!text || !SectionLogEnabled) {
        return;
    }
    HANDLE file = CreateFileA(
        path,
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

inline void DumpSections(const char* logPath = "C:\\Users\\Public\\nightsharp_orbwalker_profile.txt") {
    if (!SectionsActive || !SectionLogEnabled) {
        return;
    }
    const DWORD now = GetTickCount();
    if (now - LastSectionDumpTick < SectionDumpIntervalMs) {
        return;
    }
    LastSectionDumpTick = now;

    SectionStat snapshot[kMaxSections] = {};
    int n = SectionCount;
    if (n > kMaxSections) n = kMaxSections;
    for (int i = 0; i < n; ++i) snapshot[i] = SectionStats[i];

    for (int i = 1; i < n; ++i) {
        SectionStat key = snapshot[i];
        int j = i - 1;
        while (j >= 0 && snapshot[j].TotalMs < key.TotalMs) {
            snapshot[j + 1] = snapshot[j];
            --j;
        }
        snapshot[j + 1] = key;
    }

    char header[256] = {};
    std::snprintf(
        header,
        sizeof(header),
        "\r\n[PerfSection] tick=%lu hot-path dump (interval=%lums)\r\n",
        static_cast<unsigned long>(now),
        static_cast<unsigned long>(SectionDumpIntervalMs));
    SectionAppendLog(logPath, header);

    for (int i = 0; i < n; ++i) {
        const SectionStat& s = snapshot[i];
        if (s.Hits == 0) continue;
        const double avg = s.TotalMs / static_cast<double>(s.Hits);
        if (avg < SectionDumpMinMs && s.MaxMs < 0.05) continue;
        char line[256] = {};
        std::snprintf(
            line,
            sizeof(line),
            "  %-34s hits=%-6u total=%-9.3f avg=%-8.4f max=%-8.3f min=%-8.4f\r\n",
            s.Name ? s.Name : "",
            s.Hits,
            s.TotalMs,
            avg,
            s.MaxMs,
            (s.MinMs > 1e8 ? 0.0 : s.MinMs));
        SectionAppendLog(logPath, line);
    }
    SectionAppendLog(logPath, "[PerfSection] --- end dump ---\r\n");
}

inline void ResetSections() {
    for (int i = 0; i < SectionCount; ++i) {
        SectionStats[i].Hits = 0;
        SectionStats[i].TotalMs = 0.0;
        SectionStats[i].MaxMs = 0.0;
        SectionStats[i].MinMs = 1e9;
    }
    LastSectionDumpTick = 0;
}

class SectionProbe {
public:
    explicit SectionProbe(const char* name)
        : name_(name), start_(SectionNow()) {}

    ~SectionProbe() {
        RecordSection(name_, SectionMsSince(start_));
    }

private:
    const char* name_ = "";
    LARGE_INTEGER start_ = {};
};

} // namespace NightSharpPerf

#define NS_PROFILE(name) ::NightSharpPerf::SectionProbe _nsProf_##__LINE__(name)
