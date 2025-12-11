global _start
extern main

section .text
_start:
    ; Kernel stack setup:
    ; ESP -> argc
    ; ESP+4 -> argv[0] 
    ; ESP+8 -> argv[1]
    ; ...
    ; ESP+4*(argc+1) -> NULL
    
    ; Base pointer
    mov ebp, esp
    
    ; Get argc and argv from stack
    mov eax, [esp]      ; argc
    lea ebx, [esp + 4]  ; argv
    
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
