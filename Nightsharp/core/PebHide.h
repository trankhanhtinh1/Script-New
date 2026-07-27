#pragma once

// ============================================================================
// PebHide - Module E: unlink DLL khỏi PEB->Ldr module lists + zero name.
// ============================================================================
//
// Sau khi HideAndErase chạy, các API user-mode enumerate module KHÔNG thấy
// module ta nữa:
//   - EnumProcessModules / EnumProcessModulesEx (psapi)
//   - Module32First / Module32Next (ToolHelp32)
//   - GetModuleHandleW / A (theo tên)
//   - GetModuleFileNameEx (theo handle vẫn work — không đụng)
//
// KHÔNG bypass:
//   - Kernel-side EnumEProcessModules (Nt driver).
//   - NtQueryVirtualMemory với MemoryImageInformation trên 1 địa chỉ trong
//     module — kernel vẫn resolve qua VAD, không đụng PEB.
//   - Anti-cheat quét memory tự tay (không dùng list).
//
// Chỉ gọi 1 LẦN trong DllMain DLL_PROCESS_ATTACH sau khi:
//   1) DirectSyscall::InitAll đã xong (không lookup module ta bằng name).
//   2) StartOverlayWorker đã spawn thread (không cần base module walk nữa).
//
// Sau khi hide, GetModuleHandleW(L"KiteMod.dll") sẽ trả nullptr. Nếu cần
// lookup module ta SAU đó, phải cache hModule trước khi hide.
// ============================================================================

#include <Windows.h>
#include <cstdint>
#include <cstring>

#include "PackmanHook.h"

namespace PebHide {

// ── PEB LDR types (Win11 x64 offsets verified) ───────────────────────────────

struct UNICODE_STRING_NS {
    USHORT Length;
    USHORT MaximumLength;
    PWSTR  Buffer;
};

struct LIST_ENTRY_NS {
    LIST_ENTRY_NS* Flink;
    LIST_ENTRY_NS* Blink;
};

struct LDR_DATA_TABLE_ENTRY_NS {
    LIST_ENTRY_NS InLoadOrderLinks;             // +0x00
    LIST_ENTRY_NS InMemoryOrderLinks;           // +0x10
    LIST_ENTRY_NS InInitializationOrderLinks;   // +0x20
    PVOID         DllBase;                      // +0x30
    PVOID         EntryPoint;                   // +0x38
    ULONG         SizeOfImage;                  // +0x40
    ULONG         _pad_44;
    UNICODE_STRING_NS FullDllName;              // +0x48
    UNICODE_STRING_NS BaseDllName;              // +0x58
};

struct PEB_LDR_DATA_NS {
    ULONG Length;                               // +0x00
    ULONG Initialized;                          // +0x04
    PVOID SsHandle;                             // +0x08
    LIST_ENTRY_NS InLoadOrderModuleList;        // +0x10
    LIST_ENTRY_NS InMemoryOrderModuleList;      // +0x20
    LIST_ENTRY_NS InInitializationOrderModuleList; // +0x30
};

// PEB.Ldr ở offset 0x18 (x64). Đọc qua GS:0x60 (self-TEB.ProcessEnvironmentBlock).
inline PEB_LDR_DATA_NS* GetPebLdr() {
    auto* peb = reinterpret_cast<uint8_t*>(__readgsqword(0x60));
    if (!peb) return nullptr;
    return *reinterpret_cast<PEB_LDR_DATA_NS**>(peb + 0x18);
}

// Unlink 1 LIST_ENTRY khỏi doubly-linked list + self-loop để walker chạm
// đúng lúc unlink không nhảy lung tung.
inline void UnlinkAndSelfLoop(LIST_ENTRY_NS* e) {
    if (!e || !e->Flink || !e->Blink) return;
    e->Blink->Flink = e->Flink;
    e->Flink->Blink = e->Blink;
    e->Flink = e;
    e->Blink = e;
}

// One-shot: locate entry theo DllBase → zero name buffer → unlink 3 list.
// Return: số list đã unlink (0..3). 0 = entry không tìm thấy hoặc PEB fail.
// Idempotent: gọi lại sau khi đã hide sẽ trả 0 (không tìm thấy trong list).
inline int HideAndErase(HMODULE dllBase) {
    if (!dllBase) return 0;
    auto* ldr = GetPebLdr();
    if (!ldr) return 0;

    void* target = static_cast<void*>(dllBase);
    LDR_DATA_TABLE_ENTRY_NS* self = nullptr;

    // Locate via InLoadOrder (Flink offset = 0 trong LDR_DATA_TABLE_ENTRY).
    auto* head = &ldr->InLoadOrderModuleList;
    for (auto* cur = head->Flink; cur && cur != head; cur = cur->Flink) {
        auto* e = reinterpret_cast<LDR_DATA_TABLE_ENTRY_NS*>(cur);
        if (e->DllBase == target) { self = e; break; }
    }
    if (!self) return 0;

    // Zero name buffers TRƯỚC khi unlink (unlink chỉ đổi pointer, không xóa data).
    if (self->FullDllName.Buffer && self->FullDllName.MaximumLength > 0) {
        memset(self->FullDllName.Buffer, 0, self->FullDllName.MaximumLength);
    }
    if (self->BaseDllName.Buffer && self->BaseDllName.MaximumLength > 0) {
        memset(self->BaseDllName.Buffer, 0, self->BaseDllName.MaximumLength);
    }
    self->FullDllName.Length = 0;
    self->BaseDllName.Length = 0;

    int unlinked = 0;
    UnlinkAndSelfLoop(&self->InLoadOrderLinks);           ++unlinked;
    UnlinkAndSelfLoop(&self->InMemoryOrderLinks);         ++unlinked;
    UnlinkAndSelfLoop(&self->InInitializationOrderLinks); ++unlinked;
    return unlinked;
}

// ── ErasePeHeaders (Gap 2) ───────────────────────────────────────────────────
// Zero PE headers (DOS + NT + section headers) sau khi DLL đã load xong.
// Anti-cheat scan memory tìm PE headers để identify injected modules.
// Sau khi erase: VirtualQuery vẫn trả AllocationBase, nhưng memcmp với
// "MZ" / "PE\0\0" signature → không match → scanner bỏ qua.
//
// Phải gọi SAU khi:
//   1) DirectSyscall::InitAll() — cần đọc PE export table
//   2) PebHide::HideAndErase() — cần DllBase để tìm entry
//   3) OverlayManager đã init — không cần PE headers nữa
//
// Safe: code section (.text) không bị ảnh hưởng, chỉ zero headers (thường
// 0x400-0x1000 bytes đầu module).
inline bool ErasePeHeaders(HMODULE module) {
    if (!module) return false;

    auto* base = reinterpret_cast<uint8_t*>(module);
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
        // Already erased?
        return false;
    }

    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) {
        return false;
    }

    const size_t headerSize = nt->OptionalHeader.SizeOfHeaders;
    if (headerSize == 0 || headerSize > 0x10000) return false;

    // Make headers writable via direct syscall (bypass Packman hook)
    DWORD oldProt = 0;
    if (!DirectSyscall::VirtualProtectDirect(base, headerSize, PAGE_READWRITE, &oldProt)) {
        // Fallback
        if (!VirtualProtect(base, headerSize, PAGE_READWRITE, &oldProt)) {
            return false;
        }
    }

    // Zero everything
    memset(base, 0, headerSize);

    // Restore protection (PAGE_READONLY — headers không cần execute)
    DWORD dummy = 0;
    DirectSyscall::VirtualProtectDirect(base, headerSize, PAGE_READONLY, &dummy);

    return true;
}

} // namespace PebHide
