# Phân tích SpoofCall IssueOrder/CastSpell — Rủi ro & Đề xuất

> Phân tích bằng CE MCP trên `League of Legends.exe` PID 22032 (2026-08-08)
> Base: `0x7FF68DD00000` | stub.dll: `0x7FFAF6BC0000`

## 1. Cơ chế gọi hiện tại

### Call chain
```
NightSharp DLL (Orbwalker/Combo/Spell)
  → CoreControl::IssueOrderDetailed()           [CoreControl.h:422]
    → spoof_call(spoofTrampoline, issueOrderFn, args...)   [CoreControl.h:512]
      → _spoofer_stub (spoof.asm)
        → jmp issueOrderFn (game module)
          → [native call chain]
            → SendNetworkPacket (packet ra network)
```

### spoof_call (_spoofer_stub) — cách hoạt động

```asm
_spoofer_stub:
  pop r11              ; r11 = return addr (caller trong NightSharp)
  add rsp, 8           ; skip callee reserved
  mov rax, [rsp + 24]  ; rax = shell_param (stack)
  mov r10, [rax]       ; r10 = FF 23 gadget (game module)
  mov [rsp], r10       ; ★ OVERWRITE return addr → FF 23 gadget
  mov r10, [rax + 8]   ; r10 = IssueOrder fn
  mov [rax + 8], r11   ; shell_param.function = original return
  mov [rax + 16], rbx  ; shell_param.rbx = saved rbx
  lea rbx, fixup       ; rbx = fixup label (NightSharp .text)
  mov [rax], rbx       ; shell_param.trampoline = fixup
  mov rbx, rax         ; ★ rbx = shell_param (stack ptr)
  jmp r10              ; → IssueOrder

fixup:
  sub rsp, 16
  mov rcx, rbx         ; rcx = shell_param
  mov rbx, [rcx + 16]  ; restore rbx
  jmp [rcx + 8]        ; → original return (NightSharp caller)
```

**Khi IssueOrder đang chạy:**
- Stack top: `FF 23 gadget` (game module) ← **SPOOFED ✓**
- RBX: `shell_param` (stack address trong NightSharp frame) ← **LEAK ✗**
- shell_param trên stack: `[fixup, original_ret, saved_rbx]` ← **LEAK ✗**
- Deeper frames: NightSharp DLL addresses ← **LEAK ✗**

**Khi IssueOrder `ret`:**
- → `FF 23 gadget`: `jmp [rbx]` = `jmp [shell_param]` = `jmp fixup`
- → `fixup`: restore rbx, `jmp [shell_param+8]` = original return

---

## 2. Verification CE MCP — Packman hook status

### Các hàm trong call chain — KIỂM TRA BYTE ĐẦU

| Hàm | RVA | Address | Byte đầu | Trạng thái |
|-----|-----|---------|----------|------------|
| IssueOrder | 0x28E4F0 | 0x7FF68DFEE4F0 | `8B 01` (mov eax,[rcx]) | ✅ Nguyên vẹn |
| CastSpellSafe | 0xBE7460 | 0x7FF68E8E7460 | `40 57` (push rdi) | ✅ Nguyên vẹn |
| SendSpellCastPacket | 0x989920 | 0x7FF68E689920 | `40 53` (push rbx) | ✅ Nguyên vẹn |
| SendNetworkPacket | 0x6FB9B0 | 0x7FF68E3FB9B0 | `44 3B 0D` (cmp r9d,[..]) | ✅ Nguyên vẹn |
| BuildCastPacket | 0x970350 | 0x7FF68E670350 | `48 89 5C 24 08` (mov [rsp+8],rbx) | ✅ Nguyên vẹn |

**Kết luận:** Packman KHÔNG inline hook bất kỳ hàm nào trong call chain IssueOrder/CastSpell.
Tất cả đều có prologue nguyên vẹn, không có `FF 25` (jmp [rip]) signature.

### SpoofTrampoline (FF 23 gadget)

- Pattern `FF 23` scan toàn process: 100+ kết quả
- Không có kết quả trong range `League of Legends.exe` (0x7FF68DD00000+)
- `ForEachExecutableSection` scan tất cả executable sections, không chỉ game module
- → SpoofTrampoline có thể nằm trong system DLL hoặc mapped memory

### NightSharp DLL trong module list

- `enum_modules` trả về 94 modules — **KHÔNG có NightSharp.dll**
- → PEB scrub đang hoạt động (module đã ẩn khỏi PEB)

---

## 3. Đánh giá rủi ro

### ✅ AN TOÀN (verified)

1. **IssueOrder/CastSpellSafe/SendNetworkPacket không bị Packman hook**
   - Byte đầu nguyên vẹn tại tất cả 5 hàm trong call chain
   - Packman không thể intercept tại điểm này

2. **Return address top-of-stack = FF 23 gadget (game module)**
   - Khi IssueOrder `ret`, nhảy về gadget trong game module
   - Stack walk 1 level: thấy game module address ✓

3. **NightSharp DLL ẩn khỏi PEB**
   - `enum_modules` không thấy NightSharp
   - Module enumeration từ PEB sẽ bỏ qua

### ⚠️ RỦI RO TRUNG BÌNH

1. **RBX leak khi IssueOrder đang chạy**
   - `mov rbx, rax` → RBX = shell_param (stack address)
   - Nếu Packman capture thread context (GetThreadContext) tại IssueOrder entry:
     - RBX = stack pointer → có thể trace về NightSharp frame
   - **Nhưng:** IssueOrder không bị hook, Packman không có cơ hội đọc RBX tại đây

2. **shell_param trên stack**
   - Structure `[fixup, original_ret, saved_rbx]` nằm trên NightSharp stack frame
   - Nếu Packman scan stack memory (VirtualQuery + MEM_PRIVATE scan):
     - Thấy NightSharp .text addresses (fixup, original_ret)
   - **Nhưng:** Stack memory là MEM_PRIVATE, Packman cần scan toàn bộ stack region

### 🔴 RỦI RO CAO

1. **Deeper stack frames vẫn là NightSharp addresses**
   - spoof_call chỉ che return address IMMEDIATE (top of stack)
   - Frame phía dưới: `CoreControl::IssueOrderDetailed`, `Orbwalker::Combo`, v.v.
   - Nếu Packman walk stack (RtlVirtualUnwind / StackWalk64):
     - Frame 0: IssueOrder (game module) ✓
     - Frame 1: FF 23 gadget (game module) ✓
     - Frame 2: fixup (_spoofer_stub trong NightSharp) ✗ **LEAK**
     - Frame 3+: NightSharp DLL ✗ **LEAK**

2. **_spoofer_stub trong NightSharp .text**
   - Nếu Packman scan executable memory cho pattern đặc trưng:
     - `pop r11; add rsp,8; mov rax,[rsp+24]` = `41 5B 48 83 C4 08 48 8B 44 24 18`
     - Signature này có thể bị flag

3. **Thread context scan (periodic)**
   - Nếu Packman periodically GetThreadContext trên game thread:
     - RIP trong NightSharp DLL khi đang chạy Orbwalker logic
     - RSP trỏ đến NightSharp stack frame
   - → Phát hiện NightSharp đang chạy code trên game thread

---

## 4. Đề xuất cải thiện

### 4.1. Dùng _advanced_spoofer_stub (đã có sẵn trong spoof.asm)

**Hiện trạng:** `_advanced_spoofer_stub` đã implement nhưng KHÔNG được dùng.
`spoof_call` template chỉ gọi `_spoofer_stub` (basic).

**Cải thiện:** `_advanced_spoofer_stub`:
- Allocate aligned stack riêng (`sub rsp, 58h; and rsp, ~0xF`)
- Copy args sang new stack
- Call qua gadget
- Restore stack gốc
- → **New stack không có NightSharp addresses** → che deeper frames

**Implementation:**
```cpp
// spoofcall.h — thêm advanced path
template <typename Ret, typename... Args>
static inline auto spoof_call_advanced(
    void* trampoline,
    Ret(*fn)(Args...),
    Args... args
) -> Ret {
    // Dùng _advanced_spoofer_stub thay vì _spoofer_stub
    // RCX = fn, RDX = trampoline, R8+ = args
    // ...
}
```

### 4.2. Thread hijack (queue APC vào game thread)

**Concept:** Thay vì gọi IssueOrder trực tiếp từ NightSharp thread,
queue APC vào game thread chính. Game thread sẽ gọi IssueOrder
với call stack hoàn toàn từ game thread.

**Ưu điểm:**
- Call stack 100% từ game thread, không có NightSharp addresses
- Không cần spoof_call
- Không cần FF 23 gadget

**Nhược điểm:**
- Cần sync với game thread (APC chỉ chạy khi game thread alertable)
- Latency cao hơn (chờ APC dispatch)
- Phải identify game thread chính

### 4.3. Hardware breakpoint spoof (Blindside)

**Concept:** Đặt HW breakpoint tại IssueOrder entry. Khi hit:
- Modify context: set RIP = target, set args
- Clear HW BP
- Continue thread
- → Call stack hoàn toàn clean (không có NightSharp frame)

**ưu điểm:**
- Invisible từ OS level (HW BP không show trong debug APIs)
- Call stack clean

**Nhược điểm:**
- Cần DBVM hoặc kernel driver để đặt HW BP ẩn
- Chỉ 4 HW BP available
- Complex implementation

### 4.4. Stack scrub sau mỗi call

**Concept:** Sau khi spoof_call return, scrub stack region:
- Zero out shell_param structure
- Zero out deeper frames (nếu có thể)
- → Nếu Packman scan stack sau khi call, không thấy gì

**Ưu điểm:** Đơn giản, không cần thay đổi call mechanism

**Nhược điểm:**
- Race condition: Packman có thể scan trong khi call đang chạy
- Không che được khi IssueOrder đang execute

### 4.5. Đề xuất ưu tiên

| Method | Effort | Effectiveness | Priority |
|--------|--------|---------------|----------|
| _advanced_spoofer_stub | Thấp | Cao (che deeper frames) | **1** |
| Stack scrub | Thấp | Trung bình | **2** |
| Thread hijack (APC) | Cao | Rất cao | 3 |
| HW BP spoof (DBVM) | Rất cao | Tuyệt đối | 4 |

**Khuyến nghị:** Bắt đầu với `_advanced_spoofer_stub` (đã có sẵn code, chỉ cần wire up)
+ stack scrub sau mỗi call. Đây là quick win lớn nhất.

---

## 5. Kết luận

**Trả lời câu hỏi "có an toàn không?":**

1. **IssueOrder/CastSpell KHÔNG bị Packman hook** → không bị intercept tại hàm
2. **spoof_call che được return address immediate** → stack walk 1-2 level an toàn
3. **NHƯNG deeper stack frames vẫn leak NightSharp addresses** → nếu Packman walk stack sâu hơn, sẽ phát hiện
4. **RBX + shell_param trên stack** → leak nếu Packman đọc register/stack memory tại IssueOrder

**Mức độ rủi ro:** TRUNG BÌNH
- Packman không hook call chain → không có cơ hội inspect tại IssueOrder
- Nhưng nếu Packman có periodic stack walk / thread context scan → sẽ phát hiện

**Hành động ngay:**
1. Wire up `_advanced_spoofer_stub` (đã có sẵn trong spoof.asm)
2. Thêm stack scrub sau mỗi spoof_call
3. Verify `_advanced_spoofer_stub` hoạt động đúng với 5+ args
