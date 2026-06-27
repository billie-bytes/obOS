#include "user/command/syscall.h"
#include "lib/string.h"

static void print_number(uint32_t n) {
    if (n == 0) { sys_putchar('0', COLOR_TXT); return; }
    char buf[12]; int i = 0;
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    while (i > 0) sys_putchar(buf[--i], COLOR_TXT);
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    ProcessInfo proc_list[16];
    // Pass the element count (16), not the byte size
    int32_t count = sys_ps(proc_list, 16);
    
    if (count <= 0) {
        sys_puts("No processes found\n", 19, COLOR_TXT);
        return 0;
    }
    
    sys_puts("PID  STATE    NAME\n", 18, COLOR_TXT);
    sys_puts("---  -------  --------\n", 23, COLOR_TXT);
    
    for (int32_t i = 0; i < count; i++) {
        // Print PID
        print_number(proc_list[i].pid);
        
        // Spacing
        if (proc_list[i].pid < 10) sys_puts("    ", 4, COLOR_TXT);
        else if (proc_list[i].pid < 100) sys_puts("   ", 3, COLOR_TXT);
        else sys_puts("  ", 2, COLOR_TXT);
        
        // Print state
        const char* state_str = "UNKNOWN";
        uint32_t state_len = 7;
        if (proc_list[i].state == PROCESS_RUNNING) { state_str = "RUNNING"; }
        else if (proc_list[i].state == PROCESS_READY) { state_str = "READY  "; }
        else if (proc_list[i].state == PROCESS_TERMINATED) { state_str = "TERM   "; }
        sys_puts(state_str, state_len, COLOR_TXT);
        sys_puts("  ", 2, COLOR_TXT);
        
        // Print name
        if (proc_list[i].name_len > 0) {
            uint32_t len = proc_list[i].name_len;
            if (len > 8) len = 8;
            sys_puts(proc_list[i].name, len, COLOR_TXT);
        }
        sys_putchar('\n', COLOR_TXT);
    }
    
    return 0;
}
