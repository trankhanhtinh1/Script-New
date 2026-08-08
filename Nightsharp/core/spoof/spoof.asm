
PUBLIC _spoofer_stub
PUBLIC _hybrid_spoofer_stub

.code

_spoofer_stub PROC
  pop r11 ; poping without setting up stack frame, r11 is the return address (the one in our code)
  add rsp, 8 ; skipping callee reserved space
  mov rax, [rsp + 24] ; dereference shell_param

  mov r10, [rax] ; load shell_param.trampoline
  mov [rsp], r10 ; store address of trampoline as return address

  mov r10, [rax + 8] ; load shell_param.function
  mov [rax + 8], r11 ; store the original return address in shell_param.function

  mov [rax + 16], rbx ; preserve rbx in shell_param.rbx
  lea rbx, fixup
  mov [rax], rbx ; store address of fixup label in shell_param.trampoline
  mov rbx, rax ; preserve address of shell_param in rbx

  jmp r10 ; call shell_param.function

fixup:
  sub rsp, 16
  mov rcx, rbx ; restore address of shell_param
  mov rbx, [rcx + 16] ; restore rbx from shell_param.rbx
  jmp QWORD PTR [rcx + 8] ; jmp to the original return address
_spoofer_stub ENDP

; ---------------------------------------------------------------------------
; _hybrid_spoofer_stub
;
; Hybrid approach: combines return-address spoof (from _spoofer_stub) with
; a clean aligned stack (from _advanced_spoofer_stub), fixing the gadget
; mismatch bug and zeroing the new stack to prevent NightSharp address leaks.
;
; Calling convention (called from C++ spoof_call_hybrid):
;   rcx = target_fn          (function to call)
;   rdx = call_gadget        (address of FF 23 = jmp [rbx] in a system DLL)
;   r8  = arg1               (first arg to target_fn)
;   r9  = arg2               (second arg to target_fn)
;   [rsp+28h] = arg3         (third arg, on stack)
;   [rsp+30h] = arg4         (fourth arg, on stack)
;   [rsp+38h] = arg5         (fifth arg, on stack)
;   ...                      (up to arg8 at [rsp+50h])
;
; When target_fn is running:
;   [rsp]     = call_gadget  (system DLL — SPOOFED return address)
;   [rsp+08..] = target_fn frames on NEW stack (clean, zeroed)
;   rbx       = &shell_param on new stack (not NightSharp .text)
;   No NightSharp addresses visible via stack walk from target_fn
;
; Stack layout on new stack (offsets from new rsp, AFTER sub rsp,8 for align):
;   [rsp+00h] = call_gadget         (return addr for target_fn)
;   [rsp+08h] = 0                   (shadow space for rcx, zeroed)
;   [rsp+10h] = 0                   (shadow space for rdx, zeroed)
;   [rsp+18h] = 0                   (shadow space for r8, zeroed)
;   [rsp+20h] = 0                   (shadow space for r9, zeroed)
;   [rsp+28h] = arg5                (first stack arg for target_fn)
;   [rsp+30h] = arg6
;   [rsp+38h] = arg7
;   [rsp+40h] = arg8
;   [rsp+48h] = shell_param.trampoline  = fixup address
;   [rsp+50h] = shell_param.function    = original return address
;   [rsp+58h] = shell_param.rbx         = saved rbx
;   [rsp+60h] = original rsp
; ---------------------------------------------------------------------------
_hybrid_spoofer_stub PROC
  mov r11, rsp                     ; r11 = original rsp (caller stack)

  ; Allocate new stack region (144 bytes, 16-byte aligned, then -8 for call convention)
  sub rsp, 90h
  and rsp, 0FFFFFFFFFFFFFFF0h      ; align 16 bytes → rsp = 0 mod 16
  sub rsp, 8                       ; simulate call push → rsp = 8 mod 16 (ABI requirement)

  ; Zero entire used stack region (0x68 = 104 bytes, prevent leak of old NightSharp addresses)
  xor rax, rax
  mov [rsp], rax
  mov [rsp+08h], rax
  mov [rsp+10h], rax
  mov [rsp+18h], rax
  mov [rsp+20h], rax
  mov [rsp+28h], rax
  mov [rsp+30h], rax
  mov [rsp+38h], rax
  mov [rsp+40h], rax
  mov [rsp+48h], rax
  mov [rsp+50h], rax
  mov [rsp+58h], rax
  mov [rsp+60h], rax

  ; Save target_fn and gadget before overwriting arg registers
  mov r10, rcx                     ; r10 = target_fn
  mov rax, rdx                     ; rax = call_gadget

  ; Build shell_param on new stack at [rsp+48h..60h]
  mov [rsp+60h], r11               ; save original rsp
  mov [rsp+58h], rbx               ; save rbx
  mov rbx, [r11]                   ; load original return address
  mov [rsp+50h], rbx               ; shell_param.function = original_ret
  lea rbx, hybrid_fixup
  mov [rsp+48h], rbx               ; shell_param.trampoline = fixup

  ; Set return address for target_fn = gadget (FF 23 = jmp [rbx])
  mov [rsp], rax                   ; [rsp] = call_gadget

  ; Set rbx = &shell_param.trampoline (gadget will do jmp [rbx] = jmp fixup)
  lea rbx, [rsp+48h]

  ; Shift args: target_fn gets (arg1, arg2, arg3, arg4, arg5, ...)
  mov rcx, r8                      ; arg1
  mov rdx, r9                      ; arg2
  mov r8, [r11+28h]                ; arg3
  mov r9, [r11+30h]                ; arg4

  ; Copy stack args (arg5+) to correct positions on new stack
  ; Windows x64: arg5 at [rsp+28h], arg6 at [rsp+30h], etc.
  mov rax, [r11+38h]
  mov [rsp+28h], rax               ; arg5
  mov rax, [r11+40h]
  mov [rsp+30h], rax               ; arg6
  mov rax, [r11+48h]
  mov [rsp+38h], rax               ; arg7
  mov rax, [r11+50h]
  mov [rsp+40h], rax               ; arg8

  ; Jump to target_fn (return addr = gadget already on [rsp])
  jmp r10

hybrid_fixup:
  ; Reached after: target_fn ret -> gadget (FF 23 = jmp [rbx]) -> hybrid_fixup
  ; rbx = &shell_param.trampoline (absolute address, still valid)
  ; [rbx+00h] = trampoline (fixup addr, not needed)
  ; [rbx+08h] = function (original return address)
  ; [rbx+10h] = saved rbx
  ; [rbx+18h] = original rsp (r11, return addr still on stack)
  ; IMPORTANT: do NOT touch RAX — it holds the return value from target_fn!
  mov r10, [rbx+18h]               ; r10 = original rsp (r11)
  mov rdx, [rbx+08h]               ; rdx = original return address
  mov rcx, [rbx+10h]               ; rcx = saved rbx
  mov rbx, rcx                     ; restore rbx
  lea rsp, [r10+8]                 ; restore rsp (skip return addr, as if ret'd)
  jmp rdx                          ; return to original caller (RAX preserved!)
_hybrid_spoofer_stub ENDP

END