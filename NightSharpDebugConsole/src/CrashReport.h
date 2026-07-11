#pragma once

#include "DumpWriter.h"
#include "IncidentModel.h"

#include <Windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace nsmonitor {

struct RemoteCrashData {
    bool valid{};
    DWORD threadId{};
    DWORD exceptionCode{};
    DWORD exceptionFlags{};
    std::uint64_t exceptionAddress{};
    std::uint64_t exceptionInfo[3]{};
    std::uint64_t rip{};
    std::uint64_t rsp{};
    std::uint64_t rbp{};
    std::uint64_t rax{};
    std::uint64_t rbx{};
    std::uint64_t rcx{};
    std::uint64_t rdx{};
    std::string moduleName;
    std::uint64_t moduleBase{};
    std::uint64_t moduleOffset{};
    std::string symbol;
    std::string sourceFile;
    DWORD sourceLine{};
    CONTEXT context{};
    std::vector<std::string> stack;
};

RemoteCrashData ReadRemoteCrashData(
    HANDLE process,
    DWORD pid,
    DWORD threadId,
    std::uintptr_t remoteExceptionPointers,
    std::uint64_t nightSharpBase,
    std::uint32_t nightSharpSize);

bool ResolveSymbolsAndStack(
    HANDLE process,
    const std::wstring& nightSharpImagePath,
    RemoteCrashData& crash);

std::string FormatIncidentReport(
    IncidentClass classification,
    nscrash::CaptureSource source,
    const nscrash::SharedState& state,
    const RemoteCrashData& crash,
    const DumpResult& dump,
    const std::wstring& dumpPath,
    DWORD exitCode);

bool WriteIncidentReport(
    const std::wstring& path,
    const std::string& report,
    DWORD& error);

} // namespace nsmonitor
