#include "DumpWriter.h"

namespace nsmonitor {

DumpResult WriteExternalDump(const DumpRequest& request) {
    DumpResult result{};
    if (!request.process || request.pid == 0 || request.path.empty()) {
        result.error = ERROR_INVALID_PARAMETER;
        return result;
    }

    const std::wstring temporaryPath = request.path + L".tmp";
    HANDLE file = CreateFileW(
        temporaryPath.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        result.error = GetLastError();
        return result;
    }

    MINIDUMP_EXCEPTION_INFORMATION exceptionInfo{};
    exceptionInfo.ThreadId = request.threadId;
    exceptionInfo.ExceptionPointers = request.remoteExceptionPointers;
    exceptionInfo.ClientPointers = TRUE;

    const MINIDUMP_TYPE dumpType = static_cast<MINIDUMP_TYPE>(
        MiniDumpNormal |
        MiniDumpWithFullMemory |
        MiniDumpWithDataSegs |
        MiniDumpWithHandleData |
        MiniDumpWithIndirectlyReferencedMemory |
        MiniDumpWithPrivateReadWriteMemory |
        MiniDumpWithThreadInfo |
        MiniDumpWithUnloadedModules |
        MiniDumpWithProcessThreadData |
        MiniDumpWithFullMemoryInfo |
        MiniDumpWithModuleHeaders |
        MiniDumpIgnoreInaccessibleMemory);

    const BOOL wrote = MiniDumpWriteDump(
        request.process,
        request.pid,
        file,
        dumpType,
        request.remoteExceptionPointers ? &exceptionInfo : nullptr,
        nullptr,
        nullptr);
    result.error = wrote ? ERROR_SUCCESS : GetLastError();

    LARGE_INTEGER size{};
    if (wrote && GetFileSizeEx(file, &size)) {
        result.bytes = static_cast<std::uint64_t>(size.QuadPart);
    }
    if (wrote) {
        FlushFileBuffers(file);
    }
    CloseHandle(file);

    if (!wrote) {
        DeleteFileW(temporaryPath.c_str());
        return result;
    }
    if (!MoveFileExW(
            temporaryPath.c_str(),
            request.path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        result.error = GetLastError();
        DeleteFileW(temporaryPath.c_str());
        return result;
    }
    result.ok = true;
    return result;
}

} // namespace nsmonitor
