#pragma once

#include "../../NightSharpCrashProtocol/CrashProtocol.h"

#include <Windows.h>

#include <cstddef>
#include <string>

namespace nsmonitor {

enum class IncidentClass {
    HandledNightSharpException,
    UnhandledException,
    WerFallback,
    ForcedTerminationNoException,
    MonitorTimeout,
};

struct IncidentEvidence {
    bool bridgeCrash{};
    bool werDump{};
    bool processExited{};
    nscrash::CrashKind crashKind{nscrash::CrashKind::None};
    bool timedOut{};
};

IncidentClass ClassifyIncident(const IncidentEvidence& evidence);
const wchar_t* IncidentClassName(IncidentClass value);
std::wstring BuildArtifactBaseName(
    const SYSTEMTIME& time,
    DWORD pid,
    DWORD tid,
    const char* stage);
bool RotateCompletedIncidentSets(
    const std::wstring& directory,
    std::size_t keepCount,
    DWORD& error);

} // namespace nsmonitor
