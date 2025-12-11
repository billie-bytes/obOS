#include "syscall.h"
#include "../header/stdlib/string.h"

// Syscall 12: Kill process by PID
static inline int32_t sys_kill(uint32_t pid) {
    int32_t retval;
    syscall_do(12, pid, 0, 0);
    __asm__ volatile("mov %%eax, %0" : "=r"(retval));
    return retval;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        sys_puts("kill: missing PID\n", 18, COLOR_TXT);
        return 1;
    }
    
    // Parse PID
    uint32_t pid = 0;
    for (char* p = argv[1]; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            pid = pid * 10 + (*p - '0');
        } else {
            sys_puts("kill: invalid PID\n", 18, COLOR_TXT);
            return 1;
        }
    }
    
    int32_t result = sys_kill(pid);
    
    if (result == 0) {
        sys_puts("kill: process ", 14, COLOR_TXT);
        sys_puts(argv[1], strlen(argv[1]), COLOR_TXT);
        sys_puts(" terminated\n", 12, COLOR_TXT);
    } else {
        sys_puts("kill: process not found or cannot be killed\n", 45, COLOR_TXT);
        return 1;
    }
    
    return 0;
}
