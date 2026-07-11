#pragma once

#include <Windows.h>
#include <DbgHelp.h>

#include <cstdint>
#include <string>

namespace nsmonitor {

struct DumpRequest {
    HANDLE process{};
    DWORD pid{};
    DWORD threadId{};
    PEXCEPTION_POINTERS remoteExceptionPointers{};
    std::wstring path;
};

struct DumpResult {
    bool ok{};
    DWORD error{};
    std::uint64_t bytes{};
};

DumpResult WriteExternalDump(const DumpRequest& request);

} // namespace nsmonitor
