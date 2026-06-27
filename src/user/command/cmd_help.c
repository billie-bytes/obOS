#include "user/command/syscall.h"
#include "lib/string.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;
    
    sys_puts("Available commands:\n", 20, COLOR_TXT);
    sys_puts("  pwd      - Print working directory\n", 37, COLOR_TXT);
    sys_puts("  ls       - List directory contents\n", 37, COLOR_TXT);
    sys_puts("  cd       - Change directory\n", 30, COLOR_TXT);
    sys_puts("  cat      - Display file contents\n", 35, COLOR_TXT);
    sys_puts("  mkdir    - Create directory\n", 30, COLOR_TXT);
    sys_puts("  echo     - Print text\n", 24, COLOR_TXT);
    sys_puts("  clear    - Clear screen\n", 26, COLOR_TXT);
    sys_puts("  help     - Show this help\n", 28, COLOR_TXT);
    sys_puts("  exit     - Shutdown system\n", 29, COLOR_TXT);
    
    return 0;
}
