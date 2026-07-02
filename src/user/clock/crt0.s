global _start
extern main

section .text
_start:
    mov ebp, esp
    
    ; The kernel pushed: [esp]=fake_ret, [esp+4]=argc, [esp+8]=argv_ptr
    mov eax, [esp + 4]  ; Grab actual argc
    mov ebx, [esp + 8]  ; Grab actual argv pointer
    
    ; Push arguments for main(int argc, char* argv[])
    push ebx            ; argv
    push eax            ; argc
    
    ; Call main function
    call main
    
    ; Clean up stack
    add esp, 8
    
    ; Exit program (syscall 10 - terminate process)
    mov eax, 10
    mov ebx, 0
    int 0x30
    
    ; Should not reach here
    cli
    hlt