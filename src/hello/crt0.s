global _start
extern main

section .text
_start:
    ; Setup stack
    mov ebp, esp
    
    ; Clear registers
    xor eax, eax
    xor ebx, ebx
    xor ecx, ecx
    xor edx, edx
    xor esi, esi
    xor edi, edi
    
    ; Call main function
    call main
    
    ; Exit program (syscall 10 - process termination)
    mov eax, 10
    int 0x30
    
    ; Should never reach here
    cli
    hlt
