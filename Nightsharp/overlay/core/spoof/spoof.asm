
PUBLIC _spoofer_stub
PUBLIC _advanced_spoofer_stub
     
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
     

_advanced_spoofer_stub PROC
  mov r11, rsp
  sub rsp, 58h      ; allocate stack space for the actual call
  and rsp, 0FFFFFFFFFFFFFFF0h ; align stack 16 bytes

  mov [rsp], r11     ; save original rsp at bottom
  mov [rsp + 8h], rbx ; preserve rbx

  ; Read target function and gadget
  mov rbx, rcx       ; rbx = target_fn
  mov r10, rdx       ; r10 = call_gadget

  ; Shift args back
  mov rcx, r8        ; param 1
  mov rdx, r9        ; param 2
  mov r8, [r11 + 28h] ; param 3
  mov r9, [r11 + 30h] ; param 4

  ; Copy stack arguments (from original caller stack to our aligned stack)
  mov rax, [r11 + 38h]
  mov [rsp + 20h], rax
  mov rax, [r11 + 40h]
  mov [rsp + 28h], rax
  mov rax, [r11 + 48h]
  mov [rsp + 30h], rax
  mov rax, [r11 + 50h]
  mov [rsp + 38h], rax
  mov rax, [r11 + 58h]
  mov [rsp + 40h], rax
  
  ; Perform the call (jumps to a "call [rbx]" which pushes a safe return address and jumps to rbx)
  call r10

  ; Restore stack & return
  mov rbx, [rsp + 8h]  ; restore rbx
  mov rsp, [rsp]       ; restore original rsp
  ret
_advanced_spoofer_stub ENDP

END