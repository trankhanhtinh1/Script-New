#include "CrashReport.h"

#include <TlHelp32.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <vector>

namespace nsmonitor {

namespace {

std::mutex g_dbgHelpMutex;

template <typename T>
bool ReadRemote(HANDLE process, const void* address, T& out) {
    SIZE_T read = 0;
    return address &&
           ReadProcessMemory(process, address, &out, sizeof(out), &read) &&
           read == sizeof(out);
}

std::string Narrow(const std::wstring& value) {
    if (value.empty()) {
        return {};
    }
    const int bytes = WideCharToMultiByte(
        CP_UTF8,
        0,
        value.c_str(),
        static_cast<int>(value.size()),
        nullptr,
        0,
        nullptr,
        nullptr);
    std::string result(static_cast<std::size_t>(std::max(bytes, 0)), '\0');
    if (bytes > 0) {
        WideCharToMultiByte(
            CP_UTF8,
            0,
            value.c_str(),
            static_cast<int>(value.size()),
            result.data(),
            bytes,
            nullptr,
            nullptr);
    }
    return result;
}

const char* CaptureSourceName(nscrash::CaptureSource source) {
    switch (source) {
    case nscrash::CaptureSource::Bridge:
        return "external-bridge";
    case nscrash::CaptureSource::Wer:
        return "wer-fallback";
    case nscrash::CaptureSource::ExitOnly:
        return "exit-only-watchdog";
    default:
        return "none";
    }
}

std::string NarrowIncidentClass(IncidentClass value) {
    return Narrow(IncidentClassName(value));
}

void ResolveModule(
    DWORD pid,
    std::uint64_t address,
    std::uint64_t nightSharpBase,
    std::uint32_t nightSharpSize,
    RemoteCrashData& data) {
    if (nightSharpBase && address >= nightSharpBase &&
        address < nightSharpBase + nightSharpSize) {
        data.moduleName = "NightSharp.dll";
        data.moduleBase = nightSharpBase;
        data.moduleOffset = address - nightSharpBase;
        return;
    }

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snapshot == INVALID_HANDLE_VALUE) {
        return;
    }
    MODULEENTRY32W entry{};
    entry.dwSize = sizeof(entry);
    if (Module32FirstW(snapshot, &entry)) {
        do {
            const auto base = reinterpret_cast<std::uint64_t>(entry.modBaseAddr);
            if (address >= base && address < base + entry.modBaseSize) {
                data.moduleName = Narrow(entry.szModule);
                data.moduleBase = base;
                data.moduleOffset = address - base;
                break;
            }
        } while (Module32NextW(snapshot, &entry));
    }
    CloseHandle(snapshot);
}

BOOL CALLBACK ReadRemoteMemory64(
    HANDLE process,
    DWORD64 address,
    PVOID buffer,
    DWORD size,
    LPDWORD bytesRead) {
    SIZE_T read = 0;
    const BOOL ok = ReadProcessMemory(
        process,
        reinterpret_cast<const void*>(address),
        buffer,
        size,
        &read);
    if (bytesRead) {
        *bytesRead = static_cast<DWORD>(read);
    }
    return ok;
}

std::string HexAddress(std::uint64_t address) {
    std::ostringstream out;
    out << "0x" << std::uppercase << std::hex << std::setw(16)
        << std::setfill('0') << address;
    return out.str();
}

std::string ResolveFrameText(
    HANDLE process,
    DWORD64 address,
    DWORD frameIndex,
    std::string* symbolOut,
    std::string* sourceOut,
    DWORD* lineOut) {
    alignas(SYMBOL_INFOW) BYTE symbolStorage[
        sizeof(SYMBOL_INFOW) + MAX_SYM_NAME * sizeof(wchar_t)]{};
    auto* symbol = reinterpret_cast<SYMBOL_INFOW*>(symbolStorage);
    symbol->SizeOfStruct = sizeof(SYMBOL_INFOW);
    symbol->MaxNameLen = MAX_SYM_NAME;
    DWORD64 displacement = 0;

    std::ostringstream frame;
    frame << '#' << std::setfill('0') << std::setw(2) << frameIndex << ' ';
    if (SymFromAddrW(process, address, &displacement, symbol)) {
        const std::string name = Narrow(symbol->Name);
        frame << name << "+0x" << std::uppercase << std::hex << displacement;
        if (symbolOut) {
            *symbolOut = name + "+0x" + [&] {
                std::ostringstream value;
                value << std::uppercase << std::hex << displacement;
                return value.str();
            }();
        }
    } else {
        IMAGEHLP_MODULEW64 module{};
        module.SizeOfStruct = sizeof(module);
        if (SymGetModuleInfoW64(process, address, &module) && module.ModuleName[0]) {
            frame << Narrow(module.ModuleName) << "+0x" << std::uppercase
                  << std::hex << (address - module.BaseOfImage);
        } else {
            frame << HexAddress(address);
        }
    }

    IMAGEHLP_LINEW64 line{};
    line.SizeOfStruct = sizeof(line);
    DWORD lineDisplacement = 0;
    if (SymGetLineFromAddrW64(process, address, &lineDisplacement, &line) &&
        line.FileName) {
        const std::string file = Narrow(line.FileName);
        frame << " " << file << ':' << std::dec << line.LineNumber;
        if (sourceOut) {
            *sourceOut = file;
        }
        if (lineOut) {
            *lineOut = line.LineNumber;
        }
    }
    return frame.str();
}

} // namespace

RemoteCrashData ReadRemoteCrashData(
    HANDLE process,
    DWORD pid,
    DWORD threadId,
    std::uintptr_t remoteExceptionPointers,
    std::uint64_t nightSharpBase,
    std::uint32_t nightSharpSize) {
    RemoteCrashData data{};
    data.threadId = threadId;
    if (!process || !remoteExceptionPointers ||
        (remoteExceptionPointers % alignof(void*)) != 0) {
        return data;
    }

    EXCEPTION_POINTERS pointers{};
    EXCEPTION_RECORD record{};
    CONTEXT context{};
    if (!ReadRemote(
            process,
            reinterpret_cast<const void*>(remoteExceptionPointers),
            pointers) ||
        !ReadRemote(process, pointers.ExceptionRecord, record) ||
        !ReadRemote(process, pointers.ContextRecord, context)) {
        return data;
    }

    data.valid = true;
    data.context = context;
    data.exceptionCode = record.ExceptionCode;
    data.exceptionFlags = record.ExceptionFlags;
    data.exceptionAddress = reinterpret_cast<std::uint64_t>(record.ExceptionAddress);
    for (DWORD i = 0; i < std::min<DWORD>(record.NumberParameters, 3); ++i) {
        data.exceptionInfo[i] = record.ExceptionInformation[i];
    }
#if defined(_M_X64)
    data.rip = context.Rip;
    data.rsp = context.Rsp;
    data.rbp = context.Rbp;
    data.rax = context.Rax;
    data.rbx = context.Rbx;
    data.rcx = context.Rcx;
    data.rdx = context.Rdx;
#endif
    ResolveModule(
        pid,
        data.exceptionAddress,
        nightSharpBase,
        nightSharpSize,
        data);
    return data;
}

bool ResolveSymbolsAndStack(
    HANDLE process,
    const std::wstring& nightSharpImagePath,
    RemoteCrashData& crash) {
    if (!process || !crash.valid || crash.rip == 0 ||
        nightSharpImagePath.empty() ||
        GetFileAttributesW(nightSharpImagePath.c_str()) == INVALID_FILE_ATTRIBUTES) {
        return false;
    }

    std::lock_guard<std::mutex> lock(g_dbgHelpMutex);
    std::wstring searchPath = nightSharpImagePath;
    const std::size_t slash = searchPath.find_last_of(L"\\/");
    if (slash != std::wstring::npos) {
        searchPath.resize(slash);
    }
    SymSetOptions(
        SYMOPT_DEFERRED_LOADS | SYMOPT_LOAD_LINES | SYMOPT_UNDNAME |
        SYMOPT_FAIL_CRITICAL_ERRORS);
    if (!SymInitializeW(process, searchPath.c_str(), TRUE)) {
        return false;
    }

    SymLoadModuleExW(
        process,
        nullptr,
        nightSharpImagePath.c_str(),
        L"NightSharp",
        crash.moduleBase,
        0,
        nullptr,
        0);

    crash.stack.clear();
    crash.stack.push_back(ResolveFrameText(
        process,
        crash.rip,
        0,
        &crash.symbol,
        &crash.sourceFile,
        &crash.sourceLine));

#if defined(_M_X64)
    CONTEXT context = crash.context;
    HANDLE thread = OpenThread(
        THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT,
        FALSE,
        crash.threadId);
    STACKFRAME64 frame{};
    frame.AddrPC.Offset = context.Rip;
    frame.AddrPC.Mode = AddrModeFlat;
    frame.AddrFrame.Offset = context.Rbp;
    frame.AddrFrame.Mode = AddrModeFlat;
    frame.AddrStack.Offset = context.Rsp;
    frame.AddrStack.Mode = AddrModeFlat;
    for (DWORD index = 1; index < 48; ++index) {
        if (!StackWalk64(
                IMAGE_FILE_MACHINE_AMD64,
                process,
                thread,
                &frame,
                &context,
                &ReadRemoteMemory64,
                SymFunctionTableAccess64,
                SymGetModuleBase64,
                nullptr) ||
            frame.AddrPC.Offset == 0) {
            break;
        }
        crash.stack.push_back(ResolveFrameText(
            process,
            frame.AddrPC.Offset,
            index,
            nullptr,
            nullptr,
            nullptr));
    }
    if (thread) {
        CloseHandle(thread);
    }
#endif
    SymCleanup(process);
    return !crash.stack.empty();
}

std::string FormatIncidentReport(
    IncidentClass classification,
    nscrash::CaptureSource source,
    const nscrash::SharedState& state,
    const RemoteCrashData& crash,
    const DumpResult& dump,
    const std::wstring& dumpPath,
    DWORD exitCode) {
    std::ostringstream out;
    out << "NightSharp crash report\r\n";
    out << "classification=" << NarrowIncidentClass(classification) << "\r\n";
    out << "capture_source=" << CaptureSourceName(source) << "\r\n";
    out << "pid=" << state.pid << "\r\n";
    out << "tid=" << crash.threadId << "\r\n";
    out << "phase=" << state.phase << "\r\n";
    out << "stage=" << state.lastStage << "\r\n";
    out << std::uppercase << std::hex << std::setfill('0');
    out << "exit_code=0x" << std::setw(8) << exitCode << "\r\n";
    if (crash.valid) {
        out << "exception_context=available\r\n";
        out << "exception_code=0x" << std::setw(8) << crash.exceptionCode << "\r\n";
        out << "exception_address=0x" << std::setw(16) << crash.exceptionAddress << "\r\n";
        out << "rip=0x" << std::setw(16) << crash.rip << "\r\n";
        out << "rsp=0x" << std::setw(16) << crash.rsp << "\r\n";
        out << "rbp=0x" << std::setw(16) << crash.rbp << "\r\n";
        out << "fault_module=" << crash.moduleName << "\r\n";
        out << "fault_base=0x" << std::setw(16) << crash.moduleBase << "\r\n";
        out << "fault_offset=0x" << crash.moduleOffset << "\r\n";
        if (!crash.symbol.empty()) {
            out << "symbol=" << crash.symbol << "\r\n";
        }
        if (!crash.sourceFile.empty()) {
            out << "source=" << crash.sourceFile << ':' << std::dec << crash.sourceLine << "\r\n";
        }
        for (const std::string& frame : crash.stack) {
            out << "stack=" << frame << "\r\n";
        }
    } else {
        out << "exception_context=unavailable\r\n";
    }
    out << std::dec;
    out << "dump_ok=" << (dump.ok ? 1 : 0) << "\r\n";
    out << "dump_error=" << dump.error << "\r\n";
    out << "dump_bytes=" << dump.bytes << "\r\n";
    out << "dump=" << Narrow(dumpPath) << "\r\n";
    return out.str();
}

bool WriteIncidentReport(
    const std::wstring& path,
    const std::string& report,
    DWORD& error) {
    error = ERROR_SUCCESS;
    const std::wstring temporary = path + L".tmp";
    HANDLE file = CreateFileW(
        temporary.c_str(),
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        error = GetLastError();
        return false;
    }
    DWORD written = 0;
    const BOOL ok = WriteFile(
        file,
        report.data(),
        static_cast<DWORD>(report.size()),
        &written,
        nullptr);
    if (ok) {
        FlushFileBuffers(file);
    }
    CloseHandle(file);
    if (!ok || written != report.size()) {
        error = ok ? ERROR_WRITE_FAULT : GetLastError();
        DeleteFileW(temporary.c_str());
        return false;
    }
    if (!MoveFileExW(
            temporary.c_str(),
            path.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = GetLastError();
        DeleteFileW(temporary.c_str());
        return false;
    }
    return true;
}

} // namespace nsmonitor
