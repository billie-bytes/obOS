#include "syscall.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    sys_puts("bye\n", 4, COLOR_TXT);
    sys_shutdown();
    
    // Fallback
    __asm__ volatile("cli");
    for(;;) { __asm__ volatile("hlt"); }
    
    return 0;
}
