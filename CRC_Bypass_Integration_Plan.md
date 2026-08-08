# CRC Bypass Integration Plan — NightSharp

> **File riêng cho integration plan.**
> Phân tích kỹ thuật chi tiết (addresses, patterns, disassembly) xem tại `CRC_Bypass_Analysis.md`.
> Plan này chứa code integration + steps + checklist.

**Method:** Shadow Copy + NOP JNE (kết hợp Method 3 + Method 7.4 từ `CRC_Bypass_Analysis.md` §11.5)

- **NOP JNE** — patch 6 bytes tại comparison point (RVA `+0x10341F`) → skip mismatch handler. Đơn giản, đã verify live qua CE MCP.
- **Shadow Copy** — bản sao sạch `stub.dll` được tạo **trước** khi patch. Dùng làm fallback/redirect source cho CRC read nếu NOP JNE bị phát hiện hoặc anti-cheat self-check.

---

## 1. Trạng Thái Hiện Tại

| Component | File | Trạng thái |
|-----------|------|-----------|
| CRC Bypass master switch | `PackmanHook.h:45` | `NIGHTSHARP_ENABLE_CRC_BYPASS = 0` → **DISABLED** |
| Legacy bypass (ChaCha20 QR hook) | `PackmanHook.h:1201-1634` | **DETECTED** — cần xóa toàn bộ |
| Corehook.h AddPatchAddress calls | `Corehook.h:1106-1110` | Gọi `CRCBypass::AddPatchAddress()` — cần xóa |
| Shadow Copy | Chưa có | **Cần implement** |
| CRC comparison bypass (NOP JNE) | Chưa có | **Cần implement** |

---

## 2. Bước 1: Xóa Method Cũ (Detected)

### 2.1. Lý do xóa

Method cũ (ChaCha20 QR hook) **đã bị detect** vì:

| Vấn đề | Chi tiết |
|--------|----------|
| Inline hook 19 bytes tại ChaCha20 pattern | Pattern `49 8B 0E F3 44 0F 6F 04 29` là signature dễ scan |
| JMP đến hook stub | `FF 25` + alloc executable memory → detectable |
| Hook stub save/restore all registers | 16 push + pushfq pattern rất đặc trưng |
| `CheckMemoryBlocks` redirect r14 pointers | Thay đổi register state tại CRC read site |
| `AddPatchAddress` per-hook | Tạo faked regions — footprint lớn |
| `BuildHookStub` alloc qua section mapping | `MEM_MAPPED` + `PAGE_EXECUTE` — scanner tìm được |

### 2.2. Xóa trong `PackmanHook.h`

**Xóa toàn bộ namespace `CRCBypass` cũ (line 1201–1581):**

```
// XÓA TẤT CẢ:
namespace CRCBypass {
    struct MemoryRegion { ... }
    struct FakedMemoryRegion { ... }
    static const unsigned char kPattern[] = { 0x49, 0x8B, 0x0E, ... }
    inline std::vector<FakedMemoryRegion> g_fakedRegions;
    inline std::vector<BYTE> g_originalStubBytes;
    inline void __fastcall CheckMemoryBlocks(...)
    inline void AddPatchAddress(...)
    static void* BuildHookStub(...)
    static bool WriteStubJmp(...)
    inline bool Install()          // ← method cũ
    inline void Uninstall()        // ← method cũ
}
```

**Xóa Deferred installer (line 1583–1634):**

```
// XÓA:
inline volatile LONG g_crcInstallStarted = 0;
inline volatile LONG g_crcInstallShutdown = 0;
inline void ResetDeferredCRCInstallShutdown() { ... }
inline void RequestDeferredCRCInstallShutdown() { ... }
inline DWORD WINAPI DeferredCRCInstallThread(LPVOID) { ... }
```

**Cập nhật header comment (line 1–22):** Xóa mô tả method cũ, thay bằng mô tả method mới.

### 2.3. Xóa trong `Corehook.h`

**Xóa `AddPatchAddress` loop (line 1102–1110):**

```cpp
// XÓA:
DbgLogFmt("[CRC] %s CRC Bypass: registering target 0x%llX (stolen=%d bytes)\r\n",
          kHookSpecs[id].name, (unsigned long long)targetAddr, CoreEventHook::detail::kJmpPatchSize);
for (int i = 0; i < CoreEventHook::detail::kJmpPatchSize; ++i) {
    CRCBypass::AddPatchAddress(targetAddr + i);
}
DbgLogFmt("[CRC] %s CRC Bypass: fakedRegions=%zu fakedReady=%d\r\n",
          kHookSpecs[id].name, CRCBypass::g_fakedRegions.size(), (int)CRCBypass::g_fakedReady);
```

→ Hooks patch freely, không cần đăng ký faked regions.

### 2.4. Kiểm tra callers khác

```
grep -r "AddPatchAddress\|CheckMemoryBlocks\|CRCBypass::Install\b\|CRCBypass::Uninstall\b\|g_fakedRegions\|g_fakedReady\|DeferredCRCInstall\|g_crcInstallStarted\|g_crcInstallShutdown\|ResetDeferredCRCInstall\|RequestDeferredCRCInstall" Nightsharp/
```

Xóa/gọi thay thế tất cả references.

---

## 3. Bước 2: Enable Master Switch

`PackmanHook.h` line 45-46:
```c
#ifndef NIGHTSHARP_ENABLE_CRC_BYPASS
#define NIGHTSHARP_ENABLE_CRC_BYPASS 1    // ← đổi từ 0 → 1
#endif
```

---

## 4. Bước 3: Implement Method Mới — `CRCBypass` namespace

Thay toàn bộ namespace `CRCBypass` cũ bằng implementation mới:

```cpp
// ============================================================================
// CRC Bypass — Shadow Copy + NOP JNE
// ============================================================================
//
// Method mới (thay cho ChaCha20 QR hook đã bị detect):
//   1) Shadow Copy: memcpy toàn bộ stub.dll → RW buffer TRƯỚC khi patch.
//      Bản sao sạch giữ CRC values đúng cho mọi section.
//   2) NOP JNE: patch 6 bytes tại CRC comparison point (cmp + jne).
//      jne → nop x6 → mismatch handler unreachable.
//
// Không inline hook, không faked regions, không AddPatchAddress.
// Footprint: 1 VirtualAlloc (shadow) + 6 bytes NOP (jne).
// ============================================================================

namespace CRCBypass {

// ── State ────────────────────────────────────────────────────────────────────
inline volatile LONG g_primaryInstalled = 0;
inline volatile LONG g_shadowCreated    = 0;

// NOP JNE state
inline uintptr_t g_crcJneAddr   = 0;
inline uint8_t   g_crcJneOrig[6] = {};

// Shadow copy state
inline void*     g_shadowBase   = nullptr;
inline size_t    g_shadowSize   = 0;
inline uintptr_t g_stubBase     = 0;

// ── Shadow Copy ──────────────────────────────────────────────────────────────
// Tạo bản sao sạch stub.dll TRƯỚC khi patch.
// Dùng cho fallback redirect nếu NOP JNE bị phát hiện.
inline bool CreateShadowCopy() {
    if (InterlockedCompareExchange(&g_shadowCreated, 1, 0) != 0) return true;

    HMODULE hStub = GetModuleHandleA("stub.dll");
    if (!hStub) { g_shadowCreated = 0; return false; }

    g_stubBase = reinterpret_cast<uintptr_t>(hStub);

    // Lấy size từ PE header
    auto dosHdr = reinterpret_cast<const IMAGE_DOS_HEADER*>(g_stubBase);
    if (dosHdr->e_magic != IMAGE_DOS_SIGNATURE) { g_shadowCreated = 0; return false; }
    auto ntHdr = reinterpret_cast<const IMAGE_NT_HEADERS64*>(g_stubBase + dosHdr->e_lfanew);
    g_shadowSize = ntHdr->OptionalHeader.SizeOfImage;  // ~20.7MB

    // Allocate shadow (RW, không cần execute)
    g_shadowBase = VirtualAlloc(nullptr, g_shadowSize,
                                MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!g_shadowBase) { g_shadowCreated = 0; return false; }

    // Copy toàn bộ stub.dll → shadow (BẢN SẠCH)
    std::memcpy(g_shadowBase, reinterpret_cast<const void*>(g_stubBase), g_shadowSize);
    return true;
}

inline void DestroyShadowCopy() {
    if (g_shadowBase) {
        VirtualFree(g_shadowBase, 0, MEM_RELEASE);
        g_shadowBase = nullptr;
    }
    g_shadowSize    = 0;
    g_stubBase      = 0;
    g_shadowCreated = 0;
}

// ── NOP JNE ──────────────────────────────────────────────────────────────────
// AOB: 48 3B 05 ?? ?? ?? ?? 0F 85 → cmp rax,[rip+disp32] + jne
// 1 match trong stub.dll code section.
// Patch jne (6 bytes tại match+7) → 90 90 90 90 90 90
inline bool InstallPrimary() {
    if (InterlockedCompareExchange(&g_primaryInstalled, 1, 0) != 0) return true;

    const uint8_t* stubBase = reinterpret_cast<const uint8_t*>(g_stubBase);
    if (!stubBase) { g_primaryInstalled = 0; return false; }

    // Scan code section cho pattern: cmp rax,[rip+disp32] + jne
    // 48 3B 05 ?? ?? ?? ?? 0F 85
    for (unsigned i = 0; i + 9 <= 0x20000; ++i) {
        if (stubBase[i]     != 0x48) continue;
        if (stubBase[i+1]   != 0x3B) continue;
        if (stubBase[i+2]   != 0x05) continue;
        if (stubBase[i+8]   != 0x0F) continue;
        if (stubBase[i+9]   != 0x85) continue;

        // jne tại match + 7
        g_crcJneAddr = reinterpret_cast<uintptr_t>(stubBase) + i + 7;
        std::memcpy(g_crcJneOrig, reinterpret_cast<const void*>(g_crcJneAddr), 6);

        // Patch: jne → nop x6
        DWORD oldProt = 0;
        if (!DirectSyscall::VirtualProtectDirect(
                reinterpret_cast<void*>(g_crcJneAddr), 6,
                PAGE_EXECUTE_READWRITE, &oldProt)) {
            if (!VirtualProtect(reinterpret_cast<void*>(g_crcJneAddr), 6,
                                PAGE_EXECUTE_READWRITE, &oldProt))
                break;
        }
        std::memset(reinterpret_cast<void*>(g_crcJneAddr), 0x90, 6);
        DWORD dummy = 0;
        DirectSyscall::VirtualProtectDirect(
            reinterpret_cast<void*>(g_crcJneAddr), 6, oldProt, &dummy);
        FlushInstructionCache(GetCurrentProcess(),
                              reinterpret_cast<void*>(g_crcJneAddr), 6);
        return true;
    }
    g_primaryInstalled = 0;
    return false;
}

inline void UninstallPrimary() {
    if (!g_crcJneAddr) return;
    DWORD oldProt = 0;
    if (!DirectSyscall::VirtualProtectDirect(
            reinterpret_cast<void*>(g_crcJneAddr), 6,
            PAGE_EXECUTE_READWRITE, &oldProt)) {
        VirtualProtect(reinterpret_cast<void*>(g_crcJneAddr), 6,
                       PAGE_EXECUTE_READWRITE, &oldProt);
    }
    std::memcpy(reinterpret_cast<void*>(g_crcJneAddr), g_crcJneOrig, 6);
    DWORD dummy = 0;
    DirectSyscall::VirtualProtectDirect(
        reinterpret_cast<void*>(g_crcJneAddr), 6, oldProt, &dummy);
    FlushInstructionCache(GetCurrentProcess(),
                          reinterpret_cast<void*>(g_crcJneAddr), 6);
    g_crcJneAddr = 0;
    g_primaryInstalled = 0;
}

// ── Combined Install/Uninstall ───────────────────────────────────────────────

inline bool Install() {
    // 1. Shadow copy TRƯỚC (bản sao sạch trước khi patch)
    if (!CreateShadowCopy()) return false;
    // 2. NOP JNE (skip mismatch handler)
    if (!InstallPrimary()) {
        // Shadow copy vẫn available, không abort
        // (có thể dùng cho fallback redirect)
    }
    return true;
}

inline void Uninstall() {
    UninstallPrimary();
    DestroyShadowCopy();
}

} // namespace CRCBypass
```

**Khác biệt vs method cũ:**

| | Method cũ (DETECTED) | Method mới |
|---|---|---|
| Hook | Inline 19 bytes + JMP stub | NOP 6 bytes (no hook) |
| Pattern | `49 8B 0E F3 44 0F 6F 04 29` (ChaCha20 QR) | `48 3B 05 ?? ?? ?? ?? 0F 85` (cmp+jne) |
| Alloc | Executable section mapping (RWX) | 1× VirtualAlloc (RW only) |
| Per-hook | `AddPatchAddress` cho mỗi hook | Không cần — patch freely |
| Register redirect | `CheckMemoryBlocks` sửa r14 | Không — chỉ NOP jne |
| Footprint | 19 bytes patch + stub + faked regions | 6 bytes NOP + shadow copy |

---

## 5. Bước 4: Hook vào `Corehook.h` — `InstallInlineHooks()`

**Thứ tự bắt buộc:** Shadow Copy → NOP JNE → patch hooks.

```cpp
inline int InstallInlineHooks() {
    const uintptr_t base = CoreEventHook::shim::GetGameBase();
    if (!base) {
        Logf("Inline+EPT install failed: game base is null");
        return 0;
    }

    (void)CoreEventHook::stealth::SelfTest(base + Offsets::OnStopCast);
    (void)CoreEventHook::stealth::SelfTestWrite(base + Offsets::OnStopCast);
    (void)CoreEventHook::stealth::SelfTestCoW(base + Offsets::OnStopCast);

    // === CRC Bypass: Shadow Copy + NOP JNE ===
    // 1. Shadow Copy TRƯỚC — bản sao sạch trước khi patch
    // 2. NOP JNE — skip mismatch handler
    if (NIGHTSHARP_ENABLE_CRC_BYPASS) {
        if (!CRCBypass::Install()) {
            Logf("CRC Bypass: Install failed — abort");
            return 0;
        }
    }

    int installed = 0;
    for (int i = 0; i < HookCount; ++i) {
        // ... existing loop — KHÔNG gọi AddPatchAddress nữa
    }
    return installed;
}
```

**Trong hook loop:** Xóa block `AddPatchAddress` (line 1102–1110 cũ). Hooks patch freely.

---

## 6. Bước 5: Uninstall trong `UninstallAll()`

**Thứ tự uninstall ngược:** restore hooks → restore NOP JNE → destroy shadow.

```cpp
inline void UninstallAll() {
    UninstallInlineHooks();
    if (NIGHTSHARP_ENABLE_CRC_BYPASS) {
        CRCBypass::Uninstall();   // = UninstallPrimary() + DestroyShadowCopy()
    }
    CoreEventHook::stealth::Shutdown();
}
```

---

## 7. Bước 6: IssueOrder, CastSpell, OnProcessSpell — KHÔNG CẦN CRC

Vì `CRCBypass::InstallPrimary()` đã bypass tất cả CRC checks + shadow copy available:

```cpp
// OnProcessSpell hook — KHÔNG cần CRC update
// IssueOrder (qua OnProcessSpell) — KHÔNG cần CRC
// ProcessCastSpell — KHÔNG cần CRC
// Tất cả hooks trong kHookSpecs[] hoạt động freely
// KHÔNG gọi AddPatchAddress — đã xóa
```

---

## 8. Flow Hoàn Chỉnh

```
DllMain
  → CoreRuntime::Init()
    → PackmanHook::InitAll() (syscall stubs)
    → CoreHookTest::InstallAllHooks()
      → stealth::SelfTest() (EPT probe)
      → CRCBypass::Install()
        → CreateShadowCopy()     ← 1. Bản sao sạch stub.dll (RW)
        → InstallPrimary()       ← 2. NOP JNE bypass (6 bytes)
      → InstallHook(OnProcessSpell)   ← 3. hook freely (no AddPatchAddress)
      → InstallHook(ProcessCastSpell) ← hook freely
      → InstallHook(OnCreate)         ← hook freely
      → ... (tất cả 29 hooks)
```

**Uninstall (ngược lại):**
```
UninstallAll()
  → UninstallInlineHooks()            ← restore tất cả hooks
  → CRCBypass::Uninstall()
    → UninstallPrimary()              ← restore NOP JNE bytes
    → DestroyShadowCopy()             ← free shadow memory
```

---

## 9. Role Của Shadow Copy Trong Method Này

| Tình huống | NOP JNE | Shadow Copy |
|-----------|---------|-------------|
| CRC check bình thường | Skip (NOP) → pass | Không dùng |
| Anti-cheat self-check NOP pattern | Có thể detect | Fallback: redirect CRC read → shadow |
| NOP JNE bị restore bởi anti-cheat | Fail | Shadow vẫn clean → có thể hook NtRead |
| Game update thay đổi pattern | Scan fail | Shadow không phụ thuộc pattern |
| Crash/kick investigation | — | Dump shadow để compare CRC |

**Shadow copy là safety net, không phải primary bypass.** NOP JNE là primary.

---

## 10. Risk & Mitigation

| Risk | Mitigation |
|------|-----------|
| Pattern `48 3B 05 ?? ?? ?? ?? 0F 85` thay đổi | Secondary: `8B 05 ?? ?? ?? ?? A8 01` (flag check, 2 bytes) |
| Anti-cheat detect NOP trong code | CRC check đã bị skip → không self-check NOP pattern; shadow copy fallback |
| Multiple comparison points | Scan 128KB đầu stub.dll, patch tất cả matches |
| Shadow copy detected | Allocate ở region xa (Windows tự chọn), không adjacent |
| Shadow memory overhead (~20.7MB) | Acceptable — free khi uninstall |
| Packman HWBP detect | `HwBpDetect::CheckAndClear()` đã có trong PackmanHook.h |
| Game update thay đổi pattern | `cmp+jne` là instruction cốt lõi, ít thay đổi giữa versions |
| Anti-cheat restore NOP JNE bytes | Shadow copy vẫn clean → fallback redirect |
| Method cũ vẫn còn code | **Bước 1 xóa sạch** — không để code dead nằm trong binary |

---

## 11. Testing Checklist

### 11.1. Build

- [ ] Compile sau khi xóa method cũ — không error/warning
- [ ] `grep -r "AddPatchAddress\|CheckMemoryBlocks\|g_fakedRegions\|g_fakedReady\|BuildHookStub\|DeferredCRCInstall" Nightsharp/` → 0 matches
- [ ] `NIGHTSHARP_ENABLE_CRC_BYPASS = 1` → compile OK

### 11.2. Runtime

- [ ] Load DLL → `CRCBypass::CreateShadowCopy()` thành công (log: shadow base + size)
- [ ] `CRCBypass::InstallPrimary()` thành công (log: NOP JNE addr)
- [ ] Verify shadow copy = stub.dll (memcmp 0 bytes diff)
- [ ] `OnProcessSpell` hook hit → spells detected
- [ ] `ProcessCastSpell` hook hit → cast detected
- [ ] Play full game → no kick/crash
- [ ] Disable → `UninstallPrimary()` → bytes restored → `DestroyShadowCopy()` → memory freed
- [ ] Pattern scan fail → fallback flag check (`8B 05 ?? A8 01`)
- [ ] Shadow copy fail → abort (không patch hooks)

### 11.3. CE MCP Verification (đã làm)

- [x] AOB scan `48 3B 05 ?? ?? ?? ?? 0F 85` → 1 match trong stub.dll
- [x] Disassembly verify → cmp + jne + lea + call (khớp analysis)
- [x] CRC flag = 0x01 (ACTIVE), Current ≠ Expected (MISMATCH)
- [x] NOP JNE patch (6 bytes) → process alive
- [x] Flag check NOP (2 bytes) → process alive
- [x] Restore + áp dụng NOP JNE → process alive, bypass active

---

## 12. Tóm Tắt Thay Đổi

| File | Dòng | Thay đổi |
|------|------|---------|
| `PackmanHook.h` | 1-22 | Cập nhật header comment — mô tả method mới |
| `PackmanHook.h` | 45-46 | `NIGHTSHARP_ENABLE_CRC_BYPASS 0` → `1` |
| `PackmanHook.h` | 1201-1581 | **XÓA** toàn bộ `namespace CRCBypass` cũ (ChaCha20 QR hook) |
| `PackmanHook.h` | 1583-1634 | **XÓA** Deferred installer (`DeferredCRCInstallThread` etc.) |
| `PackmanHook.h` | thay | **THÊM** `namespace CRCBypass` mới (Shadow Copy + NOP JNE) |
| `Corehook.h` | 1102-1110 | **XÓA** `AddPatchAddress` loop + fakedRegions log |
| `Corehook.h` | `InstallInlineHooks()` | Gọi `CRCBypass::Install()` trước loop (thay cho `AddPatchAddress`) |
| `Corehook.h` | `UninstallAll()` | Gọi `CRCBypass::Uninstall()` (thay cho `CRCBypass::Uninstall()` cũ) |

---

## 13. Tham Chiếu Analysis

| Section trong `CRC_Bypass_Analysis.md` | Liên quan |
|---------------------------------------|-----------|
| §5.3 Method 3: Shadow Copy | `CreateShadowCopy()` |
| §7.4 NOP JNE | `InstallPrimary()` |
| §10.2 Method 8: Shadow+HWBP | Mục tiêu dài hạn (chưa đủ thông tin) |
| §11.5 Method tạm thời | **Plan này** — Shadow Copy + NOP JNE |
| §11.6 Roadmap | Phase 1 = plan này; Phase 2-3 = Method 8 đầy đủ |

---

## 14. So Sánh Method Cũ vs Mới

| Tiêu chí | Method cũ (DETECTED) | Method mới (Shadow + NOP JNE) |
|----------|----------------------|-------------------------------|
| Kỹ thuật | Inline hook ChaCha20 QR + faked regions | NOP 6 bytes + shadow copy |
| Footprint | 19 bytes patch + exec stub + faked regions | 6 bytes NOP + 1 RW alloc |
| Detectability | **CAO** — inline hook + exec alloc + register redirect | **THẤP** — chỉ NOP, không hook |
| Per-hook overhead | `AddPatchAddress` cho mỗi hook | Không — patch freely |
| Stealth | Thấp — pattern `49 8B 0E` dễ scan | Cao — `90 90 90 90 90 90` phổ biến |
| Maintenance | Cao — faked regions + hook stub | Thấp — AOB scan + 6 bytes |
| Verified live | Không (detected) | **Có** — CE MCP 08/08/2026 |

---

*Integration plan — 08/08/2026. Method: Shadow Copy + NOP JNE.*
*Verified via CE MCP: AOB scan + NOP patch + process alive.*
*Verify kỹ thuật: `CRC_Bypass_Analysis.md` §5.3, §7.4, §11.5*
