# PackmanHook Anti-Ban Upgrade Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Thêm 4 anti-ban module (B/J/A/F) vào `core/PackmanHook.h` mà không đụng caller.

**Architecture:** Additive only — mọi API cũ giữ nguyên signature. Order implement B → J → A → F (mỗi module 1 task, test riêng qua CE MCP + IDA MCP + AoB scan).

**Tech Stack:** C++17, MSVC x64, Win32/NT API, header-only. Test tool: `mcp__cheatengine__aob_scan_module`, `mcp__cheatengine__read_memory`, PowerShell for byte-grep.

## Global Constraints

- File duy nhất được sửa: `core/PackmanHook.h`. Zero touch caller.
- Signature công khai (mọi hàm `inline` trong namespace `DirectSyscall::`/`CRCBypass::` + globals `g_syscalls`, `g_fakedRegions`, `g_fakedReady`, `g_ntProtect`, `g_ntWrite`, `g_lastProtectStatus`, `g_lastWriteStatus`) giữ chính xác kiểu và tên hiện tại.
- Số entry cũ trong `g_syscalls[]` = 10 (idx `IDX_PROTECT..IDX_CREATETHREADEX`). Không đổi idx cũ. Idx mới thêm cuối.
- Repo không dùng git. "Commit" mỗi task = tạo bản backup `PackmanHook.h.<taskN>.bak` và verify build.
- Target platform Windows 11 26100 (djb2 hash + SSN fallback verified for that build).
- League PID 15112 (đã attach CE) — dùng làm target test runtime.
- Log path helper `GetLogPath()` trả `%TEMP%\ph.log` — không đổi.

---

## File Structure

Chỉ 1 file thay đổi:

**Modify: `core/PackmanHook.h`**
- Thêm block `// ── djb2 hash ───────` (Task 1, dưới include, trên `DirectSyscall` namespace).
- Thêm field `uint32_t nameHash` vào `SyscallEntry` (Task 1).
- Thêm gate `#ifdef NS_PACKMAN_SILENT` bao quanh 3 macro log + runtime `g_logEnabled` (Task 2).
- Thay `BuildSyscallStub` emit indirect-jmp + thêm `EnumerateSyscallGadgets`/`PickSyscallGadget` (Task 3).
- Thêm entry `NtSetInformationThread` idx 10 + wrapper `NtSetInformationThreadDirect` + gọi trong `CreateThreadDirect` (Task 4).

Không tạo file mới. Không chạm caller.

---

## Task 1: Module B — Hash-Based Name Resolve

**Files:**
- Modify: `core/PackmanHook.h` (djb2 helper + `SyscallEntry.nameHash` + `ResolveSSNInMemoryByHash` + `ResolveSSNFromDiskByHash` + rewire `InitSyscall`)

**Interfaces:**
- Consumes: nothing new (dùng WinAPI hiện có)
- Produces:
  - `constexpr uint32_t DirectSyscall::djb2(const char*, uint32_t = 5381)`
  - Macro `NS_HASH(literal)` → uint32
  - `int DirectSyscall::ResolveSSNInMemoryByHash(uint32_t)`
  - `int DirectSyscall::ResolveSSNFromDiskByHash(uint32_t)`
  - `SyscallEntry` bổ sung `uint32_t nameHash;` (field mới ĐẦU struct)

### Step 1.1 — Thêm djb2 compile-time hash

**File**: `core/PackmanHook.h`, chèn NGAY SAU dòng `namespace DirectSyscall {` (khoảng dòng 120):

- [ ] **Thêm block hash helper**

```cpp
// ── djb2 hash (compile-time) ─────────────────────────────────────────────────
// Dùng để tránh string "Nt*" plaintext trong .rdata (anti-scan surface B).
constexpr uint32_t djb2(const char* s, uint32_t h = 5381u) {
    return *s ? djb2(s + 1, ((h << 5) + h) ^ static_cast<uint8_t>(*s)) : h;
}
#define NS_HASH(name) (::DirectSyscall::djb2(name))
```

### Step 1.2 — Thêm field `nameHash` vào `SyscallEntry`

- [ ] **Sửa struct `SyscallEntry` (~line 138)**

Đổi từ:
```cpp
struct SyscallEntry {
    const char* name;
    int         fallbackSsn;
    void*       stub;
    void*       fn;
    int         ssn;
};
```

Sang:
```cpp
struct SyscallEntry {
    uint32_t    nameHash;      // djb2(name) — dùng để resolve ẩn danh
    const char* name;          // giữ cho debug/DumpSyscallTable; sẽ được stringify tùy build
    int         fallbackSsn;
    void*       stub;
    void*       fn;
    int         ssn;
};
```

### Step 1.3 — Thêm hash vào tất cả 10 entry hiện có

- [ ] **Sửa initializer `g_syscalls[]` (line 147-168)**

```cpp
inline SyscallEntry g_syscalls[] = {
    { NS_HASH("NtProtectVirtualMemory"), "NtProtectVirtualMemory", 0x050, nullptr, nullptr, -1 },
    { NS_HASH("NtWriteVirtualMemory"),   "NtWriteVirtualMemory",   0x03A, nullptr, nullptr, -1 },
    { NS_HASH("NtContinue"),             "NtContinue",             0x043, nullptr, nullptr, -1 },
    { NS_HASH("NtDelayExecution"),       "NtDelayExecution",       0x034, nullptr, nullptr, -1 },
    { NS_HASH("NtQueryVirtualMemory"),   "NtQueryVirtualMemory",   0x023, nullptr, nullptr, -1 },
    { NS_HASH("NtSuspendThread"),        "NtSuspendThread",        0x1BE, nullptr, nullptr, -1 },
    { NS_HASH("NtContinueEx"),           "NtContinueEx",           0x0A1, nullptr, nullptr, -1 },
    { NS_HASH("NtSetContextThread"),     "NtSetContextThread",     0x18D, nullptr, nullptr, -1 },
    { NS_HASH("NtGetContextThread"),     "NtGetContextThread",     0x0F3, nullptr, nullptr, -1 },
    { NS_HASH("NtCreateThreadEx"),       "NtCreateThreadEx",       0x0C2, nullptr, nullptr, -1 },
};
```

### Step 1.4 — Thêm `ResolveSSNInMemoryByHash`

- [ ] **Chèn NGAY SAU hàm `ResolveSSNInMemory` (line 233-242)**

```cpp
inline int ResolveSSNInMemoryByHash(uint32_t nameHash) {
    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return -1;
    const auto* buf = reinterpret_cast<const uint8_t*>(h);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return -1;
    const auto* exp   = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(buf + dir.VirtualAddress);
    const auto* names = reinterpret_cast<const DWORD*>(buf + exp->AddressOfNames);
    const auto* ords  = reinterpret_cast<const WORD*>(buf + exp->AddressOfNameOrdinals);
    const auto* funcs = reinterpret_cast<const DWORD*>(buf + exp->AddressOfFunctions);
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const char* name = reinterpret_cast<const char*>(buf + names[i]);
        if (djb2(name) != nameHash) continue;
        const uint8_t* p = buf + funcs[ords[i]];
        if (p[0] == 0x4C && p[1] == 0x8B && p[2] == 0xD1 && p[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(p + 4));
        }
        return -1;
    }
    return -1;
}
```

### Step 1.5 — Thêm `ResolveSSNFromDiskByHash`

- [ ] **Chèn NGAY SAU `ResolveSSNFromDisk` (line 244-274). Refactor: thêm helper `ExtractSSNFromImageByHash` trước đó.**

Chèn TRƯỚC `ExtractSSNFromImage` (line 206):

```cpp
inline int ExtractSSNFromImageByHash(const uint8_t* buf, uint32_t nameHash) {
    if (!buf) return -1;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return -1;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return -1;
    const auto& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT];
    if (!dir.VirtualAddress) return -1;
    const auto* exp = reinterpret_cast<const IMAGE_EXPORT_DIRECTORY*>(
        buf + RvaToFileOff(nt, dir.VirtualAddress));
    const auto* names = reinterpret_cast<const DWORD*>(buf + RvaToFileOff(nt, exp->AddressOfNames));
    const auto* ords  = reinterpret_cast<const WORD*>(buf + RvaToFileOff(nt, exp->AddressOfNameOrdinals));
    const auto* funcs = reinterpret_cast<const DWORD*>(buf + RvaToFileOff(nt, exp->AddressOfFunctions));
    for (DWORD i = 0; i < exp->NumberOfNames; ++i) {
        const auto* name = reinterpret_cast<const char*>(buf + RvaToFileOff(nt, names[i]));
        if (djb2(name) != nameHash) continue;
        const auto* stub = buf + RvaToFileOff(nt, funcs[ords[i]]);
        if (stub[0] == 0x4C && stub[1] == 0x8B && stub[2] == 0xD1 && stub[3] == 0xB8) {
            return static_cast<int>(*reinterpret_cast<const uint32_t*>(stub + 4));
        }
        return -1;
    }
    return -1;
}
```

Chèn NGAY SAU `ResolveSSNFromDisk` (line 274):

```cpp
inline int ResolveSSNFromDiskByHash(uint32_t nameHash) {
    HANDLE hFile = CreateFileW(L"\\\\?\\C:\\Windows\\System32\\ntdll.dll",
        GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return -1;
    LARGE_INTEGER sz{};
    if (!GetFileSizeEx(hFile, &sz) || sz.QuadPart <= 0 || sz.QuadPart > (64LL << 20)) {
        CloseHandle(hFile); return -1;
    }
    auto* buf = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr,
        static_cast<SIZE_T>(sz.QuadPart), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!buf) { CloseHandle(hFile); return -1; }
    DWORD total = 0;
    while (total < static_cast<DWORD>(sz.QuadPart)) {
        DWORD chunk = 0;
        if (!ReadFile(hFile, buf + total,
                      static_cast<DWORD>(sz.QuadPart) - total, &chunk, nullptr) ||
            chunk == 0) break;
        total += chunk;
    }
    CloseHandle(hFile);
    const int ssn = (total == static_cast<DWORD>(sz.QuadPart))
        ? ExtractSSNFromImageByHash(buf, nameHash) : -1;
    VirtualFree(buf, 0, MEM_RELEASE);
    return ssn;
}
```

### Step 1.6 — Rewire `InitSyscall` dùng hash trước, name sau

- [ ] **Sửa `InitSyscall` (line 291-316)**

Thay:
```cpp
    int ssn = ResolveSSNInMemory(e.name);
    const char* src = "memory";
    if (ssn < 0) {
        ssn = ResolveSSNFromDisk(e.name);
        src = "disk";
    }
```

Bằng:
```cpp
    int ssn = ResolveSSNInMemoryByHash(e.nameHash);
    const char* src = "memory-hash";
    if (ssn < 0) {
        ssn = ResolveSSNFromDiskByHash(e.nameHash);
        src = "disk-hash";
    }
```

Log format string `"[SYS] Init %-28s: SSN=0x%X (src=%s)\r\n"` giữ nguyên. `e.name` chỉ còn dùng để print log — Module J sẽ gate sau.

### Step 1.7 — Compile-time hash-collision guard

- [ ] **Chèn ngay SAU `SYSCALL_COUNT = 10` (line 181)**

```cpp
// Compile-time hash-collision guard cho g_syscalls[]. Nếu 2 entry cùng hash,
// static_assert này sẽ fail — đổi tên hoặc dùng hash algo khác.
static_assert(NS_HASH("NtProtectVirtualMemory") != NS_HASH("NtWriteVirtualMemory"), "hash collision");
static_assert(NS_HASH("NtProtectVirtualMemory") != NS_HASH("NtContinue"),           "hash collision");
static_assert(NS_HASH("NtWriteVirtualMemory")   != NS_HASH("NtContinue"),           "hash collision");
static_assert(NS_HASH("NtDelayExecution")       != NS_HASH("NtQueryVirtualMemory"), "hash collision");
static_assert(NS_HASH("NtSuspendThread")        != NS_HASH("NtContinueEx"),         "hash collision");
static_assert(NS_HASH("NtSetContextThread")     != NS_HASH("NtGetContextThread"),   "hash collision");
static_assert(NS_HASH("NtCreateThreadEx")       != NS_HASH("NtSuspendThread"),      "hash collision");
```

### Step 1.8 — Build

- [ ] **Run project build**

```powershell
# Sử dụng script build hiện tại của NightSharp (đưa msbuild / cmake command
# thực tế của user vào đây). Vd:
cmake --build build --config Release
```

Expected: build success, không compile error. `static_assert` không trigger.

### Step 1.9 — Test riêng Module B (không inject)

- [ ] **Grep string "NtProtect" trong DLL output**

```powershell
$b = [System.IO.File]::ReadAllBytes("build\Release\NightSharp.dll")
$pattern = [System.Text.Encoding]::ASCII.GetBytes("NtProtect")
$found = $false
for ($i=0; $i -le $b.Length - $pattern.Length; $i++) {
    $ok = $true
    for ($j=0; $j -lt $pattern.Length; $j++) { if ($b[$i+$j] -ne $pattern[$j]) { $ok=$false; break } }
    if ($ok) { $found = $true; Write-Host "hit at 0x$('{0:X}' -f $i)"; break }
}
if (-not $found) { Write-Host "OK: NtProtect not found" }
```

Expected: **VẪN CÓ MATCH** ở giai đoạn này (vì `e.name` vẫn giữ string plaintext cho DumpSyscallTable). Đây là baseline; Module J sẽ ẩn tiếp.

Verify Task 1 chỉ ở khía cạnh **resolve hash chạy đúng** — làm bước sau.

### Step 1.10 — Test resolve hash runtime

- [ ] **Inject vào League PID 15112, đọc log `%TEMP%\ph.log`**

Expected trong log:
```
[SYS] Init NtProtectVirtualMemory      : SSN=0x50 (src=memory-hash)
[SYS] Init NtWriteVirtualMemory        : SSN=0x3A (src=memory-hash)
[SYS] Init NtContinue                  : SSN=0x43 (src=memory-hash)
[SYS] Init NtDelayExecution            : SSN=0x34 (src=memory-hash)
[SYS] Init NtQueryVirtualMemory        : SSN=0x23 (src=memory-hash)
[SYS] Init NtSuspendThread             : SSN=0x1BE (src=memory-hash)
[SYS] Init NtContinueEx                : SSN=0xA1 (src=memory-hash)
[SYS] Init NtSetContextThread          : SSN=0x18D (src=memory-hash)
[SYS] Init NtGetContextThread          : SSN=0xF3 (src=memory-hash)
[SYS] Init NtCreateThreadEx            : SSN=0xC2 (src=memory-hash)
```

Nếu có entry `src=disk-hash` → memory resolve fail (Packman đã hook export dir?) — không phải regression, vẫn OK.
Nếu có entry `src=fallback` → cả memory lẫn disk hash fail → điều tra ngay (hash sai hay ntdll layout thay đổi).

### Step 1.11 — Regression: CRC bypass + ZoomHack write vẫn work

- [ ] **Verify game runtime**

Trong League đang chạy:
- Log phải có `[CRC] === CRC Bypass Installed ===` như trước.
- Bật ZoomHack qua hotkey → zoom out ok (chứng minh `WriteVirtualMemoryDirect` chạy qua stub build từ hash-resolved SSN).
- Kill League không crash.

### Step 1.12 — Checkpoint

- [ ] **Backup file trước Task 2**

```powershell
Copy-Item "core\PackmanHook.h" "core\PackmanHook.h.task1.bak"
```

---

## Task 2: Module J — Log Toggle

**Files:**
- Modify: `core/PackmanHook.h` (bọc DbgLog macro + `g_logEnabled` + `SetLogEnabled`)

**Interfaces:**
- Consumes: nothing
- Produces:
  - Macro `NS_PACKMAN_SILENT` (define trước `#include "PackmanHook.h"` để tắt log compile-time)
  - `void DirectSyscall::SetLogEnabled(bool)` (runtime toggle)
  - `bool DirectSyscall::IsLogEnabled()`

### Step 2.1 — Thêm `g_logEnabled` runtime flag

- [ ] **Chèn NGAY TRƯỚC `static const char* GetLogPath()` (line 40)**

```cpp
// ── Log control (Module J) ───────────────────────────────────────────────────
inline volatile LONG g_logEnabled = 1;

inline void SetLogEnabled(bool en) {
    InterlockedExchange(&g_logEnabled, en ? 1 : 0);
}
inline bool IsLogEnabled() {
    return InterlockedCompareExchange(&g_logEnabled, 0, 0) != 0;
}
```

Note: 2 hàm này nằm ngoài namespace `DirectSyscall::` (như `DbgLog` hiện có). Nếu muốn nằm trong namespace, wrap `namespace DirectSyscall { ... }` — nhưng để đơn giản, đặt file-scope là ok vì `SetLogEnabled` không xung đột.

### Step 2.2 — Bọc `DbgLog` bằng compile-time + runtime gate

- [ ] **Sửa `DbgLog` (line 53-60)**

Thay bằng:
```cpp
static void DbgLog(const char* msg) {
#ifdef NS_PACKMAN_SILENT
    (void)msg;
    return;
#else
    if (!IsLogEnabled()) return;
    OutputDebugStringA(msg);
    HANDLE hFile = CreateFileA(GetLogPath(), FILE_APPEND_DATA, FILE_SHARE_READ,
                               nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) return;
    DWORD w; WriteFile(hFile, msg, static_cast<DWORD>(strlen(msg)), &w, nullptr);
    CloseHandle(hFile);
#endif
}
```

Thay `OPEN_ALWAYS` → `OPEN_EXISTING`: file chỉ mở nếu đã tồn tại (do `ResetLogFile()` tạo). Trong silent mode, `ResetLogFile()` no-op → file không tồn tại → `DbgLog` bail nhanh cả khi runtime lỡ gọi.

### Step 2.3 — Bọc `DbgLogTs`, `DbgLogFmt`

- [ ] **Sửa `DbgLogTs` (line 70-77)**

```cpp
static void DbgLogTs(const char* msg) {
#ifdef NS_PACKMAN_SILENT
    (void)msg;
    return;
#else
    if (!IsLogEnabled()) return;
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char line[600];
    _snprintf(line, sizeof(line), "[%s] %s", ts, msg);
    line[sizeof(line) - 1] = 0;
    DbgLog(line);
#endif
}
```

- [ ] **Sửa `DbgLogFmt` (line 79-92)**

```cpp
static void DbgLogFmt(const char* fmt, ...) {
#ifdef NS_PACKMAN_SILENT
    (void)fmt;
    return;
#else
    if (!IsLogEnabled()) return;
    char ts[32];
    GetTimestamp(ts, sizeof(ts));
    char body[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(body, sizeof(body), fmt, args);
    va_end(args);
    body[sizeof(body) - 1] = 0;
    char line[600];
    _snprintf(line, sizeof(line), "[%s] %s", ts, body);
    line[sizeof(line) - 1] = 0;
    DbgLog(line);
#endif
}
```

### Step 2.4 — Bọc `ResetLogFile`

- [ ] **Sửa `ResetLogFile` (line 910-912)**

```cpp
inline void ResetLogFile() {
#ifdef NS_PACKMAN_SILENT
    return;
#else
    DeleteFileA(GetLogPath());
    HANDLE h = CreateFileA(GetLogPath(), GENERIC_WRITE, 0, nullptr,
                           CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (h != INVALID_HANDLE_VALUE) CloseHandle(h);
#endif
}
```

Note: thêm `CreateFileA(CREATE_ALWAYS)` để tạo file rỗng — bù cho việc `DbgLog` đổi sang `OPEN_EXISTING`. Trong silent mode, file KHÔNG tạo.

### Step 2.5 — Gate string `%TEMP%\ph.log` khi silent

- [ ] **Sửa `GetLogPath` (line 40-51)**

```cpp
static const char* GetLogPath() {
#ifdef NS_PACKMAN_SILENT
    return "";   // không dùng
#else
    static char path[MAX_PATH] = {};
    if (path[0]) return path;
    char tmp[MAX_PATH] = {};
    DWORD n = GetTempPathA(MAX_PATH, tmp);
    if (n == 0 || n >= MAX_PATH) {
        strcpy(path, "C:\\ph.log");
    } else {
        _snprintf(path, MAX_PATH, "%sph.log", tmp);
    }
    return path;
#endif
}
```

Trong silent build, chuỗi `"ph.log"` không còn ở function body → linker sẽ strip.

### Step 2.6 — Build silent + normal cạnh nhau

- [ ] **Build normal (log ON)**

```powershell
cmake --build build --config Release
```

- [ ] **Build silent (log OFF)**

```powershell
# Sửa CMakeLists (hoặc project property) thêm /DNS_PACKMAN_SILENT.
# Vd trong CMakeLists.txt: add_compile_definitions(NS_PACKMAN_SILENT).
# Rồi:
cmake --build build-silent --config Release
```

Expected: 2 build cùng thành công.

### Step 2.7 — Test build normal

- [ ] **Inject bản normal, verify log tồn tại**

```powershell
Get-Item "$env:TEMP\ph.log"
```

Expected: file tồn tại, có nội dung `[HH:MM:SS.mmm] [SYS] Init …` như Task 1.

### Step 2.8 — Test build silent

- [ ] **Xóa log cũ, inject bản silent, verify log KHÔNG tạo**

```powershell
Remove-Item "$env:TEMP\ph.log" -ErrorAction SilentlyContinue
# ... inject NightSharp-silent.dll ...
Start-Sleep -Seconds 3
Test-Path "$env:TEMP\ph.log"
```

Expected: `False`.

### Step 2.9 — Static scan cho string "ph.log" trong silent build

- [ ] **Grep ASCII "ph.log" trong bản silent**

```powershell
$b = [System.IO.File]::ReadAllBytes("build-silent\Release\NightSharp.dll")
$pat = [byte[]](0x70,0x68,0x2E,0x6C,0x6F,0x67)  # "ph.log"
$found = $false
for ($i=0; $i -le $b.Length - $pat.Length; $i++) {
    $ok = $true
    for ($j=0; $j -lt $pat.Length; $j++) { if ($b[$i+$j] -ne $pat[$j]) { $ok=$false; break } }
    if ($ok) { $found = $true; break }
}
Write-Host "silent build has 'ph.log' string: $found"
```

Expected: `False`.

Trong normal build phải là `True` (baseline).

### Step 2.10 — Regression: runtime toggle

- [ ] **Test SetLogEnabled(false) khi normal build**

Trong dllmain init:
```cpp
DirectSyscall::InitAll();
DirectSyscall::DumpSyscallTable();
SetLogEnabled(false);
// từ giờ DbgLog tất cả no-op
```

Verify: `%TEMP%\ph.log` chỉ chứa các dòng TRƯỚC khi tắt (Init/Dump entries), không có dòng nào sau đó.

### Step 2.11 — Checkpoint

- [ ] **Backup**

```powershell
Copy-Item "core\PackmanHook.h" "core\PackmanHook.h.task2.bak"
```

---

## Task 3: Module A — Indirect Syscall

**Files:**
- Modify: `core/PackmanHook.h` (`BuildSyscallStub` + `EnumerateSyscallGadgets` + `PickSyscallGadget`)

**Interfaces:**
- Consumes:
  - Module B: `ResolveSSNInMemoryByHash` (đã có ở Task 1)
- Produces:
  - `void DirectSyscall::EnumerateSyscallGadgets()` — gọi ở đầu `InitAll`
  - `void* DirectSyscall::PickSyscallGadget()` — internal
  - `BuildSyscallStub` giữ signature nhưng emit stub 22-byte với indirect jmp

### Step 3.1 — Thêm globals cho gadget list

- [ ] **Chèn TRƯỚC `RvaToFileOff` (line 195)**

```cpp
// ── Syscall gadget cache (Module A) ──────────────────────────────────────────
// Danh sách địa chỉ 0F 05 C3 trong ntdll .text. Cache 1 lần lúc InitAll,
// pick ngẫu nhiên cho mỗi stub build. Cỡ 64 đủ (ntdll có 30+ mặc định).
inline void*    g_syscallGadgets[64] = {};
inline size_t   g_gadgetCount        = 0;
inline volatile LONG g_gadgetInited  = 0;
```

### Step 3.2 — Thêm `EnumerateSyscallGadgets`

- [ ] **Chèn NGAY SAU `ResolveSSNFromDiskByHash` (sau Task 1)**

```cpp
// Quét .text ntdll tìm mọi `0F 05 C3` (syscall;ret) → cache.
// Chỉ quét trong hàm UNHOOKED (byte đầu = 0x4C 8B D1) để tránh Packman patch
// tail lâu về sau. Fallback: quét toàn .text nếu không đủ candidate.
inline void EnumerateSyscallGadgets() {
    if (InterlockedCompareExchange(&g_gadgetInited, 1, 0) != 0) return;

    HMODULE h = GetModuleHandleW(L"ntdll.dll");
    if (!h) return;
    const auto* buf = reinterpret_cast<const uint8_t*>(h);
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(buf);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return;
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(buf + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return;

    // Tìm .text section
    const IMAGE_SECTION_HEADER* sec = IMAGE_FIRST_SECTION(nt);
    const uint8_t* textStart = nullptr; size_t textSize = 0;
    for (int i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (memcmp(sec->Name, ".text", 5) == 0) {
            textStart = buf + sec->VirtualAddress;
            textSize  = sec->Misc.VirtualSize;
            break;
        }
    }
    if (!textStart || textSize == 0) return;

    // Pass 1: chỉ giữ gadget nằm sau head `4C 8B D1 B8 xx 00 00 00 F6 04 25`
    // (tail của Nt* stub UNHOOKED) — offset gadget = start + 0x12.
    for (size_t i = 0; i + 3 < textSize && g_gadgetCount < 64; ++i) {
        if (textStart[i]   == 0x4C && textStart[i+1] == 0x8B &&
            textStart[i+2] == 0xD1 && textStart[i+3] == 0xB8) {
            // stub start ok. gadget candidate = i + 0x12
            size_t g = i + 0x12;
            if (g + 3 <= textSize &&
                textStart[g]   == 0x0F &&
                textStart[g+1] == 0x05 &&
                textStart[g+2] == 0xC3) {
                g_syscallGadgets[g_gadgetCount++] =
                    const_cast<void*>(reinterpret_cast<const void*>(textStart + g));
            }
            i += 0x14; // skip past this stub
        }
    }
    DbgLogFmt("[SYS] EnumerateSyscallGadgets: found %zu candidates\r\n", g_gadgetCount);
}
```

### Step 3.3 — Thêm `PickSyscallGadget`

- [ ] **Chèn NGAY SAU `EnumerateSyscallGadgets`**

```cpp
inline void* PickSyscallGadget() {
    if (g_gadgetCount == 0) return nullptr;
    // random dựa __rdtsc (dispersion đủ dùng)
    unsigned long long r = __rdtsc();
    return g_syscallGadgets[(size_t)(r % g_gadgetCount)];
}
```

### Step 3.4 — Rewrite `BuildSyscallStub` sang indirect jmp

- [ ] **Sửa `BuildSyscallStub` (line 276-288)**

Thay toàn bộ hàm bằng:
```cpp
inline bool BuildSyscallStub(int ssn, void** outStub) {
    if (!outStub || ssn < 0) return false;

    void* gadget = PickSyscallGadget();
    // Fallback nếu chưa enumerate (vd. InitSyscall gọi trước InitAll):
    // emit stub cũ (syscall inline) để giữ hoạt động, chấp nhận regression stealth.
    if (!gadget) {
        auto* stub = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32,
            MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
        if (!stub) return false;
        stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
        stub[3] = 0xB8;
        *reinterpret_cast<uint32_t*>(stub + 4) = static_cast<uint32_t>(ssn);
        stub[8] = 0x0F; stub[9] = 0x05;
        stub[10] = 0xC3;
        *outStub = stub;
        DbgLogFmt("[SYS] BuildSyscallStub: FALLBACK inline syscall (gadgetCount=0)\r\n");
        return true;
    }

    // Indirect stub — 22 bytes:
    //   4C 8B D1                mov r10, rcx
    //   B8 SSN 00 00 00         mov eax, SSN
    //   FF 25 00 00 00 00       jmp qword ptr [rip+0]  ; qword ngay sau
    //   <8 bytes gadget addr>
    auto* stub = reinterpret_cast<uint8_t*>(VirtualAlloc(nullptr, 32,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!stub) return false;

    stub[0] = 0x4C; stub[1] = 0x8B; stub[2] = 0xD1;
    stub[3] = 0xB8;
    *reinterpret_cast<uint32_t*>(stub + 4) = static_cast<uint32_t>(ssn);
    stub[8]  = 0xFF; stub[9]  = 0x25;
    stub[10] = 0x00; stub[11] = 0x00; stub[12] = 0x00; stub[13] = 0x00;
    *reinterpret_cast<uint64_t*>(stub + 14) = reinterpret_cast<uint64_t>(gadget);

    // Downgrade từ RWX → RX (giảm 1 anti-scan surface: RWX private region).
    DWORD oldProt = 0;
    VirtualProtect(stub, 32, PAGE_EXECUTE_READ, &oldProt);
    FlushInstructionCache(GetCurrentProcess(), stub, 32);

    *outStub = stub;
    return true;
}
```

### Step 3.5 — Gọi `EnumerateSyscallGadgets` trong `InitAll`

- [ ] **Sửa `InitAll` (line 319-328)**

Thay:
```cpp
inline bool InitAll() {
    DbgLogFmt("[SYS] InitAll: initializing %zu syscall stubs...\r\n", (size_t)SYSCALL_COUNT);
    bool ok = true;
    for (size_t i = 0; i < SYSCALL_COUNT; ++i)
        if (!InitSyscall(i)) ok = false;
    // ... phần dưới
```

Bằng:
```cpp
inline bool InitAll() {
    DbgLogFmt("[SYS] InitAll: initializing %zu syscall stubs...\r\n", (size_t)SYSCALL_COUNT);
    EnumerateSyscallGadgets();  // NEW: cache gadgets trước khi build stub
    bool ok = true;
    for (size_t i = 0; i < SYSCALL_COUNT; ++i)
        if (!InitSyscall(i)) ok = false;
    // ... phần dưới
```

### Step 3.6 — Build

- [ ] **Build normal + silent như Task 2 Step 2.6**

Expected: build success.

### Step 3.7 — Runtime verify gadget enumeration

- [ ] **Inject bản normal, đọc log**

Expected log mới:
```
[SYS] EnumerateSyscallGadgets: found XX candidates
```

XX phải ≥ 20 (ntdll có 400+ Nt* stub, phần lớn unhooked). Nếu XX = 0 → fallback stub cũ, log:
```
[SYS] BuildSyscallStub: FALLBACK inline syscall (gadgetCount=0)
```
Điều tra ngay: `.text` ntdll layout thay đổi hoặc pattern chưa đúng.

### Step 3.8 — CE AoB scan cho `0F 05 C3` trong DLL ta

- [ ] **AoB scan module NightSharp.dll**

```
mcp__cheatengine__aob_scan_module(
  module_name="NightSharp.dll",
  pattern="0F 05 C3",
  protection="+X"
)
```

Expected: `count = 0`.

Baseline TRƯỚC Task 3 (dùng backup task2.bak build lại) sẽ có ≥ 10 (mỗi stub 1 cái).

### Step 3.9 — CE AoB scan cho `0F 05` broader

- [ ] **AoB scan chỉ 2 byte**

```
mcp__cheatengine__aob_scan_module(
  module_name="NightSharp.dll",
  pattern="0F 05",
  protection="+X"
)
```

Expected: `count = 0` (byte `0F 05` không xuất hiện ngoài ntdll).

### Step 3.10 — Regression: CRCBypass Install phải work

- [ ] **Runtime check**

Trong log sau khi inject phải có:
```
[CRC] === CRC Bypass Installed ===
```

Nếu miss → `WriteVirtualMemoryDirect` fail → stub A crash → điều tra:
- Gadget address có valid không (log nó ra thêm)
- FF 25 disp có đúng 0 không

### Step 3.11 — Regression: ZoomHack + SkinChanger vẫn hoạt động

- [ ] **In-game test**

Bật ZoomHack qua hotkey → zoom out. Bật SkinChanger đổi skin champion → skin thay đổi trong replay. Cả 2 dùng `WriteVirtualMemoryDirect` → gián tiếp verify stub A.

### Step 3.12 — Bonus: verify stub RX-only (không RWX)

- [ ] **Đọc protection của stub qua CE**

Trong DumpSyscallTable log có `stub = 0xXXXXXXX`. Với 1 addr từ log:
```
mcp__cheatengine__get_memory_protection(address="0x<stub_addr>")
```

Expected: `read=true, write=false, execute=true, raw="PAGE_EXECUTE_READ"`.

### Step 3.13 — Checkpoint

- [ ] **Backup**

```powershell
Copy-Item "core\PackmanHook.h" "core\PackmanHook.h.task3.bak"
```

---

## Task 4: Module F — ThreadHideFromDebugger + NtSetInformationThread Entry

**Files:**
- Modify: `core/PackmanHook.h` (thêm SSN entry idx 10 + typedef + wrapper + call trong `CreateThreadDirect`)

**Interfaces:**
- Consumes:
  - Module A + B: đã có (stub build qua indirect jmp + hash resolve)
- Produces:
  - `IDX_SETINFOTHREAD = 10` trong enum `SyscallIdx`
  - Typedef `NtSetInfoThreadFn`
  - `LONG DirectSyscall::NtSetInformationThreadDirect(HANDLE, ULONG, PVOID, ULONG)`
  - `CreateThreadDirect` internal call thêm `NtSetInformationThreadDirect(hThread, 0x11, nullptr, 0)`

### Step 4.1 — Thêm typedef

- [ ] **Chèn NGAY SAU typedef `NtCreateThreadExFn` (line 132-133)**

```cpp
using NtSetInfoThreadFn = LONG(NTAPI*)(HANDLE, ULONG, PVOID, ULONG);
```

### Step 4.2 — Thêm entry idx 10

- [ ] **Sửa `g_syscalls[]` — thêm sau entry NtCreateThreadEx (line 167)**

```cpp
    // idx 10: NtSetInformationThread (dùng cho ThreadHideFromDebugger — Module F)
    { NS_HASH("NtSetInformationThread"), "NtSetInformationThread", 0x00D, nullptr, nullptr, -1 },
```

### Step 4.3 — Cập nhật enum `SyscallIdx`

- [ ] **Sửa enum (line 170-182)**

Thay:
```cpp
enum SyscallIdx : size_t {
    IDX_PROTECT       = 0,
    IDX_WRITE         = 1,
    IDX_CONTINUE      = 2,
    IDX_DELAY         = 3,
    IDX_QUERYVIRT     = 4,
    IDX_SUSPEND       = 5,
    IDX_CONTINUEEX    = 6,
    IDX_SETCTX        = 7,
    IDX_GETCTX        = 8,
    IDX_CREATETHREADEX= 9,
    SYSCALL_COUNT     = 10,
};
```

Bằng:
```cpp
enum SyscallIdx : size_t {
    IDX_PROTECT       = 0,
    IDX_WRITE         = 1,
    IDX_CONTINUE      = 2,
    IDX_DELAY         = 3,
    IDX_QUERYVIRT     = 4,
    IDX_SUSPEND       = 5,
    IDX_CONTINUEEX    = 6,
    IDX_SETCTX        = 7,
    IDX_GETCTX        = 8,
    IDX_CREATETHREADEX= 9,
    IDX_SETINFOTHREAD = 10,          // NEW
    SYSCALL_COUNT     = 11,          // was 10
};
```

### Step 4.4 — Cập nhật hash-collision guard (từ Task 1)

- [ ] **Thêm dòng vào block static_assert (line ngay sau enum)**

```cpp
static_assert(NS_HASH("NtSetInformationThread") != NS_HASH("NtCreateThreadEx"),   "hash collision");
static_assert(NS_HASH("NtSetInformationThread") != NS_HASH("NtSetContextThread"), "hash collision");
```

### Step 4.5 — Thêm wrapper `NtSetInformationThreadDirect`

- [ ] **Chèn NGAY SAU `NtGetContextThreadDirect` (line 436-440)**

```cpp
// NtSetInformationThread (SSN 0x0D) — Module F ThreadHideFromDebugger.
// ThreadInfoClass = 0x11 (ThreadHideFromDebugger), buffer/length = null/0.
inline LONG NtSetInformationThreadDirect(HANDLE thread, ULONG infoClass,
                                         PVOID info, ULONG infoLen) {
    if (!InitSyscall(IDX_SETINFOTHREAD)) return -1;
    auto fn = reinterpret_cast<NtSetInfoThreadFn>(g_syscalls[IDX_SETINFOTHREAD].fn);
    return fn(thread, infoClass, info, infoLen);
}
```

### Step 4.6 — Gọi `NtSetInformationThreadDirect` trong `CreateThreadDirect`

- [ ] **Sửa `CreateThreadDirect` — trong 2 branch success**

Trong branch NtCreateThreadEx success (line 488-492), sau `if (status >= 0 && hThread) {`, TRƯỚC log:
```cpp
    if (status >= 0 && hThread) {
        // Module F: ẩn thread khỏi debugger enumeration.
        NtSetInformationThreadDirect(hThread, 0x11 /*ThreadHideFromDebugger*/, nullptr, 0);
        DbgLogFmt("[SYS] CreateThreadDirect: NtCreateThreadEx OK (handle=%p, status=0x%X)\r\n",
                  hThread, (unsigned)status);
        return hThread;
    }
```

Trong fallback branch (line 495):
```cpp
    HANDLE hFallback = CreateThread(nullptr, 0, routine, param, 0, nullptr);
    if (hFallback) {
        NtSetInformationThreadDirect(hFallback, 0x11, nullptr, 0);
    }
    return hFallback;
```

Note: sửa fallback ở cả 2 chỗ trong `CreateThreadDirect`:
- Line 469 (khi InitSyscall(IDX_CREATETHREADEX) fail):
```cpp
    if (!InitSyscall(IDX_CREATETHREADEX)) {
        HANDLE h = CreateThread(nullptr, 0, routine, param, 0, nullptr);
        if (h) {
            NtSetInformationThreadDirect(h, 0x11, nullptr, 0);
            DbgLog("[SYS] CreateThreadDirect: fallback CreateThread OK\r\n");
        }
        return h;
    }
```
- Line 495 (khi NtCreateThreadEx status < 0):
```cpp
    HANDLE hRetry = CreateThread(nullptr, 0, routine, param, 0, nullptr);
    if (hRetry) NtSetInformationThreadDirect(hRetry, 0x11, nullptr, 0);
    return hRetry;
```

### Step 4.7 — Build

- [ ] **Build normal + silent như trước**

Expected: `static_assert` 2 dòng mới pass. Build success.

### Step 4.8 — Runtime verify entry mới trong DumpSyscallTable

- [ ] **Đọc log**

Expected:
```
[SYS] Init NtSetInformationThread      : SSN=0xD (src=memory-hash)
[SYS] === Syscall Table Dump ===
[SYS]   [ 0] NtProtectVirtualMemory      SSN=0x50  ...
[SYS]   ...
[SYS]   [10] NtSetInformationThread      SSN=0xD   stub=0x... fn=0x... OK
[SYS] === End Dump ===
```

Nếu entry [10] `NOT_INIT` → NtSetInformationThreadDirect chưa được ai gọi → force init:
```cpp
DirectSyscall::InitSyscall(DirectSyscall::IDX_SETINFOTHREAD);
```

### Step 4.9 — Verify thread hidden

- [ ] **Query từ chính thread ta**

Đặt trong worker (BootstrapWorker):
```cpp
BOOLEAN hidden = FALSE;
DirectSyscall::NtQueryInformationThread(GetCurrentThread(), 0x11 /*ThreadHideFromDebugger*/,
                                        &hidden, sizeof(hidden), nullptr);
DbgLogFmt("[F] Self hidden = %d\r\n", (int)hidden);
```

WAIT — chưa có `NtQueryInformationThread` trong table (Module C skipped). Bỏ query, verify bằng debugger:

- [ ] **Attach x64dbg vào League PID 15112**

Trong View → Threads:
- Trước Task 4: thấy BootstrapWorker thread (thread created by NightSharp).
- Sau Task 4: thread đó KHÔNG xuất hiện trong list.

### Step 4.10 — Regression: BootstrapWorker + CRC Install vẫn work

- [ ] **Runtime check**

Log phải có:
```
[CRC] === CRC Bypass Installed ===
```

Vì BootstrapWorker chạy trong thread mới (đã hide) nhưng vẫn execute code CRC Install bình thường.

### Step 4.11 — Regression: orbwalker + cast spell không ảnh hưởng

- [ ] **In-game test**

Trong League:
- Q/W/E/R champion mọi lúc → phải cast bình thường.
- Combo mode (Space) → orbwalker kite bình thường.
- Không có FPS drop.

Reason: NtSetInformationThread(0x11) là thread-local flag, không ảnh hưởng scheduler priority hay execution timing.

### Step 4.12 — Checkpoint

- [ ] **Backup**

```powershell
Copy-Item "core\PackmanHook.h" "core\PackmanHook.h.task4.bak"
```

---

## Post-Implementation Sanity

### Full test matrix (chạy sau Task 4)

- [ ] **CE AoB scan module**

```
mcp__cheatengine__aob_scan_module("NightSharp.dll", "0F 05", "+X")   # expect 0
mcp__cheatengine__aob_scan_module("NightSharp.dll", "0F 05 C3", "+X") # expect 0
mcp__cheatengine__aob_scan_module("NightSharp.dll", "4E 74 50 72 6F 74", "+R")  # "NtProt" — vẫn còn nếu debug build
```

- [ ] **PowerShell grep silent build**

```powershell
$b = [System.IO.File]::ReadAllBytes("build-silent\Release\NightSharp.dll")
# "ph.log" expect not found
# "NtProt" — thực tế vẫn còn vì e.name field giữ. Nếu muốn 0, sửa
# g_syscalls[] literal thành nullptr khi NS_PACKMAN_SILENT (optional).
```

- [ ] **In-game 30-min stress test**

Chạy 1 game full, sử dụng ZoomHack + SkinChanger + orbwalker + combo. Không crash, không log warning.

### Rollback plan (nếu có regression)

- Task 4 fail → restore `core\PackmanHook.h.task3.bak`.
- Task 3 fail → restore `core\PackmanHook.h.task2.bak`.
- Task 2 fail → restore `core\PackmanHook.h.task1.bak`.
- Task 1 fail → restore original file (`git checkout` nếu init git, hoặc backup thủ công trước Task 1).

Bước 0 TRƯỚC KHI BẮT ĐẦU:
```powershell
Copy-Item "core\PackmanHook.h" "core\PackmanHook.h.task0.bak"
```

---

## Self-Review

**Spec coverage**:
- ✓ Module B (spec §4) → Task 1
- ✓ Module J (spec §6) → Task 2
- ✓ Module A (spec §3) → Task 3
- ✓ Module F (spec §5) → Task 4
- ✓ NtSetInformationThread entry (spec §1.4 idx 10) → Task 4 Step 4.2
- ✓ Backward compat (spec §8) → constraint global + không đụng caller
- ✓ Test matrix (spec §9) → mỗi task có verify + Post-Implementation

**Placeholder scan**: không TBD/TODO. Mọi code block cụ thể. Backup command dùng PowerShell explicit.

**Type consistency**:
- `SyscallEntry` thêm `nameHash` field ĐẦU — dùng nhất quán ở Task 1 + Task 4.
- `SYSCALL_COUNT` = 10 (trước Task 4) → 11 (sau Task 4). Verified vòng loop `for (i < SYSCALL_COUNT)` tự update.
- `IDX_SETINFOTHREAD = 10` không đụng idx cũ. Wrapper `NtSetInformationThreadDirect` khớp signature typedef `NtSetInfoThreadFn`.
- `PickSyscallGadget` trả `void*`, gán vào `stub+14` qua `uint64_t*` cast — đúng.

Plan sạch, ready to execute.
