# PackmanHook Anti-Ban Upgrade — Design Spec

**Date**: 2026-07-10
**Target**: `core/PackmanHook.h`
**Rule**: chỉ THÊM, không BỎ. Signature các API cũ giữ nguyên. Caller (`Corehook.h`, `CoreZoomHack.h`, `CoreSkinChanger.h`, `dllmain.cpp`) không phải sửa gì.

---

## 1. Bối cảnh RE (đã confirm)

### 1.1 Packman (stub.dll, dump từ League PID 15112, IDA 13337)
- CRC read site DUY NHẤT tại `stub.dll+0x3A0A` (hàm CRC = `sub_3860`, size `0x710`, SIMD hashing loop đọc 4 pointer từ r14).
- CRC entry chính = `sub_4710`, xref data tại `0x26D054` (nằm trong `.stub2`).
- Packman **không lưu string "Nt*" trong .rdata** → nó tự dùng hash-based resolve. Điều này validate technique Module B.
- Có 1 syscall-dispatcher-like function tại `stub.dll+0x137F00` (`4C 8B D1 41 C6 01 01 8D 4A F8 4C 8D 1D EF 80 EC FF …`) — Packman's own indirect syscall trampoline.

### 1.2 Ntdll (Win11 26100, IDA 13338, imagebase `0x180000000`)
- Syscall stub layout:
  ```
  4C 8B D1                       mov r10, rcx
  B8 SSN 00 00 00                mov eax, SSN
  F6 04 25 08 03 FE 7F 01        test byte ptr [0x7FFE0308], 1
  75 03                          jne short syscall
  0F 05                          syscall
  C3                             ret
  CD 2E                          int 2E   ; fallback
  C3                             ret
  ```
- Syscall gadget `0F 05 C3` xuất hiện **30+ vị trí** liên tục từ `ntdll+0x9D5A2` (mỗi Nt* stub có một cái ở offset `0x12`).

### 1.3 Hook state runtime (dump qua CE, ntdll live @ `0x7FFC90490000`)

Style hook Packman = **`40 E9 <rel32>`** (REX-prefixed JMP rel32, 6 byte) tại đầu hàm, tail từ offset 8 intact.

| Function | RVA | Live head bytes | Hooked? |
|---|---|---|---|
| NtProtectVirtualMemory | 0x9DF90 | `40 E9 8A 78 5D 82 00 00` | YES |
| NtWriteVirtualMemory | 0x9DCD0 | `4C 8B D1 B8 3A 00 00 00` | NO |
| NtContinue | 0x9DDF0 | `40 E9 1A DA 4E 82 00 00` | YES |
| NtDelayExecution | 0x9DC10 | `4C 8B D1 B8 34 00 00 00` | NO |
| NtQueryVirtualMemory | 0x9D9F0 | `40 E9 EA FB 56 82 00 00` | YES |
| NtSuspendThread | 0xA0D40 | `40 E9 7A 4E 5D 82 00 00` | YES |
| NtContinueEx | 0x9E9A0 | `40 E9 1A D2 4E 82 00 00` | YES |
| NtSetContextThread | 0xA0720 | `40 E9 4A B8 4E 82 00 00` | YES |
| NtGetContextThread | 0x9F3E0 | `40 E9 6A E1 4E 82 00 00` | YES |
| NtCreateThreadEx | 0x9EDC0 | `CC 8B D1 B8 C2 00 00 00 … 0F 05 CC CD 2E C3` | YES (INT3 head + tail INT3) |

Điểm quan trọng: **offset `0x12` (`0F 05 C3`)** intact ở **mọi** hàm kể cả hooked → có thể dùng làm gadget cho Module A ngay cả khi hàm đó bị hook.

### 1.4 SSN table extended (verified via IDA 13338 disk exports)

Đủ 28 entry cho table mới:

| Idx | Name | SSN | Đã có? |
|---|---|---|---|
| 0 | NtProtectVirtualMemory | 0x050 | có |
| 1 | NtWriteVirtualMemory | 0x03A | có |
| 2 | NtContinue | 0x043 | có |
| 3 | NtDelayExecution | 0x034 | có |
| 4 | NtQueryVirtualMemory | 0x023 | có |
| 5 | NtSuspendThread | 0x1BE | có |
| 6 | NtContinueEx | 0x0A1 | có |
| 7 | NtSetContextThread | 0x18D | có |
| 8 | NtGetContextThread | 0x0F3 | có |
| 9 | NtCreateThreadEx | 0x0C2 | có |
| 10 | NtSetInformationThread | 0x00D | **mới** (cần cho Module F) |

Các SSN khác đã trích được nhưng KHÔNG thêm vào build này (YAGNI): NtOpenProcess 0x26, NtReadVirtualMemory 0x3F, NtAllocateVirtualMemory 0x18, NtFreeVirtualMemory 0x1E, NtClose 0x0F, NtQueryInformationProcess 0x19, NtQueryInformationThread 0x25, NtQuerySystemInformation 0x36, NtCreateFile 0x55, NtOpenFile 0x33, NtWaitForSingleObject 0x04, NtMapViewOfSection 0x28, NtUnmapViewOfSection 0x2A, NtCreateSection 0x4A, NtOpenSection 0x37, NtRaiseException 0x168, NtResumeThread 0x52. Ghi vào comment `// reserved` để dễ mở sau.

---

## 2. Scope

4 module implement, 1 SSN entry mới. Toàn bộ ADDITIVE.

| Module | Mô tả ngắn | Anti-ban surface |
|---|---|---|
| A | Indirect Syscall | Byte-scan `0F 05` trong private RX |
| B | Hash-Based Name Resolve | String-scan `.rdata` cho "Nt*" |
| F | ThreadHideFromDebugger | Thread-enum trong debugger |
| J | Log Toggle | `%TEMP%\ph.log` forensic artifact |

Thứ tự implement đề xuất (test-per-module): **B → A → J → F**.
- B trước A vì A build stub dùng gadget resolve qua hash (tách resolve infra).
- J thêm sớm để test-log không sinh file.
- F cuối cùng, phụ thuộc NtSetInformationThread từ table.

---

## 3. Module A — Indirect Syscall

### 3.1 Cơ chế

Stub cũ (đang emit):
```
50 51 52 53 …           ; không có, đây chỉ là ví dụ layout syscall
4C 8B D1                mov r10, rcx
B8 SSN 00 00 00         mov eax, SSN
0F 05                   syscall           ← DÍNH khi scanner tìm 0F 05
C3                      ret
```

Stub mới:
```
4C 8B D1                mov r10, rcx              ; 3 bytes
B8 SSN 00 00 00         mov eax, SSN              ; 5 bytes
FF 25 00 00 00 00       jmp qword ptr [rip+0]     ; 6 bytes (rel32=0 → qword ngay sau)
<8 bytes>               ; qword = gadget addr (0F 05 C3 trong ntdll)
```
Tổng 3+5+6+8 = **22 bytes**, alloc PAGE_EXECUTE_READWRITE khi ghi rồi `VirtualProtectDirect` xuống PAGE_EXECUTE_READ sau khi commit (tránh RWX vĩnh viễn — thêm anti-scan bonus).

### 3.2 Gadget selection

- Ở lần InitAll, gọi `PickSyscallGadget()`:
  - Enumerate all `0F 05 C3` byte patterns trong `.text` section của ntdll (dùng `ntdll` module base + iterate).
  - Cache list vào `g_syscallGadgets[]`.
  - Khi build stub cho từng SSN, pick 1 gadget ngẫu nhiên (rand từ `__rdtsc()`) → mỗi stub có gadget khác nhau → không có pattern bulk.
- Fallback (nếu enumerate fail): dùng địa chỉ `NtWaitForSingleObject + 0x12` (đã verify unhooked ở hầu hết case).

### 3.3 API

Không đổi signature. Internal:
```cpp
static void*         g_syscallGadgets[64] = {};
static size_t        g_gadgetCount = 0;
inline void         EnumerateSyscallGadgets();  // gọi 1 lần trong InitAll()
inline void*        PickSyscallGadget();
inline bool         BuildSyscallStub(int ssn, void** outStub);  // đã đổi cách emit
```

### 3.4 Test

- **Static**: build DLL. Grep binary bytes cho `0F 05` sau khi resolve (script python), phải là 0 match trong bất kỳ section RX-owned nào của NightSharp.dll.
- **Runtime CE**: sau khi inject:
  ```
  mcp__cheatengine__aob_scan_module("NightSharp.dll", "0F 05 C3", "+X")
  ```
  → phải trả `count=0`.
- **Function**: `VirtualProtectDirect(page, size, PAGE_READONLY, &old)` phải work, return TRUE, `g_lastProtectStatus >= 0`.
- **Regression**: CRCBypass Install/Uninstall pass qua stub mới không crash.

### 3.5 Rủi ro

- Gadget trong ntdll có thể bị Packman patch trong tương lai. Mitigate bằng cách chọn gadget trong hàm ÍT bị patch (NtWaitForSingleObject, NtDelayExecution — cả 2 đã confirm không hooked ở snapshot RE).
- Nếu gadget bị patch giữa chừng runtime → stub crash. Mitigate: add `g_lastStubStatus` để caller check, fallback về emit-syscall stub cũ nếu enumerate = 0.

---

## 4. Module B — Hash-Based Name Resolve

### 4.1 Cơ chế

`SyscallEntry` cũ:
```cpp
struct SyscallEntry { const char* name; int fallbackSsn; void* stub; void* fn; int ssn; };
```

Thêm field `uint32_t nameHash`. Populate compile-time.

```cpp
constexpr uint32_t djb2(const char* s, uint32_t h = 5381) {
    return *s ? djb2(s + 1, ((h << 5) + h) ^ (uint8_t)*s) : h;
}
#define NS_HASH(name) (djb2(name))

struct SyscallEntry {
    uint32_t    nameHash;      // NEW
    const char* name;          // GIỮ (dùng cho DumpSyscallTable debug)
    int         fallbackSsn;
    void*       stub;
    void*       fn;
    int         ssn;
};

inline SyscallEntry g_syscalls[] = {
    { NS_HASH("NtProtectVirtualMemory"), "NtProtectVirtualMemory", 0x050, nullptr, nullptr, -1 },
    // ...
};
```

`name` field **giữ để debug/log**, nhưng trong RELEASE build (`NS_PACKMAN_SILENT` — Module J), name field sẽ được **thay bằng `nullptr`** qua conditional-compile để không stringify. `DumpSyscallTable` tự skip nếu name==nullptr.

### 4.2 Resolver

```cpp
inline int ResolveSSNInMemoryByHash(uint32_t nameHash) {
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return -1;
    auto* dos = (const IMAGE_DOS_HEADER*)h;
    auto* nt  = (const IMAGE_NT_HEADERS*)((const uint8_t*)h + dos->e_lfanew);
    auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    auto* exp = (const IMAGE_EXPORT_DIRECTORY*)((const uint8_t*)h + dir.VirtualAddress);
    auto* names = (const DWORD*)((const uint8_t*)h + exp->AddressOfNames);
    auto* ords  = (const WORD*)((const uint8_t*)h + exp->AddressOfNameOrdinals);
    auto* funcs = (const DWORD*)((const uint8_t*)h + exp->AddressOfFunctions);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* name = (const char*)((const uint8_t*)h + names[i]);
        if (djb2(name) == nameHash) {
            const uint8_t* p = (const uint8_t*)h + funcs[ords[i]];
            if (p[0]==0x4C && p[1]==0x8B && p[2]==0xD1 && p[3]==0xB8)
                return *(const uint32_t*)(p+4);
            return -1;
        }
    }
    return -1;
}
```

Disk variant tương tự (parse export dir từ mmap buffer).

`InitSyscall(size_t idx)` chuyển sang gọi `ResolveSSNInMemoryByHash(e.nameHash)` trước, fallback SSN sau.

### 4.3 API

- `constexpr uint32_t djb2(const char*)` public inline.
- Macro `NS_HASH(name)`.
- `int ResolveSSNInMemoryByHash(uint32_t)`, `int ResolveSSNFromDiskByHash(uint32_t)`.
- **Giữ nguyên** `ResolveSSNInMemory(const char*)` và `ResolveSSNFromDisk(const char*)` — internal fallback dùng cho debug.

### 4.4 Test

- **Static**:
  ```
  powershell -c "Select-String -Path build\NightSharp.dll -Pattern 'NtProtect' -Encoding Byte -SimpleMatch"
  ```
  → 0 hit **khi build với NS_PACKMAN_SILENT**.
- **Static (CE MCP)**:
  ```
  mcp__cheatengine__aob_scan_module("NightSharp.dll", "4E 74 50 72 6F 74", "+R")
  ```
  → 0 hit (bytes "NtProt").
- **Runtime**: `DumpSyscallTable()` với debug build vẫn print đủ tên (name field ON) → SSN đúng như table §1.4.
- **Hash-collision self-check**: `static_assert` compile-time verify tất cả 11 hash distinct.

### 4.5 Rủi ro

- djb2 32-bit collision xác suất trong ~28k export ntdll ≈ vô cùng nhỏ. Static assert protect.
- Compile-time constexpr yêu cầu C++14. Codebase đã dùng C++17 (verified qua các file `Corehook.h`). OK.

---

## 5. Module F — ThreadHideFromDebugger

### 5.1 Cơ chế

- Add entry `NtSetInformationThread` (SSN 0x0D) vào `g_syscalls[]` idx 10.
- Add typedef + wrapper `NtSetInformationThreadDirect(HANDLE, THREADINFOCLASS, PVOID, ULONG)`.
- Trong `CreateThreadDirect`, sau khi `hThread` valid nhưng TRƯỚC khi trả về:
  ```cpp
  if (hThread) {
      NtSetInformationThreadDirect(hThread, (THREADINFOCLASS)0x11 /*ThreadHideFromDebugger*/,
                                   nullptr, 0);
  }
  ```
- Áp dụng cho CẢ 2 branch (NtCreateThreadEx success + fallback CreateThread).

### 5.2 API

- Thêm typedef `NtSetInfoThreadFn`, wrapper `NtSetInformationThreadDirect`, enum `IDX_SETINFOTHREAD = 10`.
- `CreateThreadDirect` signature không đổi.

### 5.3 Test

- Attach x64dbg vào League sau khi inject → threads list không thấy thread tạo bởi NightSharp.
- Verify programmatically: `NtQueryInformationThread(hThread, 0x11, ...)` → hidden = TRUE.
- Regression: BootstrapWorker + DeferredCRCInstallThread vẫn spawn và chạy đúng (log ra `[CRC] Install: …`).

### 5.4 Rủi ro

- Rất thấp. `ThreadHideFromDebugger=0x11` là API tài liệu.

---

## 6. Module J — Log Toggle

### 6.1 Cơ chế

- Compile-time gate:
  ```cpp
  #ifdef NS_PACKMAN_SILENT
    #define DbgLog(msg)         ((void)0)
    #define DbgLogTs(msg)       ((void)0)
    #define DbgLogFmt(fmt, ...) ((void)0)
  #else
    // như hiện tại, nhưng qua g_logEnabled runtime flag
  #endif
  ```
- Runtime flag:
  ```cpp
  inline volatile LONG g_logEnabled = 1;
  inline void SetLogEnabled(bool en) { InterlockedExchange(&g_logEnabled, en ? 1 : 0); }
  ```
  `DbgLog` early-return nếu `g_logEnabled == 0`.
- `GetLogPath()` giữ nhưng nếu `NS_PACKMAN_SILENT`, file không mở → không tạo. `ResetLogFile()` cũng no-op.

### 6.2 API

- `void SetLogEnabled(bool)`.
- Macro `NS_PACKMAN_SILENT` (define trước `#include "PackmanHook.h"`).

### 6.3 Test

- Build có `-DNS_PACKMAN_SILENT` → không có `%TEMP%\ph.log`.
- Build normal + `SetLogEnabled(false)` runtime → log ngưng ghi.
- Verify string "ph.log" **không** xuất hiện trong NightSharp.dll khi silent build:
  ```
  mcp__cheatengine__aob_scan_module("NightSharp.dll", "70 68 2E 6C 6F 67", "+R")
  ```

### 6.4 Rủi ro

- Không.

---

## 7. Non-goals (explicitly out of scope this round)

- Ntdll head-byte unhook (Module D cũ) — có thể HURT anti-ban (Packman self-integrity check).
- PEB Ldr unlink (Module E cũ) — high-value nhưng risky, làm module riêng nếu cần.
- Multi-pattern CRC (Module H) — chưa cần vì CRC pattern hiện tại đã 1-match unique.
- Camouflaged stub (Module I) — không tương thích với FF 25 jmp gadget.
- Expanded Nt* table (Module C) — YAGNI. Chỉ add NtSetInformationThread cho F.

---

## 8. Backward compatibility

Caller hiện có:
- `Corehook.h`: dùng `CRCBypass::AddPatchAddress`, `g_fakedRegions`, `g_fakedReady`. **Không đụng**.
- `CoreZoomHack.h`, `CoreSkinChanger.h`: dùng `DirectSyscall::WriteVirtualMemoryDirect`. **Signature giữ**.
- `dllmain.cpp`: dùng `DirectSyscall::InitAll`, `DumpSyscallTable`, `CRCBypass::Uninstall`, `CreateThreadDirect`. **Signature giữ**, hành vi giữ. `InitAll` internal thêm bước `EnumerateSyscallGadgets()`.

Zero code change ngoài file `core/PackmanHook.h`.

---

## 9. Test matrix

Chạy TỪNG module xong test riêng, không gộp:

| Module | Verify tool | Pass criterion |
|---|---|---|
| B | CE aob_scan_module + Select-String | 0 hit cho "NtProt" / "NtWrit" / etc. trong NightSharp.dll (silent build) |
| A | CE aob_scan_module | 0 hit cho `0F 05 C3` trong NightSharp.dll +X |
| A | Runtime | `VirtualProtectDirect` trả TRUE, status ≥ 0 |
| A+B | Runtime | CRCBypass Install/Uninstall vẫn work; ZoomHack/SkinChanger writes vẫn work |
| J | Filesystem | `%TEMP%\ph.log` không tạo khi silent |
| J | CE aob_scan_module | 0 hit cho "ph.log" ASCII bytes trong NightSharp.dll |
| F | x64dbg | Thread NightSharp không xuất hiện trong Threads panel |
| F | Runtime | BootstrapWorker log `[CRC] Install: …` như bình thường |

---

## 10. Implement order + risk gate

1. **Module B** — resolve hash infrastructure. Sau khi build & test riêng, verify không regression trong luồng `InitAll`.
2. **Module J** — log toggle. Rất nhỏ, làm sớm để mọi test sau silent.
3. **Module A** — indirect syscall. Rủi ro cao nhất. Test kỹ:
   - Nếu regression CRCBypass hoặc syscall stub fail → rollback A, giữ B+J.
4. **Module F** — ThreadHide + NtSetInformationThread entry. Nhỏ, cuối.

Mỗi module xong: commit riêng (nếu repo là git — hiện chưa), test độc lập, mới sang module kế.
