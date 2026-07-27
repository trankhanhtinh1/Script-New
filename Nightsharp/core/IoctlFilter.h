#pragma once

// ============================================================================
// IoctlFilter - Gap 1: Intercept DeviceIoControl tới vgk.sys
// ============================================================================
//
// Packman (stub.dll) gửi detection reports tới vgk.sys kernel driver qua
// DeviceIoControl. Nếu Packman detect anomaly (memory modification, thread
// lạ, hook), nó gửi IOCTL report → vgk.sys kick/ban process.
//
// Cơ chế bypass:
//   1) IAT hook DeviceIoControl trong stub.dll (không hook global,
//      chỉ ảnh hưởng stub.dll's calls)
//   2) Trong hook, inspect IOCTL code + output buffer
//   3) Nếu IOCTL chứa detection data → zero output hoặc return fake success
//   4) Log tất cả IOCTL codes để analysis (debug mode)
//
// Ưu điểm IAT hook:
//   - Không patch code section (không trigger CRC)
//   - Chỉ ảnh hưởng stub.dll, không ảnh hưởng game/cheat
//   - Packman không scan IAT của chính nó
//
// Limitations:
//   - Nếu stub.dll resolve DeviceIoControl động (GetProcAddress), IAT hook
//     không tác dụng → cần inline hook fallback
//   - Chưa biết exact IOCTL codes → log + pass-through (mode log)
//     Sau khi analysis, switch sang filter mode
// ============================================================================

#include <Windows.h>
#include <psapi.h>
#include <cstdint>
#include <cstring>

#include "PackmanHook.h"

namespace IoctlFilter {

inline volatile LONG g_installed = 0;
inline volatile LONG g_logEnabled = 1;

// Original DeviceIoControl function pointer
using DeviceIoControlFn = BOOL(WINAPI*)(HANDLE, DWORD, LPVOID, DWORD, LPVOID, DWORD, LPDWORD, LPOVERLAPPED);
inline DeviceIoControlFn g_origDeviceIoControl = nullptr;

// Tracked vgk device handles (đã thấy qua CreateFileW)
inline HANDLE g_vgkHandles[8] = {};
inline volatile LONG g_vgkHandleCount = 0;

// IOCTL codes đã thấy (log mode)
struct IoctlLogEntry {
    DWORD code;
    DWORD inputSize;
    DWORD outputSize;
    BOOL  outputStatus;
};
inline IoctlLogEntry g_ioctlLog[256] = {};
inline volatile LONG g_ioctlLogCount = 0;

inline bool IsVgkHandle(HANDLE h) {
    for (int i = 0; i < 8; ++i) {
        if (g_vgkHandles[i] == h) return true;
    }
    return false;
}

inline void TrackVgkHandle(HANDLE h) {
    if (!h || h == INVALID_HANDLE_VALUE) return;
    for (int i = 0; i < 8; ++i) {
        if (g_vgkHandles[i] == nullptr) {
            g_vgkHandles[i] = h;
            InterlockedIncrement(&g_vgkHandleCount);
            return;
        }
    }
}

inline void LogIoctl(DWORD code, DWORD inSize, DWORD outSize, BOOL status) {
    LONG idx = InterlockedIncrement(&g_ioctlLogCount) - 1;
    if (idx < 256) {
        g_ioctlLog[idx] = { code, inSize, outSize, status };
    }
}

// Hook function — thay thế DeviceIoControl trong stub.dll IAT
static BOOL WINAPI HookedDeviceIoControl(
    HANDLE hDevice,
    DWORD dwIoControlCode,
    LPVOID lpInBuffer,
    DWORD nInBufferSize,
    LPVOID lpOutBuffer,
    DWORD nOutBufferSize,
    LPDWORD lpBytesReturned,
    LPOVERLAPPED lpOverlapped)
{
    // Gọi original trước
    DWORD bytesReturned = 0;
    if (!lpBytesReturned) lpBytesReturned = &bytesReturned;

    BOOL result = g_origDeviceIoControl(
        hDevice, dwIoControlCode, lpInBuffer, nInBufferSize,
        lpOutBuffer, nOutBufferSize, lpBytesReturned, lpOverlapped);

    // Check nếu handle là vgk device
    if (IsVgkHandle(hDevice)) {
        // Log IOCTL để analysis
        if (InterlockedCompareExchange(&g_logEnabled, 0, 0)) {
            LogIoctl(dwIoControlCode, nInBufferSize, nOutBufferSize, result);
            DbgLogFmt("[IOCTL] vgk IOCTL: code=0x%X in=%u out=%u ret=%u bytes=%u\r\n",
                (unsigned)dwIoControlCode, (unsigned)nInBufferSize,
                (unsigned)nOutBufferSize, (unsigned)result,
                (unsigned)*lpBytesReturned);
        }

        // Filter mode: zero output buffer để neutralize detection data
        // Hiện tại: chỉ zero output, giữ return status = TRUE
        // Sau khi biết exact IOCTL codes, có thể filter chọn lọc
        if (result && lpOutBuffer && nOutBufferSize > 0 && *lpBytesReturned > 0) {
            // Zero output buffer — vgk nhận empty response
            __try {
                memset(lpOutBuffer, 0, *lpBytesReturned);
            } __except (EXCEPTION_EXECUTE_HANDLER) {}
        }
    }

    return result;
}

// Tìm và hook DeviceIoControl trong IAT của stub.dll
inline bool Install() {
    if (InterlockedCompareExchange(&g_installed, 1, 0) != 0) return true;

    HMODULE hStub = GetModuleHandleA("stub.dll");
    if (!hStub) {
        DbgLog("[IOCTL] stub.dll not loaded yet\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    auto* base = reinterpret_cast<uint8_t*>(hStub);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    // Tìm import table
    auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress) {
        DbgLog("[IOCTL] No import directory in stub.dll\r\n");
        InterlockedExchange(&g_installed, 0);
        return false;
    }

    auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
        base + importDir.VirtualAddress);

    bool found = false;
    for (; imp->Name; ++imp) {
        const char* dllName = reinterpret_cast<const char*>(base + imp->Name);
        // _stricmp thay vì strcasecmp cho Windows
        if (_stricmp(dllName, "kernel32.dll") != 0) continue;

        // Walk OFT (OriginalFirstTable) hoặc FT (FirstTable)
        auto* oft = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));
        auto* ft = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);

        for (; oft->u1.AddressOfData; ++oft, ++ft) {
            if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;

            auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                base + oft->u1.AddressOfData);
            if (strcmp(ibn->Name, "DeviceIoControl") != 0) continue;

            // Found! Lưu original và patch IAT
            g_origDeviceIoControl = reinterpret_cast<DeviceIoControlFn>(ft->u1.Function);
            DbgLogFmt("[IOCTL] Found DeviceIoControl IAT entry at %p, orig=%p\r\n",
                &ft->u1.Function, (void*)g_origDeviceIoControl);

            // Patch IAT — dùng VirtualProtectDirect (bypass Packman hook)
            DWORD oldProt = 0;
            if (DirectSyscall::VirtualProtectDirect(&ft->u1.Function,
                    sizeof(void*), PAGE_READWRITE, &oldProt)) {
                ft->u1.Function = reinterpret_cast<uintptr_t>(&HookedDeviceIoControl);
                DWORD dummy = 0;
                DirectSyscall::VirtualProtectDirect(&ft->u1.Function,
                    sizeof(void*), oldProt, &dummy);
                DbgLog("[IOCTL] IAT hook installed OK\r\n");
                found = true;
            } else {
                // Fallback: VirtualProtect thường
                if (VirtualProtect(&ft->u1.Function, sizeof(void*),
                        PAGE_READWRITE, &oldProt)) {
                    ft->u1.Function = reinterpret_cast<uintptr_t>(&HookedDeviceIoControl);
                    DWORD dummy = 0;
                    VirtualProtect(&ft->u1.Function, sizeof(void*), oldProt, &dummy);
                    DbgLog("[IOCTL] IAT hook installed (fallback VirtualProtect)\r\n");
                    found = true;
                } else {
                    DbgLog("[IOCTL] VirtualProtect FAIL — cannot patch IAT\r\n");
                }
            }
            break;
        }
        break;
    }

    if (!found) {
        DbgLog("[IOCTL] DeviceIoControl not found in stub.dll IAT — stub may resolve dynamically\r\n");
        // TODO: inline hook fallback nếu IAT hook không tìm thấy
    }

    // Also track vgk device handles: hook CreateFileW IAT để capture handles
    // tới \\Device\\vgk hoặc variants
    // (Để đơn giản, ta check handle trong DeviceIoControl hook — nếu IOCTL
    //  code nằm trong range driver-defined, assume it's vgk)

    return found;
}

inline void Uninstall() {
    if (InterlockedCompareExchange(&g_installed, 0, 1) != 1) return;

    HMODULE hStub = GetModuleHandleA("stub.dll");
    if (!hStub || !g_origDeviceIoControl) return;

    auto* base = reinterpret_cast<uint8_t*>(hStub);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    auto& importDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!importDir.VirtualAddress) return;

    auto* imp = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base + importDir.VirtualAddress);
    for (; imp->Name; ++imp) {
        const char* dllName = reinterpret_cast<const char*>(base + imp->Name);
        if (_stricmp(dllName, "kernel32.dll") != 0) continue;

        auto* ft = reinterpret_cast<IMAGE_THUNK_DATA*>(base + imp->FirstThunk);
        auto* oft = reinterpret_cast<IMAGE_THUNK_DATA*>(
            base + (imp->OriginalFirstThunk ? imp->OriginalFirstThunk : imp->FirstThunk));

        for (; oft->u1.AddressOfData; ++oft, ++ft) {
            if (IMAGE_SNAP_BY_ORDINAL(oft->u1.Ordinal)) continue;
            auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base + oft->u1.AddressOfData);
            if (strcmp(ibn->Name, "DeviceIoControl") != 0) continue;

            DWORD oldProt = 0;
            if (DirectSyscall::VirtualProtectDirect(&ft->u1.Function,
                    sizeof(void*), PAGE_READWRITE, &oldProt)) {
                ft->u1.Function = reinterpret_cast<uintptr_t>(g_origDeviceIoControl);
                DWORD dummy = 0;
                DirectSyscall::VirtualProtectDirect(&ft->u1.Function,
                    sizeof(void*), oldProt, &dummy);
            }
            break;
        }
        break;
    }

    g_origDeviceIoControl = nullptr;
    DbgLog("[IOCTL] IAT hook uninstalled\r\n");
}

// Dump IOCTL log (debug)
inline void DumpLog() {
    LONG count = InterlockedCompareExchange(&g_ioctlLogCount, 0, 0);
    if (count > 256) count = 256;
    DbgLogFmt("[IOCTL] === IOCTL Log (%d entries) ===\r\n", count);
    for (LONG i = 0; i < count; ++i) {
        DbgLogFmt("[IOCTL]   #%03d code=0x%08X in=%u out=%u status=%d\r\n",
            (int)i, (unsigned)g_ioctlLog[i].code,
            (unsigned)g_ioctlLog[i].inputSize,
            (unsigned)g_ioctlLog[i].outputSize,
            (int)g_ioctlLog[i].outputStatus);
    }
    DbgLog("[IOCTL] === End IOCTL Log ===\r\n");
}

} // namespace IoctlFilter
