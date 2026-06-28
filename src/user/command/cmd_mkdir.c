#include "user/command/syscall.h"
#include "lib/string.h"

int main(int argc, char* argv[]) {
    if (argc < 2) { 
        sys_puts("mkdir: missing operand\n", 23, COLOR_TXT); 
        return 1; 
    }
    
    const char* target_path = argv[1];

    if (sys_stat(target_path, NULL) != 0) {
        sys_puts("mkdir: file or directory already exists\n", 40, COLOR_TXT);
        return 1;
    }
    
    int32_t rc = sys_mkdir(target_path);
    
    if (rc == 0) {
        sys_puts("mkdir: success\n", 15, COLOR_TXT);
        return 0;
    }
    
    sys_puts("mkdir: error code ", 18, COLOR_TXT);
    sys_putchar('0' + (rc * -1), COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
    return 1;
}