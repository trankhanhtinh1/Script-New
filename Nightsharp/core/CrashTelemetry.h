#pragma once

#include <Windows.h>
#include <cstdio>

namespace CrashTelemetry {

    inline volatile LONG g_installed = 0;
    inline volatile LONG g_exceptionCount = 0;
    inline const char* g_stage = "startup";

    inline void SetStage(const char* stage) {
        g_stage = (stage && *stage) ? stage : "unknown";
    }

    inline void AppendLine(const char* text) {
        if (!text || !*text) {
            return;
        }

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_crash.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(hFile, text, (DWORD)lstrlenA(text), &written, nullptr);
        CloseHandle(hFile);
    }

    inline void AppendStageLine(const char* text) {
        if (!text || !*text) {
            return;
        }

        HANDLE hFile = CreateFileA(
            "C:\\Users\\Public\\ns_stage.txt",
            FILE_APPEND_DATA,
            FILE_SHARE_READ,
            nullptr,
            OPEN_ALWAYS,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (hFile == INVALID_HANDLE_VALUE) {
            return;
        }

        DWORD written = 0;
        WriteFile(hFile, text, (DWORD)lstrlenA(text), &written, nullptr);
        CloseHandle(hFile);
    }

    // Look up which loaded module contains a given code address.
    // Writes "<module.dll>+0xOFFSET" into outBuf, or "???" if unresolvable.
    inline void ResolveAddrModule(unsigned long long addr, char* outBuf, size_t outLen) {
        if (!outBuf || outLen == 0) return;
        outBuf[0] = 0;

        HMODULE hMod = nullptr;
        if (addr == 0 ||
            !GetModuleHandleExA(
                GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                reinterpret_cast<LPCSTR>(static_cast<uintptr_t>(addr)),
                &hMod) ||
            !hMod) {
            std::snprintf(outBuf, outLen, "???");
            return;
        }

        char path[MAX_PATH] = {};
        if (!GetModuleFileNameA(hMod, path, sizeof(path))) {
            std::snprintf(outBuf, outLen, "???+0x%llX",
                          addr - reinterpret_cast<unsigned long long>(hMod));
            return;
        }

        // Extract base name (strip path).
        const char* base = path;
        for (const char* p = path; *p; ++p) {
            if (*p == '\\' || *p == '/') base = p + 1;
        }

        std::snprintf(outBuf, outLen, "%s+0x%llX",
                      base,
                      addr - reinterpret_cast<unsigned long long>(hMod));
    }

    inline void LogExceptionRecord(const char* source, EXCEPTION_POINTERS* ep) {
        char buf[1536] = {};
        const EXCEPTION_RECORD* er = ep ? ep->ExceptionRecord : nullptr;
        const CONTEXT* ctx = ep ? ep->ContextRecord : nullptr;

        const DWORD code = er ? er->ExceptionCode : 0;
        const void* address = er ? er->ExceptionAddress : nullptr;
        const unsigned long long rip = ctx ? (unsigned long long)ctx->Rip : 0ull;
        const unsigned long long rsp = ctx ? (unsigned long long)ctx->Rsp : 0ull;
        const unsigned long long rbp = ctx ? (unsigned long long)ctx->Rbp : 0ull;
        const unsigned long long rcx = ctx ? (unsigned long long)ctx->Rcx : 0ull;
        const unsigned long long rdx = ctx ? (unsigned long long)ctx->Rdx : 0ull;
        const unsigned long long r8 = ctx ? (unsigned long long)ctx->R8 : 0ull;
        const unsigned long long r9 = ctx ? (unsigned long long)ctx->R9 : 0ull;
        const LONG count = InterlockedIncrement(&g_exceptionCount);

        char ripModule[128] = {};
        char addrModule[128] = {};
        ResolveAddrModule(rip, ripModule, sizeof(ripModule));
        ResolveAddrModule(reinterpret_cast<unsigned long long>(address),
                          addrModule, sizeof(addrModule));

        std::snprintf(
            buf, sizeof(buf),
            "[NightSharp][Crash] #%ld source=%s stage=%s tid=%lu code=0x%08lX "
            "addr=0x%p (%s) rip=0x%llX (%s) rsp=0x%llX rbp=0x%llX "
            "rcx=0x%llX rdx=0x%llX r8=0x%llX r9=0x%llX\r\n",
            count,
            source ? source : "unknown",
            g_stage ? g_stage : "unknown",
            GetCurrentThreadId(),
            code,
            address, addrModule,
            rip, ripModule,
            rsp,
            rbp,
            rcx,
            rdx,
            r8,
            r9);

        OutputDebugStringA(buf);
        AppendLine(buf);
    }

    inline LONG WINAPI TopLevelFilter(EXCEPTION_POINTERS* ep) {
        LogExceptionRecord("TopLevelFilter", ep);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline int ReportAndHandle(const char* source, EXCEPTION_POINTERS* ep) {
        LogExceptionRecord(source, ep);
        return EXCEPTION_EXECUTE_HANDLER;
    }

    inline void Install() {
        if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) {
            return;
        }

        SetUnhandledExceptionFilter(TopLevelFilter);
        AppendLine("[NightSharp][Crash] telemetry installed\r\n");
    }

} // namespace CrashTelemetry
