global process_context_switch

process_context_switch:
    ; Context switch is a way we can do multiple process concurrently.
    ; The general consessus are each process are isolated from each other and each time a 2 process does a context switch, OS is involved.
    ; If the OS is involved, then we need a way to go from a user process to kernel and go back to user process (Ring 3 -> Ring 0 -> Ring 3).
    ; Right now, in context switch, we are in Ring 0 and we need to go back to Ring 3.
    ; The method we are gonna be doing: iret.
    ; in order to satisfy iret, push SS -> ESP -> EFLAGS -> CS -> EIP
    ; Step 1: find a pointer that point to the data of a struct Context because right now esp is pointing at return address (old eip)
    ;         since address takes up 4 bytes (32 bit), we need a placeholder register to point the actual data of a context, 
    ;         starting from right above the return address which is (esp + 0x04)
    lea ecx, [esp + 0x04]

    ; Now we got a pointer that point to struct Context, we can now push the neccessary register to satisfy iret
    ; Step 2: to find SS register, we need to calculate the SS offset which is:
    ; sizeof(CPURegister) + sizeof(eip) + sizeof(cs) + sizeof(eflags) + sizeof(esp)
    ;       0x30          +     0x4     +     0x4    +      0x4       +     0x4     = 0x40
    ; ss
    mov eax, [ecx + 0x40]
    push eax

    ; Step 3: do the same for esp (offset = 0x3c), eflags (offset = 0x38), cs (offset = 0x34), eip (offset = 0x30) [already calculated on process.h]
    ; esp
    mov eax, [ecx + 0x3c]
    push eax

    ; eflags
    mov eax, [ecx + 0x38]
    push eax

    ; cs
    mov eax, [ecx + 0x34]
    push eax

    ; eip
    mov eax, [ecx + 0x30]
    push eax

    ; Step 4: load the next PCB to the cpu register (we can do at any order except we need to push ecx and eax in the last order)
    ; Register segment
    mov ds, [ecx + 0x2C]
    mov es, [ecx + 0x28] 
    mov fs, [ecx + 0x24]
    mov gs, [ecx + 0x20]

    ; General purpose register (except for eax & ecx)
    mov edi, [ecx + 0x00]
    mov esi, [ecx + 0x04]
    mov ebp, [ecx + 0x08]
    mov ebx, [ecx + 0x10]
    mov edx, [ecx + 0x14]

    ; Load eax
    mov eax, [ecx + 0x1C]

    ; Load ecx
    mov ecx, [ecx + 0x18]

    ; Step 5: enter Ring 3
    iret
