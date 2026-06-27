#include "user/command/syscall.h"
#include "lib/string.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    char current_path[256];
    sys_getcwd(current_path, sizeof(current_path));
    
    sys_puts(current_path, strlen(current_path), COLOR_TXT);
    sys_putchar('\n', COLOR_TXT);
    
    return 0;
}
